/*
 * CLM HTTP/1.1 "It works" example.
 *
 * Serves a local index.html (cached; reloaded on a browser hard reload) using GET / and
 * GET /index.html. Every successfully parsed request receives a
 * Server: clm net/web response header. Other methods receive 405.
 *
 * This is a minimal HTTP example, not a production static-file server.
 */

#include "clm/console.h"
#include "clm/net_web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#define EXAMPLE_DEFAULT_PORT 8080u
#define EXAMPLE_MAX_HTML_BYTES (1024u * 1024u)

typedef struct {
    unsigned char* data;
    size_t length;
} example_page;

/**
 * Loads the HTML document from disk into a new buffer.
 *
 * @param path Path to the local index.html file.
 * @param page Receives an owned buffer on success; initialized to empty first.
 * @return 0 on success, -1 on file, size, or memory error.
 * @ownership Caller must free page->data.
 */
static int example_page_load(const char* path, example_page* page)
{
    FILE* file = NULL;
    long file_size = 0;
    size_t size = 0;
    unsigned char* data = NULL;

    if (path == NULL || page == NULL) {
        return -1;
    }

    page->data = NULL;
    page->length = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    file_size = ftell(file);
    if (file_size <= 0 || (unsigned long)file_size > EXAMPLE_MAX_HTML_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    size = (size_t)file_size;
    data = (unsigned char*)malloc(size);
    if (data == NULL) {
        fclose(file);
        return -1;
    }

    size_t bytes_read = fread(data, 1, size, file);
    int close_status = fclose(file);
    if (bytes_read != size || close_status != 0) {
        free(data);
        return -1;
    }

    page->data = data;
    page->length = size;
    return 0;
}

/**
 * Adds the identifying Server header and rejects all methods except GET.
 *
 * Called for successfully parsed HTTP requests, including unmatched paths.
 * @return CONTINUE for GET, DONE for non-GET, or a negative response error.
 */
static clm_int example_get_only(
    clm_net_web_request* request,
    clm_net_web_response* response,
    void* middleware_context)
{
    clm_int status;

    (void)middleware_context;

    status = clm_net_web_response_set_header(response, "Server", "clm net/web");
    if (status != CLM_NET_WEB_SUCCESS) {
        return status;
    }

    if (clm_string_httpp_span_equals(request->message.method, "GET")) {
        return CLM_NET_WEB_MIDDLEWARE_CONTINUE;
    }

    status = clm_net_web_response_set_status(response, 405);
    if (status != CLM_NET_WEB_SUCCESS) {
        return status;
    }

    status = clm_net_web_response_set_header(response, "Allow", "GET");
    if (status != CLM_NET_WEB_SUCCESS) {
        return status;
    }

    status = clm_net_web_response_set_header(
        response, "Content-Type", "text/plain; charset=utf-8");
    if (status != CLM_NET_WEB_SUCCESS) {
        return status;
    }

    status = clm_net_web_response_write_string(response, "Method Not Allowed. Use GET.\n");
    return status == CLM_NET_WEB_SUCCESS
        ? CLM_NET_WEB_MIDDLEWARE_DONE
        : status;
}

typedef struct {
    const char* path;
    const char* content_type;
    mtx_t lock;
    example_page page;
} example_cache;

/** Loads the file into the cache and creates its lock. Returns 0 or -1. */
static int example_cache_init(example_cache* cache, const char* path, const char* content_type)
{
    cache->path = path;
    cache->content_type = content_type;
    cache->page.data = NULL;
    cache->page.length = 0;
    if (example_page_load(path, &cache->page) != 0) {
        fprintf(stderr, "Cannot read '%s' (maximum size: 1 MiB).\n", path);
        return -1;
    }
    if (mtx_init(&cache->lock, mtx_plain) != thrd_success) {
        fprintf(stderr, "Cannot initialize cache lock.\n");
        free(cache->page.data);
        cache->page.data = NULL;
        return -1;
    }
    return 0;
}

static void example_cache_destroy(example_cache* cache)
{
    if (cache->page.data != NULL) {
        free(cache->page.data);
        cache->page.data = NULL;
        mtx_destroy(&cache->lock);
    }
}

/**
 * Builds the path of a file that sits next to base_path, e.g. styles.css
 * next to ./site/index.html. Returns 0 or -1 if it does not fit.
 */
static int example_sibling_path(
    const char* base_path, const char* name, char* out, size_t out_size)
{
    const char* slash = strrchr(base_path, '/');
    const char* backslash = strrchr(base_path, '\\');
    size_t dir_length;
    int written;

    if (backslash != NULL && (slash == NULL || backslash > slash)) {
        slash = backslash;
    }
    dir_length = slash == NULL ? 0 : (size_t)(slash - base_path) + 1;
    written = snprintf(out, out_size, "%.*s%s", (int)dir_length, base_path, name);
    return written > 0 && (size_t)written < out_size ? 0 : -1;
}

/** Browsers send "no-cache" in Cache-Control/Pragma on a hard reload. */
static bool example_is_hard_reload(const clm_net_web_request* request)
{
    static const char* const names[] = {"Cache-Control", "Pragma"};
    clm_string_httpp_span value;
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (clm_net_web_request_header(request, names[i], &value) &&
            clm_string_httpp_span_equals_ignore_case(value, "no-cache")) {
            return true;
        }
    }
    return false;
}

/**
 * Serves a cached file (index.html or styles.css). The cache is reloaded from
 * disk only when the request is a browser hard reload.
 *
 * @param request Borrowed request (not modified).
 * @param response Borrowed response used to set headers and append the body.
 * @param handler_context Borrowed example_cache shared by all workers.
 * @return CLM_NET_WEB_SUCCESS or a negative CLM_NET_WEB error.
 */
static clm_int example_serve_file(
    clm_net_web_request* request,
    clm_net_web_response* response,
    void* handler_context)
{
    example_cache* cache = (example_cache*)handler_context;
    clm_int status;

    if (cache == NULL || mtx_lock(&cache->lock) != thrd_success) {
        return CLM_NET_WEB_HANDLER_ERROR;
    }

    if (example_is_hard_reload(request)) {
        example_page fresh = {0};

        printf("Hard reload detected: reloading '%s'.\n", cache->path);
        fflush(stdout);
        if (example_page_load(cache->path, &fresh) == 0) {
            free(cache->page.data);
            cache->page = fresh;
        } else {
            fprintf(stderr, "Reload failed; serving previously cached page.\n");
        }
    }

    if (cache->page.data == NULL) {
        mtx_unlock(&cache->lock);
        return CLM_NET_WEB_HANDLER_ERROR;
    }

    status = clm_net_web_response_set_header(
        response, "Content-Type", cache->content_type);
    if (status == CLM_NET_WEB_SUCCESS) {
        status = clm_net_web_response_set_header(response, "Cache-Control", "no-store");
    }
    if (status == CLM_NET_WEB_SUCCESS) {
        status = clm_net_web_response_set_header(response, "X-Content-Type-Options", "nosniff");
    }
    if (status == CLM_NET_WEB_SUCCESS) {
        status = clm_net_web_response_write(
            response, cache->page.data, cache->page.length);
    }

    mtx_unlock(&cache->lock);
    return status;
}

/**
 * Parses a decimal port from the command line.
 *
 * @param text NUL-terminated decimal port string.
 * @param out_port Receives a nonzero TCP port on success.
 * @return 0 on success, -1 on malformed or out-of-range port.
 */
static int example_parse_port(const char* text, uint16_t* out_port)
{
    unsigned long value = 0;
    const unsigned char* cursor = (const unsigned char*)text;

    if (text == NULL || *text == '\0' || out_port == NULL) {
        return -1;
    }

    while (*cursor != '\0') {
        if (*cursor < '0' || *cursor > '9') {
            return -1;
        }
        value = value * 10ul + (unsigned long)(*cursor - '0');
        if (value > 65535ul) {
            return -1;
        }
        ++cursor;
    }

    if (value == 0) {
        return -1;
    }

    *out_port = (uint16_t)value;
    return 0;
}

/* Best-effort launch of the default browser; failure is non-fatal. */
static void example_open_browser(uint16_t port)
{
    char url[64];
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/", (unsigned int)port);
#if defined(_WIN32)
    (void)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
    char command[128];
#if defined(__APPLE__)
    (void)snprintf(command, sizeof(command), "open '%s' >/dev/null 2>&1 &", url);
#else
    (void)snprintf(command, sizeof(command), "xdg-open '%s' >/dev/null 2>&1 &", url);
#endif
    (void)system(command);
#endif
}

int main(int argc, char** argv)
{
    const char* html_path = argc >= 2 ? argv[1] : "index.html";
    uint16_t port = EXAMPLE_DEFAULT_PORT;
    clm_net_address address;
    clm_net_web_config config;
    clm_net_web_server* server = NULL;
    example_cache cache = {0};
    example_cache styles = {0};
    char css_path[1024];
    clm_int status = CLM_NET_WEB_SUCCESS;
    int result = EXIT_FAILURE;
    bool network_initialized = false;

    if (argc > 3 || (argc == 3 && example_parse_port(argv[2], &port) != 0)) {
        fprintf(stderr, "Usage: %s [index.html] [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (example_sibling_path(html_path, "styles.css", css_path, sizeof(css_path)) != 0) {
        fprintf(stderr, "Path to '%s' is too long.\n", html_path);
        return EXIT_FAILURE;
    }
    if (example_cache_init(&cache, html_path, "text/html; charset=utf-8") != 0) {
        return EXIT_FAILURE;
    }
    if (example_cache_init(&styles, css_path, "text/css; charset=utf-8") != 0) {
        example_cache_destroy(&cache);
        return EXIT_FAILURE;
    }

    if (clm_net_initialize() != CLM_NET_SUCCESS) {
        fprintf(stderr, "Cannot initialize CLM networking.\n");
        goto cleanup;
    }
    network_initialized = true;

    if (clm_net_address_parse("127.0.0.1", port, &address) != CLM_NET_SUCCESS) {
        fprintf(stderr, "Cannot parse listen address.\n");
        goto cleanup;
    }

    clm_net_web_config_init(&config);
    config.enable_gzip = true;

    status = clm_net_web_server_create(&config, &server);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Cannot create HTTP server (status %d).\n", (int)status);
        goto cleanup;
    }

    status = clm_net_web_middleware_add(server, example_get_only, NULL);
    if (status == CLM_NET_WEB_SUCCESS) {
        status = clm_net_web_route_add(server, "GET", "/", example_serve_file, &cache);
    }
    if (status == CLM_NET_WEB_SUCCESS) {
        status = clm_net_web_route_add(server, "GET", "/index.html", example_serve_file, &cache);
    }
    if (status == CLM_NET_WEB_SUCCESS) {
        status = clm_net_web_route_add(server, "GET", "/styles.css", example_serve_file, &styles);
    }
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Cannot register HTTP routes (status %d).\n", (int)status);
        goto cleanup;
    }

    clm_console_write_line_format("Starting CLM HTTP/1.1 server at http://127.0.0.1:%u/ (port %u)\n",
           (unsigned int)port, (unsigned int)port);
    fflush(stdout);

    status = clm_net_web_server_start(server, &address);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Cannot start HTTP server (status %d).\n", (int)status);
        goto cleanup;
    }

    printf("CLM HTTP/1.1 server is running.\n");
    example_open_browser(port);
    printf("GET routes: /, /index.html and /styles.css\n");
    printf("Response header: Server: clm net/web\n");
    printf("Press ENTER to stop.\n");
    fflush(stdout);
    (void)getchar();

    status = clm_net_web_server_stop(server);
    if (status != CLM_NET_WEB_SUCCESS) {
        fprintf(stderr, "Failed to stop server safely (status %d).\n", (int)status);
        /* The process is exiting; don't release state while workers may use it. */
        return EXIT_FAILURE;
    }

    result = EXIT_SUCCESS;

cleanup:
    if (server != NULL) {
        clm_net_web_server_destroy(server);
    }
    if (network_initialized && clm_net_finalize() != CLM_NET_SUCCESS) {
        fprintf(stderr, "Warning: networking could not be finalized cleanly.\n");
        result = EXIT_FAILURE;
    }
    example_cache_destroy(&styles);
    example_cache_destroy(&cache);
    return result;
}
