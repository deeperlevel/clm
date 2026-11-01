/*
 * =============================================================================
 * CLM net/web + clm_template: a tiny, real web application.
 * =============================================================================
 *
 * LEARNING JOURNEY
 * ----------------
 * This example deliberately walks through the same layers you would assemble
 * when building a larger web application with Cross Language Modules:
 *
 *   1. Read application assets from disk.
 *   2. Compile an HTML template ONCE, before accepting requests.
 *   3. Initialize the networking subsystem.
 *   4. Configure an HTTP server and register middleware/routes.
 *   5. For each GET /, create a fresh template context from request data.
 *   6. Render the template through clm_net_web_response_render_template().
 *   7. Serve the stylesheet as a separate, correctly typed HTTP resource.
 *   8. Stop the server before releasing data that request handlers borrow.
 *
 * Browser demo:
 *   http://127.0.0.1:8080/
 *   http://127.0.0.1:8080/?name=Ada
 *   http://127.0.0.1:8080/index.html?name=CLM
 *
 * Routes are GET-only. The template is compiled, cached, and automatically
 * rendered afresh for each request; a browser hard refresh also attempts to
 * recompile it from disk (and keeps the last valid version if compilation
 * fails). This is an educational example, not a production static-file host.
 *
 * IMPORTANT: The example uses the new clm_http_utils names and the current
 * non-const clm_net_web_middleware_fn signature.
 */

#include <stddef.h>  /* size_t: include before CLM headers for older async.h snapshots. */

#include "clm/net_web.h"          /* HTTP server, routes, middleware, response helpers */
#include "clm/http_utils.h"       /* HTTP span helpers; formerly string_httpp */
#include "clm/template.h"         /* Compiled templates and per-request contexts */

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <shellapi.h>
#endif

#define EXAMPLE_DEFAULT_PORT 8080u
#define EXAMPLE_MAX_ASSET_BYTES (1024u * 1024u)
#define EXAMPLE_MAX_NAME_BYTES 80u

/*
 * STEP 1: Define application-owned state.
 *
 * The HTTP server does not own or copy application handler_context pointers.
 * This struct must therefore stay alive until after server_stop() returns.
 * All workers share the COMPILED template, but each request creates its own
 * clm_template_context. A lock protects recompilation during hard refresh.
 */
typedef struct {
    const char* path;                  /* Borrowed command-line asset path. */
    mtx_t lock;                        /* Protects the compiled template. */
    bool lock_ready;
    clm_template compiled;             /* Owns its parsed template program. */
    atomic_uint_fast64_t page_views;   /* Counts HTML responses, not CSS. */
    uint16_t port;
} example_application;

/* The CSS path and bytes are independent of the compiled HTML template. */
typedef struct {
    const char* path;
    unsigned char* bytes;
    size_t length;
    mtx_t lock;
    bool lock_ready;
} example_stylesheet;

/*
 * STEP 2: A bounded asset loader.
 *
 * Files are read into caller-owned memory, with a 1 MiB cap so a mistaken
 * file path cannot cause unbounded allocation. The extra byte is a convenience
 * for text diagnostics, but the template parser still receives the exact
 * byte count: it does not require a terminating NUL.
 */
static int example_read_file(const char* path, unsigned char** out, size_t* out_size)
{
    FILE* file;
    long length;
    unsigned char* bytes;
    size_t read_count;

    if (!path || !out || !out_size)
        return -1;
    file = fopen(path, "rb");
    if (!file)
        return -1;
    if (fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) <= 0 ||
        (unsigned long)length > EXAMPLE_MAX_ASSET_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    bytes = malloc((size_t)length + 1);
    if (!bytes) {
        fclose(file);
        return -1;
    }
    read_count = fread(bytes, 1, (size_t)length, file);
    if (fclose(file) != 0 || read_count != (size_t)length) {
        free(bytes);
        return -1;
    }
    bytes[length] = '\0';
    *out = bytes;
    *out_size = (size_t)length;
    return 0;
}

/* Build styles.css next to whichever index.html was selected on the CLI. */
static int example_sibling_path(const char* html, const char* leaf, char* out, size_t capacity)
{
    const char* slash = strrchr(html, '/');
    const char* backslash = strrchr(html, '\\');
    size_t prefix;
    int size;
    if (!slash || (backslash && backslash > slash))
        slash = backslash;
    prefix = slash ? (size_t)(slash - html) + 1 : 0;
    size = snprintf(out, capacity, "%.*s%s", (int)prefix, html, leaf);
    return size > 0 && (size_t)size < capacity ? 0 : -1;
}

/*
 * STEP 3: Compile the page BEFORE starting HTTP.
 *
 * clm_template_compile() copies its source text into its own representation.
 * That means we can free the file bytes once compilation succeeds. The
 * template lives for the whole server lifetime; no parsing is needed for the
 * normal request fast path. Syntax failures include source line/column.
 *
 * Note the failure-safe swap: a bad edited template never replaces the last
 * working one. Call this only while holding app->lock, or during startup.
 */
static int example_compile_page(example_application* app)
{
    unsigned char* source = NULL;
    size_t source_length = 0;
    clm_template replacement = {0};
    clm_template_error error = {0};
    clm_int status;

    if (example_read_file(app->path, &source, &source_length) != 0) {
        fprintf(stderr, "Could not read HTML template '%s'.\n", app->path);
        return -1;
    }
    status = clm_template_compile(&replacement, (const char*)source,
                                  source_length, NULL, &error);
    free(source);
    if (status != CLM_TEMPLATE_SUCCESS) {
        fprintf(stderr, "Template error in '%s' at %zu:%zu: %s (code %d)\n",
                app->path, error.line, error.column, error.message, (int)status);
        clm_template_dispose(&replacement);
        return -1;
    }
    clm_template_dispose(&app->compiled);
    app->compiled = replacement;
    return 0;
}

static int example_application_init(example_application* app, const char* path, uint16_t port)
{
    memset(app, 0, sizeof(*app));
    app->path = path;
    app->port = port;
    atomic_init(&app->page_views, 0);
    if (mtx_init(&app->lock, mtx_plain) != thrd_success)
        return -1;
    app->lock_ready = true;
    if (example_compile_page(app) != 0)
        return -1;
    return 0;
}

static void example_application_dispose(example_application* app)
{
    clm_template_dispose(&app->compiled);
    if (app->lock_ready)
        mtx_destroy(&app->lock);
    app->lock_ready = false;
}

/*
 * The stylesheet is ordinary static content; there's no reason to run it
 * through the template renderer. Keep its bytes cached and replace them only
 * on an explicit browser hard refresh, mirroring the HTML edit experience.
 */
static int example_stylesheet_init(example_stylesheet* css, const char* path)
{
    memset(css, 0, sizeof(*css));
    css->path = path;
    if (example_read_file(path, &css->bytes, &css->length) != 0)
        return -1;
    if (mtx_init(&css->lock, mtx_plain) != thrd_success) {
        free(css->bytes);
        css->bytes = NULL;
        return -1;
    }
    css->lock_ready = true;
    return 0;
}

static void example_stylesheet_dispose(example_stylesheet* css)
{
    free(css->bytes);
    css->bytes = NULL;
    if (css->lock_ready)
        mtx_destroy(&css->lock);
    css->lock_ready = false;
}

/*
 * HTTP request headers contain borrowed spans, NOT necessarily NUL-terminated
 * strings. To detect a hard refresh, inspect the length-aware span directly.
 * We use the explicit no-cache directive as the educational reload trigger.
 */
static bool example_hard_reload(const clm_net_web_request* request)
{
    static const char* names[] = {"Cache-Control", "Pragma"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        clm_http_utils_span value;
        if (clm_net_web_request_header(request, names[i], &value)) {
            for (size_t pos = 0; pos + 8 <= value.length; ++pos) {
                clm_http_utils_span candidate = {value.data + pos, 8};
                if (clm_http_utils_span_equals_ignore_case(candidate, "no-cache"))
                    return true;
            }
        }
    }
    return false;
}

/*
 * STEP 4: A small, deliberate URL-query decoder.
 *
 * We want the demo to feel like an actual application: visit /?name=Ada and
 * the server will greet Ada. The decoding is intentionally restricted to one
 * parameter, bounded to 80 bytes. '+' becomes space and %HH is decoded.
 *
 * IMPORTANT: decoding URL syntax is not HTML escaping. That is why render
 * options below explicitly enable autoescape. Never mark user input |safe.
 * A real application should use a dedicated reusable query-parser utility.
 */
static int example_hex(unsigned char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
    return -1;
}

static bool example_read_visitor_name(const clm_net_web_request* request,
                                      char out[EXAMPLE_MAX_NAME_BYTES + 1])
{
    clm_http_utils_span target = request->message.target;
    const char* query = memchr(target.data, '?', target.length);
    const char* end = target.data + target.length;
    if (!query)
        return false;
    ++query;
    while (query < end) {
        const char* separator = memchr(query, '&', (size_t)(end - query));
        const char* stop = separator ? separator : end;
        const char* equals = memchr(query, '=', (size_t)(stop - query));
        if (equals && (size_t)(equals - query) == 4 && memcmp(query, "name", 4) == 0) {
            size_t count = 0;
            const char* cursor = equals + 1;
            for (; cursor < stop; ++cursor) {
                unsigned char c = (unsigned char)*cursor;
                if (c == '+') c = ' ';
                else if (c == '%') {
                    int hi, lo;
                    if (stop - cursor < 3 ||
                        (hi = example_hex((unsigned char)cursor[1])) < 0 ||
                        (lo = example_hex((unsigned char)cursor[2])) < 0)
                        return false;
                    c = (unsigned char)((hi << 4) | lo);
                    cursor += 2;
                }
                /* Refuse control bytes and NUL; the context accepts C strings. */
                if (c < 0x20 || c == 0x7f || count >= EXAMPLE_MAX_NAME_BYTES)
                    return false;
                out[count++] = (char)c;
            }
            out[count] = '\0';
            return count > 0;
        }
        query = separator ? separator + 1 : end;
    }
    return false;
}

/* The template wants a human-readable UTC timestamp, not a deadline counter. */
static void example_utc_timestamp(char out[40])
{
    time_t now = time(NULL);
    struct tm calendar;
#if defined(_WIN32)
    if (gmtime_s(&calendar, &now) != 0) {
#else
    if (gmtime_r(&now, &calendar) == NULL) {
#endif
        strcpy(out, "unavailable");
        return;
    }
    if (!strftime(out, 40, "%Y-%m-%d %H:%M:%S UTC", &calendar))
        strcpy(out, "unavailable");
}

/*
 * STEP 5: Global HTTP middleware.
 *
 * The latest clm_net_web_middleware_fn signature accepts NON-CONST request
 * and NON-CONST context pointers. Middleware executes before a matched route;
 * returning DONE bypasses the handler. This is where we implement a uniform
 * GET-only rule and attach a Server response header to every parsed request.
 *
 * The framework's own unmatched routes still return 404 automatically.
 */
static clm_int example_get_only(clm_net_web_request* request,
                                clm_net_web_response* response, void* context)
{
    clm_int status;
    (void)context;
    status = clm_net_web_response_set_header(response, "Server", "clm net/web");
    if (status != CLM_NET_WEB_SUCCESS)
        return status;
    if (clm_http_utils_span_equals(request->message.method, "GET"))
        return CLM_NET_WEB_MIDDLEWARE_CONTINUE;

    /* Explicit 405 for methods other than GET, even on unmatched URLs. */
    status = clm_net_web_response_set_status(response, 405);
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_set_header(response, "Allow", "GET");
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_set_header(response,
                    "Content-Type", "text/plain; charset=utf-8");
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_write_string(response,
                    "Method Not Allowed. This demo supports GET only.\n");
    return status == CLM_NET_WEB_SUCCESS ? CLM_NET_WEB_MIDDLEWARE_DONE : status;
}

/*
 * STEP 6: The application's dynamic page handler.
 *
 * Distinguish these lifetimes carefully:
 *
 *   app->compiled : shared, compiled once; safe to render concurrently unless
 *                   another thread is replacing it during a hard refresh.
 *   context       : created separately FOR EACH REQUEST, then disposed.
 *   response      : borrowed from clm_net_web and owned by the HTTP runtime.
 *
 * autoescape=true makes user-provided visitor_name and request_target safe
 * to print as ordinary HTML text. strict_undefined=true catches typos in the
 * template, rather than silently showing blank fields.
 */
static clm_int example_home(clm_net_web_request* request,
                            clm_net_web_response* response, void* context)
{
    example_application* app = (example_application*)context;
    clm_template_context values = {0};
    clm_template_error error = {0};
    clm_template_options render_options = {0};
    char visitor[EXAMPLE_MAX_NAME_BYTES + 1] = {0};
    char timestamp[40];
    char id_text[32];
    char request_target[256];
    uint64_t view_number;
    size_t target_length;
    bool has_name;
    clm_int status;

    if (!app)
        return CLM_NET_WEB_HANDLER_ERROR;

    /* A hard refresh recompiles the source; ordinary page views never do. */
    if (mtx_lock(&app->lock) != thrd_success)
        return CLM_NET_WEB_HANDLER_ERROR;
    if (example_hard_reload(request) && example_compile_page(app) != 0)
        fprintf(stderr, "Keeping the previous valid HTML template.\n");

    status = clm_template_context_init(&values);
    if (status != CLM_TEMPLATE_SUCCESS) {
        mtx_unlock(&app->lock);
        return CLM_NET_WEB_HANDLER_ERROR;
    }

    view_number = atomic_fetch_add_explicit(&app->page_views, 1,
                                           memory_order_relaxed) + 1;
    has_name = example_read_visitor_name(request, visitor);
    example_utc_timestamp(timestamp);
    (void)snprintf(id_text, sizeof(id_text), "%" PRIu64,
                   (uint64_t)clm_net_web_context_request_id(request->context));
    target_length = request->message.target.length;
    if (target_length >= sizeof(request_target))
        target_length = sizeof(request_target) - 1;
    memcpy(request_target, request->message.target.data, target_length);
    request_target[target_length] = '\0';

    /*
     * Every value referenced by index.html is bound here. In a larger app,
     * these fields would often come from domain services and clm_tree data.
     * We intentionally use all three basic context types: text, integer,
     * and Boolean, so readers can see that a template is more than a simple
     * printf() substitute.
     */
    if (clm_template_context_set_string(&values, "visitor_name",
            has_name ? visitor : "friend") != CLM_TEMPLATE_SUCCESS ||
        clm_template_context_set_bool(&values, "has_visitor_name", has_name) != CLM_TEMPLATE_SUCCESS ||
        clm_template_context_set_string(&values, "request_target", request_target) != CLM_TEMPLATE_SUCCESS ||
        clm_template_context_set_string(&values, "request_id", id_text) != CLM_TEMPLATE_SUCCESS ||
        clm_template_context_set_string(&values, "rendered_at", timestamp) != CLM_TEMPLATE_SUCCESS ||
        clm_template_context_set_int(&values, "page_views", (int64_t)view_number) != CLM_TEMPLATE_SUCCESS ||
        clm_template_context_set_int(&values, "server_port", (int64_t)app->port) != CLM_TEMPLATE_SUCCESS) {
        clm_template_context_dispose(&values);
        mtx_unlock(&app->lock);
        return CLM_NET_WEB_NO_MEMORY;
    }

    render_options.autoescape = true;
    render_options.strict_undefined = true;
    render_options.max_output = EXAMPLE_MAX_ASSET_BYTES;
    render_options.max_iterations = 100;
    status = clm_net_web_response_set_header(response, "Cache-Control", "no-store");
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_set_header(response,
                    "X-Content-Type-Options", "nosniff");
    if (status == CLM_NET_WEB_SUCCESS) {
        /* This helper calls clm_template_render() and sets the HTML MIME type. */
        status = clm_net_web_response_render_template(
            response, &app->compiled, &values, &render_options, &error);
        if (status != CLM_NET_WEB_SUCCESS)
            fprintf(stderr, "Rendering failed at %zu:%zu: %s (code %d)\n",
                    error.line, error.column, error.message, (int)status);
    }

    clm_template_context_dispose(&values);
    mtx_unlock(&app->lock);
    return status;
}

/*
 * STEP 7: Static assets are a separate route.
 *
 * The browser loads /styles.css after reading index.html. We serve the CSS
 * bytes as text/css, not HTML, and skip the template engine completely.
 * This tiny example has an explicit file allowlist; never concatenate an
 * untrusted URL path to a filesystem directory and call fopen() on it.
 */
static clm_int example_css(clm_net_web_request* request,
                           clm_net_web_response* response, void* context)
{
    example_stylesheet* css = (example_stylesheet*)context;
    clm_int status;
    if (!css || mtx_lock(&css->lock) != thrd_success)
        return CLM_NET_WEB_HANDLER_ERROR;
    if (example_hard_reload(request)) {
        unsigned char* replacement = NULL;
        size_t replacement_length = 0;
        if (example_read_file(css->path, &replacement, &replacement_length) == 0) {
            free(css->bytes);
            css->bytes = replacement;
            css->length = replacement_length;
        } else {
            fprintf(stderr, "Unable to reload CSS; using cached stylesheet.\n");
        }
    }
    status = clm_net_web_response_set_header(response,
                "Content-Type", "text/css; charset=utf-8");
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_set_header(response, "Cache-Control", "no-store");
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_set_header(response, "X-Content-Type-Options", "nosniff");
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_response_write(response, css->bytes, css->length);
    mtx_unlock(&css->lock);
    return status;
}

/* A port override keeps this example usable alongside another local server. */
static int example_parse_port(const char* text, uint16_t* out_port)
{
    unsigned long port = 0;
    if (!text || !*text || !out_port)
        return -1;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (*p < '0' || *p > '9')
            return -1;
        port = port * 10 + (unsigned long)(*p - '0');
        if (port > 65535)
            return -1;
    }
    if (!port)
        return -1;
    *out_port = (uint16_t)port;
    return 0;
}

/* Optional convenience only; no browser launch is required for the server. */
static void example_open_browser(uint16_t port)
{
    char url[64];
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/", (unsigned int)port);
#if defined(_WIN32)
    (void)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
    char command[128];
#  if defined(__APPLE__)
    (void)snprintf(command, sizeof(command), "open '%s' >/dev/null 2>&1 &", url);
#  else
    (void)snprintf(command, sizeof(command), "xdg-open '%s' >/dev/null 2>&1 &", url);
#  endif
    (void)system(command);
#endif
}

/*
 * STEP 8: The main application composition root.
 *
 * Notice how little code is CLM-specific at the top level. The modules
 * cooperate, but there is no mandatory framework bootstrapper or service
 * container. The server manages sockets/tasks while OUR code owns the
 * compiled template, stylesheet and handler contexts.
 *
 * Shutdown order is critical in concurrent C:
 *   stop server -> join handler workers -> destroy server -> free app state.
 * Never free app or css while an in-flight handler may still access them.
 */
int main(int argc, char** argv)
{
    const char* html_path = argc >= 2 ? argv[1] : "index.html";
    uint16_t port = EXAMPLE_DEFAULT_PORT;
    char css_path[1024];
    example_application app = {0};
    example_stylesheet css = {0};
    clm_net_web_server* server = NULL;
    clm_net_web_config config;
    clm_net_address bind_address;
    bool net_initialized = false;
    int exit_code = EXIT_FAILURE;
    clm_int status;

    if (argc > 3 || (argc == 3 && example_parse_port(argv[2], &port) != 0)) {
        fprintf(stderr, "Usage: %s [index.html] [port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (example_sibling_path(html_path, "styles.css", css_path, sizeof(css_path)) != 0) {
        fprintf(stderr, "HTML path is too long.\n");
        return EXIT_FAILURE;
    }

    /* Both assets must load successfully before the listener becomes visible. */
    if (example_application_init(&app, html_path, port) != 0 ||
        example_stylesheet_init(&css, css_path) != 0) {
        fprintf(stderr, "Startup failed: index.html and styles.css must sit together.\n");
        goto cleanup;
    }

    if (clm_net_initialize() != CLM_NET_SUCCESS) {
        fprintf(stderr, "clm_net_initialize() failed.\n");
        goto cleanup;
    }
    net_initialized = true;
    if (clm_net_address_parse("127.0.0.1", port, &bind_address) != CLM_NET_SUCCESS) {
        fprintf(stderr, "Invalid listening address.\n");
        goto cleanup;
    }

    clm_net_web_config_init(&config);
    config.enable_gzip = true;            /* Negotiated automatically where suitable. */
    /* The new reactor, min-heap deadlines, parked-task queues and persistent
     * poller run behind this same API; no event-loop code is needed here. */
    status = clm_net_web_server_create(&config, &server);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Unable to create CLM WebServer: %d\n", (int)status);
        goto cleanup;
    }

    status = clm_net_web_middleware_add(server, example_get_only, NULL);
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_route_add(server, "GET", "/", example_home, &app);
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_route_add(server, "GET", "/index.html", example_home, &app);
    if (status == CLM_NET_WEB_SUCCESS)
        status = clm_net_web_route_add(server, "GET", "/styles.css", example_css, &css);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Unable to register routes/middleware: %d\n", (int)status);
        goto cleanup;
    }

    status = clm_net_web_server_start(server, &bind_address);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Unable to start HTTP listener: %d\n", (int)status);
        goto cleanup;
    }

    printf("\nCLM WebServer + clm_template is running!\n");
    printf("  URL:         http://127.0.0.1:%u/\n", (unsigned int)port);
    printf("  Dynamic:     GET / and GET /index.html\n");
    printf("  Static:      GET /styles.css\n");
    printf("  Try:         http://127.0.0.1:%u/?name=Ada\n", (unsigned int)port);
    printf("  Header:      Server: clm net/web\n");
    printf("  Hot reload:  Browser hard refresh (Cache-Control: no-cache)\n");
    printf("  Stop:        Press ENTER\n\n");
    fflush(stdout);
    example_open_browser(port);
    (void)getchar();

    /* If stopping fails, do NOT free resources still reachable by workers. */
    status = clm_net_web_server_stop(server);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Server did not stop safely (%d).\n", (int)status);
        return EXIT_FAILURE;
    }
    exit_code = EXIT_SUCCESS;

cleanup:
    if (server)
        clm_net_web_server_destroy(server);
    if (net_initialized && clm_net_finalize() != CLM_NET_SUCCESS) {
        fprintf(stderr, "Warning: CLM network finalization failed.\n");
        exit_code = EXIT_FAILURE;
    }
    example_stylesheet_dispose(&css);
    example_application_dispose(&app);
    return exit_code;
}
