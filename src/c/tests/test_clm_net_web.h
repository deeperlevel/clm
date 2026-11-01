#pragma once

#include "test.h"
#include "../clm/net_web.h"

static bool clm_net_web_test_response_complete(const char* data, size_t length)
{
    const char* body = strstr(data, "\r\n\r\n");
    const char* field = strstr(data, "Content-Length:");
    if (!body || !field || field > body)
        return false;
    const char* value = field + sizeof("Content-Length:") - 1;
    while (*value == ' ' || *value == '\t')
        ++value;
    if (*value < '0' || *value > '9')
        return false;
    size_t body_length = 0;
    while (*value >= '0' && *value <= '9') {
        size_t digit = (size_t)(*value - '0');
        if (body_length > ((size_t)-1 - digit) / 10)
            return false;
        body_length = body_length * 10 + digit;
        ++value;
    }
    size_t header_length = (size_t)(body + 4 - data);
    return body_length <= (size_t)-1 - header_length &&
        length >= header_length + body_length;
}

static clm_int clm_net_web_test_handler(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)context;
    clm_net_web_span name;
    if (!clm_net_web_request_param(request, "name", &name))
        return CLM_NET_WEB_HANDLER_ERROR;
    if (clm_net_web_response_set_header(response, "Content-Type", "text/plain; charset=utf-8") !=
            CLM_NET_WEB_SUCCESS ||
        clm_net_web_response_write(response, name.data, name.length) != CLM_NET_WEB_SUCCESS)
        return CLM_NET_WEB_HANDLER_ERROR;
    return CLM_NET_WEB_SUCCESS;
}

static clm_int clm_net_web_test_middleware(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)request;
    (void)response;
    return context ? CLM_NET_WEB_MIDDLEWARE_CONTINUE : CLM_NET_WEB_HANDLER_ERROR;
}

static clm_int clm_net_web_test_stream_handler(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)request;
    (void)context;
    if (clm_net_web_response_set_header(response, "Content-Type", "text/plain") !=
            CLM_NET_WEB_SUCCESS ||
        clm_net_web_response_write_string(response, "first") != CLM_NET_WEB_SUCCESS ||
        clm_net_web_response_flush(response) != CLM_NET_WEB_SUCCESS ||
        clm_net_web_response_write_string(response, "second") != CLM_NET_WEB_SUCCESS)
        return CLM_NET_WEB_HANDLER_ERROR;
    return CLM_NET_WEB_SUCCESS;
}

static clm_int clm_net_web_test_body_handler(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)context;
    unsigned char piece[2];
    for (;;) {
        size_t received = 0;
        clm_int status = clm_net_web_request_read_body(
            request, piece, sizeof(piece), &received);
        if (status == CLM_NET_WEB_END)
            return CLM_NET_WEB_SUCCESS;
        if (status != CLM_NET_WEB_SUCCESS || received == 0)
            return CLM_NET_WEB_HANDLER_ERROR;
        if (clm_net_web_response_write(response, piece, received) != CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_HANDLER_ERROR;
    }
}

static bool clm_net_web_test_open(
    clm_id id, const clm_net_address* address, const char* request)
{
    size_t sent = 0;
    size_t length = strlen(request);
    return clm_net_socket_create(id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_connect(id, address) == CLM_NET_SUCCESS &&
        clm_net_send(id, request, length, &sent) == CLM_NET_SUCCESS && sent == length;
}

/* Reads until the server closes the connection or no data arrives in time. */
static size_t clm_net_web_test_read_all(clm_id id, char* buffer, size_t capacity,
    uint32_t timeout_ms)
{
    size_t total = 0;
    while (total + 1 < capacity) {
        unsigned int events = 0;
        size_t received = 0;
        if (clm_net_wait(id, CLM_NET_EVENT_READ, timeout_ms, &events) != CLM_NET_SUCCESS)
            break;
        clm_net_status status = clm_net_receive(id, buffer + total,
            capacity - total - 1, &received);
        if (status != CLM_NET_SUCCESS || !received)
            break;
        total += received;
    }
    buffer[total] = '\0';
    return total;
}

static clm_int clm_net_web_test_big_handler(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)request;
    (void)context;
    static unsigned char block[65536];
    memset(block, 'x', sizeof(block));
    for (size_t i = 0; i < 64; ++i)
        if (clm_net_web_response_write(response, block, sizeof(block)) != CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_HANDLER_ERROR;
    return CLM_NET_WEB_SUCCESS;
}

static int clm_net_web_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Net Web");

    clm_net_web_config config;
    clm_net_web_config_init(&config);
    CLM_ASSERT(config.worker_count == 4 && config.max_pending_connections > 0 &&
                   config.max_request_bytes == CLM_NET_WEB_DEFAULT_REQUEST_LIMIT &&
                   config.enable_gzip,
               "initialize bounded HTTP server defaults");

    clm_net_web_server* server = NULL;
    int user_context = 1;
    CLM_ASSERT(clm_net_web_server_create(&config, &server) == CLM_NET_WEB_SUCCESS && server &&
                   clm_net_web_route_add(server, "GET", "/hello/:name",
                       clm_net_web_test_handler, &user_context) == CLM_NET_WEB_SUCCESS &&
                   clm_net_web_route_add(server, "*", "/fallback/*path",
                       clm_net_web_test_handler, &user_context) == CLM_NET_WEB_SUCCESS &&
                   clm_net_web_middleware_add(server, clm_net_web_test_middleware,
                       &user_context) == CLM_NET_WEB_SUCCESS,
               "create a server and register parameterized routes and middleware");
    CLM_ASSERT(clm_net_web_route_add(server, "GET", "relative",
                       clm_net_web_test_handler, NULL) == CLM_NET_WEB_INVALID_ARGUMENT &&
                   clm_net_web_route_add(server, "bad method", "/", clm_net_web_test_handler, NULL) ==
                       CLM_NET_WEB_INVALID_ARGUMENT &&
                   clm_net_web_server_local_address(server, &(clm_net_address){0}) ==
                       CLM_NET_WEB_INVALID_ARGUMENT,
               "reject malformed routes and address queries before start");
    clm_net_web_server_destroy(server);

    clm_net_web_request request = {0};
    clm_http_utils_header request_headers[1] = {{{"Content-Type", 12},
        {"multipart/form-data; boundary=web-boundary", 42}}};
    clm_http_utils_message_init(&request.message, CLM_HTTP_UTILS_REQUEST);
    request.message.headers = request_headers;
    request.message.header_count = 1;
    const char multipart_body[] = "--web-boundary\r\n\r\nvalue\r\n--web-boundary--\r\n";
    request.body = (clm_bytes_view){(const unsigned char*)multipart_body, sizeof(multipart_body) - 1};
    clm_multipart_part parts[1];
    clm_multipart_message parsed;
    CLM_ASSERT(clm_net_web_request_multipart(&request, parts, 1, NULL, 0, &parsed) ==
                   CLM_MULTIPART_SUCCESS && parsed.part_count == 1 &&
                   parts[0].body.length == 5 && memcmp(parts[0].body.data, "value", 5) == 0,
               "parse multipart request bodies through the web request helper");
    request.body = (clm_bytes_view){(const unsigned char*)"abcdef", 6};
    request.body_offset = 0;
    char body_piece[4] = {0};
    size_t body_read = 0;
    CLM_ASSERT(clm_net_web_request_read_body(&request, body_piece, 3, &body_read) ==
                   CLM_NET_WEB_SUCCESS && body_read == 3 && memcmp(body_piece, "abc", 3) == 0 &&
                   clm_net_web_request_read_body(&request, body_piece, sizeof(body_piece),
                       &body_read) == CLM_NET_WEB_SUCCESS && body_read == 3 &&
                   memcmp(body_piece, "def", 3) == 0 &&
                   clm_net_web_request_read_body(&request, body_piece, sizeof(body_piece),
                       &body_read) == CLM_NET_WEB_END && body_read == 0,
               "read a bounded request body incrementally");

    clm_net_web_config live_config;
    clm_net_web_config_init(&live_config);
    live_config.worker_count = 2;
    live_config.max_pending_connections = 4;
    live_config.max_connections = 16;
    live_config.io_timeout_ms = 2000;
    live_config.idle_timeout_ms = 250;
    live_config.read_timeout_ms = 2000;
    live_config.write_timeout_ms = 2000;
    clm_net_web_server* live_server = NULL;
    clm_id client_id = 98901;
    clm_net_address listen_address;
    clm_net_address bound_address;
    clm_id idle_client_ids[8] = {0};
    size_t idle_client_count = 0;
    bool client_open = false;
    bool live_ok = clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_web_server_create(&live_config, &live_server) == CLM_NET_WEB_SUCCESS &&
            clm_net_web_route_add(live_server, "GET", "/stream",
            clm_net_web_test_stream_handler, NULL) == CLM_NET_WEB_SUCCESS &&
            clm_net_web_route_add(live_server, "GET", "/big",
                clm_net_web_test_big_handler, NULL) == CLM_NET_WEB_SUCCESS &&
            clm_net_web_route_add(live_server, "HEAD", "/stream",
                clm_net_web_test_stream_handler, NULL) == CLM_NET_WEB_SUCCESS &&
            clm_net_web_route_add(live_server, "POST", "/upload",
                clm_net_web_test_body_handler, NULL) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add(live_server, "GET", "/hello/:name",
            clm_net_web_test_handler, &user_context) == CLM_NET_WEB_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &listen_address) == CLM_NET_SUCCESS &&
        clm_net_web_server_start(live_server, &listen_address) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_server_local_address(live_server, &bound_address) == CLM_NET_WEB_SUCCESS &&
        clm_net_socket_create(client_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS;
    client_open = live_ok;
    CLM_ASSERT(live_ok, "start the loopback server and create a client socket");
    for (size_t i = 0; live_ok && i < sizeof(idle_client_ids) / sizeof(idle_client_ids[0]); ++i) {
        clm_id idle_id = (clm_id)(98910 + i);
        if (clm_net_socket_create(idle_id, CLM_NET_IPV4, CLM_NET_TCP) != CLM_NET_SUCCESS ||
            clm_net_connect(idle_id, &bound_address) != CLM_NET_SUCCESS) {
            if (clm_net_socket_local_address(idle_id, &bound_address) == CLM_NET_SUCCESS)
                (void)clm_net_socket_close(idle_id);
            live_ok = false;
            break;
        }
        idle_client_ids[idle_client_count++] = idle_id;
    }
    CLM_ASSERT(live_ok && idle_client_count == sizeof(idle_client_ids) / sizeof(idle_client_ids[0]),
               "accept more idle connections than handler workers");
    bool active_connected = live_ok &&
        clm_net_connect(client_id, &bound_address) == CLM_NET_SUCCESS;
    CLM_ASSERT(active_connected, "connect an active client alongside idle connections");
    live_ok = active_connected;
    char live_request[512];
    const char request_prefix[] = "GET /hello/";
    const char request_suffix[] =
        " HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip;q=0.5\r\nConnection: close\r\n\r\n";
    size_t live_request_length = sizeof(request_prefix) - 1 + 300 + sizeof(request_suffix) - 1;
    memcpy(live_request, request_prefix, sizeof(request_prefix) - 1);
    memset(live_request + sizeof(request_prefix) - 1, 'A', 300);
    memcpy(live_request + sizeof(request_prefix) - 1 + 300,
        request_suffix, sizeof(request_suffix) - 1);
    size_t sent = 0;
    if (live_ok)
        live_ok = clm_net_send(client_id, live_request, live_request_length, &sent) ==
            CLM_NET_SUCCESS && sent == live_request_length;
    CLM_ASSERT(live_ok, "send a request while other connections remain idle");
    char live_response[2048] = {0};
    size_t response_length = 0;
    bool complete = false;
    for (size_t attempt = 0; live_ok && attempt < 8 && !complete; ++attempt) {
        unsigned int events = 0;
        if (clm_net_wait(client_id, CLM_NET_EVENT_READ, 2000, &events) != CLM_NET_SUCCESS ||
            !(events & CLM_NET_EVENT_READ)) {
            live_ok = false;
            break;
        }
        size_t received = 0;
        clm_net_status read_status = clm_net_receive(client_id,
            live_response + response_length, sizeof(live_response) - response_length - 1, &received);
        if (read_status == CLM_NET_CLOSED)
            break;
        if (read_status != CLM_NET_SUCCESS || !received) {
            live_ok = false;
            break;
        }
        response_length += received;
        live_response[response_length] = '\0';
        complete = clm_net_web_test_response_complete(live_response, response_length);
    }
    char* live_body = strstr(live_response, "\r\n\r\n");
    bool response_received = live_ok && complete && live_body != NULL;
    bool response_headers_ok = response_received &&
        strstr(live_response, "HTTP/1.1 200 OK\r\n") &&
        strstr(live_response, "Content-Encoding: gzip\r\n");
    CLM_ASSERT(response_received, "receive a complete response from the active connection");
    CLM_ASSERT(response_headers_ok, "route the request and negotiate gzip response headers");
    clm_bytes_buffer decoded_response = {0};
    bool gzip_response = response_headers_ok &&
        clm_bytes_buffer_init(&decoded_response, 0) == CLM_BYTES_SUCCESS &&
        clm_compress_decode(CLM_COMPRESS_GZIP,
            (clm_bytes_view){(const unsigned char*)(live_body + 4),
                response_length - (size_t)(live_body + 4 - live_response)},
            NULL, &decoded_response) == CLM_COMPRESS_SUCCESS &&
        decoded_response.length == 300;
    if (gzip_response)
        for (size_t i = 0; i < decoded_response.length; ++i)
            gzip_response = gzip_response && decoded_response.storage[decoded_response.offset + i] == 'A';
    clm_bytes_buffer_dispose(&decoded_response);
    CLM_ASSERT(gzip_response, "decode the streamed gzip response body");
    live_ok = live_ok && gzip_response;

    const clm_id stream_client_id = 98902;
    bool stream_ok = live_ok &&
        clm_net_socket_create(stream_client_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_connect(stream_client_id, &bound_address) == CLM_NET_SUCCESS;
    static const char stream_request[] =
        "GET /stream HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    sent = 0;
    if (stream_ok)
        stream_ok = clm_net_send(stream_client_id, stream_request,
            sizeof(stream_request) - 1, &sent) == CLM_NET_SUCCESS &&
            sent == sizeof(stream_request) - 1;
    char stream_response[2048] = {0};
    size_t stream_length = 0;
    for (size_t attempt = 0; stream_ok && attempt < 8; ++attempt) {
        unsigned int events = 0;
        if (clm_net_wait(stream_client_id, CLM_NET_EVENT_READ, 2000, &events) != CLM_NET_SUCCESS ||
            !(events & CLM_NET_EVENT_READ)) {
            stream_ok = false;
            break;
        }
        size_t received = 0;
        clm_net_status read_status = clm_net_receive(stream_client_id,
            stream_response + stream_length, sizeof(stream_response) - stream_length - 1, &received);
        if (read_status == CLM_NET_CLOSED)
            break;
        if (read_status != CLM_NET_SUCCESS || !received) {
            stream_ok = false;
            break;
        }
        stream_length += received;
        stream_response[stream_length] = '\0';
        if (strstr(stream_response, "0\r\n\r\n"))
            break;
    }
    stream_ok = stream_ok && strstr(stream_response, "HTTP/1.1 200 OK\r\n") &&
        strstr(stream_response, "Transfer-Encoding: chunked\r\n") &&
        strstr(stream_response, "5\r\nfirst\r\n6\r\nsecond\r\n0\r\n\r\n") != NULL;
    (void)clm_net_socket_close(stream_client_id);
    CLM_ASSERT(stream_ok,
               "flush and stream multiple chunked response writes");

    const clm_id body_client_id = 98903;
    bool body_stream_ok = live_ok &&
        clm_net_socket_create(body_client_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_connect(body_client_id, &bound_address) == CLM_NET_SUCCESS;
    static const char body_request_headers[] =
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\nConnection: close\r\n\r\n";
    sent = 0;
    if (body_stream_ok)
        body_stream_ok = clm_net_send(body_client_id, body_request_headers,
            sizeof(body_request_headers) - 1, &sent) == CLM_NET_SUCCESS &&
            sent == sizeof(body_request_headers) - 1;
    if (body_stream_ok)
        body_stream_ok = clm_net_send(body_client_id, "abcdef", 6, &sent) ==
            CLM_NET_SUCCESS && sent == 6;
    char body_response[1024] = {0};
    size_t body_response_length = 0;
    bool body_response_complete = false;
    for (size_t attempt = 0; body_stream_ok && attempt < 8 && !body_response_complete; ++attempt) {
        unsigned int events = 0;
        if (clm_net_wait(body_client_id, CLM_NET_EVENT_READ, 2000, &events) != CLM_NET_SUCCESS ||
            !(events & CLM_NET_EVENT_READ)) {
            body_stream_ok = false;
            break;
        }
        size_t received = 0;
        clm_net_status read_status = clm_net_receive(body_client_id,
            body_response + body_response_length,
            sizeof(body_response) - body_response_length - 1, &received);
        if (read_status == CLM_NET_CLOSED)
            break;
        if (read_status != CLM_NET_SUCCESS || !received) {
            body_stream_ok = false;
            break;
        }
        body_response_length += received;
        body_response[body_response_length] = '\0';
        body_response_complete = clm_net_web_test_response_complete(
            body_response, body_response_length);
    }
    char* body_response_data = strstr(body_response, "\r\n\r\n");
    body_stream_ok = body_stream_ok && body_response_complete && body_response_data &&
        strstr(body_response, "HTTP/1.1 200 OK\r\n") &&
        memcmp(body_response_data + 4, "abcdef", 6) == 0;
    (void)clm_net_socket_close(body_client_id);
    CLM_ASSERT(body_stream_ok,
               "read a fixed-length request body incrementally and echo it");
    CLM_ASSERT(gzip_response, "route the request and return a gzip response");

    char edge[2048];
    const clm_id head_id = 98941;
    bool head_ok = live_ok && clm_net_web_test_open(head_id, &bound_address,
        "HEAD /stream HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    if (head_ok) {
        clm_net_web_test_read_all(head_id, edge, sizeof(edge), 2000);
        const char* head_end = strstr(edge, "\r\n\r\n");
        head_ok = strstr(edge, "HTTP/1.1 200 OK\r\n") && strstr(edge, "Content-Length: 11\r\n") &&
            !strstr(edge, "Transfer-Encoding") && head_end && head_end[4] == '\0';
    }
    (void)clm_net_socket_close(head_id);
    CLM_ASSERT(head_ok, "answer HEAD requests with flushing handlers without sending a body");

    const clm_id http10_id = 98942;
    bool http10_ok = live_ok && clm_net_web_test_open(http10_id, &bound_address,
        "GET /stream HTTP/1.0\r\nHost: localhost\r\n\r\n");
    if (http10_ok) {
        clm_net_web_test_read_all(http10_id, edge, sizeof(edge), 2000);
        const char* http10_body = strstr(edge, "\r\n\r\n");
        http10_ok = strstr(edge, " 200 OK\r\n") && strstr(edge, "Connection: close\r\n") &&
            !strstr(edge, "Transfer-Encoding") && http10_body &&
            strcmp(http10_body + 4, "firstsecond") == 0;
    }
    (void)clm_net_socket_close(http10_id);
    CLM_ASSERT(http10_ok, "stream HTTP/1.0 responses without chunked framing and then close");

    const clm_id expect_id = 98943;
    bool expect_ok = live_ok && clm_net_web_test_open(expect_id, &bound_address,
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\n"
        "Expect: 100-continue\r\nConnection: close\r\n\r\n");
    if (expect_ok) {
        size_t received = 0;
        unsigned int events = 0;
        expect_ok = clm_net_wait(expect_id, CLM_NET_EVENT_READ, 2000, &events) ==
                CLM_NET_SUCCESS &&
            clm_net_receive(expect_id, edge, sizeof(edge) - 1, &received) == CLM_NET_SUCCESS &&
            received == 25 && memcmp(edge, "HTTP/1.1 100 Continue\r\n\r\n", 25) == 0 &&
            clm_net_send(expect_id, "abcdef", 6, &received) == CLM_NET_SUCCESS;
        if (expect_ok) {
            clm_net_web_test_read_all(expect_id, edge, sizeof(edge), 2000);
            expect_ok = strstr(edge, "HTTP/1.1 200 OK\r\n") && strstr(edge, "\r\n\r\nabcdef");
        }
    }
    (void)clm_net_socket_close(expect_id);
    CLM_ASSERT(expect_ok, "send 100 Continue before reading a streamed request body");

    const clm_id expect_chunked_id = 98944;
    bool expect_chunked_ok = live_ok && clm_net_web_test_open(expect_chunked_id, &bound_address,
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n"
        "Expect: 100-continue\r\nConnection: close\r\n\r\n");
    if (expect_chunked_ok) {
        size_t received = 0;
        unsigned int events = 0;
        expect_chunked_ok = clm_net_wait(expect_chunked_id, CLM_NET_EVENT_READ, 2000, &events) ==
                CLM_NET_SUCCESS &&
            clm_net_receive(expect_chunked_id, edge, sizeof(edge) - 1, &received) ==
                CLM_NET_SUCCESS && received == 25 &&
            memcmp(edge, "HTTP/1.1 100 Continue\r\n\r\n", 25) == 0 &&
            clm_net_send(expect_chunked_id, "6\r\nabcdef\r\n0\r\n\r\n", 16, &received) ==
                CLM_NET_SUCCESS;
        if (expect_chunked_ok) {
            clm_net_web_test_read_all(expect_chunked_id, edge, sizeof(edge), 2000);
            expect_chunked_ok = strstr(edge, "HTTP/1.1 200 OK\r\n") &&
                strstr(edge, "\r\n\r\nabcdef");
        }
    }
    (void)clm_net_socket_close(expect_chunked_id);
    CLM_ASSERT(expect_chunked_ok, "send 100 Continue before buffering a chunked request body");

    const clm_id partial_id = 98945;
    bool partial_ok = live_ok && clm_net_web_test_open(partial_id, &bound_address,
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\n"
        "Connection: close\r\n\r\nabc");
    if (partial_ok) {
        clm_net_web_test_read_all(partial_id, edge, sizeof(edge), 5000);
        partial_ok = strstr(edge, "HTTP/1.1 408 Request Timeout\r\n") != NULL;
    }
    (void)clm_net_socket_close(partial_id);
    CLM_ASSERT(partial_ok, "return 408 when a request body stops before Content-Length bytes");

    const clm_id chunk_id = 98946;
    bool chunk_ok = live_ok && clm_net_web_test_open(chunk_id, &bound_address,
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nab");
    if (chunk_ok) {
        size_t sent_now = 0;
        static const char part2[] = "c\r\n3;ext=1\r\ndef\r\n0\r\nX-Trailer: 1\r\n\r\n";
        static const char next_request[] =
            "GET /hello/after HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        chunk_ok = clm_net_send(chunk_id, part2, sizeof(part2) - 1, &sent_now) ==
                CLM_NET_SUCCESS &&
            clm_net_send(chunk_id, next_request, sizeof(next_request) - 1, &sent_now) ==
                CLM_NET_SUCCESS;
        if (chunk_ok) {
            clm_net_web_test_read_all(chunk_id, edge, sizeof(edge), 2000);
            const char* first = strstr(edge, "\r\n\r\nabcdef");
            const char* second = first ? strstr(first, "HTTP/1.1 200 OK\r\n") : NULL;
            chunk_ok = first && second && strstr(second, "\r\n\r\nafter");
        }
    }
    (void)clm_net_socket_close(chunk_id);
    CLM_ASSERT(chunk_ok,
               "decode a chunked request body incrementally across reads and keep the connection");

    const clm_id bad_chunk_id = 98947;
    bool bad_chunk_ok = live_ok && clm_net_web_test_open(bad_chunk_id, &bound_address,
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\nzz\r\nabc\r\n0\r\n\r\n");
    if (bad_chunk_ok) {
        clm_net_web_test_read_all(bad_chunk_id, edge, sizeof(edge), 2000);
        bad_chunk_ok = strstr(edge, "HTTP/1.1 400 Bad Request\r\n") != NULL;
    }
    (void)clm_net_socket_close(bad_chunk_id);
    CLM_ASSERT(bad_chunk_ok, "reject a malformed chunk size while streaming");

    const clm_id big_chunk_id = 98948;
    bool big_chunk_ok = live_ok && clm_net_web_test_open(big_chunk_id, &bound_address,
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\nFFFFFFFFFFFFFFFFF\r\n");
    if (big_chunk_ok) {
        clm_net_web_test_read_all(big_chunk_id, edge, sizeof(edge), 2000);
        big_chunk_ok = strstr(edge, "HTTP/1.1 413 Content Too Large\r\n") != NULL;
    }
    (void)clm_net_socket_close(big_chunk_id);
    CLM_ASSERT(big_chunk_ok, "fail a streamed chunk that overflows or exceeds the request limit");

    const clm_id slow_a = 98950;
    const clm_id slow_b = 98951;
    const clm_id fast_id = 98952;
    static const char big_request[] =
        "GET /big HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    bool park_ok = live_ok &&
        clm_net_web_test_open(slow_a, &bound_address, big_request) &&
        clm_net_web_test_open(slow_b, &bound_address, big_request);
    if (park_ok) {
        /* Let both workers hit a full socket buffer, then require a third client to be served. */
        thrd_sleep(&(struct timespec){0, 300 * 1000 * 1000}, NULL);
        park_ok = clm_net_web_test_open(fast_id, &bound_address,
            "GET /hello/fast HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
        if (park_ok) {
            clm_net_web_test_read_all(fast_id, edge, sizeof(edge), 1000);
            park_ok = strstr(edge, "HTTP/1.1 200 OK\r\n") && strstr(edge, "\r\n\r\nfast");
        }
    }
    CLM_ASSERT(park_ok, "serve other clients while slow readers hold large responses");
    const size_t big_total = 64 * 65536;
    char* big_buffer = malloc(big_total + 4096);
    bool drained_ok = park_ok && big_buffer;
    for (size_t i = 0; drained_ok && i < 2; ++i) {
        clm_id id = i ? slow_b : slow_a;
        size_t total = clm_net_web_test_read_all(id, big_buffer, big_total + 4096, 2000);
        const char* body = strstr(big_buffer, "\r\n\r\n");
        drained_ok = body && total - (size_t)(body + 4 - big_buffer) == big_total &&
            strstr(big_buffer, "Content-Length: 4194304\r\n") != NULL;
    }
    free(big_buffer);
    (void)clm_net_socket_close(slow_a);
    (void)clm_net_socket_close(slow_b);
    (void)clm_net_socket_close(fast_id);
    CLM_ASSERT(drained_ok, "deliver parked responses completely once the client reads");

    const clm_id up_a = 98960;
    const clm_id up_b = 98961;
    const clm_id up_fast = 98962;
    static const char upload_headers[] =
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\nConnection: close\r\n\r\n";
    bool upload_ok = live_ok &&
        clm_net_web_test_open(up_a, &bound_address, upload_headers) &&
        clm_net_web_test_open(up_b, &bound_address, upload_headers);
    if (upload_ok) {
        thrd_sleep(&(struct timespec){0, 300 * 1000 * 1000}, NULL);
        upload_ok = clm_net_web_test_open(up_fast, &bound_address,
            "GET /hello/fast HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
        if (upload_ok) {
            clm_net_web_test_read_all(up_fast, edge, sizeof(edge), 1000);
            upload_ok = strstr(edge, "HTTP/1.1 200 OK\r\n") && strstr(edge, "\r\n\r\nfast");
        }
    }
    CLM_ASSERT(upload_ok, "serve other clients while slow uploads have not sent their bodies");
    bool upload_done = upload_ok;
    for (size_t i = 0; upload_done && i < 2; ++i) {
        clm_id id = i ? up_b : up_a;
        size_t sent_body = 0;
        upload_done = clm_net_send(id, "abcdef", 6, &sent_body) == CLM_NET_SUCCESS;
        if (upload_done) {
            clm_net_web_test_read_all(id, edge, sizeof(edge), 2000);
            upload_done = strstr(edge, "HTTP/1.1 200 OK\r\n") && strstr(edge, "\r\n\r\nabcdef");
        }
    }
    (void)clm_net_socket_close(up_a);
    (void)clm_net_socket_close(up_b);
    (void)clm_net_socket_close(up_fast);
    CLM_ASSERT(upload_done, "run the handler once a parked upload body arrives");

    const clm_id cap_a = 98970;
    const clm_id cap_b = 98971;
    static const char large_upload_headers[] =
        "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 200000\r\nConnection: close\r\n\r\nabc";
    bool cap_ok = live_ok && clm_net_web_test_open(cap_a, &bound_address, large_upload_headers);
    if (cap_ok) {
        /* cap_a now streams its body and holds the only streaming slot (2 workers -> cap 1). */
        thrd_sleep(&(struct timespec){0, 300 * 1000 * 1000}, NULL);
        cap_ok = clm_net_web_test_open(cap_b, &bound_address, large_upload_headers);
        if (cap_ok) {
            clm_net_web_test_read_all(cap_b, edge, sizeof(edge), 2000);
            cap_ok = strstr(edge, "HTTP/1.1 503 Service Unavailable\r\n") != NULL;
        }
    }
    (void)clm_net_socket_close(cap_a);
    (void)clm_net_socket_close(cap_b);
    CLM_ASSERT(cap_ok, "reject a streamed upload with 503 once the streaming cap is reached");
    thrd_sleep(&(struct timespec){0, 300 * 1000 * 1000}, NULL);

    const clm_id slow_client_id = 98931;
    bool slow_client_created = live_ok &&
        clm_net_socket_create(slow_client_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS;
    bool read_timeout_ok = slow_client_created &&
        clm_net_connect(slow_client_id, &bound_address) == CLM_NET_SUCCESS;
    static const char partial_request[] = "GET /hello HTTP/1.1\r\nHost:";
    sent = 0;
    if (read_timeout_ok)
        read_timeout_ok = clm_net_send(slow_client_id, partial_request,
            sizeof(partial_request) - 1, &sent) == CLM_NET_SUCCESS &&
            sent == sizeof(partial_request) - 1;
    char timeout_response[512] = {0};
    if (read_timeout_ok) {
        unsigned int events = 0;
        size_t received = 0;
        read_timeout_ok = clm_net_wait(slow_client_id, CLM_NET_EVENT_READ, 3000, &events) ==
                CLM_NET_SUCCESS && (events & CLM_NET_EVENT_READ) != 0 &&
            clm_net_receive(slow_client_id, timeout_response, sizeof(timeout_response) - 1,
                &received) == CLM_NET_SUCCESS && received != 0;
        timeout_response[received] = '\0';
        read_timeout_ok = read_timeout_ok &&
            strstr(timeout_response, "HTTP/1.1 408 Request Timeout\r\n") != NULL;
    }
    if (slow_client_created)
        (void)clm_net_socket_close(slow_client_id);
    CLM_ASSERT(read_timeout_ok, "return 408 when a partial request exceeds the read timeout");

    clm_id idle_timeout_client = 98930;
    bool idle_timeout_ok = live_ok &&
        clm_net_socket_create(idle_timeout_client, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_connect(idle_timeout_client, &bound_address) == CLM_NET_SUCCESS;
    if (idle_timeout_ok) {
        unsigned int events = 0;
        size_t received = 0;
        unsigned char byte = 0;
        idle_timeout_ok = clm_net_wait(idle_timeout_client, CLM_NET_EVENT_READ, 3000, &events) ==
                CLM_NET_SUCCESS && (events & CLM_NET_EVENT_READ) != 0 &&
            clm_net_receive(idle_timeout_client, &byte, 1, &received) == CLM_NET_CLOSED;
        (void)clm_net_socket_close(idle_timeout_client);
    }
    CLM_ASSERT(idle_timeout_ok, "close an idle connection at its configured deadline");
    live_ok = live_ok && idle_timeout_ok;
    if (live_server)
        live_ok = clm_net_web_server_stop(live_server) == CLM_NET_WEB_SUCCESS && live_ok;
    if (client_open)
        (void)clm_net_socket_close(client_id);
    for (size_t i = 0; i < idle_client_count; ++i)
        (void)clm_net_socket_close(idle_client_ids[i]);
    clm_net_web_server_destroy(live_server);
    (void)clm_net_finalize();
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}

static uint64_t clm_net_web_test_now_ms(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

/* The reactor must not depend on the number of parked connections: it has to
 * register new parks promptly (via its wake-up), serve traffic beside a large
 * parked population, and expire every parked connection through the deadline heap. */
static int clm_net_web_reactor_scale_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Net Web reactor");

    enum { PARKED = 120 }; /* More than the 64 sockets select() can watch. */
    clm_net_web_config config;
    clm_net_web_config_init(&config);
    config.worker_count = 2;
    config.max_connections = 256;
    config.max_pending_connections = 256;
    config.idle_timeout_ms = 1200;
    config.read_timeout_ms = 3000;
    config.write_timeout_ms = 3000;
    config.backlog = 256;
    clm_net_web_server* server = NULL;
    clm_net_address listen_address;
    clm_net_address bound_address;
    int user_context = 1;
    bool ok = clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_web_server_create(&config, &server) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add(server, "GET", "/hello/:name", clm_net_web_test_handler,
            &user_context) == CLM_NET_WEB_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &listen_address) == CLM_NET_SUCCESS &&
        clm_net_web_server_start(server, &listen_address) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_server_local_address(server, &bound_address) == CLM_NET_WEB_SUCCESS;
    CLM_ASSERT(ok, "start a server for the reactor scale test");

    const clm_id base_id = 99100;
    size_t opened = 0;
    for (size_t i = 0; ok && i < PARKED; ++i) {
        clm_id id = base_id + (clm_id)i;
        if (clm_net_socket_create(id, CLM_NET_IPV4, CLM_NET_TCP) != CLM_NET_SUCCESS)
            break;
        ++opened;
        if (clm_net_connect(id, &bound_address) != CLM_NET_SUCCESS) {
            ok = false;
            break;
        }
    }
    ok = ok && opened == PARKED;
    CLM_ASSERT(ok, "hold more idle connections than select() can watch");

    /* Connecting and sending at once means the worker parks this connection while the
     * reactor sleeps in a long poller wait; only the wake-up lets it notice promptly. */
    char response[1024];
    const clm_id active_id = 99090;
    uint64_t started = clm_net_web_test_now_ms();
    bool active_ok = ok && clm_net_web_test_open(active_id, &bound_address,
        "GET /hello/scale HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    size_t length = active_ok ?
        clm_net_web_test_read_all(active_id, response, sizeof(response), 2000) : 0;
    uint64_t elapsed = clm_net_web_test_now_ms() - started;
    CLM_ASSERT(active_ok && strstr(response, "HTTP/1.1 200 OK") && length > 0 &&
            strstr(response, "scale"),
        "serve a request beside a large parked population");
    CLM_ASSERT(active_ok && elapsed < 600,
        "register a freshly parked connection without waiting for the poll timeout");
    (void)clm_net_socket_close(active_id);

    /* Every parked connection shares roughly one deadline, so this drives the heap
     * through a large batch of expirations in one reactor pass. */
    size_t closed = 0;
    for (size_t i = 0; ok && i < PARKED; ++i) {
        clm_id id = base_id + (clm_id)i;
        unsigned int events = 0;
        size_t received = 0;
        char byte;
        if (clm_net_wait(id, CLM_NET_EVENT_READ, 4000, &events) == CLM_NET_SUCCESS &&
            clm_net_receive(id, &byte, 1, &received) == CLM_NET_CLOSED)
            ++closed;
    }
    CLM_ASSERT(ok && closed == PARKED,
        "expire every parked connection at its idle deadline");

    /* Reaping is driven by finish notifications; the server must settle and still
     * accept new work after a whole population disappeared. */
    bool again = ok && clm_net_web_test_open(active_id, &bound_address,
        "GET /hello/after HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    length = again ? clm_net_web_test_read_all(active_id, response, sizeof(response), 2000) : 0;
    CLM_ASSERT(again && strstr(response, "after"),
        "keep serving after all parked connections were reaped");
    (void)clm_net_socket_close(active_id);

    for (size_t i = 0; i < opened; ++i)
        (void)clm_net_socket_close(base_id + (clm_id)i);
    bool stopped = server && clm_net_web_server_stop(server) == CLM_NET_WEB_SUCCESS;
    CLM_ASSERT(stopped, "stop the server with the reactor idle");
    clm_net_web_server_destroy(server);
    (void)clm_net_finalize();
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}

static bool clm_net_web_test_send_all(clm_id id, const void* data, size_t length)
{
    const char* bytes = data;
    size_t offset = 0;
    while (offset < length) {
        size_t sent = 0;
        clm_net_status status = clm_net_send(id, bytes + offset, length - offset, &sent);
        if (status == CLM_NET_SUCCESS && sent) {
            offset += sent;
            continue;
        }
        unsigned int events = 0;
        if (status != CLM_NET_WOULD_BLOCK ||
            clm_net_wait(id, CLM_NET_EVENT_WRITE, 2000, &events) != CLM_NET_SUCCESS)
            return false;
    }
    return true;
}

/* Request-local counter kept in the context, because an async handler's locals do
 * not survive returning CLM_NET_WEB_AGAIN. */
static size_t* clm_net_web_test_async_counter(clm_net_web_request* request)
{
    size_t* counter = clm_net_web_context_get(request->context, "count");
    if (counter)
        return counter;
    counter = calloc(1, sizeof(*counter));
    if (counter && clm_net_web_context_set(request->context, "count", counter, free) !=
            CLM_NET_WEB_SUCCESS) {
        free(counter);
        counter = NULL;
    }
    return counter;
}

/* Echoes the request body as a streamed response, never waiting on the peer. */
static clm_int clm_net_web_test_async_echo(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)context;
    size_t* total = clm_net_web_test_async_counter(request);
    if (!total)
        return CLM_NET_WEB_HANDLER_ERROR;
    char buffer[512];
    for (;;) {
        clm_int status = clm_net_web_response_flush(response);
        if (status == CLM_NET_WEB_AGAIN)
            return CLM_NET_WEB_AGAIN;
        if (status != CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_HANDLER_ERROR;
        size_t received = 0;
        status = clm_net_web_request_read_body(request, buffer, sizeof(buffer), &received);
        if (status == CLM_NET_WEB_AGAIN)
            return CLM_NET_WEB_AGAIN;
        if (status == CLM_NET_WEB_END)
            return CLM_NET_WEB_SUCCESS;
        if (status != CLM_NET_WEB_SUCCESS ||
            clm_net_web_response_write(response, buffer, received) != CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_HANDLER_ERROR;
        *total += received;
    }
}

/* Reads the whole body and replies with a buffered (non-streamed) response. */
static clm_int clm_net_web_test_async_sink(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)context;
    size_t* total = clm_net_web_test_async_counter(request);
    if (!total)
        return CLM_NET_WEB_HANDLER_ERROR;
    char buffer[1024];
    for (;;) {
        size_t received = 0;
        clm_int status = clm_net_web_request_read_body(request, buffer, sizeof(buffer), &received);
        if (status == CLM_NET_WEB_AGAIN)
            return CLM_NET_WEB_AGAIN;
        if (status == CLM_NET_WEB_END)
            break;
        if (status != CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_HANDLER_ERROR;
        *total += received;
    }
    char text[64];
    int length = snprintf(text, sizeof(text), "got %zu bytes", *total);
    return clm_net_web_response_write(response, text, (size_t)length);
}

/* Produces 4 MiB, flushing one block at a time; stalls when the peer stops reading. */
static clm_int clm_net_web_test_async_flood(
    clm_net_web_request* request, clm_net_web_response* response, void* context)
{
    (void)context;
    size_t* blocks = clm_net_web_test_async_counter(request);
    if (!blocks)
        return CLM_NET_WEB_HANDLER_ERROR;
    static const unsigned char block[65536] = {1};
    while (*blocks < 64) {
        clm_int status = clm_net_web_response_flush(response);
        if (status == CLM_NET_WEB_AGAIN)
            return CLM_NET_WEB_AGAIN;
        if (status != CLM_NET_WEB_SUCCESS ||
            clm_net_web_response_write(response, block, sizeof(block)) != CLM_NET_WEB_SUCCESS)
            return CLM_NET_WEB_HANDLER_ERROR;
        ++*blocks;
    }
    return CLM_NET_WEB_SUCCESS;
}

/* The whole point of async routes: with ONE worker, a stalled upload, a stalled
 * download and a stalled chunked decode must not stop other requests being served. */
static int clm_net_web_async_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Net Web async routes");

    clm_net_web_config config;
    clm_net_web_config_init(&config);
    config.worker_count = 1;
    config.read_timeout_ms = 1500;
    config.write_timeout_ms = 8000;
    config.idle_timeout_ms = 4000;
    clm_net_web_server* server = NULL;
    clm_net_address listen_address;
    clm_net_address bound_address;
    int user_context = 1;
    bool ok = clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_web_server_create(&config, &server) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add(server, "GET", "/hello/:name", clm_net_web_test_handler,
            &user_context) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add_async(server, "POST", "/echo", clm_net_web_test_async_echo,
            NULL) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add_async(server, "POST", "/sink", clm_net_web_test_async_sink,
            NULL) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add_async(server, "GET", "/flood", clm_net_web_test_async_flood,
            NULL) == CLM_NET_WEB_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &listen_address) == CLM_NET_SUCCESS &&
        clm_net_web_server_start(server, &listen_address) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_server_local_address(server, &bound_address) == CLM_NET_WEB_SUCCESS;
    CLM_ASSERT(ok, "start a single-worker server with async routes");

    enum { BODY = 150000, OTHER_ID = 99300 };
    char* body = malloc(BODY);
    char* response = malloc(BODY * 2);
    char tiny[512];
    ok = ok && body && response;
    if (body)
        memset(body, 'a', BODY);

    /* 1. Stalled upload: headers + part of a body larger than max_buffered_body_bytes. */
    const clm_id upload_id = 99301;
    bool upload_open = ok && clm_net_web_test_open(upload_id, &bound_address,
        "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 150000\r\nConnection: close\r\n\r\n") &&
        clm_net_web_test_send_all(upload_id, body, 80000);
    CLM_ASSERT(upload_open, "start an async upload and stall it halfway");
    uint64_t started = clm_net_web_test_now_ms();
    bool other = upload_open && clm_net_web_test_open(OTHER_ID, &bound_address,
        "GET /hello/other HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    size_t length = other ? clm_net_web_test_read_all(OTHER_ID, tiny, sizeof(tiny), 1000) : 0;
    (void)clm_net_socket_close(OTHER_ID);
    CLM_ASSERT(other && length && strstr(tiny, "200 OK") && strstr(tiny, "other") &&
            clm_net_web_test_now_ms() - started < 1000,
        "serve another request on the only worker while an upload is stalled");
    bool rest = upload_open && clm_net_web_test_send_all(upload_id, body, BODY - 80000);
    length = rest ? clm_net_web_test_read_all(upload_id, response, BODY * 2, 3000) : 0;
    CLM_ASSERT(rest && strstr(response, "200 OK") && length > BODY &&
            strstr(response + length - 5, "0\r\n\r\n"),
        "resume the upload and stream the whole echoed body");
    (void)clm_net_socket_close(upload_id);

    /* 2. A chunked upload stalled in the middle of a chunk-size line. */
    const clm_id chunked_id = 99302;
    static const char chunk_head[] =
        "POST /echo HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n10100\r\n";
    bool chunked_open = ok && clm_net_web_test_open(chunked_id, &bound_address, chunk_head) &&
        clm_net_web_test_send_all(chunked_id, body, 0x10100) &&
        clm_net_web_test_send_all(chunked_id, "\r\n5", 3);
    other = chunked_open && clm_net_web_test_open(OTHER_ID, &bound_address,
        "GET /hello/again HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    length = other ? clm_net_web_test_read_all(OTHER_ID, tiny, sizeof(tiny), 1000) : 0;
    (void)clm_net_socket_close(OTHER_ID);
    CLM_ASSERT(other && length && strstr(tiny, "again"),
        "serve another request while a chunked decode waits mid-line");
    bool chunk_rest = chunked_open &&
        clm_net_web_test_send_all(chunked_id, "\r\nhello\r\n0\r\n\r\n", 15);
    length = chunk_rest ? clm_net_web_test_read_all(chunked_id, response, BODY * 2, 3000) : 0;
    CLM_ASSERT(chunk_rest && strstr(response, "200 OK") && length > 0x10100 + 5 &&
            strstr(response + length - 5, "0\r\n\r\n") &&
            strstr(response + length - 20, "hello"),
        "finish a chunk-size line split across reads and decode the rest");
    (void)clm_net_socket_close(chunked_id);

    /* 3. Stalled download: the client reads nothing, so the socket buffers fill. */
    const clm_id flood_id = 99303;
    bool flood_open = ok && clm_net_web_test_open(flood_id, &bound_address,
        "GET /flood HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    clm_net_wait(flood_id, CLM_NET_EVENT_READ, 300, &(unsigned int){0});
    other = flood_open && clm_net_web_test_open(OTHER_ID, &bound_address,
        "GET /hello/third HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    length = other ? clm_net_web_test_read_all(OTHER_ID, tiny, sizeof(tiny), 1000) : 0;
    (void)clm_net_socket_close(OTHER_ID);
    CLM_ASSERT(other && length && strstr(tiny, "third"),
        "serve another request while an async download is blocked on a slow reader");
    size_t downloaded = 0;
    char* sink = malloc(65536);
    for (;flood_open && sink;) {
        unsigned int events = 0;
        size_t received = 0;
        if (clm_net_wait(flood_id, CLM_NET_EVENT_READ, 3000, &events) != CLM_NET_SUCCESS ||
            clm_net_receive(flood_id, sink, 65536, &received) != CLM_NET_SUCCESS || !received)
            break;
        downloaded += received;
    }
    free(sink);
    CLM_ASSERT(downloaded > 64u * 65536u && downloaded < 64u * 65536u + 4096,
        "deliver the entire flushed download once the reader resumes");
    (void)clm_net_socket_close(flood_id);

    /* 4. A stalled read ends in 408 instead of holding anything. */
    const clm_id stall_id = 99304;
    bool stall_open = ok && clm_net_web_test_open(stall_id, &bound_address,
        "POST /sink HTTP/1.1\r\nHost: x\r\nContent-Length: 150000\r\nConnection: close\r\n\r\n") &&
        clm_net_web_test_send_all(stall_id, body, 80000);
    length = stall_open ? clm_net_web_test_read_all(stall_id, response, BODY, 4000) : 0;
    CLM_ASSERT(stall_open && strstr(response, "408"),
        "time out a stalled async body read with 408");
    (void)clm_net_socket_close(stall_id);

    /* 5. A complete small body runs the async handler in one pass. */
    const clm_id sink_id = 99305;
    bool sink_open = ok && clm_net_web_test_open(sink_id, &bound_address,
        "POST /sink HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello");
    length = sink_open ? clm_net_web_test_read_all(sink_id, tiny, sizeof(tiny), 2000) : 0;
    CLM_ASSERT(sink_open && strstr(tiny, "got 5 bytes"),
        "serve a small body through an async route");
    (void)clm_net_socket_close(sink_id);

    /* 6. Stopping with a handler still suspended must not hang or leak it. */
    const clm_id parked_id = 99306;
    bool parked_open = ok && clm_net_web_test_open(parked_id, &bound_address,
        "POST /sink HTTP/1.1\r\nHost: x\r\nContent-Length: 150000\r\nConnection: close\r\n\r\n") &&
        clm_net_web_test_send_all(parked_id, body, 80000);
    clm_net_wait(parked_id, CLM_NET_EVENT_READ, 200, &(unsigned int){0});
    uint64_t stop_started = clm_net_web_test_now_ms();
    bool stopped = server && clm_net_web_server_stop(server) == CLM_NET_WEB_SUCCESS;
    CLM_ASSERT(parked_open && stopped && clm_net_web_test_now_ms() - stop_started < 3000,
        "stop promptly while an async handler is suspended");
    (void)clm_net_socket_close(parked_id);
    free(body);
    free(response);
    clm_net_web_server_destroy(server);
    (void)clm_net_finalize();
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}