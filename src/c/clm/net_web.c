#include "net_web.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

typedef struct {
    char* name;
    char* value;
} clm_net_web_owned_header;

typedef struct clm_net_web_route {
    char* method;
    char* pattern;
    clm_net_web_handler_fn handler;
    void* context;
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
    struct clm_net_web_task_ref* next;
} clm_net_web_task_ref;

struct clm_net_web_context {
    clm_id request_id;
    struct clm_net_web_server* server;
    atomic_bool cancelled;
    clm_net_web_context_value* values;
};

struct clm_net_web_response {
    struct clm_net_web_server* server;
    clm_uint status;
    clm_net_web_owned_header* headers;
    size_t header_count;
    size_t header_capacity;
    clm_bytes_buffer body;
};

struct clm_net_web_connection {
    struct clm_net_web_server* server;
    clm_id socket_id;
    clm_id task_id;
    clm_net_address peer;
    clm_tls_session* tls;
};

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
    clm_net_web_task_ref* tasks;
};

typedef struct {
    clm_net_web_connection* connection;
} clm_net_web_stream_writer;

static atomic_uint_fast64_t clm_net_web_ids = ATOMIC_VAR_INIT(UINT64_C(0xc100000000000001));

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

static unsigned char web_lower(unsigned char value)
{
    return value >= 'A' && value <= 'Z' ? (unsigned char)(value + ('a' - 'A')) : value;
}

static bool web_span_equal_ci(const char* left, size_t left_length, const char* right)
{
    if (!right || left_length != strlen(right))
        return false;
    for (size_t i = 0; i < left_length; ++i)
        if (web_lower((unsigned char)left[i]) != web_lower((unsigned char)right[i]))
            return false;
    return true;
}

static bool web_header_name_valid(const char* name)
{
    if (!name || !*name)
        return false;
    for (const unsigned char* p = (const unsigned char*)name; *p; ++p) {
        unsigned char c = *p;
        bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        bool digit = c >= '0' && c <= '9';
        if (!alpha && !digit && !strchr("!#$%&'*+-.^_`|~", c))
            return false;
    }
    return true;
}

static clm_int web_wait(clm_net_web_connection* connection, unsigned int event)
{
    uint64_t start_ms;
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    start_ms = (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
    for (;;) {
        if (atomic_load_explicit(&connection->server->stopping, memory_order_acquire))
            return CLM_NET_WEB_BUSY;
        timespec_get(&now, TIME_UTC);
        uint64_t current_ms = (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
        uint64_t elapsed = current_ms >= start_ms ? current_ms - start_ms : 0;
        if (elapsed >= connection->server->config.io_timeout_ms)
            return CLM_NET_WEB_BUSY;
        uint64_t remaining = connection->server->config.io_timeout_ms - elapsed;
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

static clm_int web_receive(clm_net_web_connection* connection, void* buffer,
                           size_t capacity, size_t* received)
{
    if (connection->tls) {
        clm_int status = clm_tls_read(connection->tls, buffer, capacity, received);
        if (status == CLM_TLS_SUCCESS)
            return CLM_NET_WEB_SUCCESS;
        if (status == CLM_TLS_CLOSED)
            return CLM_NET_WEB_NOT_FOUND;
        return status == CLM_TLS_NO_MEMORY ? CLM_NET_WEB_NO_MEMORY : CLM_NET_WEB_SYSTEM_ERROR;
    }
    for (;;) {
        clm_net_status status = clm_net_receive(connection->socket_id, buffer, capacity, received);
        if (status == CLM_NET_SUCCESS)
            return CLM_NET_WEB_SUCCESS;
        if (status == CLM_NET_CLOSED)
            return CLM_NET_WEB_NOT_FOUND;
        if (status != CLM_NET_WOULD_BLOCK)
            return CLM_NET_WEB_SYSTEM_ERROR;
        clm_int wait_status = web_wait(connection, CLM_NET_EVENT_READ);
        if (wait_status != CLM_NET_WEB_SUCCESS)
            return wait_status;
    }
}

static clm_int web_send(clm_net_web_connection* connection, const void* data,
                        size_t length)
{
    const unsigned char* bytes = data;
    size_t offset = 0;
    while (offset < length) {
        if (connection->tls) {
            size_t written = 0;
            clm_int status = clm_tls_write(connection->tls, bytes + offset,
                length - offset, &written);
            if (status != CLM_TLS_SUCCESS || written == 0)
                return CLM_NET_WEB_SYSTEM_ERROR;
            offset += written;
            continue;
        }
        size_t written = 0;
        clm_net_status status = clm_net_send(connection->socket_id,
            bytes + offset, length - offset, &written);
        if (status == CLM_NET_SUCCESS && written) {
            offset += written;
            continue;
        }
        if (status != CLM_NET_WOULD_BLOCK)
            return CLM_NET_WEB_SYSTEM_ERROR;
        clm_int wait_status = web_wait(connection, CLM_NET_EVENT_WRITE);
        if (wait_status != CLM_NET_WEB_SUCCESS)
            return wait_status;
    }
    return CLM_NET_WEB_SUCCESS;
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

static bool web_header_value_has_token(clm_string_httpp_span value, const char* token)
{
    size_t index = 0;
    while (index < value.length) {
        while (index < value.length && (value.data[index] == ' ' || value.data[index] == '\t' ||
                                        value.data[index] == ','))
            ++index;
        size_t begin = index;
        while (index < value.length && value.data[index] != ',')
            ++index;
        size_t end = index;
        while (end > begin && (value.data[end - 1] == ' ' || value.data[end - 1] == '\t'))
            --end;
        size_t semicolon = begin;
        while (semicolon < end && value.data[semicolon] != ';')
            ++semicolon;
        while (semicolon > begin && (value.data[semicolon - 1] == ' ' || value.data[semicolon - 1] == '\t'))
            --semicolon;
        if (web_span_equal_ci(value.data + begin, semicolon - begin, token)) {
            size_t param = semicolon;
            while (param < end) {
                if (value.data[param++] != ';')
                    break;
                while (param < end && (value.data[param] == ' ' || value.data[param] == '\t'))
                    ++param;
                if (param + 2 <= end && web_lower((unsigned char)value.data[param]) == 'q' &&
                    value.data[param + 1] == '=') {
                    param += 2;
                    while (param < end && (value.data[param] == ' ' || value.data[param] == '\t'))
                        ++param;
                    bool nonzero = false;
                    while (param < end && value.data[param] != ';') {
                        if (value.data[param] >= '1' && value.data[param] <= '9')
                            nonzero = true;
                        ++param;
                    }
                    if (!nonzero)
                        return false;
                }
                while (param < end && value.data[param] != ';')
                    ++param;
            }
            return true;
        }
    }
    return false;
}
static clm_net_web_span web_path_from_target(clm_string_httpp_span target)
{
    size_t length = 0;
    while (length < target.length && target.data[length] != '?' && target.data[length] != '#')
        ++length;
    return (clm_net_web_span){target.data, length};
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
            clm_string_httpp_span_equals(request->message.method, server->routes[i].method);
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
                                const char* name, clm_string_httpp_span* out_value)
{
    return request && clm_string_httpp_find_header(&request->message, name, out_value);
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

clm_int clm_net_web_request_multipart(
    const clm_net_web_request* request, clm_multipart_part* parts,
    size_t part_capacity, clm_multipart_header* headers,
    size_t header_capacity, clm_multipart_message* out_message)
{
    if (!request || !out_message)
        return CLM_MULTIPART_INVALID_ARGUMENT;
    clm_string_httpp_span content_type;
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
    if (!response || !web_header_name_valid(name) || !value)
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
    if (!response || status < 100 || status > 599)
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

static const char* web_reason(clm_uint status)
{
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 413: return "Content Too Large";
    case 415: return "Unsupported Media Type";
    case 417: return "Expectation Failed";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default: return "Status";
    }
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

static bool web_date(char* out, size_t capacity)
{
    time_t now = time(NULL);
    struct tm value;
    #ifdef _WIN32
    if (gmtime_s(&value, &now) != 0)
        return false;
    #else
    if (!gmtime_r(&now, &value))
        return false;
    #endif
    return strftime(out, capacity, "%a, %d %b %Y %H:%M:%S GMT", &value) != 0;
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

static clm_int web_send_response(clm_net_web_connection* connection,
                                 clm_net_web_response* response,
                                 clm_string_httpp_span request_method,
                                 bool keep_alive, bool accepts_gzip)
{
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
    if (!web_has_response_header(response, "Date") && web_date(date, sizeof(date)))
        (void)web_response_header_set(response, "Date", date);
    if (!web_has_response_header(response, "Server"))
        (void)web_response_header_set(response, "Server", "CLM");
    if (!keep_alive || atomic_load_explicit(&response->server->stopping, memory_order_acquire))
        (void)web_response_header_set(response, "Connection", "close");

    clm_string_httpp_header* headers = calloc(response->header_count + 1, sizeof(*headers));
    if (!headers) {
        clm_bytes_buffer_dispose(&compressed);
        return CLM_NET_WEB_NO_MEMORY;
    }
    for (size_t i = 0; i < response->header_count; ++i) {
        headers[i].name = (clm_string_httpp_span){response->headers[i].name,
            strlen(response->headers[i].name)};
        headers[i].value = (clm_string_httpp_span){response->headers[i].value,
            strlen(response->headers[i].value)};
    }
    clm_string_httpp_message message;
    clm_string_httpp_message_init(&message, CLM_STRING_HTTPP_RESPONSE);
    message.status = response->status;
    message.reason = (clm_string_httpp_span){web_reason(response->status), strlen(web_reason(response->status))};
    message.headers = headers;
    message.header_count = response->header_count;
    message.header_capacity = response->header_count + 1;
    message.body_kind = no_body ? CLM_STRING_HTTPP_BODY_NONE : CLM_STRING_HTTPP_BODY_LENGTH;
    message.body = no_body ? (clm_string_httpp_span){NULL, 0}
        : (clm_string_httpp_span){(const char*)body.data, body.length};

    size_t required = 0;
    clm_int status = clm_string_httpp_serialize(&message, NULL, 0, &required);
    if (status != CLM_STRING_HTTPP_SUCCESS) {
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
    status = clm_string_httpp_serialize(&message, wire, required, &required);
    free(headers);
    clm_bytes_buffer_dispose(&compressed);
    if (status != CLM_STRING_HTTPP_SUCCESS) {
        free(wire);
        return CLM_NET_WEB_HANDLER_ERROR;
    }
    size_t wire_length = required - 1;
    if (clm_string_httpp_span_equals(request_method, "HEAD")) {
        for (size_t i = 0; i + 3 < wire_length; ++i) {
            if (memcmp(wire + i, "\r\n\r\n", 4) == 0) {
                wire_length = i + 4;
                break;
            }
        }
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
    if (request->message.body_kind == CLM_STRING_HTTPP_BODY_CHUNKED) {
        clm_int init = clm_bytes_buffer_init(transfer_decoded, 0);
        if (init != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_NO_MEMORY;
        (void)clm_bytes_buffer_set_limit(transfer_decoded, limit);
        size_t decoded_length = 0;
        clm_int status = clm_string_httpp_decode_chunked(
            (const char*)body.data, body.length, NULL, 0, &decoded_length, NULL);
        if (status != CLM_STRING_HTTPP_SUCCESS || decoded_length > limit)
            return CLM_NET_WEB_LIMIT_EXCEEDED;
        unsigned char* tail = NULL;
        size_t free_space = 0;
        if (clm_bytes_buffer_prepare(transfer_decoded, decoded_length,
                &tail, &free_space) != CLM_BYTES_SUCCESS || free_space < decoded_length)
            return CLM_NET_WEB_NO_MEMORY;
        status = clm_string_httpp_decode_chunked((const char*)body.data, body.length,
            (char*)tail, free_space, &decoded_length, NULL);
        if (status != CLM_STRING_HTTPP_SUCCESS ||
            clm_bytes_buffer_commit(transfer_decoded, decoded_length) != CLM_BYTES_SUCCESS)
            return CLM_NET_WEB_SYSTEM_ERROR;
        body = clm_bytes_buffer_view(transfer_decoded);
    }
    clm_string_httpp_span encoding;
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

static bool web_request_keep_alive(const clm_string_httpp_message* message)
{
    clm_string_httpp_span connection;
    if (clm_string_httpp_find_header(message, "Connection", &connection)) {
        if (web_header_value_has_token(connection, "close"))
            return false;
        if (message->version_minor == 0)
            return web_header_value_has_token(connection, "keep-alive");
    }
    return message->version_major == 1 && message->version_minor >= 1;
}

static clm_int web_dispatch(clm_net_web_server* server,
                            clm_net_web_request* request,
                            clm_net_web_response* response)
{
    bool method_not_allowed = false;
    clm_net_web_route* route = web_find_route(server, request, &method_not_allowed);
    clm_int result = CLM_NET_WEB_SUCCESS;
    bool done = false;
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
    if (!done && result == CLM_NET_WEB_SUCCESS) {
        if (route) {
            result = route->handler(request, response, route->context);
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
    (void)clm_net_web_response_set_header(&response, "Content-Type", "text/plain; charset=utf-8");
    (void)clm_net_web_response_write_string(&response, text);
    clm_int result = web_send_response(connection, &response,
        (clm_string_httpp_span){"GET", 3}, false, false);
    web_response_dispose(&response);
    return result;
}

static clm_int web_handle_request(clm_net_web_connection* connection,
                                  clm_bytes_buffer* input,
                                  clm_string_httpp_header* headers,
                                  size_t request_number)
{
    clm_net_web_server* server = connection->server;
    clm_string_httpp_message message;
    clm_int parse_status = clm_string_httpp_parse_request(
        web_input_data(input), input->length,
        headers, server->config.max_headers, &message);
    if (parse_status == CLM_STRING_HTTPP_TOO_MANY_HEADERS)
        return web_error_response(connection, 431, "Request Header Fields Too Large\n");
    if (parse_status != CLM_STRING_HTTPP_SUCCESS)
        return web_error_response(connection, 400, "Bad Request\n");

    clm_net_web_request request = {0};
    request.message = message;
    request.remote_address = connection->peer;
    request.path = web_path_from_target(message.target);
    request.body = (clm_bytes_view){(const unsigned char*)message.body.data, message.body.length};
    clm_net_web_context context = {0};
    context.request_id = web_next_id();
    context.server = server;
    atomic_init(&context.cancelled, false);
    request.context = &context;

    clm_bytes_buffer transfer_decoded = {0};
    clm_bytes_buffer content_decoded = {0};
    clm_int body_status = web_decode_request_body(&request, &transfer_decoded,
        &content_decoded, server->config.max_request_bytes);
    clm_net_web_response response;
    clm_int response_status = web_response_init(&response, server);
    if (response_status != CLM_NET_WEB_SUCCESS) {
        clm_bytes_buffer_dispose(&transfer_decoded);
        clm_bytes_buffer_dispose(&content_decoded);
        web_context_clear(&context);
        return response_status;
    }
    bool keep_alive = web_request_keep_alive(&message) &&
        request_number < server->config.max_keep_alive_requests;
    clm_string_httpp_span accept_encoding = {0};
    bool accepts_gzip = clm_net_web_request_header(&request, "Accept-Encoding", &accept_encoding) &&
        web_header_value_has_token(accept_encoding, "gzip");
    if (body_status == CLM_NET_WEB_LIMIT_EXCEEDED) {
        response.status = 413;
        (void)clm_net_web_response_write_string(&response, "Content Too Large\n");
        keep_alive = false;
    } else if (body_status == CLM_NET_WEB_NOT_FOUND) {
        response.status = 415;
        (void)clm_net_web_response_write_string(&response, "Unsupported Media Type\n");
        keep_alive = false;
    } else if (body_status != CLM_NET_WEB_SUCCESS) {
        response.status = 400;
        (void)clm_net_web_response_write_string(&response, "Bad Request\n");
        keep_alive = false;
    } else {
        (void)web_dispatch(server, &request, &response);
    }
    clm_int send_status = web_send_response(connection, &response, message.method,
        keep_alive, accepts_gzip);
    size_t consumed = message.consumed;
    web_response_dispose(&response);
    web_context_clear(&context);
    clm_bytes_buffer_dispose(&transfer_decoded);
    clm_bytes_buffer_dispose(&content_decoded);
    if (consumed > input->length ||
        clm_bytes_buffer_consume(input, consumed) != CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_SYSTEM_ERROR;
    return send_status == CLM_NET_WEB_SUCCESS && keep_alive
        ? CLM_NET_WEB_SUCCESS : CLM_NET_WEB_NOT_FOUND;
}

static clm_int web_connection_run(clm_net_web_connection* connection)
{
    clm_net_web_server* server = connection->server;
    if (clm_net_socket_set_nonblocking(connection->socket_id, true) != CLM_NET_SUCCESS)
        return CLM_NET_WEB_SYSTEM_ERROR;
    if (server->certificate_file) {
        clm_int tls_status = clm_tls_server_open(connection->socket_id,
            &server->tls_options, &connection->tls);
        if (tls_status != CLM_TLS_SUCCESS)
            return CLM_NET_WEB_TLS_ERROR;
    }
    clm_bytes_buffer input = {0};
    if (clm_bytes_buffer_init(&input, 0) != CLM_BYTES_SUCCESS)
        return CLM_NET_WEB_NO_MEMORY;
    (void)clm_bytes_buffer_set_limit(&input, server->config.max_request_bytes);
    clm_string_httpp_header* headers = calloc(server->config.max_headers, sizeof(*headers));
    if (!headers) {
        clm_bytes_buffer_dispose(&input);
        return CLM_NET_WEB_NO_MEMORY;
    }
    clm_int result = CLM_NET_WEB_SUCCESS;
    for (size_t request_number = 1;
         request_number <= server->config.max_keep_alive_requests; ++request_number) {
        clm_string_httpp_message message;
        clm_int parsed = clm_string_httpp_parse_request(
            web_input_data(&input), input.length,
            headers, server->config.max_headers, &message);
        while (parsed == CLM_STRING_HTTPP_INCOMPLETE) {
            unsigned char* tail = NULL;
            size_t free_space = 0;
            clm_int prepared = clm_bytes_buffer_prepare(&input, 1, &tail, &free_space);
            if (prepared != CLM_BYTES_SUCCESS || !free_space) {
                (void)web_error_response(connection, 413, "Content Too Large\n");
                result = CLM_NET_WEB_LIMIT_EXCEEDED;
                goto done;
            }
            size_t received = 0;
            clm_int read_status = web_receive(connection, tail, free_space, &received);
            if (read_status != CLM_NET_WEB_SUCCESS) {
                if (read_status == CLM_NET_WEB_NOT_FOUND && input.length == 0) {
                    result = CLM_NET_WEB_SUCCESS;
                    goto done;
                }
                if (read_status == CLM_NET_WEB_BUSY)
                    (void)web_error_response(connection, 408, "Request Timeout\n");
                result = read_status;
                goto done;
            }
            if (!received || clm_bytes_buffer_commit(&input, received) != CLM_BYTES_SUCCESS) {
                result = CLM_NET_WEB_SYSTEM_ERROR;
                goto done;
            }
            parsed = clm_string_httpp_parse_request(
                web_input_data(&input), input.length,
                headers, server->config.max_headers, &message);
        }
        if (parsed == CLM_STRING_HTTPP_TOO_MANY_HEADERS) {
            (void)web_error_response(connection, 431, "Request Header Fields Too Large\n");
            result = CLM_NET_WEB_LIMIT_EXCEEDED;
            goto done;
        }
        if (parsed != CLM_STRING_HTTPP_SUCCESS) {
            (void)web_error_response(connection, 400, "Bad Request\n");
            result = CLM_NET_WEB_INVALID_ARGUMENT;
            goto done;
        }
        result = web_handle_request(connection, &input, headers, request_number);
        if (result != CLM_NET_WEB_SUCCESS)
            goto done;
    }
done:
    free(headers);
    clm_bytes_buffer_dispose(&input);
    return result;
}

static clm_async_step_result web_connection_step(clm_id task_id, clm_id context_id)
{
    (void)task_id;
    clm_net_web_connection* connection = (clm_net_web_connection*)(uintptr_t)context_id;
    if (!connection)
        return CLM_ASYNC_STEP_FAIL;
    clm_int status = web_connection_run(connection);
    return status == CLM_NET_WEB_SUCCESS || status == CLM_NET_WEB_NOT_FOUND
        ? CLM_ASYNC_STEP_COMPLETE : CLM_ASYNC_STEP_FAIL;
}

static void web_connection_cleanup(clm_id context_id)
{
    clm_net_web_connection* connection = (clm_net_web_connection*)(uintptr_t)context_id;
    if (!connection)
        return;
    if (connection->tls)
        (void)clm_tls_close(connection->tls);
    if (connection->socket_id)
        (void)clm_net_socket_close(connection->socket_id);
    free(connection);
}

static void web_task_reap(clm_net_web_server* server)
{
    clm_net_web_task_ref** link = &server->tasks;
    while (*link) {
        clm_net_web_task_ref* task = *link;
        clm_async_task_state state;
        if (clm_async_pool_get_state(server->pool_id, task->task_id, &state) ==
                CLM_ASYNC_OPERATION_SUCCESS && clm_async_task_is_terminal(state)) {
            (void)clm_async_pool_task_release(server->pool_id, task->task_id);
            *link = task->next;
            free(task);
        } else {
            link = &task->next;
        }
    }
}

static int web_listener_main(void* argument)
{
    clm_net_web_server* server = argument;
    while (!atomic_load_explicit(&server->stopping, memory_order_acquire)) {
        web_task_reap(server);
        unsigned int events = 0;
        clm_net_status status = clm_net_wait(server->listener_id,
            CLM_NET_EVENT_READ, 100, &events);
        if (status == CLM_NET_SUCCESS && (events & CLM_NET_EVENT_READ)) {
            for (;;) {
                clm_id probe = web_next_id();
                clm_net_address peer;
                clm_net_status accepted = clm_net_accept(server->listener_id, probe, &peer);
                if (accepted == CLM_NET_WOULD_BLOCK)
                    break;
                if (accepted != CLM_NET_SUCCESS)
                    break;
                clm_net_web_connection* connection = calloc(1, sizeof(*connection));
                clm_net_web_task_ref* task = calloc(1, sizeof(*task));
                if (!connection || !task) {
                    free(connection);
                    free(task);
                    (void)clm_net_socket_close(probe);
                    continue;
                }
                connection->server = server;
                connection->socket_id = probe;
                connection->peer = peer;
                connection->task_id = web_next_id();
                task->task_id = connection->task_id;
                task->next = server->tasks;
                server->tasks = task;
                clm_int spawned = clm_async_pool_spawn(server->pool_id, task->task_id,
                    web_connection_step, (clm_id)(uintptr_t)connection,
                    web_connection_cleanup);
                if (spawned != CLM_ASYNC_OPERATION_SUCCESS) {
                    server->tasks = task->next;
                    free(task);
                    web_connection_cleanup((clm_id)(uintptr_t)connection);
                }
            }
        }
    }
    web_task_reap(server);
    return 0;
}

void clm_net_web_config_init(clm_net_web_config* config)
{
    if (!config)
        return;
    memset(config, 0, sizeof(*config));
    config->worker_count = 4;
    config->max_pending_connections = 128;
    config->max_request_bytes = CLM_NET_WEB_DEFAULT_REQUEST_LIMIT;
    config->max_response_bytes = CLM_NET_WEB_DEFAULT_RESPONSE_LIMIT;
    config->max_headers = CLM_NET_WEB_DEFAULT_HEADER_COUNT;
    config->max_keep_alive_requests = CLM_NET_WEB_DEFAULT_KEEP_ALIVE_REQUESTS;
    config->io_timeout_ms = 30000;
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
    if (!server->config.max_request_bytes) server->config.max_request_bytes = defaults.max_request_bytes;
    if (!server->config.max_response_bytes) server->config.max_response_bytes = defaults.max_response_bytes;
    if (!server->config.max_headers) server->config.max_headers = defaults.max_headers;
    if (!server->config.max_keep_alive_requests) server->config.max_keep_alive_requests = defaults.max_keep_alive_requests;
    if (!server->config.io_timeout_ms) server->config.io_timeout_ms = defaults.io_timeout_ms;
    if (!server->config.backlog) server->config.backlog = defaults.backlog;
    if (server->config.worker_count > 256 || server->config.max_headers > 4096 ||
        server->config.io_timeout_ms > INT_MAX || server->config.backlog < 0) {
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
    if (strcmp(method, "*") != 0 && !web_header_name_valid(method))
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
        method_copy, pattern_copy, handler, handler_context};
    return CLM_NET_WEB_SUCCESS;
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
        1,
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
    atomic_store_explicit(&server->stopping, false, memory_order_release);
    if (thrd_create(&server->listener_thread, web_listener_main, server) != thrd_success) {
        atomic_store_explicit(&server->stopping, true, memory_order_release);
        (void)clm_async_pool_destroy(server->pool_id);
        server->pool_open = false;
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
        clm_net_web_task_ref* next = server->tasks->next;
        free(server->tasks);
        server->tasks = next;
    }
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