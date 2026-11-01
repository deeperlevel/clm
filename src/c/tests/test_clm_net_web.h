#pragma once

#include "test.h"
#include "../clm/net_web.h"

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
    clm_string_httpp_header request_headers[1] = {{{"Content-Type", 12},
        {"multipart/form-data; boundary=web-boundary", 42}}};
    clm_string_httpp_message_init(&request.message, CLM_STRING_HTTPP_REQUEST);
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

    clm_net_web_config live_config;
    clm_net_web_config_init(&live_config);
    live_config.worker_count = 2;
    live_config.max_pending_connections = 4;
    live_config.io_timeout_ms = 2000;
    clm_net_web_server* live_server = NULL;
    clm_id client_id = 98901;
    clm_net_address listen_address;
    clm_net_address bound_address;
    bool client_open = false;
    bool live_ok = clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_web_server_create(&live_config, &live_server) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_route_add(live_server, "GET", "/hello/:name",
            clm_net_web_test_handler, &user_context) == CLM_NET_WEB_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &listen_address) == CLM_NET_SUCCESS &&
        clm_net_web_server_start(live_server, &listen_address) == CLM_NET_WEB_SUCCESS &&
        clm_net_web_server_local_address(live_server, &bound_address) == CLM_NET_WEB_SUCCESS &&
        clm_net_socket_create(client_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS;
    client_open = live_ok;
    if (live_ok)
        live_ok = clm_net_connect(client_id, &bound_address) == CLM_NET_SUCCESS;
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
        char* body_start = strstr(live_response, "\r\n\r\n");
        complete = body_start && response_length >= (size_t)(body_start + 4 - live_response) + 3;
    }
    char* live_body = strstr(live_response, "\r\n\r\n");
    clm_bytes_buffer decoded_response = {0};
    bool gzip_response = live_ok && complete && live_body &&
        strstr(live_response, "HTTP/1.1 200 OK\r\n") &&
        strstr(live_response, "Content-Encoding: gzip\r\n") &&
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
    live_ok = live_ok && gzip_response;
    if (live_server)
        live_ok = clm_net_web_server_stop(live_server) == CLM_NET_WEB_SUCCESS && live_ok;
    if (client_open)
        (void)clm_net_socket_close(client_id);
    clm_net_web_server_destroy(live_server);
    (void)clm_net_finalize();
    CLM_ASSERT(live_ok,
               "serve a routed loopback request asynchronously with negotiated gzip output");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}