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
 * Handlers are synchronous and run on pool workers. Network waits use bounded
 * readiness waits. Each active connection occupies a worker, including while
 * waiting for input; long-running handlers do likewise until they return.
 */

#include "async_pool.h"
#include "bytes.h"
#include "compress.h"
#include "multipart.h"
#include "stream.h"
#include "string_httpp.h"
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

#define CLM_NET_WEB_DEFAULT_REQUEST_LIMIT ((size_t)8 * 1024 * 1024)
#define CLM_NET_WEB_DEFAULT_RESPONSE_LIMIT ((size_t)16 * 1024 * 1024)
#define CLM_NET_WEB_DEFAULT_HEADER_COUNT ((size_t)128)
#define CLM_NET_WEB_DEFAULT_KEEP_ALIVE_REQUESTS ((size_t)100)

typedef struct clm_net_web_server clm_net_web_server;
typedef struct clm_net_web_context clm_net_web_context;
typedef struct clm_net_web_response clm_net_web_response;

typedef struct {
    const char* data;
    size_t length;
} clm_net_web_span;

typedef struct {
    clm_net_web_span name;
    clm_net_web_span value;
} clm_net_web_param;

typedef struct {
    size_t worker_count;
    size_t max_pending_connections;
    size_t max_request_bytes;
    size_t max_response_bytes;
    size_t max_headers;
    size_t max_keep_alive_requests;
    uint32_t io_timeout_ms;
    int backlog;
    bool enable_gzip;
    /** NULL serves plain HTTP; otherwise the server uses clm_tls. The server
     * copies the options and strings during create. */
    const clm_tls_options* tls;
} clm_net_web_config;

typedef struct {
    clm_string_httpp_message message;
    clm_bytes_view body;
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
    clm_string_httpp_span* out_value);
bool clm_net_web_request_param(
    const clm_net_web_request* request, const char* name,
    clm_net_web_span* out_value);

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
clm_int clm_net_web_response_redirect(
    clm_net_web_response* response, const char* location, clm_uint status);
clm_int clm_net_web_response_render_template(
    clm_net_web_response* response, const clm_template* tmpl,
    const clm_template_context* context,
    const clm_template_options* options, clm_template_error* error);

#ifdef __cplusplus
}
#endif