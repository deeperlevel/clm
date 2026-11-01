#pragma once

#include "test.h"
#include "../clm/string_httpp.h"

#define CLM_HTTPP_TEST_PARSE_REQUEST(text, message, headers) \
    clm_string_httpp_parse_request((text), strlen(text), (headers), 8, &(message))

static int clm_string_httpp_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String HTTPP");
    clm_string_httpp_header headers[8];
    clm_string_httpp_message message;
    clm_string_httpp_span value;

    const char get[] =
        "GET /index.html?q=1 HTTP/1.1\r\n"
        "Host:  example.com \r\n"
        "Accept: text/html\r\n"
        "\r\n";
    CLM_ASSERT(CLM_HTTPP_TEST_PARSE_REQUEST(get, message, headers) == CLM_STRING_HTTPP_SUCCESS &&
                   message.kind == CLM_STRING_HTTPP_REQUEST &&
                   clm_string_httpp_span_equals(message.method, "GET") &&
                   clm_string_httpp_span_equals(message.target, "/index.html?q=1") &&
                   message.version_major == 1 && message.version_minor == 1 &&
                   message.header_count == 2 && message.body_kind == CLM_STRING_HTTPP_BODY_NONE &&
                   message.consumed == sizeof(get) - 1,
               "parse a request line, headers, and an empty body");
    CLM_ASSERT(clm_string_httpp_find_header(&message, "host", &value) &&
                   clm_string_httpp_span_equals(value, "example.com") &&
                   clm_string_httpp_span_equals(message.headers[0].name, "Host") &&
                   !clm_string_httpp_find_header(&message, "Missing", &value),
               "trim optional whitespace and find headers ignoring case");

    const char pipelined[] =
        "\r\nPOST /submit HTTP/1.0\r\nContent-Length: 5\r\nX-A: 1\r\nX-A: 2\r\n\r\nhelloGET / HTTP/1.1\r\nHost: a\r\n\r\n";
    CLM_ASSERT(CLM_HTTPP_TEST_PARSE_REQUEST(pipelined, message, headers) == CLM_STRING_HTTPP_SUCCESS &&
                   message.version_minor == 0 && message.body_kind == CLM_STRING_HTTPP_BODY_LENGTH &&
                   clm_string_httpp_span_equals(message.body, "hello") &&
                   message.header_count == 3 && message.consumed == strstr(pipelined, "GET") - pipelined &&
                   clm_string_httpp_parse_request(pipelined + message.consumed,
                       strlen(pipelined) - message.consumed, headers, 8, &message) == CLM_STRING_HTTPP_SUCCESS &&
                   clm_string_httpp_span_equals(message.method, "GET"),
               "skip a leading empty line, frame by Content-Length, and parse pipelined messages");

    CLM_ASSERT(clm_string_httpp_parse_request(get, 10, headers, 8, &message) == CLM_STRING_HTTPP_INCOMPLETE &&
                   clm_string_httpp_parse_request(get, sizeof(get) - 2, headers, 8, &message) ==
                       CLM_STRING_HTTPP_INCOMPLETE &&
                   clm_string_httpp_parse_request(pipelined, strstr(pipelined, "hello") - pipelined + 3,
                       headers, 8, &message) == CLM_STRING_HTTPP_INCOMPLETE &&
                   clm_string_httpp_parse_request("", 0, headers, 8, &message) == CLM_STRING_HTTPP_INCOMPLETE,
               "report incomplete headers and bodies");

    const char chunked[] =
        "POST /c HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: Chunked\r\n\r\n"
        "4;ext=1\r\nWiki\r\n6\r\npedia!\r\n0\r\nX-Sum: 1\r\n\r\n";
    char decoded[32];
    size_t decoded_length = 0, chunk_consumed = 0;
    CLM_ASSERT(CLM_HTTPP_TEST_PARSE_REQUEST(chunked, message, headers) == CLM_STRING_HTTPP_SUCCESS &&
                   message.body_kind == CLM_STRING_HTTPP_BODY_CHUNKED &&
                   message.consumed == sizeof(chunked) - 1 &&
                   clm_string_httpp_decode_chunked(message.body.data, message.body.length, decoded,
                       sizeof(decoded), &decoded_length, &chunk_consumed) == CLM_STRING_HTTPP_SUCCESS &&
                   decoded_length == 10 && memcmp(decoded, "Wikipedia!", 10) == 0 &&
                   chunk_consumed == message.body.length,
               "frame chunked bodies and decode extensions and trailers");
    CLM_ASSERT(clm_string_httpp_decode_chunked(message.body.data, message.body.length, NULL, 0,
                       &decoded_length, NULL) == CLM_STRING_HTTPP_SUCCESS && decoded_length == 10 &&
                   clm_string_httpp_decode_chunked(message.body.data, message.body.length, decoded, 4,
                       &decoded_length, NULL) == CLM_STRING_HTTPP_BUFFER_TOO_SMALL && decoded_length == 10 &&
                   clm_string_httpp_decode_chunked("4\r\nWi", 5, NULL, 0, NULL, NULL) ==
                       CLM_STRING_HTTPP_INCOMPLETE &&
                   clm_string_httpp_decode_chunked("zz\r\n", 4, NULL, 0, NULL, NULL) == CLM_STRING_HTTPP_FAILURE &&
                   clm_string_httpp_decode_chunked("1\r\nabc\r\n0\r\n\r\n", 13, NULL, 0, NULL, NULL) ==
                       CLM_STRING_HTTPP_FAILURE,
               "measure chunked output and reject truncated and malformed chunks");

    const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: text/plain\r\n\r\nokEXTRA";
    CLM_ASSERT(clm_string_httpp_parse_response(response, strlen(response), false, headers, 8, &message) ==
                   CLM_STRING_HTTPP_SUCCESS && message.kind == CLM_STRING_HTTPP_RESPONSE &&
                   message.status == 200 && clm_string_httpp_span_equals(message.reason, "OK") &&
                   clm_string_httpp_span_equals(message.body, "ok") &&
                   message.consumed == strlen(response) - 5,
               "parse a response with Content-Length framing");
    const char no_reason[] = "HTTP/1.1 404 \r\n\r\nbody";
    const char bare[] = "HTTP/1.1 404\r\n\r\n";
    const char head[] = "HTTP/1.1 200 OK\r\nContent-Length: 99\r\n\r\n";
    const char no_content[] = "HTTP/1.1 204 No Content\r\n\r\n";
    CLM_ASSERT(clm_string_httpp_parse_response_ex(no_reason, strlen(no_reason), false, false, true, headers, 8,
                       &message) == CLM_STRING_HTTPP_SUCCESS && message.status == 404 && message.reason.length == 0 &&
                   message.body_kind == CLM_STRING_HTTPP_BODY_UNTIL_CLOSE &&
                   clm_string_httpp_span_equals(message.body, "body") &&
                   clm_string_httpp_parse_response_ex(bare, strlen(bare), false, false, true, headers, 8, &message) ==
                       CLM_STRING_HTTPP_SUCCESS && message.reason.length == 0 && message.body.length == 0 &&
                   clm_string_httpp_parse_response(no_reason, strlen(no_reason), false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_INCOMPLETE &&
                   clm_string_httpp_parse_response(head, strlen(head), true, headers, 8, &message) ==
                       CLM_STRING_HTTPP_SUCCESS && message.body_kind == CLM_STRING_HTTPP_BODY_NONE &&
                   clm_string_httpp_parse_response(head, strlen(head), false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_INCOMPLETE &&
                   clm_string_httpp_parse_response_ex(head, strlen(head), false, false, true, headers, 8, &message) ==
                       CLM_STRING_HTTPP_FAILURE &&
                   clm_string_httpp_parse_response(no_content, strlen(no_content), false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_SUCCESS && message.body_kind == CLM_STRING_HTTPP_BODY_NONE,
               "handle empty reasons, until-close bodies only after EOF, HEAD responses, and 204");
    const char tunnel[] = "HTTP/1.1 200 Connection established\r\nContent-Length: 5\r\n\r\nabc";
    CLM_ASSERT(clm_string_httpp_parse_response_ex(tunnel, strlen(tunnel), false, true, false, headers, 8, &message) ==
                   CLM_STRING_HTTPP_SUCCESS && message.body_kind == CLM_STRING_HTTPP_BODY_NONE &&
                   message.consumed == strstr(tunnel, "abc") - tunnel &&
                   clm_string_httpp_parse_response(tunnel, strlen(tunnel), false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_INCOMPLETE,
               "end a successful CONNECT response at the header section");

    const char* bad[] = {
        "GET / HTTP/1.1\nHost: a\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: a\n\r\n",
        "GET / HTTP/1.1\r\nHost : a\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: a\r\n b\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: a\x01\r\n\r\n",
        "GET / HTTP/1.1\r\nNoColon\r\n\r\n",
        "GET / HTTP/2.0\r\n\r\n",
        "GET / HTTP/1.1 \r\n\r\n",
        "G(T / HTTP/1.1\r\n\r\n",
        "GET  HTTP/1.1\r\n\r\n",
        "GET /\r\n\r\n",
        "POST / HTTP/1.1\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n",
        "POST / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nab",
        "POST / HTTP/1.1\r\nContent-Length: 1, 1\r\n\r\na",
        "POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n",
        "POST / HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n",
        "GET / HTTP/1.1\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: a\r\nHost: a\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\na",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: gzip, chunked\r\n\r\n0\r\n\r\n",
        "POST / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 1\r\n\r\n",
    };
    int rejected = 0;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
        rejected += clm_string_httpp_parse_request(bad[i], strlen(bad[i]), headers, 8, &message) ==
            CLM_STRING_HTTPP_FAILURE;
    CLM_ASSERT(rejected == (int)(sizeof(bad) / sizeof(bad[0])),
               "reject bare LF, folding, spaced names, controls, bad lines, and conflicting framing");
    CLM_ASSERT(clm_string_httpp_parse_response("HTTP/1.1 99 X\r\n\r\n", 17, false, headers, 8, &message) ==
                   CLM_STRING_HTTPP_FAILURE &&
                   clm_string_httpp_parse_response("HTTP/1.1 2000 X\r\n\r\n", 19, false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_FAILURE &&
                   clm_string_httpp_parse_response("HTTP/1.1 200X\r\n\r\n", 17, false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_FAILURE,
               "reject malformed status lines");
    CLM_ASSERT(clm_string_httpp_parse_request(get, strlen(get), headers, 1, &message) ==
                   CLM_STRING_HTTPP_TOO_MANY_HEADERS &&
                   clm_string_httpp_parse_request(get, strlen(get), NULL, 0, &message) ==
                       CLM_STRING_HTTPP_TOO_MANY_HEADERS &&
                   clm_string_httpp_parse_request(NULL, 1, headers, 8, &message) ==
                       CLM_STRING_HTTPP_INVALID_ARGUMENT &&
                   clm_string_httpp_parse_request(get, strlen(get), headers, 8, NULL) ==
                       CLM_STRING_HTTPP_INVALID_ARGUMENT,
               "enforce header capacity and reject invalid arguments");

    char text[512];
    size_t required = 0;
    CLM_ASSERT(clm_string_httpp_parse_request(pipelined + 2, strlen(pipelined) - 2, headers, 8, &message) ==
                   CLM_STRING_HTTPP_SUCCESS &&
                   clm_string_httpp_serialize(&message, NULL, 0, &required) == CLM_STRING_HTTPP_SUCCESS &&
                   required == strstr(pipelined, "GET") - (pipelined + 2) + 1 &&
                   clm_string_httpp_serialize(&message, text, sizeof(text), &required) ==
                       CLM_STRING_HTTPP_SUCCESS &&
                   strncmp(text, pipelined + 2, required - 1) == 0 && text[required - 1] == '\0' &&
                   clm_string_httpp_serialize(&message, text, required - 1, &required) ==
                       CLM_STRING_HTTPP_BUFFER_TOO_SMALL,
               "serialize a parsed request back to identical text and report short buffers");
    CLM_ASSERT(clm_string_httpp_parse_response(response, strlen(response), false, headers, 8, &message) ==
                   CLM_STRING_HTTPP_SUCCESS &&
                   clm_string_httpp_serialize(&message, text, sizeof(text), &required) ==
                       CLM_STRING_HTTPP_SUCCESS && strcmp(text, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
                       "Content-Type: text/plain\r\n\r\nok") == 0,
               "serialize a parsed response");

    clm_string_httpp_header built[2] = {
        {{"Host", 4}, {"example.com", 11}},
        {{"Accept", 6}, {"*/*", 3}},
    };
    CLM_ASSERT(clm_string_httpp_message_init(&message, CLM_STRING_HTTPP_REQUEST) == CLM_STRING_HTTPP_SUCCESS &&
                   (message.method = (clm_string_httpp_span){"PUT", 3}, message.target = (clm_string_httpp_span){"/r", 2},
                    message.headers = built, message.header_count = 2, message.header_capacity = 2,
                    message.body_kind = CLM_STRING_HTTPP_BODY_LENGTH, message.body = (clm_string_httpp_span){"data", 4}, 1) &&
                   clm_string_httpp_serialize(&message, text, sizeof(text), &required) ==
                       CLM_STRING_HTTPP_SUCCESS &&
                   strcmp(text, "PUT /r HTTP/1.1\r\nHost: example.com\r\nAccept: */*\r\n"
                       "Content-Length: 4\r\n\r\ndata") == 0,
               "build a request and add Content-Length automatically");

    char encoded[64];
    CLM_ASSERT(clm_string_httpp_encode_chunked("hello", 5, encoded, sizeof(encoded), &required) ==
                   CLM_STRING_HTTPP_SUCCESS && required == 15 && memcmp(encoded, "5\r\nhello\r\n0\r\n\r\n", 15) == 0 &&
                   clm_string_httpp_encode_chunked("", 0, encoded, sizeof(encoded), &required) ==
                       CLM_STRING_HTTPP_SUCCESS && required == 5 && memcmp(encoded, "0\r\n\r\n", 5) == 0 &&
                   clm_string_httpp_encode_chunked("hello", 5, encoded, 14, &required) ==
                       CLM_STRING_HTTPP_BUFFER_TOO_SMALL && required == 15,
               "encode chunked bodies and report short buffers");
    CLM_ASSERT(clm_string_httpp_message_init(&message, CLM_STRING_HTTPP_RESPONSE) == CLM_STRING_HTTPP_SUCCESS &&
                   clm_string_httpp_encode_chunked("hello", 5, encoded, sizeof(encoded), &required) ==
                       CLM_STRING_HTTPP_SUCCESS &&
                   (message.reason = (clm_string_httpp_span){"OK", 2},
                    message.body_kind = CLM_STRING_HTTPP_BODY_CHUNKED,
                    message.body = (clm_string_httpp_span){encoded, required}, 1) &&
                   clm_string_httpp_serialize(&message, text, sizeof(text), &required) ==
                       CLM_STRING_HTTPP_SUCCESS &&
                   strcmp(text, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                       "5\r\nhello\r\n0\r\n\r\n") == 0 &&
                   clm_string_httpp_parse_response(text, strlen(text), false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_SUCCESS && message.body_kind == CLM_STRING_HTTPP_BODY_CHUNKED,
               "round-trip a chunked response through serialize and parse");

    clm_string_httpp_header injected[1] = {{{"X", 1}, {"a\r\nEvil: 1", 10}}};
    clm_string_httpp_header mismatch[1] = {{{"Content-Length", 14}, {"9", 1}}};
    clm_string_httpp_header both[2] = {{{"Content-Length", 14}, {"4", 1}}, {{"Transfer-Encoding", 17}, {"chunked", 7}}};
    clm_string_httpp_message_init(&message, CLM_STRING_HTTPP_REQUEST);
    message.method = (clm_string_httpp_span){"GET", 3};
    message.target = (clm_string_httpp_span){"/", 1};
    message.headers = injected;
    message.header_count = 1;
    CLM_ASSERT(clm_string_httpp_serialize(&message, text, sizeof(text), &required) == CLM_STRING_HTTPP_FAILURE,
               "reject header values containing line breaks");
    message.headers = mismatch;
    message.body_kind = CLM_STRING_HTTPP_BODY_LENGTH;
    message.body = (clm_string_httpp_span){"data", 4};
    CLM_ASSERT(clm_string_httpp_serialize(&message, text, sizeof(text), &required) == CLM_STRING_HTTPP_FAILURE,
               "reject a Content-Length header that disagrees with the body");
    message.headers = both;
    message.header_count = 2;
    CLM_ASSERT(clm_string_httpp_serialize(&message, text, sizeof(text), &required) == CLM_STRING_HTTPP_FAILURE,
               "reject messages with both Content-Length and Transfer-Encoding");
    message.headers = NULL;
    message.header_count = 0;
    message.method = (clm_string_httpp_span){"GE T", 4};
    CLM_ASSERT(clm_string_httpp_serialize(&message, text, sizeof(text), &required) == CLM_STRING_HTTPP_FAILURE,
               "reject invalid method tokens");
    message.method = (clm_string_httpp_span){"GET", 3};
    message.body_kind = CLM_STRING_HTTPP_BODY_UNTIL_CLOSE;
    CLM_ASSERT(clm_string_httpp_serialize(&message, text, sizeof(text), &required) == CLM_STRING_HTTPP_FAILURE &&
                   clm_string_httpp_serialize(NULL, text, sizeof(text), &required) ==
                       CLM_STRING_HTTPP_INVALID_ARGUMENT,
               "reject until-close request bodies and NULL messages");

    const clm_id source_id = 77300;
    const clm_id destination_id = 77301;
    CLM_ASSERT(clm_string_new(source_id, "HTTP/1.1 201 Created\r\nContent-Length: 3\r\n\r\nabc") &&
                   clm_string_new(destination_id, "") &&
                   clm_string_httpp_loads_response(source_id, false, headers, 8, &message) ==
                       CLM_STRING_HTTPP_SUCCESS && message.status == 201 &&
                   clm_string_httpp_span_equals(message.body, "abc") &&
                   clm_string_httpp_dumps(&message, destination_id) == CLM_STRING_HTTPP_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(destination_id)->data,
                       "HTTP/1.1 201 Created\r\nContent-Length: 3\r\n\r\nabc") == 0 &&
                   clm_string_httpp_loads_request(source_id, headers, 8, &message) == CLM_STRING_HTTPP_FAILURE &&
                   clm_string_httpp_loads_request(77399, headers, 8, &message) ==
                       CLM_STRING_HTTPP_INVALID_ARGUMENT &&
                   clm_string_httpp_dumps(&message, 77399) == CLM_STRING_HTTPP_INVALID_ARGUMENT,
               "load from and dump to CLM string blocks");
    CLM_ASSERT(clm_string_delete(source_id) && clm_string_delete(destination_id), "release HTTPP test strings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
