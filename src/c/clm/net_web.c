#include "net_web.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "thread.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define WEB_TIMEOUT_NONE 0
#define WEB_TIMEOUT_IDLE 1
#define WEB_TIMEOUT_READ 2
#define WEB_TIMEOUT_WRITE 3
/* Internal status: the response tail is queued on the connection for the reactor. */
#define WEB_SEND_PENDING ((clm_int)-100)
/* Internal status: an async handler asked to be resumed; the exchange is parked. */
#define WEB_REQUEST_SUSPENDED ((clm_int)-101)

typedef struct {
    char* name;
    char* value;
} clm_net_web_owned_header;

typedef struct clm_net_web_route {
    char* method;
    char* pattern;
    clm_net_web_handler_fn handler;
    void* context;
    bool async;
} clm_net_web_route;

typedef struct clm_net_web_middleware_entry {
    clm_net_web_middleware_fn callback;
    void* context;
} clm_net_web_middleware_entry;

typedef struct clm_net_web_context_value {
    char* key;
    void* value;
    clm_net_web_value_dispose_fn dispose;
    struct clm_net_web_context_value* next;
} clm_net_web_context_value;

typedef struct clm_net_web_connection clm_net_web_connection;

typedef struct clm_net_web_task_ref {
    clm_id task_id;
    clm_net_web_connection* connection;
    struct clm_net_web_task_ref* next;
    struct clm_net_web_task_ref* prev; /* Doubly linked so reaping is O(1). */
} clm_net_web_task_ref;

struct clm_net_web_context {
    clm_id request_id;
    struct clm_net_web_server* server;
    clm_net_web_connection* connection;
    atomic_bool cancelled;
    clm_net_web_context_value* values;
};

struct clm_net_web_response {
    struct clm_net_web_server* server;
    clm_net_web_connection* connection;
    clm_http_utils_span request_method;
    clm_uint status;
    clm_net_web_owned_header* headers;
    size_t header_count;
    size_t header_capacity;
    clm_bytes_buffer body;
    bool keep_alive;
    bool accepts_gzip;
    bool committed;
    bool chunked;
    bool no_body;
    bool failed;
    bool http10;
    bool raw_stream;
    bool allow_park;
};

struct clm_net_web_connection {
    struct clm_net_web_server* server;
    clm_id socket_id;
    clm_id task_id;
    clm_net_address peer;
    clm_tls_session* tls;
    clm_async_task_token token;
    clm_bytes_buffer input;
    clm_http_utils_header* headers;
    size_t request_number;
    atomic_uint waiting_events;
    atomic_bool task_finished;
    atomic_bool cancelled;
    atomic_int timeout_kind;
    atomic_uint_fast64_t deadline_ms;
    uint64_t last_activity_ms;
    bool tls_ready;
    bool continue_sent;
    unsigned char* out_data;
    size_t out_length;
    size_t out_offset;
    size_t out_capacity;
    bool close_after_send;
    bool holds_stream_slot;
    /* Async routes: while true (worker-side only), peer I/O queues or reports
     * CLM_NET_WEB_AGAIN instead of waiting; async_events records what blocked. */
    bool nonblocking;
    unsigned int async_events;
    /* A suspended async request, resumed by the step function. */
    struct web_exchange* exchange;
    struct clm_net_web_task_ref* task;
    /* Reactor-thread-only state: interest registered with the poller, and the
     * position in the deadline heap (WEB_HEAP_NONE when absent). */
    unsigned int poll_events;
    size_t heap_index;
    uint64_t heap_deadline_ms;
    /* Guarded by server->queue_mutex. The two flags make queuing idempotent. */
    bool park_queued;
    bool finish_queued;
    struct clm_net_web_connection* park_prev;
    struct clm_net_web_connection* park_next;
    struct clm_net_web_connection* finish_next;
};

#define WEB_HEAP_NONE ((size_t)-1)
/* Upper bound for one poller wait. Everything that matters wakes the reactor
 * explicitly; this only bounds how long a missed wake-up could be delayed. */
#define WEB_REACTOR_MAX_WAIT_MS 1000
/* Poll interval while a finished task has not yet reached a terminal pool state. */
#define WEB_REACTOR_REAP_RETRY_MS 2

typedef enum web_chunk_state {
    WEB_CHUNK_SIZE,
    WEB_CHUNK_DATA,
    WEB_CHUNK_DATA_END,
    WEB_CHUNK_TRAILERS,
    WEB_CHUNK_DONE
} web_chunk_state;

#define WEB_CHUNK_LINE_LIMIT 4096
/* Events fetched per poller wait; extra ready sockets are reported by the next wait. */
#define WEB_POLL_BATCH 256
#define WEB_CHUNK_TRAILER_LIMIT 8192

struct clm_net_web_body_reader {
    clm_net_web_connection* connection;
    size_t remaining;
    bool continue_pending;
    bool chunked;
    web_chunk_state chunk_state;
    size_t chunk_remaining;
    size_t decoded_total;
    size_t limit;
    size_t trailer_bytes;
    clm_int error;
    /* A chunk-size/trailer line being assembled. It lives here, not on the stack,
     * because a nonblocking read can stop mid-line and resume later. */
    size_t line_length;
    char line[WEB_CHUNK_LINE_LIMIT];
};

/* Everything one request needs between handler calls. Normally it lives on the
 * stack of web_handle_request; only a request whose async handler returns AGAIN is
 * moved to the heap and hung off connection->exchange. */
typedef struct web_exchange {
    clm_net_web_request request;
    clm_net_web_context context;
    clm_net_web_response response;
    clm_net_web_body_reader reader;
    clm_http_utils_message message;
    clm_net_web_route* route;       /* Set once an async handler is running. */
    char* stable_headers;           /* Owned; header spans point into it. */
    clm_bytes_buffer transfer_decoded;
    clm_bytes_buffer content_decoded;
    bool keep_alive;
    bool accepts_gzip;
    bool has_reader;
} web_exchange;

struct clm_net_web_server {
    clm_net_web_config config;
    clm_tls_options tls_options;
    char* certificate_file;
    char* private_key_file;
    char* certificate_password;
    clm_net_web_route* routes;
    size_t route_count;
    size_t route_capacity;
    clm_net_web_middleware_entry* middleware;
    size_t middleware_count;
    size_t middleware_capacity;
    clm_id listener_id;
    clm_id pool_id;
    clm_net_address local_address;
    thrd_t listener_thread;
    bool listener_thread_created;
    bool listener_open;
    bool pool_open;
    bool started;
    bool stopped;
    atomic_bool stopping;
    /* Requests currently allowed to block a worker on peer I/O; see max_streaming_connections. */
    atomic_size_t streaming_active;
    /* Owned and used by the listener thread only while it runs. */
    clm_net_poller* poller;
    clm_net_web_connection** heap;  /* Min-heap of parked connections by deadline. */
    size_t heap_count;
    size_t heap_capacity;
    /* Workers hand work to the reactor through these lists, so the reactor never
     * scans every connection: connections that just parked (park_head) and
     * connections whose task finished (finish_head). Both use queue_mutex. */
    mtx_t queue_mutex;
    bool reactor_open;
    struct clm_net_web_connection* park_head;
    struct clm_net_web_connection* finish_head;
    /* The reactor sleeps inside the poller, so a worker interrupts it by sending a
     * datagram to wake_receive_id. wake_pending coalesces bursts into one datagram. */
    clm_id wake_receive_id;
    clm_id wake_send_id;
    clm_net_address wake_address;
    atomic_bool wake_pending;
    size_t connection_count;
    clm_net_web_task_ref* tasks;
};

typedef struct {
    clm_net_web_connection* connection;
} clm_net_web_stream_writer;

static atomic_uint_fast64_t clm_net_web_ids = ATOMIC_VAR_INIT(UINT64_C(0xc100000000000001));
static uint64_t web_now_ms(void);
static clm_int web_send_continue(clm_net_web_connection* connection);

/* Streaming slots are per request: acquired lazily, released by the step function
 * after the handler returns, whichever path (body streaming or flush) took it. */
static bool web_stream_acquire(clm_net_web_connection* connection)
{
    if (connection->holds_stream_slot)
        return true;
    clm_net_web_server* server = connection->server;
    size_t active = atomic_load_explicit(&server->streaming_active, memory_order_relaxed);
    while (active < server->config.max_streaming_connections) {
        if (atomic_compare_exchange_weak_explicit(&server->streaming_active, &active,
                active + 1, memory_order_acq_rel, memory_order_relaxed)) {
            connection->holds_stream_slot = true;
            return true;
        }
    }
    return false;
}

static void web_stream_release(clm_net_web_connection* connection)
{
    if (!connection->holds_stream_slot)
        return;
    connection->holds_stream_slot = false;
    (void)atomic_fetch_sub_explicit(&connection->server->streaming_active, 1,
        memory_order_acq_rel);
}
static clm_async_step_result web_connection_read_more(clm_net_web_connection* connection);

static clm_id web_next_id(void)
{
    uint64_t value = atomic_fetch_add_explicit(&clm_net_web_ids, 1, memory_order_relaxed);
    return value ? (clm_id)value : (clm_id)atomic_fetch_add_explicit(
        &clm_net_web_ids, 1, memory_order_relaxed);
}

static char* web_copy_string(const char* text)
{
    if (!text)
        return NULL;
    size_t length = strlen(text);
    if (length == (size_t)-1)
        return NULL;
    char* copy = malloc(length + 1);
    if (copy)
        memcpy(copy, text, length + 1);
    return copy;
}

static bool web_span_equal_ci(const char* left, size_t left_length, const char* right)
{
    return clm_http_utils_span_equals_ignore_case(
        (clm_http_utils_span){left, left_length}, right);
}

/* Blocks the calling worker until the socket is ready. Waits in 100 ms slices so a
 * server stop is noticed promptly. This is the synchronous path used by streaming
 * handlers and TLS; ordinary request/response I/O parks on the reactor instead. */
static clm_int web_wait(clm_net_web_connection* connection, unsigned int event)
{
    uint32_t timeout_ms = (event & CLM_NET_EVENT_WRITE)
        ? connection->server->config.write_timeout_ms
        : connection->server->config.read_timeout_ms;
    uint64_t start_ms = web_now_ms();
    for (;;) {
        if (atomic_load_explicit(&connection->server->stopping, memory_order_acquire))
            return CLM_NET_WEB_BUSY;
        uint64_t current_ms = web_now_ms();
        uint64_t elapsed = current_ms >= start_ms ? current_ms - start_ms : 0;
        if (elapsed >= timeout_ms)
            return CLM_NET_WEB_BUSY;
        uint64_t remaining = timeout_ms - elapsed;
        uint32_t slice = (uint32_t)(remaining > 100 ? 100 : remaining);
        unsigned int observed = 0;
        clm_net_status status = clm_net_wait(connection->socket_id, event, slice, &observed);
        if (status == CLM_NET_TIMEOUT)
            continue;
        if (status != CLM_NET_SUCCESS)
            return CLM_NET_WEB_SYSTEM_ERROR;
        return (observed & event) ? CLM_NET_WEB_SUCCESS : CLM_NET_WEB_SYSTEM_ERROR;
    }
}

static const char* web_input_data(const clm_bytes_buffer* input)
{
    return input->storage ? (const char*)input->storage + input->offset : "";
}

/* Nonblocking handler I/O applies to TLS too: the TLS session is always nonblocking, and
 * WANT_READ/WANT_WRITE map onto the same park/suspend events as a socket that would block. */
static bool web_nonblocking(const clm_net_web_connection* connection)
{
    return connection->nonblocking;
}

static unsigned int web_tls_event(clm_int status)
{
    return status == CLM_TLS_WANT_WRITE ? CLM_NET_EVENT_WRITE : CLM_NET_EVENT_READ;
}

/* One transport write. SUCCESS means *written (> 0) bytes were accepted, AGAIN means
 * the transport needs *event first, anything else is fatal. Over TLS, accepted bytes
 * may still sit encrypted in the session until web_transport_flush() succeeds. */
static clm_int web_transport_send(clm_net_web_connection* connection, const void* data,
                                  size_t length, size_t* written, unsigned int* event)
{
    *written = 0;
    if (connection->tls) {
        clm_int status = clm_tls_write(connection->tls, data, length, written);
        if (status == CLM_TLS_SUCCESS && *written)
            return CLM_NET_WEB_SUCCESS;
        if (status == CLM_TLS_WANT_READ || status == CLM_TLS_WANT_WRITE) {
            *event = web_tls_event(status);
            return CLM_NET_WEB_AGAIN;
        }
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    clm_net_status status = clm_net_send(connection->socket_id, data, length, written);
    if (status == CLM_NET_SUCCESS && *written)
        return CLM_NET_WEB_SUCCESS;
    if (status == CLM_NET_WOULD_BLOCK) {
        *event = CLM_NET_EVENT_WRITE;
        return CLM_NET_WEB_AGAIN;
    }
    return CLM_NET_WEB_SYSTEM_ERROR;
}

/* Pushes ciphertext the TLS session is still holding; a no-op for plain sockets. */
static clm_int web_transport_flush(clm_net_web_connection* connection, unsigned int* event)
{
    if (!connection->tls)
        return CLM_NET_WEB_SUCCESS;
    clm_int status = clm_tls_flush(connection->tls);
    if (status == CLM_TLS_SUCCESS)
        return CLM_NET_WEB_SUCCESS;
    if (status == CLM_TLS_WANT_READ || status == CLM_TLS_WANT_WRITE) {
        *event = web_tls_event(status);
        return CLM_NET_WEB_AGAIN;
    }
    return CLM_NET_WEB_SYSTEM_ERROR;
}

/* Appends to the connection's output queue, preserving order with anything queued. */
static clm_int web_out_append(clm_net_web_connection* connection, const void* data, size_t length)
{
    if (!length)
        return CLM_NET_WEB_SUCCESS;
    size_t pending = connection->out_length - connection->out_offset;
    if (connection->out_data && connection->out_offset) {
        memmove(connection->out_data, connection->out_data + connection->out_offset, pending);
        connection->out_length = pending;
        connection->out_offset = 0;
    }
    if (length > SIZE_MAX - pending)
        return CLM_NET_WEB_LIMIT_EXCEEDED;
    size_t required = pending + length;
    if (required > connection->out_capacity) {
        size_t capacity = connection->out_capacity ? connection->out_capacity : 4096;
        while (capacity < required)
            capacity = capacity > SIZE_MAX / 2 ? required : capacity * 2;
        unsigned char* grown = realloc(connection->out_data, capacity);
        if (!grown)
            return CLM_NET_WEB_NO_MEMORY;
        connection->out_data = grown;
        connection->out_capacity = capacity;
    }
    memcpy(connection->out_data + connection->out_length, data, length);
    connection->out_length += length;
    return CLM_NET_WEB_SUCCESS;
}

/* Sends as much of the queue as the socket takes. Returns SUCCESS once empty (the
 * buffer is freed), AGAIN if the socket would block, or SYSTEM_ERROR. */
static clm_int web_out_drain(clm_net_web_connection* connection)
{
    unsigned int event = 0;
    while (connection->out_offset < connection->out_length) {
        size_t written = 0;
        clm_int status = web_transport_send(connection,
            connection->out_data + connection->out_offset,
            connection->out_length - connection->out_offset, &written, &event);
        if (status == CLM_NET_WEB_SUCCESS) {
            connection->out_offset += written;
            continue;
        }
        if (status == CLM_NET_WEB_AGAIN) {
            connection->async_events |= event;
            return CLM_NET_WEB_AGAIN;
        }
        atomic_store_explicit(&connection->cancelled, true, memory_order_release);
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    clm_int flushed = web_transport_flush(connection, &event);
    if (flushed == CLM_NET_WEB_AGAIN) {
        connection->async_events |= event;
        return CLM_NET_WEB_AGAIN;
    }
    if (flushed != CLM_NET_WEB_SUCCESS) {
        atomic_store_explicit(&connection->cancelled, true, memory_order_release);
        return flushed;
    }
    free(connection->out_data);
    connection->out_data = NULL;
    connection->out_length = connection->out_offset = connection->out_capacity = 0;
    return CLM_NET_WEB_SUCCESS;
}

static clm_int web_receive(clm_net_web_connection* connection, void* buffer,
                           size_t capacity, size_t* received)
{
    if (connection->tls) {
        for (;;) {
            clm_int status = clm_tls_read(connection->tls, buffer, capacity, received);
            if (status == CLM_TLS_SUCCESS)
                return CLM_NET_WEB_SUCCESS;
            if (status == CLM_TLS_CLOSED)
                return CLM_NET_WEB_NOT_FOUND;
            if (status != CLM_TLS_WANT_READ && status != CLM_TLS_WANT_WRITE)
                return status == CLM_TLS_NO_MEMORY ? CLM_NET_WEB_NO_MEMORY
                                                   : CLM_NET_WEB_SYSTEM_ERROR;
            unsigned int event = web_tls_event(status);
            if (connection->nonblocking) {
                connection->async_events |= event;
                return CLM_NET_WEB_AGAIN;
            }
            clm_int wait_status = web_wait(connection, event);
            if (wait_status != CLM_NET_WEB_SUCCESS)
                return wait_status;
        }
    }
    for (;;) {
        clm_net_status status = clm_net_receive(connection->socket_id, buffer, capacity, received);
        if (status == CLM_NET_SUCCESS)
            return CLM_NET_WEB_SUCCESS;
        if (status == CLM_NET_CLOSED)
            return CLM_NET_WEB_NOT_FOUND;
        if (status != CLM_NET_WOULD_BLOCK)
            return CLM_NET_WEB_SYSTEM_ERROR;
        if (connection->nonblocking) {
            connection->async_events |= CLM_NET_EVENT_READ;
            return CLM_NET_WEB_AGAIN;
        }
        clm_int wait_status = web_wait(connection, CLM_NET_EVENT_READ);
        if (wait_status != CLM_NET_WEB_SUCCESS)
            return wait_status;
    }
}

static clm_int web_send(clm_net_web_connection* connection, const void* data,
                        size_t length)
{
    if (web_nonblocking(connection)) {
        /* Always accepts the data: it is queued and drained opportunistically.
         * Back-pressure is applied by response_flush(), not here. */
        clm_int queued = web_out_append(connection, data, length);
        if (queued != CLM_NET_WEB_SUCCESS)
            return queued;
        clm_int drained = web_out_drain(connection);
        return drained == CLM_NET_WEB_AGAIN ? CLM_NET_WEB_SUCCESS : drained;
    }
    const unsigned char* bytes = data;
    size_t offset = 0;
    while (offset < length) {
        size_t written = 0;
        unsigned int event = 0;
        clm_int status = web_transport_send(connection, bytes + offset, length - offset,
            &written, &event);
        if (status == CLM_NET_WEB_SUCCESS) {
            offset += written;
            continue;
        }
        if (status != CLM_NET_WEB_AGAIN) {
            atomic_store_explicit(&connection->cancelled, true, memory_order_release);
            return CLM_NET_WEB_SYSTEM_ERROR;
        }
        clm_int wait_status = web_wait(connection, event);
        if (wait_status != CLM_NET_WEB_SUCCESS) {
            if (wait_status != CLM_NET_WEB_BUSY)
                atomic_store_explicit(&connection->cancelled, true, memory_order_release);
            return wait_status;
        }
    }
    for (;;) {
        unsigned int event = 0;
        clm_int status = web_transport_flush(connection, &event);
        if (status == CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_SUCCESS;
        if (status != CLM_NET_WEB_AGAIN) {
            atomic_store_explicit(&connection->cancelled, true, memory_order_release);
            return CLM_NET_WEB_SYSTEM_ERROR;
        }
        clm_int wait_status = web_wait(connection, event);
        if (wait_status != CLM_NET_WEB_SUCCESS) {
            if (wait_status != CLM_NET_WEB_BUSY)
                atomic_store_explicit(&connection->cancelled, true, memory_order_release);
            return wait_status;
        }
    }
}

static clm_int web_stream_write(clm_stream_sink* sink, const void* data,
                                size_t length, size_t* written)
{
    clm_net_web_stream_writer* writer = sink->user;
    clm_int status = web_send(writer->connection, data, length);
    if (status == CLM_NET_WEB_SUCCESS) {
        *written = length;
        return CLM_STREAM_SUCCESS;
    }
    return status == CLM_NET_WEB_BUSY ? CLM_STREAM_WOULD_BLOCK : CLM_STREAM_FAILURE;
}

static clm_int web_send_view(clm_net_web_connection* connection, clm_bytes_view view)
{
    clm_net_web_stream_writer writer = {connection};
    clm_stream_source source;
    clm_stream_sink sink;
    if (clm_stream_source_memory(&source, view) != CLM_STREAM_SUCCESS ||
        clm_stream_sink_init(&sink, web_stream_write, NULL, NULL, &writer) != CLM_STREAM_SUCCESS)
        return CLM_NET_WEB_SYSTEM_ERROR;
    clm_int status = clm_stream_copy(&source, &sink, NULL, NULL);
    return status == CLM_STREAM_SUCCESS ? CLM_NET_WEB_SUCCESS : CLM_NET_WEB_SYSTEM_ERROR;
}

static clm_int web_send_chunk(clm_net_web_connection* connection,
                              const void* data, size_t length)
{
    char header[2 * sizeof(size_t) + 4];
    int header_length = snprintf(header, sizeof(header), "%zx\r\n", length);
    if (header_length <= 0 || (size_t)header_length >= sizeof(header))
        return CLM_NET_WEB_SYSTEM_ERROR;
    clm_int status = web_send(connection, header, (size_t)header_length);
    if (status == CLM_NET_WEB_SUCCESS && length)
        status = web_send(connection, data, length);
    if (status == CLM_NET_WEB_SUCCESS)
        status = web_send(connection, "\r\n", 2);
    return status;
}

static clm_int web_send_stream(clm_net_web_response* response,
                               const void* data, size_t length)
{
    return response->raw_stream
        ? web_send(response->connection, data, length)
        : web_send_chunk(response->connection, data, length);
}

static clm_net_web_span web_path_from_target(clm_http_utils_span target)
{
    clm_http_utils_span path = clm_http_utils_path_from_target(target);
    return (clm_net_web_span){path.data, path.length};
}

static bool web_route_match(const clm_net_web_route* route, clm_net_web_span path,
                            clm_net_web_param** out_params, size_t* out_count,
                            size_t* out_score)
{
    size_t max_params = 1;
    for (const char* p = route->pattern; *p; ++p)
        if (*p == '/')
            ++max_params;
    clm_net_web_param* params = calloc(max_params, sizeof(*params));
    if (!params)
        return false;
    size_t route_pos = 0, path_pos = 0, count = 0, score = 0;
    size_t route_length = strlen(route->pattern);
    bool matched = true;
    while (route_pos < route_length || path_pos < path.length) {
        if (route_pos >= route_length || path_pos >= path.length ||
            route->pattern[route_pos] != '/' || path.data[path_pos] != '/') {
            matched = false;
            break;
        }
        ++route_pos;
        ++path_pos;
        size_t route_end = route_pos;
        while (route_end < route_length && route->pattern[route_end] != '/')
            ++route_end;
        size_t path_end = path_pos;
        while (path_end < path.length && path.data[path_end] != '/')
            ++path_end;
        size_t route_part_length = route_end - route_pos;
        size_t path_part_length = path_end - path_pos;
        if (route_part_length && route->pattern[route_pos] == '*') {
            if (route_end != route_length || route_part_length == 1) {
                matched = false;
                break;
            }
            params[count++] = (clm_net_web_param){
                {route->pattern + route_pos + 1, route_part_length - 1},
                {path.data + path_pos, path.length - path_pos}};
            path_pos = path.length;
            route_pos = route_length;
            score += 1;
            break;
        }
        if (route_part_length && route->pattern[route_pos] == ':') {
            if (route_part_length == 1 || path_part_length == 0 || count == max_params) {
                matched = false;
                break;
            }
            params[count++] = (clm_net_web_param){
                {route->pattern + route_pos + 1, route_part_length - 1},
                {path.data + path_pos, path_part_length}};
            score += 1;
        } else {
            if (route_part_length != path_part_length ||
                memcmp(route->pattern + route_pos, path.data + path_pos, route_part_length) != 0) {
                matched = false;
                break;
            }
            score += route_part_length + 16;
        }
        route_pos = route_end;
        path_pos = path_end;
    }
    if (route_pos != route_length || path_pos != path.length)
        matched = false;
    if (!matched) {
        free(params);
        return false;
    }
    *out_params = params;
    *out_count = count;
    *out_score = score;
    return true;
}

static clm_net_web_route* web_find_route(clm_net_web_server* server,
                                         clm_net_web_request* request,
                                         bool* method_not_allowed)
{
    clm_net_web_route* best = NULL;
    clm_net_web_param* best_params = NULL;
    size_t best_count = 0, best_score = 0;
    *method_not_allowed = false;
    for (size_t i = 0; i < server->route_count; ++i) {
        clm_net_web_param* params = NULL;
        size_t count = 0, score = 0;
        if (!web_route_match(&server->routes[i], request->path, &params, &count, &score))
            continue;
        bool method_matches = strcmp(server->routes[i].method, "*") == 0 ||
            clm_http_utils_span_equals(request->message.method, server->routes[i].method);
        if (!method_matches) {
            *method_not_allowed = true;
            free(params);
            continue;
        }
        if (!best || score > best_score) {
            free(best_params);
            best = &server->routes[i];
            best_params = params;
            best_count = count;
            best_score = score;
        } else {
            free(params);
        }
    }
    request->params = best_params;
    request->param_count = best_count;
    return best;
}

static void web_context_clear(clm_net_web_context* context)
{
    clm_net_web_context_value* value = context->values;
    while (value) {
        clm_net_web_context_value* next = value->next;
        if (value->dispose)
            value->dispose(value->value);
        free(value->key);
        free(value);
        value = next;
    }
}

bool clm_net_web_context_is_cancelled(const clm_net_web_context* context)
{
    return !context || atomic_load_explicit(&context->cancelled, memory_order_acquire) ||
        (context->connection && atomic_load_explicit(
            &context->connection->cancelled, memory_order_acquire)) ||
        atomic_load_explicit(&context->server->stopping, memory_order_acquire);
}

clm_id clm_net_web_context_request_id(const clm_net_web_context* context)
{
    return context ? context->request_id : 0;
}

clm_int clm_net_web_context_set(clm_net_web_context* context, const char* key,
                                void* value, clm_net_web_value_dispose_fn dispose)
{
    if (!context || !key || !*key)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    for (clm_net_web_context_value* current = context->values; current; current = current->next) {
        if (strcmp(current->key, key) == 0) {
            if (current->dispose)
                current->dispose(current->value);
            current->value = value;
            current->dispose = dispose;
            return CLM_NET_WEB_SUCCESS;
        }
    }
    clm_net_web_context_value* item = calloc(1, sizeof(*item));
    if (!item)
        return CLM_NET_WEB_NO_MEMORY;
    item->key = web_copy_string(key);
    if (!item->key) {
        free(item);
        return CLM_NET_WEB_NO_MEMORY;
    }
    item->value = value;
    item->dispose = dispose;
    item->next = context->values;
    context->values = item;
    return CLM_NET_WEB_SUCCESS;
}

void* clm_net_web_context_get(const clm_net_web_context* context, const char* key)
{
    if (!context || !key)
        return NULL;
    for (clm_net_web_context_value* item = context->values; item; item = item->next)
        if (strcmp(item->key, key) == 0)
            return item->value;
    return NULL;
}

bool clm_net_web_request_header(const clm_net_web_request* request,
                                const char* name, clm_http_utils_span* out_value)
{
    return request && clm_http_utils_find_header(&request->message, name, out_value);
}

bool clm_net_web_request_param(const clm_net_web_request* request,
                               const char* name, clm_net_web_span* out_value)
{
    if (!request || !name || !out_value)
        return false;
    for (size_t i = 0; i < request->param_count; ++i)
        if (web_span_equal_ci(request->params[i].name.data,
                request->params[i].name.length, name)) {
            *out_value = request->params[i].value;
            return true;
        }
    return false;
}

static bool web_reader_pending(const clm_net_web_body_reader* reader)
{
    return reader->chunked ? reader->chunk_state != WEB_CHUNK_DONE : reader->remaining != 0;
}

/* Guarantees at least one unread byte in the connection input buffer. */
static clm_int web_reader_fill(clm_net_web_body_reader* reader, size_t max_bytes)
{
    clm_bytes_buffer* input = &reader->connection->input;
    if (input->length)
        return CLM_NET_WEB_SUCCESS;
    unsigned char* tail = NULL;
    size_t free_space = 0;
    if (clm_bytes_buffer_prepare(input, 1, &tail, &free_space) != CLM_BYTES_SUCCESS || !free_space)
        return CLM_NET_WEB_LIMIT_EXCEEDED;
    if (free_space > max_bytes)
        free_space = max_bytes;
    size_t received = 0;
    clm_int status = web_receive(reader->connection, tail, free_space, &received);
    if (status != CLM_NET_WEB_SUCCESS)
        return status == CLM_NET_WEB_NOT_FOUND ? CLM_NET_WEB_SYSTEM_ERROR : status;
    if (!received || clm_bytes_buffer_commit(input, received) != CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_SYSTEM_ERROR;
    return CLM_NET_WEB_SUCCESS;
}

/* Reads one CRLF-terminated line into reader->line without its terminator. A bare LF
 * or a CR anywhere but directly before the LF is rejected: lenient framing here would
 * disagree with a stricter proxy and enable request smuggling. Progress is kept in the
 * reader, so AGAIN can be returned mid-line and the call repeated later. */
static clm_int web_chunk_read_line(clm_net_web_body_reader* reader, size_t* out_length)
{
    for (;;) {
        clm_int status = web_reader_fill(reader, SIZE_MAX);
        if (status != CLM_NET_WEB_SUCCESS)
            return status;
        clm_bytes_buffer* input = &reader->connection->input;
        char c = (char)clm_bytes_buffer_view(input).data[0];
        if (clm_bytes_buffer_consume(input, 1) != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_SYSTEM_ERROR;
        if (c == '\n') {
            size_t length = reader->line_length;
            if (!length || reader->line[length - 1] != '\r')
                return CLM_NET_WEB_INVALID_ARGUMENT;
            --length;
            reader->line_length = 0;
            *out_length = length;
            return CLM_NET_WEB_SUCCESS;
        }
        if (reader->line_length && reader->line[reader->line_length - 1] == '\r')
            return CLM_NET_WEB_INVALID_ARGUMENT;
        if (reader->line_length >= sizeof(reader->line))
            return CLM_NET_WEB_LIMIT_EXCEEDED;
        reader->line[reader->line_length++] = c;
    }
}

static clm_int web_chunk_parse_size(const char* line, size_t length, size_t* out_size)
{
    clm_int status = clm_http_utils_parse_chunk_size(line, length, out_size);
    if (status == CLM_HTTP_UTILS_SUCCESS)
        return CLM_NET_WEB_SUCCESS;
    return status == CLM_HTTP_UTILS_LIMIT_EXCEEDED
        ? CLM_NET_WEB_LIMIT_EXCEEDED : CLM_NET_WEB_INVALID_ARGUMENT;
}

static clm_int web_chunked_read(clm_net_web_body_reader* reader,
                                void* buffer, size_t capacity, size_t* out_read)
{
    for (;;) {
        size_t length = 0;
        clm_int status;
        switch (reader->chunk_state) {
        case WEB_CHUNK_SIZE: {
            size_t size = 0;
            status = web_chunk_read_line(reader, &length);
            if (status == CLM_NET_WEB_SUCCESS)
                status = web_chunk_parse_size(reader->line, length, &size);
            if (status != CLM_NET_WEB_SUCCESS)
                return status;
            if (size > reader->limit - reader->decoded_total)
                return CLM_NET_WEB_LIMIT_EXCEEDED;
            reader->chunk_remaining = size;
            reader->chunk_state = size ? WEB_CHUNK_DATA : WEB_CHUNK_TRAILERS;
            break;
        }
        case WEB_CHUNK_DATA: {
            status = web_reader_fill(reader, reader->chunk_remaining);
            if (status != CLM_NET_WEB_SUCCESS)
                return status;
            clm_bytes_buffer* input = &reader->connection->input;
            clm_bytes_view pending = clm_bytes_buffer_view(input);
            size_t count = capacity < reader->chunk_remaining ? capacity : reader->chunk_remaining;
            if (count > pending.length)
                count = pending.length;
            memcpy(buffer, pending.data, count);
            if (clm_bytes_buffer_consume(input, count) != CLM_BYTES_SUCCESS)
                return CLM_NET_WEB_SYSTEM_ERROR;
            reader->chunk_remaining -= count;
            reader->decoded_total += count;
            if (!reader->chunk_remaining)
                reader->chunk_state = WEB_CHUNK_DATA_END;
            *out_read = count;
            return CLM_NET_WEB_SUCCESS;
        }
        case WEB_CHUNK_DATA_END:
            status = web_chunk_read_line(reader, &length);
            if (status != CLM_NET_WEB_SUCCESS)
                return status;
            if (length)
                return CLM_NET_WEB_INVALID_ARGUMENT;
            reader->chunk_state = WEB_CHUNK_SIZE;
            break;
        case WEB_CHUNK_TRAILERS:
            status = web_chunk_read_line(reader, &length);
            if (status != CLM_NET_WEB_SUCCESS)
                return status;
            if (!length) {
                reader->chunk_state = WEB_CHUNK_DONE;
                break;
            }
            reader->trailer_bytes += length;
            if (reader->trailer_bytes > WEB_CHUNK_TRAILER_LIMIT)
                return CLM_NET_WEB_LIMIT_EXCEEDED;
            if (clm_http_utils_validate_trailer(reader->line, length) != CLM_HTTP_UTILS_SUCCESS)
                return CLM_NET_WEB_INVALID_ARGUMENT;
            break;
        case WEB_CHUNK_DONE:
            *out_read = 0;
            return CLM_NET_WEB_END;
        }
    }
}

clm_int clm_net_web_request_read_body(
    clm_net_web_request* request, void* buffer, size_t capacity, size_t* out_read)
{
    if (request && request->body_reader) {
        clm_net_web_body_reader* reader = request->body_reader;
        if (!buffer || !capacity || !out_read || !reader->connection)
            return CLM_NET_WEB_INVALID_ARGUMENT;
        if (!web_reader_pending(reader)) {
            *out_read = 0;
            return CLM_NET_WEB_END;
        }
        if (reader->continue_pending) {
            clm_int continue_status = web_send_continue(reader->connection);
            if (continue_status != CLM_NET_WEB_SUCCESS)
                return continue_status;
            reader->continue_pending = false;
        }
        /* Decoder errors are sticky: after a malformed chunk the stream position is
         * unknown, so later reads (including the keep-alive discard loop) must keep
         * failing instead of resynchronising on whatever bytes follow. */
        if (reader->error)
            return reader->error;
        if (reader->chunked) {
            clm_int chunk_status = web_chunked_read(reader, buffer, capacity, out_read);
            if (chunk_status != CLM_NET_WEB_SUCCESS && chunk_status != CLM_NET_WEB_END &&
                chunk_status != CLM_NET_WEB_AGAIN)
                reader->error = chunk_status;
            return chunk_status;
        }
        size_t wanted = capacity < reader->remaining ? capacity : reader->remaining;
        clm_bytes_buffer* input = &reader->connection->input;
        clm_int fill_status = web_reader_fill(reader, wanted);
        if (fill_status != CLM_NET_WEB_SUCCESS)
            return fill_status;
        clm_bytes_view pending = clm_bytes_buffer_view(input);
        size_t count = wanted < pending.length ? wanted : pending.length;
        memcpy(buffer, pending.data, count);
        if (clm_bytes_buffer_consume(input, count) != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_SYSTEM_ERROR;
        reader->remaining -= count;
        *out_read = count;
        return CLM_NET_WEB_SUCCESS;
    }
    if (!request || !buffer || !capacity || !out_read ||
        request->body_offset > request->body.length ||
        (!request->body.data && request->body.length))
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (request->body_offset == request->body.length) {
        *out_read = 0;
        return CLM_NET_WEB_END;
    }
    size_t count = request->body.length - request->body_offset;
    if (count > capacity)
        count = capacity;
    memcpy(buffer, request->body.data + request->body_offset, count);
    request->body_offset += count;
    *out_read = count;
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_request_multipart(
    const clm_net_web_request* request, clm_multipart_part* parts,
    size_t part_capacity, clm_multipart_header* headers,
    size_t header_capacity, clm_multipart_message* out_message)
{
    if (!request || !out_message)
        return CLM_MULTIPART_INVALID_ARGUMENT;
    clm_http_utils_span content_type;
    if (!clm_net_web_request_header(request, "Content-Type", &content_type))
        return CLM_MULTIPART_FAILURE;
    char boundary[72];
    size_t required = 0;
    clm_int status = clm_multipart_boundary_from_content_type(
        (clm_multipart_span){content_type.data, content_type.length},
        boundary, sizeof(boundary), &required);
    if (status != CLM_MULTIPART_SUCCESS)
        return status;
    return clm_multipart_parse(request->body.data, request->body.length,
        (clm_multipart_span){boundary, required - 1}, parts, part_capacity,
        headers, header_capacity, out_message);
}

static clm_int web_response_header_set(clm_net_web_response* response,
                                       const char* name, const char* value)
{
    if (!response || response->committed || !clm_http_utils_is_token(name) || !value)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    for (const unsigned char* p = (const unsigned char*)value; *p; ++p)
        if (*p == '\r' || *p == '\n')
            return CLM_NET_WEB_INVALID_ARGUMENT;
    for (size_t i = 0; i < response->header_count; ++i) {
        if (web_span_equal_ci(response->headers[i].name,
                strlen(response->headers[i].name), name)) {
            char* replacement = web_copy_string(value);
            if (!replacement)
                return CLM_NET_WEB_NO_MEMORY;
            free(response->headers[i].value);
            response->headers[i].value = replacement;
            return CLM_NET_WEB_SUCCESS;
        }
    }
    if (response->header_count == response->header_capacity) {
        size_t next = response->header_capacity ? response->header_capacity * 2 : 8;
        if (next > response->server->config.max_headers)
            next = response->server->config.max_headers;
        if (next <= response->header_capacity)
            return CLM_NET_WEB_LIMIT_EXCEEDED;
        clm_net_web_owned_header* resized = realloc(response->headers, next * sizeof(*resized));
        if (!resized)
            return CLM_NET_WEB_NO_MEMORY;
        response->headers = resized;
        response->header_capacity = next;
    }
    char* name_copy = web_copy_string(name);
    char* value_copy = web_copy_string(value);
    if (!name_copy || !value_copy) {
        free(name_copy);
        free(value_copy);
        return CLM_NET_WEB_NO_MEMORY;
    }
    response->headers[response->header_count++] = (clm_net_web_owned_header){name_copy, value_copy};
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_response_set_status(clm_net_web_response* response, clm_uint status)
{
    if (!response || response->committed || status < 100 || status > 599)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    response->status = status;
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_response_set_header(
    clm_net_web_response* response, const char* name, const char* value)
{
    return web_response_header_set(response, name, value);
}

clm_int clm_net_web_response_write(clm_net_web_response* response,
                                   const void* data, size_t length)
{
    if (!response || (!data && length))
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (response->failed)
        return CLM_NET_WEB_SYSTEM_ERROR;
    if (response->committed) {
        if (response->no_body)
            return CLM_NET_WEB_SUCCESS;
        if (!response->chunked && !response->raw_stream)
            return CLM_NET_WEB_INVALID_ARGUMENT;
        const unsigned char* bytes = data;
        size_t offset = 0;
        while (offset < length) {
            size_t count = length - offset;
            if (count > 16384)
                count = 16384;
            clm_int send_status = web_send_stream(response, bytes + offset, count);
            if (send_status != CLM_NET_WEB_SUCCESS) {
                response->failed = true;
                return send_status;
            }
            offset += count;
        }
        return CLM_NET_WEB_SUCCESS;
    }
    clm_int status = clm_bytes_buffer_append(&response->body, data, length);
    if (status == CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_SUCCESS;
    return status == CLM_BYTES_NO_MEMORY ? CLM_NET_WEB_NO_MEMORY : CLM_NET_WEB_LIMIT_EXCEEDED;
}

clm_int clm_net_web_response_write_string(clm_net_web_response* response, const char* text)
{
    return text ? clm_net_web_response_write(response, text, strlen(text))
        : CLM_NET_WEB_INVALID_ARGUMENT;
}

clm_int clm_net_web_response_redirect(
    clm_net_web_response* response, const char* location, clm_uint status)
{
    if (!response || !location || (status != 301 && status != 302 && status != 303 &&
        status != 307 && status != 308))
        return CLM_NET_WEB_INVALID_ARGUMENT;
    clm_int result = clm_net_web_response_set_header(response, "Location", location);
    if (result == CLM_NET_WEB_SUCCESS)
        result = clm_net_web_response_set_status(response, status);
    return result;
}

clm_int clm_net_web_response_render_template(
    clm_net_web_response* response, const clm_template* tmpl,
    const clm_template_context* context, const clm_template_options* options,
    clm_template_error* error)
{
    if (!response || !tmpl)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    clm_bytes_buffer rendered = {0};
    clm_int status = clm_bytes_buffer_init(&rendered, 0);
    if (status != CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_NO_MEMORY;
    (void)clm_bytes_buffer_set_limit(&rendered, response->server->config.max_response_bytes);
    status = clm_template_render(tmpl, context, options, &rendered, error);
    if (status == CLM_TEMPLATE_SUCCESS) {
        status = clm_net_web_response_write(response,
            rendered.storage ? rendered.storage + rendered.offset : NULL, rendered.length);
        if (status == CLM_NET_WEB_SUCCESS &&
            web_response_header_set(response, "Content-Type", "text/html; charset=utf-8") !=
                CLM_NET_WEB_SUCCESS)
            status = CLM_NET_WEB_NO_MEMORY;
    } else {
        status = status == CLM_TEMPLATE_NO_MEMORY ? CLM_NET_WEB_NO_MEMORY : CLM_NET_WEB_HANDLER_ERROR;
    }
    clm_bytes_buffer_dispose(&rendered);
    return status;
}

static bool web_has_response_header(const clm_net_web_response* response, const char* name)
{
    for (size_t i = 0; i < response->header_count; ++i)
        if (web_span_equal_ci(response->headers[i].name,
                strlen(response->headers[i].name), name))
            return true;
    return false;
}

static void web_remove_response_header(clm_net_web_response* response, const char* name);

static void web_response_dispose(clm_net_web_response* response)
{
    for (size_t i = 0; i < response->header_count; ++i) {
        free(response->headers[i].name);
        free(response->headers[i].value);
    }
    free(response->headers);
    clm_bytes_buffer_dispose(&response->body);
}

static void web_remove_response_header(clm_net_web_response* response, const char* name)
{
    for (size_t i = 0; i < response->header_count; ++i) {
        if (web_span_equal_ci(response->headers[i].name,
                strlen(response->headers[i].name), name)) {
            free(response->headers[i].name);
            free(response->headers[i].value);
            memmove(&response->headers[i], &response->headers[i + 1],
                (response->header_count - i - 1) * sizeof(*response->headers));
            --response->header_count;
            return;
        }
    }
}

static clm_int web_send_chunked_headers(clm_net_web_response* response)
{
    static const char empty_chunk[] = "0\r\n\r\n";
    if (!response || !response->connection || response->status < 200 ||
        response->status == 204 || response->status == 304 ||
        clm_http_utils_span_equals(response->request_method, "HEAD"))
        return CLM_NET_WEB_INVALID_ARGUMENT;

    char date[64];
    if (!web_has_response_header(response, "Date") && clm_http_utils_format_date(date, sizeof(date)))
        (void)web_response_header_set(response, "Date", date);
    if (!web_has_response_header(response, "Server"))
        (void)web_response_header_set(response, "Server", "CLM");
    if (!response->keep_alive ||
        atomic_load_explicit(&response->server->stopping, memory_order_acquire))
        (void)web_response_header_set(response, "Connection", "close");
    web_remove_response_header(response, "Content-Length");
    if (response->http10) {
        web_remove_response_header(response, "Transfer-Encoding");
        (void)web_response_header_set(response, "Connection", "close");
    } else if (web_response_header_set(response, "Transfer-Encoding", "chunked") !=
            CLM_NET_WEB_SUCCESS)
        return CLM_NET_WEB_LIMIT_EXCEEDED;
    if (response->server->config.enable_gzip && response->accepts_gzip &&
        !web_has_response_header(response, "Vary"))
        (void)web_response_header_set(response, "Vary", "Accept-Encoding");

    clm_http_utils_header* headers = calloc(response->header_count + 1, sizeof(*headers));
    if (!headers)
        return CLM_NET_WEB_NO_MEMORY;
    for (size_t i = 0; i < response->header_count; ++i) {
        headers[i].name = (clm_http_utils_span){response->headers[i].name,
            strlen(response->headers[i].name)};
        headers[i].value = (clm_http_utils_span){response->headers[i].value,
            strlen(response->headers[i].value)};
    }
    clm_http_utils_message message;
    clm_http_utils_message_init(&message, CLM_HTTP_UTILS_RESPONSE);
    message.status = response->status;
    message.reason = (clm_http_utils_span){clm_http_utils_reason_phrase(response->status),
        strlen(clm_http_utils_reason_phrase(response->status))};
    message.headers = headers;
    message.header_count = response->header_count;
    message.header_capacity = response->header_count + 1;
    message.body_kind = response->http10
        ? CLM_HTTP_UTILS_BODY_UNTIL_CLOSE : CLM_HTTP_UTILS_BODY_CHUNKED;
    message.body = response->http10 ? (clm_http_utils_span){NULL, 0}
        : (clm_http_utils_span){empty_chunk, sizeof(empty_chunk) - 1};
    size_t required = 0;
    clm_int status = clm_http_utils_serialize(&message, NULL, 0, &required);
    if (status != CLM_HTTP_UTILS_SUCCESS || required < sizeof(empty_chunk)) {
        free(headers);
        return CLM_NET_WEB_HANDLER_ERROR;
    }
    char* wire = malloc(required);
    if (!wire) {
        free(headers);
        return CLM_NET_WEB_NO_MEMORY;
    }
    status = clm_http_utils_serialize(&message, wire, required, &required);
    free(headers);
    if (status == CLM_HTTP_UTILS_SUCCESS) {
        size_t wire_length = required - 1 -
            (response->http10 ? 0 : sizeof(empty_chunk) - 1);
        status = web_send(response->connection, wire, wire_length);
    } else {
        status = CLM_NET_WEB_HANDLER_ERROR;
    }
    free(wire);
    return status;
}

/* backpressure: refuse new data while an earlier flush is still queued. The final
 * flush of a response skips it, because the handler already returned. */
static clm_int web_response_flush(clm_net_web_response* response, bool backpressure)
{
    if (!response || !response->connection)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (response->failed)
        return CLM_NET_WEB_SYSTEM_ERROR;
    bool nonblocking = web_nonblocking(response->connection);
    if (nonblocking && backpressure) {
        /* Back-pressure: accept no new data while the previous flush is still queued. */
        clm_int drained = web_out_drain(response->connection);
        if (drained != CLM_NET_WEB_SUCCESS) {
            if (drained != CLM_NET_WEB_AGAIN)
                response->failed = true;
            return drained;
        }
    }
    if (!response->committed) {
        /* HEAD responses carry no body; keep buffering so Content-Length stays accurate. */
        if (clm_http_utils_span_equals(response->request_method, "HEAD"))
            return CLM_NET_WEB_SUCCESS;
        /* Over the streaming cap: leave the response buffered rather than pin another worker.
         * A nonblocking flush never pins a worker, so it needs no slot. */
        if (!nonblocking && !web_stream_acquire(response->connection))
            return CLM_NET_WEB_BUSY;
        clm_int status = web_send_chunked_headers(response);
        if (status != CLM_NET_WEB_SUCCESS) {
            response->failed = true;
            return status;
        }
        response->committed = true;
        response->chunked = !response->http10;
        response->raw_stream = response->http10;
    }
    clm_bytes_view pending = clm_bytes_buffer_view(&response->body);
    if (!pending.length)
        return CLM_NET_WEB_SUCCESS;
    clm_int status = web_send_stream(response, pending.data, pending.length);
    if (status != CLM_NET_WEB_SUCCESS) {
        response->failed = true;
        return status;
    }
    (void)clm_bytes_buffer_clear(&response->body);
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_response_flush(clm_net_web_response* response)
{
    return web_response_flush(response, true);
}

static clm_int web_send_response(clm_net_web_connection* connection,
                                 clm_net_web_response* response,
                                 clm_http_utils_span request_method,
                                 bool keep_alive, bool accepts_gzip)
{
    if (response->committed) {
        clm_int status = web_response_flush(response, false);
        if (status != CLM_NET_WEB_SUCCESS)
            return status;
        if (response->raw_stream)
            return CLM_NET_WEB_SUCCESS;
        status = web_send(connection, "0\r\n\r\n", 5);
        if (status != CLM_NET_WEB_SUCCESS)
            response->failed = true;
        return status;
    }
    clm_bytes_buffer compressed = {0};
    clm_bytes_view body = clm_bytes_buffer_view(&response->body);
    bool no_body = (response->status >= 100 && response->status < 200) ||
        response->status == 204 || response->status == 304;
    if (no_body) {
        web_remove_response_header(response, "Content-Length");
        web_remove_response_header(response, "Transfer-Encoding");
    }
    if (!no_body && response->server->config.enable_gzip && accepts_gzip && body.length >= 256 &&
        !web_has_response_header(response, "Content-Encoding") &&
        clm_bytes_buffer_init(&compressed, 0) == CLM_BYTES_SUCCESS) {
        (void)clm_bytes_buffer_set_limit(&compressed, response->server->config.max_response_bytes);
        if (clm_compress_encode(CLM_COMPRESS_GZIP, body, NULL, &compressed) == CLM_COMPRESS_SUCCESS) {
            bool added_vary = !web_has_response_header(response, "Vary");
            clm_int vary_status = added_vary
                ? web_response_header_set(response, "Vary", "Accept-Encoding")
                : CLM_NET_WEB_SUCCESS;
            clm_int encoding_status = vary_status == CLM_NET_WEB_SUCCESS
                ? web_response_header_set(response, "Content-Encoding", "gzip") : vary_status;
            if (encoding_status == CLM_NET_WEB_SUCCESS) {
                body = clm_bytes_buffer_view(&compressed);
                web_remove_response_header(response, "Content-Length");
            } else {
                if (added_vary)
                    web_remove_response_header(response, "Vary");
                clm_bytes_buffer_dispose(&compressed);
            }
        } else {
            clm_bytes_buffer_dispose(&compressed);
        }
    }
    char date[64];
    if (!web_has_response_header(response, "Date") && clm_http_utils_format_date(date, sizeof(date)))
        (void)web_response_header_set(response, "Date", date);
    if (!web_has_response_header(response, "Server"))
        (void)web_response_header_set(response, "Server", "CLM");
    if (!keep_alive || atomic_load_explicit(&response->server->stopping, memory_order_acquire))
        (void)web_response_header_set(response, "Connection", "close");

    clm_http_utils_header* headers = calloc(response->header_count + 1, sizeof(*headers));
    if (!headers) {
        clm_bytes_buffer_dispose(&compressed);
        return CLM_NET_WEB_NO_MEMORY;
    }
    for (size_t i = 0; i < response->header_count; ++i) {
        headers[i].name = (clm_http_utils_span){response->headers[i].name,
            strlen(response->headers[i].name)};
        headers[i].value = (clm_http_utils_span){response->headers[i].value,
            strlen(response->headers[i].value)};
    }
    clm_http_utils_message message;
    clm_http_utils_message_init(&message, CLM_HTTP_UTILS_RESPONSE);
    message.status = response->status;
    message.reason = (clm_http_utils_span){clm_http_utils_reason_phrase(response->status),
        strlen(clm_http_utils_reason_phrase(response->status))};
    message.headers = headers;
    message.header_count = response->header_count;
    message.header_capacity = response->header_count + 1;
    message.body_kind = no_body ? CLM_HTTP_UTILS_BODY_NONE : CLM_HTTP_UTILS_BODY_LENGTH;
    message.body = no_body ? (clm_http_utils_span){NULL, 0}
        : (clm_http_utils_span){(const char*)body.data, body.length};

    size_t required = 0;
    clm_int status = clm_http_utils_serialize(&message, NULL, 0, &required);
    if (status != CLM_HTTP_UTILS_SUCCESS) {
        free(headers);
        clm_bytes_buffer_dispose(&compressed);
        return CLM_NET_WEB_HANDLER_ERROR;
    }
    char* wire = malloc(required);
    if (!wire) {
        free(headers);
        clm_bytes_buffer_dispose(&compressed);
        return CLM_NET_WEB_NO_MEMORY;
    }
    status = clm_http_utils_serialize(&message, wire, required, &required);
    free(headers);
    clm_bytes_buffer_dispose(&compressed);
    if (status != CLM_HTTP_UTILS_SUCCESS) {
        free(wire);
        return CLM_NET_WEB_HANDLER_ERROR;
    }
    size_t wire_length = required - 1;
    if (clm_http_utils_span_equals(request_method, "HEAD")) {
        for (size_t i = 0; i + 3 < wire_length; ++i) {
            if (memcmp(wire + i, "\r\n\r\n", 4) == 0) {
                wire_length = i + 4;
                break;
            }
        }
    }
    if (response->allow_park) {
        /* Send what the transport accepts now; the rest stays queued so no worker waits
         * on a slow reader. The queue takes ownership of the wire buffer. */
        if (connection->out_data) {
            /* Earlier output (e.g. 100-continue) is still queued; keep the wire order. */
            clm_int queued = web_out_append(connection, wire, wire_length);
            free(wire);
            if (queued != CLM_NET_WEB_SUCCESS)
                return queued;
        } else {
            connection->out_data = (unsigned char*)wire;
            connection->out_length = wire_length;
            connection->out_offset = 0;
            connection->out_capacity = wire_length;
        }
        clm_int drained = web_out_drain(connection);
        return drained == CLM_NET_WEB_AGAIN ? WEB_SEND_PENDING : drained;
    }
    clm_int send_status = web_send_view(connection,
        (clm_bytes_view){(const unsigned char*)wire, wire_length});
    free(wire);
    return send_status;
}

static clm_int web_decode_request_body(clm_net_web_request* request,
                                       clm_bytes_buffer* transfer_decoded,
                                       clm_bytes_buffer* content_decoded,
                                       size_t limit)
{
    clm_bytes_view body = request->body;
    if (request->message.body_kind == CLM_HTTP_UTILS_BODY_CHUNKED) {
        clm_int init = clm_bytes_buffer_init(transfer_decoded, 0);
        if (init != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_NO_MEMORY;
        (void)clm_bytes_buffer_set_limit(transfer_decoded, limit);
        size_t decoded_length = 0;
        clm_int status = clm_http_utils_decode_chunked(
            (const char*)body.data, body.length, NULL, 0, &decoded_length, NULL);
        if (status != CLM_HTTP_UTILS_SUCCESS || decoded_length > limit)
            return CLM_NET_WEB_LIMIT_EXCEEDED;
        unsigned char* tail = NULL;
        size_t free_space = 0;
        if (clm_bytes_buffer_prepare(transfer_decoded, decoded_length,
                &tail, &free_space) != CLM_BYTES_SUCCESS || free_space < decoded_length)
            return CLM_NET_WEB_NO_MEMORY;
        status = clm_http_utils_decode_chunked((const char*)body.data, body.length,
            (char*)tail, free_space, &decoded_length, NULL);
        if (status != CLM_HTTP_UTILS_SUCCESS ||
            clm_bytes_buffer_commit(transfer_decoded, decoded_length) != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_SYSTEM_ERROR;
        body = clm_bytes_buffer_view(transfer_decoded);
    }
    clm_http_utils_span encoding;
    if (clm_net_web_request_header(request, "Content-Encoding", &encoding) &&
        !web_span_equal_ci(encoding.data, encoding.length, "identity")) {
        char name[32];
        if (!encoding.length || encoding.length >= sizeof(name))
            return CLM_NET_WEB_INVALID_ARGUMENT;
        memcpy(name, encoding.data, encoding.length);
        name[encoding.length] = '\0';
        clm_compress_algorithm algorithm;
        if (clm_compress_algorithm_from_name(name, &algorithm) != CLM_COMPRESS_SUCCESS)
            return CLM_NET_WEB_NOT_FOUND;
        if (clm_bytes_buffer_init(content_decoded, 0) != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_NO_MEMORY;
        (void)clm_bytes_buffer_set_limit(content_decoded, limit);
        clm_compress_options options = {0, limit};
        clm_int status = clm_compress_decode(algorithm, body, &options, content_decoded);
        if (status == CLM_COMPRESS_LIMIT_EXCEEDED)
            return CLM_NET_WEB_LIMIT_EXCEEDED;
        if (status != CLM_COMPRESS_SUCCESS)
            return CLM_NET_WEB_INVALID_ARGUMENT;
        body = clm_bytes_buffer_view(content_decoded);
    }
    request->body = body;
    return CLM_NET_WEB_SUCCESS;
}

static clm_int web_request_content_length(
    const clm_http_utils_message* message, size_t* out_length)
{
    clm_int status = clm_http_utils_content_length(message, out_length);
    if (status == CLM_HTTP_UTILS_SUCCESS)
        return CLM_NET_WEB_SUCCESS;
    return status == CLM_HTTP_UTILS_LIMIT_EXCEEDED
        ? CLM_NET_WEB_LIMIT_EXCEEDED : CLM_NET_WEB_INVALID_ARGUMENT;
}

static clm_int web_send_continue(clm_net_web_connection* connection)
{
    static const char interim[] = "HTTP/1.1 100 Continue\r\n\r\n";
    return web_send(connection, interim, sizeof(interim) - 1);
}

/* *async_route is in/out: NULL on the first call (route lookup and middleware run),
 * and the async route again when resuming a suspended handler. Returns AGAIN, with
 * the request params kept alive, when an async handler asks to be called again. */
static clm_int web_dispatch(clm_net_web_server* server,
                            clm_net_web_request* request,
                            clm_net_web_response* response,
                            clm_net_web_route** async_route)
{
    bool method_not_allowed = false;
    clm_net_web_route* route = *async_route;
    clm_int result = CLM_NET_WEB_SUCCESS;
    bool done = false;
    if (!route) {
        route = web_find_route(server, request, &method_not_allowed);
        for (size_t i = 0; i < server->middleware_count; ++i) {
            clm_int status = server->middleware[i].callback(
                request, response, server->middleware[i].context);
            if (status == CLM_NET_WEB_MIDDLEWARE_DONE) {
                done = true;
                break;
            }
            if (status != CLM_NET_WEB_MIDDLEWARE_CONTINUE) {
                result = CLM_NET_WEB_HANDLER_ERROR;
                break;
            }
        }
    }
    if (!done && result == CLM_NET_WEB_SUCCESS) {
        if (route) {
            if (route->async) {
                /* Stays on until the exchange finishes or is parked. */
                response->connection->nonblocking = true;
                response->connection->async_events = 0;
                *async_route = route;
            }
            result = route->handler(request, response, route->context);
            if (result == CLM_NET_WEB_AGAIN && route->async)
                return CLM_NET_WEB_AGAIN;
        } else {
            response->status = method_not_allowed ? 405 : 404;
            const char* text = method_not_allowed ? "Method Not Allowed\n" : "Not Found\n";
            (void)clm_net_web_response_set_header(response, "Content-Type", "text/plain; charset=utf-8");
            (void)clm_net_web_response_write_string(response, text);
        }
    }
    free(request->params);
    request->params = NULL;
    request->param_count = 0;
    if (result != CLM_NET_WEB_SUCCESS) {
        if (response->committed)
            return result;
        response->status = 500;
        (void)clm_bytes_buffer_clear(&response->body);
        (void)clm_net_web_response_set_header(response, "Content-Type", "text/plain; charset=utf-8");
        (void)clm_net_web_response_write_string(response, "Internal Server Error\n");
    }
    return CLM_NET_WEB_SUCCESS;
}

static clm_int web_response_init(clm_net_web_response* response,
                                 clm_net_web_server* server)
{
    memset(response, 0, sizeof(*response));
    response->server = server;
    response->status = 200;
    clm_int status = clm_bytes_buffer_init(&response->body, 0);
    if (status != CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_NO_MEMORY;
    if (clm_bytes_buffer_set_limit(&response->body, server->config.max_response_bytes) != CLM_BYTES_SUCCESS) {
        clm_bytes_buffer_dispose(&response->body);
        return CLM_NET_WEB_INVALID_ARGUMENT;
    }
    return CLM_NET_WEB_SUCCESS;
}

static clm_int web_error_response(clm_net_web_connection* connection, clm_uint status,
                                  const char* text)
{
    clm_net_web_response response;
    if (web_response_init(&response, connection->server) != CLM_NET_WEB_SUCCESS)
        return CLM_NET_WEB_NO_MEMORY;
    response.status = status;
    response.connection = connection;
    response.request_method = (clm_http_utils_span){"GET", 3};
    response.keep_alive = false;
    (void)clm_net_web_response_set_header(&response, "Content-Type", "text/plain; charset=utf-8");
    (void)clm_net_web_response_write_string(&response, text);
    clm_int result = web_send_response(connection, &response,
        (clm_http_utils_span){"GET", 3}, false, false);
    web_response_dispose(&response);
    return result;
}

static void web_exchange_dispose(web_exchange* ex)
{
    web_response_dispose(&ex->response);
    web_context_clear(&ex->context);
    clm_bytes_buffer_dispose(&ex->transfer_decoded);
    clm_bytes_buffer_dispose(&ex->content_decoded);
    free(ex->request.params);
    free(ex->stable_headers);
}

/* Moves a stack exchange to the heap so it can outlive web_handle_request. Fixes the
 * one pointer that referred back into the exchange itself (and the reader's, if any);
 * memcpy because the context holds an atomic. Returns NULL when out of memory. */
static web_exchange* web_exchange_detach(const web_exchange* ex)
{
    web_exchange* moved = malloc(sizeof(*moved));
    if (!moved)
        return NULL;
    memcpy(moved, ex, sizeof(*moved));
    moved->request.context = &moved->context;
    if (moved->has_reader)
        moved->request.body_reader = &moved->reader;
    return moved;
}

/* Sends the response, discards what is left of the request body, and consumes the
 * request from the input buffer. Always disposes the exchange (and frees it when it
 * is on the heap). Returns the status web_handle_request reports. */
static clm_int web_exchange_finish(web_exchange* ex, bool heap)
{
    clm_net_web_connection* connection = ex->context.connection;
    clm_bytes_buffer* input = &connection->input;
    clm_net_web_request* request = &ex->request;
    clm_net_web_response* response = &ex->response;
    clm_net_web_body_reader* body_reader = ex->has_reader ? &ex->reader : NULL;
    bool keep_alive = ex->keep_alive;
    if (body_reader && web_reader_pending(body_reader) && body_reader->continue_pending) {
        /* The client is still waiting for 100-continue; answer and close instead. */
        keep_alive = false;
    } else if (body_reader && web_reader_pending(body_reader)) {
        unsigned char discard[8192];
        while (web_reader_pending(body_reader)) {
            size_t discarded = 0;
            clm_int read_status = clm_net_web_request_read_body(
                request, discard, sizeof(discard), &discarded);
            if (read_status == CLM_NET_WEB_END)
                break;
            if (read_status == CLM_NET_WEB_AGAIN) {
                /* Async route: never wait for the rest of an unread body. */
                keep_alive = false;
                break;
            }
            if (read_status != CLM_NET_WEB_SUCCESS || !discarded) {
                keep_alive = false;
                if (!response->committed) {
                    response->status = read_status == CLM_NET_WEB_BUSY ? 408
                        : read_status == CLM_NET_WEB_LIMIT_EXCEEDED ? 413 : 400;
                    (void)clm_bytes_buffer_clear(&response->body);
                    (void)clm_net_web_response_write_string(response,
                        response->status == 408 ? "Request Timeout\n"
                        : response->status == 413 ? "Content Too Large\n" : "Bad Request\n");
                } else {
                    response->failed = true;
                }
                break;
            }
        }
    }
    if (response->raw_stream)
        keep_alive = false;  /* HTTP/1.0 streams have no framing, so EOF is the only terminator. */
    response->allow_park = true;
    clm_int send_status = web_send_response(connection, response, ex->message.method,
        keep_alive, ex->accepts_gzip);
    /* An async route's nonblocking sends queue their tail instead of waiting. */
    if (send_status == CLM_NET_WEB_SUCCESS && connection->out_data)
        send_status = WEB_SEND_PENDING;
    if (send_status == WEB_SEND_PENDING)
        connection->close_after_send = !keep_alive;
    connection->nonblocking = false;
    size_t consumed = ex->message.consumed;
    if (connection->exchange == ex)
        connection->exchange = NULL;
    web_exchange_dispose(ex);
    if (heap)
        free(ex);
    if (consumed > input->length ||
        clm_bytes_buffer_consume(input, consumed) != CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_SYSTEM_ERROR;
    if (send_status == WEB_SEND_PENDING)
        return WEB_SEND_PENDING;
    return send_status == CLM_NET_WEB_SUCCESS && keep_alive
        ? CLM_NET_WEB_SUCCESS : CLM_NET_WEB_NOT_FOUND;
}

/* Runs the route handler, first call or resume. If an async handler asks to be called
 * again the exchange is parked on the connection and WEB_REQUEST_SUSPENDED returned;
 * otherwise the exchange is finished. */
static clm_int web_exchange_advance(web_exchange* ex, bool heap)
{
    clm_net_web_connection* connection = ex->context.connection;
    clm_int dispatched = web_dispatch(connection->server, &ex->request, &ex->response,
        &ex->route);
    if (dispatched == CLM_NET_WEB_AGAIN) {
        connection->nonblocking = false;
        if (!heap) {
            web_exchange* moved = web_exchange_detach(ex);
            if (!moved) {
                connection->nonblocking = false;
                web_exchange_dispose(ex);
                return CLM_NET_WEB_NO_MEMORY;
            }
            ex = moved;
        }
        connection->exchange = ex;
        return WEB_REQUEST_SUSPENDED;
    }
    if (dispatched != CLM_NET_WEB_SUCCESS)
        ex->keep_alive = false;
    return web_exchange_finish(ex, heap);
}

/* Takes ownership of owned_headers (the copy that header spans point into) on every
 * path, including failures. */
static clm_int web_handle_request(clm_net_web_connection* connection,
                                  clm_bytes_buffer* input,
                                  clm_http_utils_header* headers,
                                  size_t request_number,
                                  const clm_http_utils_message* parsed_message,
                                  const clm_net_web_body_reader* body_reader,
                                  char* owned_headers)
{
    clm_net_web_server* server = connection->server;
    clm_http_utils_message message;
    clm_int parse_status = parsed_message
        ? (message = *parsed_message, CLM_HTTP_UTILS_SUCCESS)
        : clm_http_utils_parse_request(web_input_data(input), input->length,
            headers, server->config.max_headers, &message);
    if (parse_status != CLM_HTTP_UTILS_SUCCESS) {
        free(owned_headers);
        return parse_status == CLM_HTTP_UTILS_TOO_MANY_HEADERS
            ? web_error_response(connection, 431, "Request Header Fields Too Large\n")
            : web_error_response(connection, 400, "Bad Request\n");
    }

    web_exchange exchange;
    memset(&exchange, 0, sizeof(exchange));
    web_exchange* ex = &exchange;
    ex->stable_headers = owned_headers;
    ex->message = message;
    clm_net_web_request* request = &ex->request;
    request->message = message;
    request->remote_address = connection->peer;
    request->path = web_path_from_target(message.target);
    request->body = (clm_bytes_view){(const unsigned char*)message.body.data, message.body.length};
    if (body_reader) {
        ex->reader = *body_reader;
        ex->has_reader = true;
        request->body_reader = &ex->reader;
    }
    ex->context.request_id = web_next_id();
    ex->context.server = server;
    ex->context.connection = connection;
    atomic_init(&ex->context.cancelled, false);
    request->context = &ex->context;

    clm_int body_status = body_reader ? CLM_NET_WEB_SUCCESS
        : web_decode_request_body(request, &ex->transfer_decoded,
            &ex->content_decoded, server->config.max_request_bytes);
    clm_net_web_response* response = &ex->response;
    clm_int response_status = web_response_init(response, server);
    if (response_status != CLM_NET_WEB_SUCCESS) {
        web_exchange_dispose(ex);
        return response_status;
    }
    ex->keep_alive = clm_http_utils_keep_alive(&message) &&
        request_number < server->config.max_keep_alive_requests;
    clm_http_utils_span accept_encoding = {0};
    ex->accepts_gzip = clm_net_web_request_header(request, "Accept-Encoding", &accept_encoding) &&
        clm_http_utils_header_has_token(accept_encoding, "gzip");
    response->connection = connection;
    response->request_method = message.method;
    response->keep_alive = ex->keep_alive;
    response->accepts_gzip = ex->accepts_gzip;
    response->http10 = message.version_major == 1 && message.version_minor == 0;
    if (body_status == CLM_NET_WEB_LIMIT_EXCEEDED) {
        response->status = 413;
        (void)clm_net_web_response_write_string(response, "Content Too Large\n");
        ex->keep_alive = false;
    } else if (body_status == CLM_NET_WEB_NOT_FOUND) {
        response->status = 415;
        (void)clm_net_web_response_write_string(response, "Unsupported Media Type\n");
        ex->keep_alive = false;
    } else if (body_status != CLM_NET_WEB_SUCCESS) {
        response->status = 400;
        (void)clm_net_web_response_write_string(response, "Bad Request\n");
        ex->keep_alive = false;
    } else {
        return web_exchange_advance(ex, false);
    }
    return web_exchange_finish(ex, false);
}

/* Resumes the async handler parked on this connection. */
static clm_int web_exchange_resume(clm_net_web_connection* connection)
{
    return web_exchange_advance(connection->exchange, true);
}

/* The tail of a response that did not fit in the socket buffer is queued on the
 * connection instead of blocking: the step returns WAIT, the reactor watches for
 * writability, and a worker resumes here. The queued buffer is owned by the
 * connection (out_data) and freed on completion or dispose. */
/* Interrupts the reactor's poller wait. Safe from any thread. The flag is
 * cleared by the reactor BEFORE it drains the queues, so a park that lands after
 * the drain always finds the flag clear and sends a fresh datagram. */
static void web_reactor_wake(clm_net_web_server* server)
{
    if (atomic_exchange_explicit(&server->wake_pending, true, memory_order_acq_rel))
        return;
    size_t sent = 0;
    clm_net_status status = clm_net_send_to(server->wake_send_id, &server->wake_address,
        "w", 1, &sent);
    /* WOULD_BLOCK means the datagram queue is full, so a wake-up is already queued. */
    if (status != CLM_NET_SUCCESS && status != CLM_NET_WOULD_BLOCK)
        atomic_store_explicit(&server->wake_pending, false, memory_order_release);
}

/* Called by a worker that is about to return CLM_ASYNC_STEP_WAIT. Publishes the
 * wait first, then hands the connection to the reactor, which registers it with
 * the poller and the deadline heap. deadline_ms == 0 means no deadline. */
static void web_connection_park(clm_net_web_connection* connection,
    unsigned int events, int timeout_kind, uint64_t deadline_ms)
{
    clm_net_web_server* server = connection->server;
    atomic_store_explicit(&connection->timeout_kind, timeout_kind, memory_order_release);
    atomic_store_explicit(&connection->deadline_ms, deadline_ms, memory_order_release);
    atomic_store_explicit(&connection->waiting_events, events, memory_order_release);
    mtx_lock(&server->queue_mutex);
    if (!connection->park_queued) {
        connection->park_queued = true;
        connection->park_prev = NULL;
        connection->park_next = server->park_head;
        if (server->park_head)
            server->park_head->park_prev = connection;
        server->park_head = connection;
    }
    mtx_unlock(&server->queue_mutex);
    web_reactor_wake(server);
}

static clm_async_step_result web_connection_flush_output(clm_net_web_connection* connection)
{
    clm_int drained = web_out_drain(connection);
    if (drained == CLM_NET_WEB_AGAIN) {
        web_connection_park(connection, CLM_NET_EVENT_WRITE, WEB_TIMEOUT_WRITE,
            web_now_ms() + connection->server->config.write_timeout_ms);
        return CLM_ASYNC_STEP_WAIT;
    }
    if (drained != CLM_NET_WEB_SUCCESS)
        return CLM_ASYNC_STEP_COMPLETE;
    connection->last_activity_ms = web_now_ms();
    atomic_store_explicit(&connection->deadline_ms, 0, memory_order_release);
    return connection->close_after_send ? CLM_ASYNC_STEP_COMPLETE : CLM_ASYNC_STEP_YIELD;
}

/* Parks a suspended async handler until the I/O it blocked on can proceed. With no
 * recorded block the handler just asked to run again, so yield the worker instead. */
static clm_async_step_result web_connection_suspend(clm_net_web_connection* connection)
{
    unsigned int events = connection->async_events;
    if (!events)
        return CLM_ASYNC_STEP_YIELD;
    bool write = (events & CLM_NET_EVENT_WRITE) != 0;
    uint32_t timeout_ms = write ? connection->server->config.write_timeout_ms
                                : connection->server->config.read_timeout_ms;
    web_connection_park(connection, write ? CLM_NET_EVENT_WRITE : CLM_NET_EVENT_READ,
        write ? WEB_TIMEOUT_WRITE : WEB_TIMEOUT_READ, web_now_ms() + timeout_ms);
    return CLM_ASYNC_STEP_WAIT;
}

/* Common tail after a request was handled or resumed. */
static clm_async_step_result web_connection_after_request(
    clm_net_web_connection* connection, clm_int status)
{
    if (status == WEB_REQUEST_SUSPENDED)
        return web_connection_suspend(connection);
    connection->last_activity_ms = web_now_ms();
    atomic_store_explicit(&connection->deadline_ms, 0, memory_order_release);
    if (status == WEB_SEND_PENDING)
        return web_connection_flush_output(connection);
    return status == CLM_NET_WEB_SUCCESS
        ? CLM_ASYNC_STEP_YIELD : CLM_ASYNC_STEP_COMPLETE;
}

/* True when the request would be served by an async route, in which case its body
 * never needs a streaming slot: the handler cannot block the worker. */
static bool web_request_is_async(clm_net_web_server* server, const clm_http_utils_message* message)
{
    clm_net_web_request probe = {0};
    probe.message = *message;
    probe.path = web_path_from_target(message->target);
    bool method_not_allowed = false;
    clm_net_web_route* route = web_find_route(server, &probe, &method_not_allowed);
    free(probe.params);
    return route && route->async;
}

static clm_async_step_result web_connection_step(clm_id task_id, clm_id context_id)
{
    (void)task_id;
    clm_net_web_connection* connection = (clm_net_web_connection*)(uintptr_t)context_id;
    if (!connection)
        return CLM_ASYNC_STEP_FAIL;
    clm_net_web_server* server = connection->server;
    if (atomic_load_explicit(&server->stopping, memory_order_acquire) ||
        atomic_load_explicit(&connection->cancelled, memory_order_acquire))
        return CLM_ASYNC_STEP_COMPLETE;
    int timeout_kind = atomic_exchange_explicit(&connection->timeout_kind,
        WEB_TIMEOUT_NONE, memory_order_acq_rel);
    if (timeout_kind == WEB_TIMEOUT_WRITE)
        return CLM_ASYNC_STEP_COMPLETE;
    /* A stalled handshake is simply dropped: nothing may be written in plaintext on a TLS port. */
    if (timeout_kind != WEB_TIMEOUT_NONE && connection->tls && !connection->tls_ready)
        return CLM_ASYNC_STEP_COMPLETE;
    if (connection->exchange) {
        if (timeout_kind == WEB_TIMEOUT_READ) {
            /* The peer stalled while an async handler waited for body data. */
            web_exchange* stalled = connection->exchange;
            if (!stalled->response.committed && !connection->out_data)
                (void)web_error_response(connection, 408, "Request Timeout\n");
            return CLM_ASYNC_STEP_COMPLETE;
        }
        return web_connection_after_request(connection, web_exchange_resume(connection));
    }
    if (connection->out_data)
        return web_connection_flush_output(connection);
    if (timeout_kind == WEB_TIMEOUT_IDLE && connection->input.length == 0)
        return CLM_ASYNC_STEP_COMPLETE;
    if (timeout_kind == WEB_TIMEOUT_READ) {
        (void)web_error_response(connection, 408, "Request Timeout\n");
        return CLM_ASYNC_STEP_COMPLETE;
    }
    if (!connection->tls_ready && server->certificate_file) {
        if (!connection->tls) {
            unsigned int events = 0;
            clm_net_status ready = clm_net_wait(connection->socket_id,
                CLM_NET_EVENT_READ, 0, &events);
            if (ready == CLM_NET_TIMEOUT) {
                web_connection_park(connection, CLM_NET_EVENT_READ, WEB_TIMEOUT_IDLE,
                    connection->last_activity_ms + server->config.idle_timeout_ms);
                return CLM_ASYNC_STEP_WAIT;
            }
            if (ready != CLM_NET_SUCCESS || !(events & CLM_NET_EVENT_READ))
                return CLM_ASYNC_STEP_FAIL;
            if (clm_tls_server_new(connection->socket_id, &server->tls_options,
                    &connection->tls) != CLM_TLS_SUCCESS)
                return CLM_ASYNC_STEP_FAIL;
            (void)clm_tls_set_nonblocking(connection->tls, true);
        }
        /* Each step advances the handshake as far as the socket allows, then parks on
         * whatever it needs next, so a slow client never holds a worker. */
        clm_int tls_status = clm_tls_handshake(connection->tls);
        if (tls_status == CLM_TLS_WANT_READ || tls_status == CLM_TLS_WANT_WRITE) {
            web_connection_park(connection, web_tls_event(tls_status), WEB_TIMEOUT_READ,
                web_now_ms() + server->config.read_timeout_ms);
            return CLM_ASYNC_STEP_WAIT;
        }
        if (tls_status != CLM_TLS_SUCCESS)
            return CLM_ASYNC_STEP_FAIL;
        connection->tls_ready = true;
        connection->last_activity_ms = web_now_ms();
    }

    clm_http_utils_message header_message;
    clm_int parsed = clm_http_utils_parse_request_headers(
        web_input_data(&connection->input), connection->input.length,
        connection->headers, server->config.max_headers, &header_message);
    if (parsed == CLM_HTTP_UTILS_TOO_MANY_HEADERS) {
        (void)web_error_response(connection, 431, "Request Header Fields Too Large\n");
        return CLM_ASYNC_STEP_COMPLETE;
    }
    if (parsed == CLM_HTTP_UTILS_INCOMPLETE)
        return web_connection_read_more(connection);
    if (parsed != CLM_HTTP_UTILS_SUCCESS) {
        (void)web_error_response(connection, 400, "Bad Request\n");
        return CLM_ASYNC_STEP_COMPLETE;
    }

    clm_http_utils_span content_type = {0};
    clm_http_utils_span content_encoding = {0};
    bool chunked_body = header_message.body_kind == CLM_HTTP_UTILS_BODY_CHUNKED;
    bool streamable = chunked_body ||
        header_message.body_kind == CLM_HTTP_UTILS_BODY_NONE ||
        header_message.body_kind == CLM_HTTP_UTILS_BODY_LENGTH;
    if (clm_http_utils_find_header(&header_message, "Content-Type", &content_type) &&
        content_type.length >= 10 &&
        web_span_equal_ci(content_type.data, 10, "multipart/"))
        streamable = false;
    if (clm_http_utils_find_header(&header_message, "Content-Encoding", &content_encoding) &&
        !web_span_equal_ci(content_encoding.data, content_encoding.length, "identity"))
        streamable = false;

    size_t body_length = 0;
    if (streamable && !chunked_body &&
        web_request_content_length(&header_message, &body_length) != CLM_NET_WEB_SUCCESS)
        streamable = false;
    if (streamable) {
        /* Small bodies are received while parked so a slow client never holds a worker. */
        size_t threshold = server->config.max_buffered_body_bytes;
        size_t have = connection->input.length > header_message.consumed
            ? connection->input.length - header_message.consumed : 0;
        bool wait_for_body = false;
        if (!chunked_body) {
            wait_for_body = body_length <= threshold && have < body_length &&
                header_message.consumed <= server->config.max_request_bytes &&
                body_length <= server->config.max_request_bytes - header_message.consumed;
        } else if (have <= threshold) {
            clm_http_utils_message complete;
            clm_int full = clm_http_utils_parse_request(
                web_input_data(&connection->input), connection->input.length,
                connection->headers, server->config.max_headers, &complete);
            if (full == CLM_HTTP_UTILS_SUCCESS)
                streamable = false;
            else
                wait_for_body = full == CLM_HTTP_UTILS_INCOMPLETE;
            /* The probe parse reuses connection->headers, whose spans point into the input
             * buffer; re-parse the headers so header_message stays valid. */
            if (full == CLM_HTTP_UTILS_SUCCESS || wait_for_body)
                (void)clm_http_utils_parse_request_headers(
                    web_input_data(&connection->input), connection->input.length,
                    connection->headers, server->config.max_headers, &header_message);
        }
        if (wait_for_body) {
            if (!connection->continue_sent && clm_http_utils_expects_continue(&header_message)) {
                if (web_send_continue(connection) != CLM_NET_WEB_SUCCESS)
                    return CLM_ASYNC_STEP_COMPLETE;
                connection->continue_sent = true;
            }
            return web_connection_read_more(connection);
        }
    }
    if (streamable) {
        if (!chunked_body &&
            (header_message.consumed > server->config.max_request_bytes ||
             body_length > server->config.max_request_bytes - header_message.consumed)) {
            (void)web_error_response(connection, 413, "Content Too Large\n");
            return CLM_ASYNC_STEP_COMPLETE;
        }
        /* Headers are copied out because consuming them from the input buffer would
         * invalidate the spans that header_message and connection->headers hold. */
        size_t header_length = header_message.consumed;
        char* stable_headers = malloc(header_length);
        if (!stable_headers)
            return CLM_ASYNC_STEP_FAIL;
        memcpy(stable_headers, web_input_data(&connection->input), header_length);
        parsed = clm_http_utils_parse_request_headers(stable_headers, header_length,
            connection->headers, server->config.max_headers, &header_message);
        if (parsed != CLM_HTTP_UTILS_SUCCESS ||
            clm_bytes_buffer_consume(&connection->input, header_length) != CLM_BYTES_SUCCESS) {
            free(stable_headers);
            (void)web_error_response(connection, 400, "Bad Request\n");
            return CLM_ASYNC_STEP_COMPLETE;
        }
        header_message.consumed = 0;
        header_message.body = (clm_http_utils_span){NULL, 0};
        clm_net_web_body_reader reader = {0};
        reader.connection = connection;
        reader.remaining = body_length;
        reader.chunked = chunked_body;
        reader.chunk_state = WEB_CHUNK_SIZE;
        reader.limit = server->config.max_request_bytes;
        reader.continue_pending = !connection->continue_sent &&
            clm_http_utils_expects_continue(&header_message) &&
            (chunked_body ? connection->input.length == 0
                          : body_length > connection->input.length);
        connection->continue_sent = false;
        /* Only a body that is not already buffered can make a blocking handler wait on the
         * peer; async routes never block, so they need no slot. */
        bool needs_slot = (chunked_body ? true : body_length > connection->input.length) &&
            !web_request_is_async(server, &header_message);
        if (needs_slot && !web_stream_acquire(connection)) {
            free(stable_headers);
            (void)web_error_response(connection, 503, "Service Unavailable\n");
            return CLM_ASYNC_STEP_COMPLETE;
        }
        clm_int status = web_handle_request(connection, &connection->input,
            connection->headers, ++connection->request_number, &header_message, &reader,
            stable_headers);
        web_stream_release(connection);
        return web_connection_after_request(connection, status);
    }

    clm_http_utils_message message;
    parsed = clm_http_utils_parse_request(web_input_data(&connection->input),
        connection->input.length, connection->headers, server->config.max_headers, &message);
    if (parsed == CLM_HTTP_UTILS_TOO_MANY_HEADERS) {
        (void)web_error_response(connection, 431, "Request Header Fields Too Large\n");
        return CLM_ASYNC_STEP_COMPLETE;
    }
    if (parsed == CLM_HTTP_UTILS_INCOMPLETE) {
        if (!connection->continue_sent && clm_http_utils_expects_continue(&header_message)) {
            if (web_send_continue(connection) != CLM_NET_WEB_SUCCESS)
                return CLM_ASYNC_STEP_COMPLETE;
            connection->continue_sent = true;
        }
        return web_connection_read_more(connection);
    }
    if (parsed != CLM_HTTP_UTILS_SUCCESS) {
        (void)web_error_response(connection, 400, "Bad Request\n");
        return CLM_ASYNC_STEP_COMPLETE;
    }
    clm_int status = web_handle_request(connection, &connection->input,
        connection->headers, ++connection->request_number, NULL, NULL, NULL);
    web_stream_release(connection);
    connection->continue_sent = false;
    return web_connection_after_request(connection, status);
}

static void web_connection_cleanup(clm_id context_id)
{
    clm_net_web_connection* connection = (clm_net_web_connection*)(uintptr_t)context_id;
    if (!connection)
        return;
    clm_net_web_server* server = connection->server;
    atomic_store_explicit(&connection->task_finished, true, memory_order_release);
    /* Tell the reactor so it reaps just this connection instead of scanning all. */
    mtx_lock(&server->queue_mutex);
    if (!connection->finish_queued) {
        connection->finish_queued = true;
        connection->finish_next = server->finish_head;
        server->finish_head = connection;
    }
    mtx_unlock(&server->queue_mutex);
    web_reactor_wake(server);
}

static void web_connection_dispose(clm_net_web_connection* connection)
{
    if (!connection)
        return;
    if (connection->tls)
        (void)clm_tls_close(connection->tls);
    if (connection->socket_id)
        (void)clm_net_socket_close(connection->socket_id);
    free(connection->headers);
    free(connection->out_data);
    if (connection->exchange) {
        web_exchange_dispose(connection->exchange);
        free(connection->exchange);
    }
    clm_bytes_buffer_dispose(&connection->input);
    free(connection);
}

static clm_async_step_result web_connection_read_more(clm_net_web_connection* connection)
{
    clm_net_web_server* server = connection->server;
    unsigned char* tail = NULL;
    size_t free_space = 0;
    if (clm_bytes_buffer_prepare(&connection->input, 1, &tail, &free_space) !=
            CLM_BYTES_SUCCESS || !free_space) {
        (void)web_error_response(connection, 413, "Content Too Large\n");
        return CLM_ASYNC_STEP_COMPLETE;
    }
    size_t received = 0;
    clm_net_status status;
    unsigned int wait_event = CLM_NET_EVENT_READ;
    if (connection->tls) {
        /* Map TLS results onto the socket vocabulary used below. */
        clm_int tls_status = clm_tls_read(connection->tls, tail, free_space, &received);
        if (tls_status == CLM_TLS_SUCCESS) {
            status = CLM_NET_SUCCESS;
        } else if (tls_status == CLM_TLS_WANT_READ || tls_status == CLM_TLS_WANT_WRITE) {
            status = CLM_NET_WOULD_BLOCK;
            wait_event = web_tls_event(tls_status);
        } else if (tls_status == CLM_TLS_CLOSED) {
            status = CLM_NET_CLOSED;
        } else {
            status = CLM_NET_ERROR_SYSTEM;
        }
    } else {
        status = clm_net_receive(connection->socket_id, tail, free_space, &received);
    }
    if (status == CLM_NET_WOULD_BLOCK) {
        int kind = connection->input.length == 0 ? WEB_TIMEOUT_IDLE : WEB_TIMEOUT_READ;
        uint32_t timeout_ms = kind == WEB_TIMEOUT_IDLE
            ? server->config.idle_timeout_ms : server->config.read_timeout_ms;
        web_connection_park(connection, wait_event, kind,
            connection->last_activity_ms + timeout_ms);
        return CLM_ASYNC_STEP_WAIT;
    }
    if (status == CLM_NET_CLOSED) {
        if (connection->input.length == 0)
            return CLM_ASYNC_STEP_COMPLETE;
        atomic_store_explicit(&connection->cancelled, true, memory_order_release);
        return CLM_ASYNC_STEP_FAIL;
    }
    if (status != CLM_NET_SUCCESS || !received ||
        clm_bytes_buffer_commit(&connection->input, received) != CLM_BYTES_SUCCESS) {
        atomic_store_explicit(&connection->cancelled, true, memory_order_release);
        return CLM_ASYNC_STEP_FAIL;
    }
    connection->last_activity_ms = web_now_ms();
    atomic_store_explicit(&connection->deadline_ms, 0, memory_order_release);
    return CLM_ASYNC_STEP_YIELD;
}

/* ---------- reactor: deadline min-heap (reactor thread only) ----------
 * One entry per parked connection with a deadline; connection->heap_index tracks
 * the slot, so a connection that is woken early is removed in O(log n) and the
 * heap never holds pointers to connections that may be freed. */

static void web_heap_place(clm_net_web_server* server, size_t index,
    clm_net_web_connection* connection)
{
    server->heap[index] = connection;
    connection->heap_index = index;
}

static void web_heap_sift_up(clm_net_web_server* server, size_t index)
{
    clm_net_web_connection* moving = server->heap[index];
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (server->heap[parent]->heap_deadline_ms <= moving->heap_deadline_ms)
            break;
        web_heap_place(server, index, server->heap[parent]);
        index = parent;
    }
    web_heap_place(server, index, moving);
}

static void web_heap_sift_down(clm_net_web_server* server, size_t index)
{
    clm_net_web_connection* moving = server->heap[index];
    for (;;) {
        size_t child = index * 2 + 1;
        if (child >= server->heap_count)
            break;
        if (child + 1 < server->heap_count &&
            server->heap[child + 1]->heap_deadline_ms < server->heap[child]->heap_deadline_ms)
            ++child;
        if (moving->heap_deadline_ms <= server->heap[child]->heap_deadline_ms)
            break;
        web_heap_place(server, index, server->heap[child]);
        index = child;
    }
    web_heap_place(server, index, moving);
}

static void web_heap_remove(clm_net_web_server* server, clm_net_web_connection* connection)
{
    size_t index = connection->heap_index;
    if (index == WEB_HEAP_NONE)
        return;
    connection->heap_index = WEB_HEAP_NONE;
    clm_net_web_connection* last = server->heap[--server->heap_count];
    if (last == connection)
        return;
    web_heap_place(server, index, last);
    /* The replacement may belong above or below this slot. */
    web_heap_sift_up(server, last->heap_index);
    web_heap_sift_down(server, last->heap_index);
}

/* Inserts or re-keys the connection. Returns false only when memory is short. */
static bool web_heap_set(clm_net_web_server* server, clm_net_web_connection* connection,
    uint64_t deadline_ms)
{
    connection->heap_deadline_ms = deadline_ms;
    if (connection->heap_index != WEB_HEAP_NONE) {
        web_heap_sift_up(server, connection->heap_index);
        web_heap_sift_down(server, connection->heap_index);
        return true;
    }
    if (server->heap_count == server->heap_capacity) {
        size_t capacity = server->heap_capacity ? server->heap_capacity * 2 : 64;
        clm_net_web_connection** grown = realloc(server->heap, capacity * sizeof(*grown));
        if (!grown)
            return false;
        server->heap = grown;
        server->heap_capacity = capacity;
    }
    web_heap_place(server, server->heap_count++, connection);
    web_heap_sift_up(server, connection->heap_index);
    return true;
}

/* ---------- reactor: waking, registering and reaping ---------- */

/* Ends a connection's park from the reactor side. timeout_kind is NONE when the
 * socket became ready; otherwise it tells the worker why it is being resumed.
 * The poller registration is dropped here: readiness is level-triggered, so a
 * socket the worker has not read yet would be reported on every wait. */
static void web_reactor_resume(clm_net_web_server* server,
    clm_net_web_connection* connection, int timeout_kind)
{
    if (connection->poll_events) {
        (void)clm_net_poller_remove(server->poller, connection->socket_id);
        connection->poll_events = 0;
    }
    web_heap_remove(server, connection);
    if (connection->token.task_id == 0)
        return;
    (void)atomic_exchange_explicit(&connection->waiting_events, 0, memory_order_acq_rel);
    atomic_store_explicit(&connection->timeout_kind, timeout_kind, memory_order_release);
    atomic_store_explicit(&connection->deadline_ms, 0, memory_order_release);
    (void)clm_async_pool_wake_token(server->pool_id, connection->token);
}

/* Applies a worker's parked state (interest and deadline) to the poller and heap. */
static void web_reactor_arm(clm_net_web_server* server, clm_net_web_connection* connection)
{
    unsigned int wanted = atomic_load_explicit(&connection->waiting_events,
        memory_order_acquire);
    if (wanted == 0) {
        /* Already resumed or cancelled; make sure nothing stale remains. */
        if (connection->poll_events) {
            (void)clm_net_poller_remove(server->poller, connection->socket_id);
            connection->poll_events = 0;
        }
        web_heap_remove(server, connection);
        return;
    }
    /* If the poller or heap cannot take the connection it could wait forever, so
     * it is resumed as a write timeout, which makes the worker close it. */
    if (clm_net_poller_set(server->poller, connection->socket_id, wanted, connection) !=
            CLM_NET_SUCCESS) {
        web_reactor_resume(server, connection, WEB_TIMEOUT_WRITE);
        return;
    }
    connection->poll_events = wanted;
    uint64_t deadline = atomic_load_explicit(&connection->deadline_ms, memory_order_acquire);
    if (deadline == 0)
        web_heap_remove(server, connection);
    else if (!web_heap_set(server, connection, deadline))
        web_reactor_resume(server, connection, WEB_TIMEOUT_WRITE);
}

static void web_reactor_drain_parked(clm_net_web_server* server)
{
    for (;;) {
        mtx_lock(&server->queue_mutex);
        clm_net_web_connection* connection = server->park_head;
        if (connection) {
            server->park_head = connection->park_next;
            if (server->park_head)
                server->park_head->park_prev = NULL;
            connection->park_next = connection->park_prev = NULL;
            connection->park_queued = false;
        }
        mtx_unlock(&server->queue_mutex);
        if (!connection)
            return;
        web_reactor_arm(server, connection);
    }
}

static void web_reactor_expire(clm_net_web_server* server)
{
    uint64_t now_ms = web_now_ms();
    while (server->heap_count && server->heap[0]->heap_deadline_ms <= now_ms) {
        clm_net_web_connection* connection = server->heap[0];
        int kind = atomic_load_explicit(&connection->timeout_kind, memory_order_acquire);
        web_reactor_resume(server, connection, kind == WEB_TIMEOUT_NONE ? WEB_TIMEOUT_IDLE : kind);
    }
}

static void web_task_unlink(clm_net_web_server* server, clm_net_web_task_ref* task)
{
    if (task->prev)
        task->prev->next = task->next;
    else
        server->tasks = task->next;
    if (task->next)
        task->next->prev = task->prev;
    task->next = task->prev = NULL;
}

/* Reaps connections whose task has finished. Returns true when one is not yet
 * terminal in the pool (cleanup can run just before the state flips), so the
 * reactor should look again shortly. */
static bool web_task_reap(clm_net_web_server* server)
{
    mtx_lock(&server->queue_mutex);
    clm_net_web_connection* list = server->finish_head;
    server->finish_head = NULL;
    mtx_unlock(&server->queue_mutex);
    bool retry = false;
    while (list) {
        clm_net_web_connection* connection = list;
        list = connection->finish_next;
        clm_net_web_task_ref* task = connection->task;
        clm_async_task_state state;
        if (clm_async_pool_get_state(server->pool_id, task->task_id, &state) !=
                CLM_ASYNC_OPERATION_SUCCESS || !clm_async_task_is_terminal(state)) {
            mtx_lock(&server->queue_mutex);
            connection->finish_next = server->finish_head;
            server->finish_head = connection;
            mtx_unlock(&server->queue_mutex);
            retry = true;
            continue;
        }
        (void)clm_async_pool_task_release(server->pool_id, task->task_id);
        web_task_unlink(server, task);
        mtx_lock(&server->queue_mutex);
        if (connection->park_queued) {
            if (connection->park_prev)
                connection->park_prev->park_next = connection->park_next;
            else
                server->park_head = connection->park_next;
            if (connection->park_next)
                connection->park_next->park_prev = connection->park_prev;
        }
        mtx_unlock(&server->queue_mutex);
        /* Unregister before the socket closes so the poller can still tell this
         * registration apart from a later socket reusing the native handle. */
        if (connection->poll_events)
            (void)clm_net_poller_remove(server->poller, connection->socket_id);
        web_heap_remove(server, connection);
        web_connection_dispose(connection);
        if (server->connection_count)
            --server->connection_count;
        free(task);
    }
    return retry;
}

static void web_accept_connections(clm_net_web_server* server)
{
    for (;;) {
        clm_id socket_id = web_next_id();
        clm_net_address peer;
        clm_net_status status = clm_net_accept(server->listener_id, socket_id, &peer);
        if (status == CLM_NET_WOULD_BLOCK)
            return;
        if (status != CLM_NET_SUCCESS)
            return;
        if (server->connection_count >= server->config.max_connections ||
            clm_net_socket_set_nonblocking(socket_id, true) != CLM_NET_SUCCESS) {
            (void)clm_net_socket_close(socket_id);
            continue;
        }

        clm_net_web_connection* connection = calloc(1, sizeof(*connection));
        clm_net_web_task_ref* task = calloc(1, sizeof(*task));
        if (!connection || !task) {
            free(connection);
            free(task);
            (void)clm_net_socket_close(socket_id);
            continue;
        }
        connection->server = server;
        connection->socket_id = socket_id;
        connection->peer = peer;
        connection->task_id = web_next_id();
        connection->headers = calloc(server->config.max_headers, sizeof(*connection->headers));
        if (!connection->headers ||
            clm_bytes_buffer_init(&connection->input, 0) != CLM_BYTES_SUCCESS) {
            free(connection->headers);
            free(connection);
            free(task);
            (void)clm_net_socket_close(socket_id);
            continue;
        }
        (void)clm_bytes_buffer_set_limit(&connection->input, server->config.max_request_bytes);
        atomic_init(&connection->waiting_events, 0);
        atomic_init(&connection->task_finished, false);
        atomic_init(&connection->cancelled, false);
        atomic_init(&connection->timeout_kind, WEB_TIMEOUT_NONE);
        connection->last_activity_ms = web_now_ms();
        atomic_init(&connection->deadline_ms,
            connection->last_activity_ms + server->config.idle_timeout_ms);

        task->task_id = connection->task_id;
        task->connection = connection;
        connection->task = task;
        connection->heap_index = WEB_HEAP_NONE;
        task->next = server->tasks;
        if (server->tasks)
            server->tasks->prev = task;
        server->tasks = task;
        ++server->connection_count;
        status = (clm_net_status)clm_async_pool_spawn(server->pool_id, task->task_id,
            web_connection_step, (clm_id)(uintptr_t)connection, web_connection_cleanup);
        if (status != CLM_ASYNC_OPERATION_SUCCESS) {
            web_task_unlink(server, task);
            --server->connection_count;
            free(task);
            web_connection_dispose(connection);
            continue;
        }
        if (clm_async_pool_task_get_token(server->pool_id, task->task_id,
                &connection->token) != CLM_ASYNC_OPERATION_SUCCESS) {
            (void)clm_async_pool_cancel(server->pool_id, task->task_id);
        }
    }
}

static void web_reactor_drain_wake_socket(clm_net_web_server* server)
{
    /* Clear the flag first: any park after this point must send a new datagram. */
    atomic_store_explicit(&server->wake_pending, false, memory_order_release);
    char scratch[64];
    for (;;) {
        size_t received = 0;
        if (clm_net_receive_from(server->wake_receive_id, scratch, sizeof(scratch),
                &received, NULL) != CLM_NET_SUCCESS)
            return;
    }
}

/* The reactor thread. Its per-iteration cost is proportional to the work that
 * happened (events, newly parked connections, expired deadlines, finished
 * tasks), not to the number of open connections. */
static int web_listener_main(void* argument)
{
    clm_net_web_server* server = argument;
    clm_net_poller_event events[WEB_POLL_BATCH];
    while (!atomic_load_explicit(&server->stopping, memory_order_acquire)) {
        bool reap_retry = web_task_reap(server);
        web_reactor_drain_parked(server);
        uint32_t poll_timeout_ms = WEB_REACTOR_MAX_WAIT_MS;
        if (server->heap_count) {
            uint64_t now_ms = web_now_ms();
            uint64_t next = server->heap[0]->heap_deadline_ms;
            uint64_t remaining = next > now_ms ? next - now_ms : 0;
            if (remaining < poll_timeout_ms)
                poll_timeout_ms = (uint32_t)remaining;
        }
        if (reap_retry && poll_timeout_ms > WEB_REACTOR_REAP_RETRY_MS)
            poll_timeout_ms = WEB_REACTOR_REAP_RETRY_MS;
        size_t ready = 0;
        clm_net_status status = clm_net_poller_wait(server->poller, events,
            WEB_POLL_BATCH, poll_timeout_ms, &ready);
        if (status == CLM_NET_SUCCESS) {
            for (size_t i = 0; i < ready; ++i) {
                /* Listener: NULL user data. Wake socket: its own address as a tag.
                 * Anything else is a connection. Connections are only freed by
                 * web_task_reap at the top of the loop, after these events. */
                void* tag = events[i].user_data;
                if (!tag) {
                    web_accept_connections(server);
                } else if (tag == &server->wake_receive_id) {
                    web_reactor_drain_wake_socket(server);
                } else {
                    web_reactor_resume(server, tag, WEB_TIMEOUT_NONE);
                }
            }
        }
        web_reactor_expire(server);
    }
    (void)web_task_reap(server);
    return 0;
}

/* Creates the poller, the wake-up socket pair and the queue mutex. */
static bool web_reactor_open(clm_net_web_server* server)
{
    if (mtx_init(&server->queue_mutex, mtx_plain) != thrd_success)
        return false;
    server->reactor_open = true;
    atomic_init(&server->wake_pending, false);
    clm_net_address loopback;
    if (clm_net_poller_create(&server->poller) != CLM_NET_SUCCESS ||
        clm_net_poller_set(server->poller, server->listener_id, CLM_NET_EVENT_READ, NULL) !=
            CLM_NET_SUCCESS ||
        clm_net_address_parse("127.0.0.1", 0, &loopback) != CLM_NET_SUCCESS)
        return false;
    server->wake_receive_id = web_next_id();
    if (clm_net_socket_create(server->wake_receive_id, CLM_NET_IPV4, CLM_NET_UDP) !=
        CLM_NET_SUCCESS) {
        server->wake_receive_id = 0;
        return false;
    }
    server->wake_send_id = web_next_id();
    if (clm_net_socket_create(server->wake_send_id, CLM_NET_IPV4, CLM_NET_UDP) !=
        CLM_NET_SUCCESS) {
        server->wake_send_id = 0;
        return false;
    }
    return clm_net_socket_set_nonblocking(server->wake_receive_id, true) == CLM_NET_SUCCESS &&
        clm_net_socket_set_nonblocking(server->wake_send_id, true) == CLM_NET_SUCCESS &&
        clm_net_bind(server->wake_receive_id, &loopback) == CLM_NET_SUCCESS &&
        clm_net_socket_local_address(server->wake_receive_id, &server->wake_address) ==
            CLM_NET_SUCCESS &&
        clm_net_poller_set(server->poller, server->wake_receive_id, CLM_NET_EVENT_READ,
            &server->wake_receive_id) == CLM_NET_SUCCESS;
}

/* Safe to call after a failed or partial web_reactor_open. Must run only after
 * the pool is destroyed, because task cleanup callbacks use the mutex. */
static void web_reactor_close(clm_net_web_server* server)
{
    clm_net_poller_destroy(server->poller);
    server->poller = NULL;
    if (server->wake_receive_id)
        (void)clm_net_socket_close(server->wake_receive_id);
    if (server->wake_send_id)
        (void)clm_net_socket_close(server->wake_send_id);
    server->wake_receive_id = server->wake_send_id = 0;
    free(server->heap);
    server->heap = NULL;
    server->heap_count = server->heap_capacity = 0;
    if (server->reactor_open) {
        mtx_destroy(&server->queue_mutex);
        server->reactor_open = false;
    }
}

void clm_net_web_config_init(clm_net_web_config* config)
{
    if (!config)
        return;
    memset(config, 0, sizeof(*config));
    config->worker_count = 4;
    config->max_pending_connections = 128;
    config->max_connections = 1024;
    config->max_request_bytes = CLM_NET_WEB_DEFAULT_REQUEST_LIMIT;
    config->max_response_bytes = CLM_NET_WEB_DEFAULT_RESPONSE_LIMIT;
    config->max_headers = CLM_NET_WEB_DEFAULT_HEADER_COUNT;
    config->max_keep_alive_requests = CLM_NET_WEB_DEFAULT_KEEP_ALIVE_REQUESTS;
    config->max_buffered_body_bytes = CLM_NET_WEB_DEFAULT_BUFFERED_BODY;
    config->io_timeout_ms = 0;
    config->idle_timeout_ms = 30000;
    config->read_timeout_ms = 30000;
    config->write_timeout_ms = 30000;
    config->backlog = 128;
    config->enable_gzip = true;
}

clm_int clm_net_web_server_create(
    const clm_net_web_config* config, clm_net_web_server** out_server)
{
    if (!config || !out_server)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    clm_net_web_server* server = calloc(1, sizeof(*server));
    if (!server)
        return CLM_NET_WEB_NO_MEMORY;
    clm_net_web_config defaults;
    clm_net_web_config_init(&defaults);
    server->config = *config;
    if (!server->config.worker_count) server->config.worker_count = defaults.worker_count;
    if (!server->config.max_pending_connections) server->config.max_pending_connections = defaults.max_pending_connections;
    if (!server->config.max_connections) server->config.max_connections = defaults.max_connections;
    if (!server->config.max_request_bytes) server->config.max_request_bytes = defaults.max_request_bytes;
    if (!server->config.max_response_bytes) server->config.max_response_bytes = defaults.max_response_bytes;
    if (!server->config.max_headers) server->config.max_headers = defaults.max_headers;
    if (!server->config.max_keep_alive_requests) server->config.max_keep_alive_requests = defaults.max_keep_alive_requests;
    if (!server->config.max_buffered_body_bytes) server->config.max_buffered_body_bytes = defaults.max_buffered_body_bytes;
    if (!server->config.max_streaming_connections)
        server->config.max_streaming_connections = server->config.worker_count > 1
            ? server->config.worker_count - 1 : 1;
    if (server->config.io_timeout_ms) {
        if (!server->config.idle_timeout_ms) server->config.idle_timeout_ms = server->config.io_timeout_ms;
        if (!server->config.read_timeout_ms) server->config.read_timeout_ms = server->config.io_timeout_ms;
        if (!server->config.write_timeout_ms) server->config.write_timeout_ms = server->config.io_timeout_ms;
    }
    if (!server->config.idle_timeout_ms) server->config.idle_timeout_ms = defaults.idle_timeout_ms;
    if (!server->config.read_timeout_ms) server->config.read_timeout_ms = defaults.read_timeout_ms;
    if (!server->config.write_timeout_ms) server->config.write_timeout_ms = defaults.write_timeout_ms;
    if (!server->config.io_timeout_ms) server->config.io_timeout_ms = server->config.read_timeout_ms;
    if (!server->config.backlog) server->config.backlog = defaults.backlog;
    if (server->config.worker_count > 256 || server->config.max_headers > 4096 ||
        server->config.max_connections > 65536 || server->config.io_timeout_ms > INT_MAX ||
        server->config.idle_timeout_ms > INT_MAX || server->config.read_timeout_ms > INT_MAX ||
        server->config.write_timeout_ms > INT_MAX || server->config.backlog < 0) {
        free(server);
        return CLM_NET_WEB_INVALID_ARGUMENT;
    }
    if (config->tls) {
        server->tls_options = *config->tls;
        server->certificate_file = web_copy_string(config->tls->certificate_file);
        server->private_key_file = web_copy_string(config->tls->private_key_file);
        server->certificate_password = web_copy_string(config->tls->certificate_password);
        if (!server->certificate_file ||
            (config->tls->private_key_file && !server->private_key_file) ||
            (config->tls->certificate_password && !server->certificate_password)) {
            free(server->certificate_file);
            free(server->private_key_file);
            free(server->certificate_password);
            free(server);
            return CLM_NET_WEB_NO_MEMORY;
        }
        server->tls_options.certificate_file = server->certificate_file;
        server->tls_options.private_key_file = server->private_key_file;
        server->tls_options.certificate_password = server->certificate_password;
        if (!server->tls_options.timeout_ms)
            server->tls_options.timeout_ms = server->config.io_timeout_ms;
    }
    atomic_init(&server->stopping, false);
    *out_server = server;
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_route_add(
    clm_net_web_server* server, const char* method, const char* pattern,
    clm_net_web_handler_fn handler, void* handler_context)
{
    if (!server || server->started || !method || !*method || !pattern || pattern[0] != '/' || !handler)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (strcmp(method, "*") != 0 && !clm_http_utils_is_token(method))
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (server->route_count == server->route_capacity) {
        size_t next = server->route_capacity ? server->route_capacity * 2 : 8;
        clm_net_web_route* resized = realloc(server->routes, next * sizeof(*resized));
        if (!resized)
            return CLM_NET_WEB_NO_MEMORY;
        server->routes = resized;
        server->route_capacity = next;
    }
    char* method_copy = web_copy_string(method);
    char* pattern_copy = web_copy_string(pattern);
    if (!method_copy || !pattern_copy) {
        free(method_copy);
        free(pattern_copy);
        return CLM_NET_WEB_NO_MEMORY;
    }
    server->routes[server->route_count++] = (clm_net_web_route){
        method_copy, pattern_copy, handler, handler_context, false};
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_route_add_async(
    clm_net_web_server* server, const char* method, const char* pattern,
    clm_net_web_handler_fn handler, void* handler_context)
{
    clm_int status = clm_net_web_route_add(server, method, pattern, handler, handler_context);
    if (status == CLM_NET_WEB_SUCCESS)
        server->routes[server->route_count - 1].async = true;
    return status;
}

clm_int clm_net_web_middleware_add(
    clm_net_web_server* server, clm_net_web_middleware_fn middleware,
    void* middleware_context)
{
    if (!server || server->started || !middleware)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (server->middleware_count == server->middleware_capacity) {
        size_t next = server->middleware_capacity ? server->middleware_capacity * 2 : 4;
        clm_net_web_middleware_entry* resized = realloc(server->middleware,
            next * sizeof(*resized));
        if (!resized)
            return CLM_NET_WEB_NO_MEMORY;
        server->middleware = resized;
        server->middleware_capacity = next;
    }
    server->middleware[server->middleware_count++] =
        (clm_net_web_middleware_entry){middleware, middleware_context};
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_server_start(
    clm_net_web_server* server, const clm_net_address* address)
{
    if (!server || !address || server->started ||
        (address->family != CLM_NET_IPV4 && address->family != CLM_NET_IPV6))
        return CLM_NET_WEB_INVALID_ARGUMENT;
    clm_id listener_id = 0;
    clm_net_status net_status = CLM_NET_ERROR_ALREADY_EXISTS;
    for (size_t attempt = 0; attempt < 1024; ++attempt) {
        listener_id = web_next_id();
        net_status = clm_net_socket_create(listener_id, address->family, CLM_NET_TCP);
        if (net_status == CLM_NET_SUCCESS)
            break;
        if (net_status != CLM_NET_ERROR_ALREADY_EXISTS)
            return CLM_NET_WEB_SYSTEM_ERROR;
    }
    if (net_status != CLM_NET_SUCCESS)
        return CLM_NET_WEB_BUSY;
    server->listener_id = listener_id;
    if (clm_net_socket_set_reuse_address(listener_id, true) != CLM_NET_SUCCESS ||
        clm_net_bind(listener_id, address) != CLM_NET_SUCCESS ||
        clm_net_listen(listener_id, server->config.backlog) != CLM_NET_SUCCESS ||
        clm_net_socket_set_nonblocking(listener_id, true) != CLM_NET_SUCCESS) {
        (void)clm_net_socket_close(listener_id);
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    if (clm_net_socket_local_address(listener_id, &server->local_address) != CLM_NET_SUCCESS) {
        (void)clm_net_socket_close(listener_id);
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    server->listener_open = true;
    clm_async_pool_config pool_config = {
        server->config.worker_count,
        server->config.max_connections,
        server->config.max_pending_connections,
        4,
        CLM_ASYNC_OVERFLOW_QUEUE
    };
    clm_int pool_status = CLM_ASYNC_OPERATION_BUSY;
    for (size_t attempt = 0; attempt < 1024; ++attempt) {
        server->pool_id = web_next_id();
        pool_status = clm_async_pool_create(server->pool_id, &pool_config);
        if (pool_status == CLM_ASYNC_OPERATION_SUCCESS)
            break;
        if (pool_status != CLM_ASYNC_OPERATION_BUSY)
            break;
    }
    if (pool_status != CLM_ASYNC_OPERATION_SUCCESS) {
        (void)clm_net_socket_close(listener_id);
        server->listener_open = false;
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    server->pool_open = true;
    if (!web_reactor_open(server)) {
        (void)clm_async_pool_destroy(server->pool_id);
        server->pool_open = false;
        web_reactor_close(server);
        (void)clm_net_socket_close(listener_id);
        server->listener_open = false;
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    atomic_store_explicit(&server->stopping, false, memory_order_release);
    if (thrd_create(&server->listener_thread, web_listener_main, server) != thrd_success) {
        atomic_store_explicit(&server->stopping, true, memory_order_release);
        (void)clm_async_pool_destroy(server->pool_id);
        server->pool_open = false;
        web_reactor_close(server);
        (void)clm_net_socket_close(listener_id);
        server->listener_open = false;
        return CLM_NET_WEB_SYSTEM_ERROR;
    }
    server->listener_thread_created = true;
    server->started = true;
    return CLM_NET_WEB_SUCCESS;
}

clm_int clm_net_web_server_stop(clm_net_web_server* server)
{
    if (!server)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    if (!server->started || server->stopped)
        return CLM_NET_WEB_SUCCESS;
    atomic_store_explicit(&server->stopping, true, memory_order_release);
    web_reactor_wake(server); /* Otherwise the reactor sleeps out its current wait. */
    if (server->listener_thread_created) {
        int thread_result = 0;
        if (thrd_join(server->listener_thread, &thread_result) != thrd_success)
            return CLM_NET_WEB_SYSTEM_ERROR;
        server->listener_thread_created = false;
    }
    if (server->listener_open) {
        (void)clm_net_socket_close(server->listener_id);
        server->listener_open = false;
    }
    if (server->pool_open) {
        clm_int pool_status = clm_async_pool_destroy(server->pool_id);
        if (pool_status != CLM_ASYNC_OPERATION_SUCCESS)
            return CLM_NET_WEB_SYSTEM_ERROR;
        server->pool_open = false;
    }
    while (server->tasks) {
        clm_net_web_task_ref* task = server->tasks;
        server->tasks = task->next;
        web_connection_dispose(task->connection);
        free(task);
    }
    web_reactor_close(server);
    server->stopped = true;
    return CLM_NET_WEB_SUCCESS;
}

void clm_net_web_server_destroy(clm_net_web_server* server)
{
    if (!server)
        return;
    (void)clm_net_web_server_stop(server);
    for (size_t i = 0; i < server->route_count; ++i) {
        free(server->routes[i].method);
        free(server->routes[i].pattern);
    }
    free(server->routes);
    free(server->middleware);
    free(server->certificate_file);
    free(server->private_key_file);
    if (server->certificate_password) {
        volatile char* p = server->certificate_password;
        size_t length = strlen(server->certificate_password);
        while (length--)
            *p++ = 0;
        free(server->certificate_password);
    }
    free(server);
}

clm_int clm_net_web_server_local_address(
    const clm_net_web_server* server, clm_net_address* out_address)
{
    if (!server || !out_address || !server->started || !server->listener_open)
        return CLM_NET_WEB_INVALID_ARGUMENT;
    *out_address = server->local_address;
    return CLM_NET_WEB_SUCCESS;
}

/* Monotonic milliseconds for deadlines only; the value has no calendar meaning and
 * must never be mixed with wall-clock time (such as the HTTP Date header). */
static uint64_t web_now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
#endif
}