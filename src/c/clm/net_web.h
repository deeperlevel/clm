#pragma once

/*
 * =============================================================================
 * CLM - Cross Language Modules
 *
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain - CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 * =============================================================================
 */

/* HTTP/1.1 server with asynchronous connection dispatch through clm_async_pool.
 * Connection tasks park while waiting for socket readiness, so idle keep-alive
 * sockets do not occupy worker execution. Synchronous handlers run on pool
 * workers; body reads and response writes can wait there under backpressure.
 */

#include "async_pool.h"
#include "bytes.h"
#include "compress.h"
#include "multipart.h"
#include "stream.h"
#include "http_utils.h"
#include "template.h"
#include "tls.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLM_NET_WEB_SUCCESS ((clm_int)0)
#define CLM_NET_WEB_NOT_FOUND ((clm_int)1)
#define CLM_NET_WEB_INVALID_ARGUMENT ((clm_int)-1)
#define CLM_NET_WEB_SYSTEM_ERROR ((clm_int)-2)
#define CLM_NET_WEB_BUSY ((clm_int)-3)
#define CLM_NET_WEB_NO_MEMORY ((clm_int)-4)
#define CLM_NET_WEB_LIMIT_EXCEEDED ((clm_int)-5)
#define CLM_NET_WEB_HANDLER_ERROR ((clm_int)-6)
#define CLM_NET_WEB_TLS_ERROR ((clm_int)-7)
/* End of a request body. Distinct from CLM_NET_WEB_NOT_FOUND (1) so a body reader
 * cannot confuse "no more data" with "no matching route or unsupported encoding". */
#define CLM_NET_WEB_END ((clm_int)2)
/* Returned by request_read_body() and response_flush() when called from an async
 * route (clm_net_web_route_add_async) and the operation cannot make progress
 * without waiting on the peer. An async handler returns it to be called again. */
#define CLM_NET_WEB_AGAIN ((clm_int)3)

#define CLM_NET_WEB_DEFAULT_REQUEST_LIMIT ((size_t)8 * 1024 * 1024)
#define CLM_NET_WEB_DEFAULT_RESPONSE_LIMIT ((size_t)16 * 1024 * 1024)
#define CLM_NET_WEB_DEFAULT_HEADER_COUNT ((size_t)128)
#define CLM_NET_WEB_DEFAULT_KEEP_ALIVE_REQUESTS ((size_t)100)
#define CLM_NET_WEB_DEFAULT_BUFFERED_BODY ((size_t)64 * 1024)

typedef struct clm_net_web_server clm_net_web_server;
typedef struct clm_net_web_context clm_net_web_context;
typedef struct clm_net_web_response clm_net_web_response;
typedef struct clm_net_web_body_reader clm_net_web_body_reader;

typedef struct {
    const char* data;
    size_t length;
} clm_net_web_span;

typedef struct {
    clm_net_web_span name;
    clm_net_web_span value;
} clm_net_web_param;

typedef struct {
    /** Zero selects the defaults; worker_count defaults to 4. */
    size_t worker_count;
    /** Queue capacity when all workers have reached their task limit. */
    size_t max_pending_connections;
    /** Hard cap on live sockets, including idle keep-alive connections. */
    size_t max_connections;
    size_t max_request_bytes;
    size_t max_response_bytes;
    size_t max_headers;
    size_t max_keep_alive_requests;
    /** Streamed request bodies up to this size are fully received while the
     * connection is parked, before the handler runs, so slow uploads do not
     * occupy a worker. Larger bodies are streamed to the handler. Zero selects
     * the default of 64 KiB. */
    size_t max_buffered_body_bytes;
    /** Maximum number of requests that may hold a worker while streaming: bodies
     * larger than max_buffered_body_bytes and handlers that call
     * clm_net_web_response_flush(). Those requests block their worker on slow
     * peers, so the cap keeps some workers free for ordinary requests. Zero
     * selects worker_count - 1 (at least 1). A request streaming a body over the
     * cap gets 503; flush() returns CLM_NET_WEB_BUSY and the response stays
     * buffered. */
    size_t max_streaming_connections;
    /** Legacy fallback for timeout fields left zero; prefer the three below. */
    uint32_t io_timeout_ms;
    /** Idle keep-alive deadline; zero selects the default. */
    uint32_t idle_timeout_ms;
    /** Rolling deadline while reading request headers or body. */
    uint32_t read_timeout_ms;
    /** Deadline while sending buffered or streamed response data. */
    uint32_t write_timeout_ms;
    int backlog;
    bool enable_gzip;
    /** NULL serves plain HTTP; otherwise the server uses clm_tls. The server
     * copies the options and strings during create. */
    const clm_tls_options* tls;
} clm_net_web_config;

typedef struct {
    clm_http_utils_message message;
    clm_bytes_view body;
    size_t body_offset;
    clm_net_web_body_reader* body_reader;
    clm_net_address remote_address;
    clm_net_web_span path;
    clm_net_web_param* params;
    size_t param_count;
    clm_net_web_context* context;
} clm_net_web_request;

typedef clm_int (*clm_net_web_handler_fn)(
    clm_net_web_request* request, clm_net_web_response* response,
    void* handler_context);

/** Middleware returns CONTINUE to proceed, DONE to stop dispatch, or a
 * negative CLM_NET_WEB_* error to produce an internal-server-error response.
 */
#define CLM_NET_WEB_MIDDLEWARE_CONTINUE ((clm_int)0)
#define CLM_NET_WEB_MIDDLEWARE_DONE ((clm_int)1)
typedef clm_int (*clm_net_web_middleware_fn)(
    clm_net_web_request* request, clm_net_web_response* response,
    void* middleware_context);
typedef void (*clm_net_web_value_dispose_fn)(void* value);

/** Zero fields to select defaults. worker_count defaults to 4. */
void clm_net_web_config_init(clm_net_web_config* config);

/** Creates the listener state and worker pool. Networking must be initialized
 * with clm_net_initialize before start. Routes are registered before start.
 */
clm_int clm_net_web_server_create(
    const clm_net_web_config* config, clm_net_web_server** out_server);

/** Adds an exact or parameterized route. Patterns support :name path segments
 * and a final *name catch-all. A method of "*" matches any method.
 */
clm_int clm_net_web_route_add(
    clm_net_web_server* server, const char* method, const char* pattern,
    clm_net_web_handler_fn handler, void* handler_context);

/** Adds a route whose handler never blocks its worker on peer I/O.
 *
 * The handler has the clm_net_web_handler_fn signature but is resumable:
 *  - request_read_body() returns CLM_NET_WEB_AGAIN instead of waiting for data.
 *  - response_flush() sends what the socket accepts and queues the rest; it
 *    returns CLM_NET_WEB_AGAIN, without accepting new data, while an earlier
 *    flush is still queued. Data written but not yet flushed stays buffered.
 *  - Returning CLM_NET_WEB_AGAIN parks the connection on the reactor until the
 *    socket is readable (after a blocked read) or writable (after a blocked
 *    flush), then calls the handler again with the SAME request and response.
 *    Returning it after neither blocked simply yields the worker and resumes.
 *  - Local variables do not survive a return, so keep progress in
 *    clm_net_web_context_set()/get(); the context lives for the whole request.
 *  - Middleware runs once, before the first call. Rolling read/write timeouts
 *    apply while parked: a stalled read yields 408 (or a close once the response
 *    is committed), a stalled write closes the connection.
 * Over TLS the I/O calls may still wait, because clm_tls has no nonblocking mode.
 * If the handler does not consume the whole request body, only the bytes already
 * received are discarded and the connection is closed after the response. */
clm_int clm_net_web_route_add_async(
    clm_net_web_server* server, const char* method, const char* pattern,
    clm_net_web_handler_fn handler, void* handler_context);

/** Adds middleware in registration order. Routes/middleware are immutable once
 * the server starts; callback contexts are borrowed until server destruction.
 */
clm_int clm_net_web_middleware_add(
    clm_net_web_server* server, clm_net_web_middleware_fn middleware,
    void* middleware_context);

/** Starts listening and returns immediately. address is numeric; port zero is
 * allowed. HTTPS is enabled when the config contains TLS server credentials.
 */
clm_int clm_net_web_server_start(
    clm_net_web_server* server, const clm_net_address* address);

/** Stops accepting, cancels queued connections, joins the listener, and waits
 * for active handlers to return. May block while application handlers run.
 */
clm_int clm_net_web_server_stop(clm_net_web_server* server);

/** Stops if needed, destroys the worker pool, and releases server resources. */
void clm_net_web_server_destroy(clm_net_web_server* server);

/** Reads the bound endpoint after start; supports ephemeral port discovery. */
clm_int clm_net_web_server_local_address(
    const clm_net_web_server* server, clm_net_address* out_address);

bool clm_net_web_context_is_cancelled(const clm_net_web_context* context);
clm_id clm_net_web_context_request_id(const clm_net_web_context* context);

/** Context values are request-local. Replacing a key disposes its old value. */
clm_int clm_net_web_context_set(
    clm_net_web_context* context, const char* key, void* value,
    clm_net_web_value_dispose_fn dispose);
void* clm_net_web_context_get(
    const clm_net_web_context* context, const char* key);

bool clm_net_web_request_header(
    const clm_net_web_request* request, const char* name,
    clm_http_utils_span* out_value);
bool clm_net_web_request_param(
    const clm_net_web_request* request, const char* name,
    clm_net_web_span* out_value);

/** Reads the next body bytes. Fixed-length and chunked requests selected for
 * streaming are read (and, for chunked, decoded) incrementally from the
 * connection; compressed and multipart requests read from the bounded
 * buffered body view. Returns SUCCESS, END, or an error. In an async route it
 * returns CLM_NET_WEB_AGAIN when no data has arrived yet.
 */
clm_int clm_net_web_request_read_body(
    clm_net_web_request* request, void* buffer, size_t capacity,
    size_t* out_read);

/** Parses a multipart request body using its Content-Type boundary parameter. */
clm_int clm_net_web_request_multipart(
    const clm_net_web_request* request,
    clm_multipart_part* parts, size_t part_capacity,
    clm_multipart_header* headers, size_t header_capacity,
    clm_multipart_message* out_message);

clm_int clm_net_web_response_set_status(
    clm_net_web_response* response, clm_uint status);
clm_int clm_net_web_response_set_header(
    clm_net_web_response* response, const char* name, const char* value);
clm_int clm_net_web_response_write(
    clm_net_web_response* response, const void* data, size_t length);
clm_int clm_net_web_response_write_string(
    clm_net_web_response* response, const char* text);
/** Commits headers and flushes buffered bytes as HTTP/1.1 chunked output.
 * Later writes stream bounded chunks and apply the configured write timeout.
 * Status and headers cannot be changed after the first flush. In an async route
 * the flush never waits; see clm_net_web_route_add_async.
 */
clm_int clm_net_web_response_flush(clm_net_web_response* response);
clm_int clm_net_web_response_redirect(
    clm_net_web_response* response, const char* location, clm_uint status);
clm_int clm_net_web_response_render_template(
    clm_net_web_response* response, const clm_template* tmpl,
    const clm_template_context* context,
    const clm_template_options* options, clm_template_error* error);

#ifdef __cplusplus
}
#endif