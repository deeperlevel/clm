#pragma once

#include "test.h"
#include "../clm/http_utils.h"

#define CLM_HTTP_UTILS_TEST_PARSE_REQUEST(text, message, headers) \
    clm_http_utils_parse_request((text), strlen(text), (headers), 8, &(message))

static int clm_http_utils_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - HTTP Utils");
    clm_http_utils_header headers[8];
    clm_http_utils_message message;
    clm_http_utils_span value;

    const char get[] =
        "GET /index.html?q=1 HTTP/1.1\r\n"
        "Host:  example.com \r\n"
        "Accept: text/html\r\n"
        "\r\n";
    CLM_ASSERT(CLM_HTTP_UTILS_TEST_PARSE_REQUEST(get, message, headers) == CLM_HTTP_UTILS_SUCCESS &&
                   message.kind == CLM_HTTP_UTILS_REQUEST &&
                   clm_http_utils_span_equals(message.method, "GET") &&
                   clm_http_utils_span_equals(message.target, "/index.html?q=1") &&
                   message.version_major == 1 && message.version_minor == 1 &&
                   message.header_count == 2 && message.body_kind == CLM_HTTP_UTILS_BODY_NONE &&
                   message.consumed == sizeof(get) - 1,
               "parse a request line, headers, and an empty body");
    CLM_ASSERT(clm_http_utils_find_header(&message, "host", &value) &&
                   clm_http_utils_span_equals(value, "example.com") &&
                   clm_http_utils_span_equals(message.headers[0].name, "Host") &&
                   !clm_http_utils_find_header(&message, "Missing", &value),
               "trim optional whitespace and find headers ignoring case");

    const char pipelined[] =
        "\r\nPOST /submit HTTP/1.0\r\nContent-Length: 5\r\nX-A: 1\r\nX-A: 2\r\n\r\nhelloGET / HTTP/1.1\r\nHost: a\r\n\r\n";
    CLM_ASSERT(CLM_HTTP_UTILS_TEST_PARSE_REQUEST(pipelined, message, headers) == CLM_HTTP_UTILS_SUCCESS &&
                   message.version_minor == 0 && message.body_kind == CLM_HTTP_UTILS_BODY_LENGTH &&
                   clm_http_utils_span_equals(message.body, "hello") &&
                   message.header_count == 3 && message.consumed == strstr(pipelined, "GET") - pipelined &&
                   clm_http_utils_parse_request(pipelined + message.consumed,
                       strlen(pipelined) - message.consumed, headers, 8, &message) == CLM_HTTP_UTILS_SUCCESS &&
                   clm_http_utils_span_equals(message.method, "GET"),
               "skip a leading empty line, frame by Content-Length, and parse pipelined messages");

    const char headers_only[] =
        "POST /upload HTTP/1.1\r\nHost: example.test\r\nContent-Length: 9\r\n\r\n";
    CLM_ASSERT(clm_http_utils_parse_request_headers(headers_only,
                       sizeof(headers_only) - 1, headers, 8, &message) == CLM_HTTP_UTILS_SUCCESS &&
                   message.body_kind == CLM_HTTP_UTILS_BODY_LENGTH && message.body.length == 0 &&
                   message.consumed == sizeof(headers_only) - 1 &&
                   clm_http_utils_parse_request(headers_only, sizeof(headers_only) - 1,
                       headers, 8, &message) == CLM_HTTP_UTILS_INCOMPLETE,
               "parse and validate request headers before the framed body arrives");

    CLM_ASSERT(clm_http_utils_parse_request(get, 10, headers, 8, &message) == CLM_HTTP_UTILS_INCOMPLETE &&
                   clm_http_utils_parse_request(get, sizeof(get) - 2, headers, 8, &message) ==
                       CLM_HTTP_UTILS_INCOMPLETE &&
                   clm_http_utils_parse_request(pipelined, strstr(pipelined, "hello") - pipelined + 3,
                       headers, 8, &message) == CLM_HTTP_UTILS_INCOMPLETE &&
                   clm_http_utils_parse_request("", 0, headers, 8, &message) == CLM_HTTP_UTILS_INCOMPLETE,
               "report incomplete headers and bodies");

    const char chunked[] =
        "POST /c HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: Chunked\r\n\r\n"
        "4;ext=1\r\nWiki\r\n6\r\npedia!\r\n0\r\nX-Sum: 1\r\n\r\n";
    char decoded[32];
    size_t decoded_length = 0, chunk_consumed = 0;
    CLM_ASSERT(CLM_HTTP_UTILS_TEST_PARSE_REQUEST(chunked, message, headers) == CLM_HTTP_UTILS_SUCCESS &&
                   message.body_kind == CLM_HTTP_UTILS_BODY_CHUNKED &&
                   message.consumed == sizeof(chunked) - 1 &&
                   clm_http_utils_decode_chunked(message.body.data, message.body.length, decoded,
                       sizeof(decoded), &decoded_length, &chunk_consumed) == CLM_HTTP_UTILS_SUCCESS &&
                   decoded_length == 10 && memcmp(decoded, "Wikipedia!", 10) == 0 &&
                   chunk_consumed == message.body.length,
               "frame chunked bodies and decode extensions and trailers");
    CLM_ASSERT(clm_http_utils_decode_chunked(message.body.data, message.body.length, NULL, 0,
                       &decoded_length, NULL) == CLM_HTTP_UTILS_SUCCESS && decoded_length == 10 &&
                   clm_http_utils_decode_chunked(message.body.data, message.body.length, decoded, 4,
                       &decoded_length, NULL) == CLM_HTTP_UTILS_BUFFER_TOO_SMALL && decoded_length == 10 &&
                   clm_http_utils_decode_chunked("4\r\nWi", 5, NULL, 0, NULL, NULL) ==
                       CLM_HTTP_UTILS_INCOMPLETE &&
                   clm_http_utils_decode_chunked("zz\r\n", 4, NULL, 0, NULL, NULL) == CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_decode_chunked("1\r\nabc\r\n0\r\n\r\n", 13, NULL, 0, NULL, NULL) ==
                       CLM_HTTP_UTILS_FAILURE,
               "measure chunked output and reject truncated and malformed chunks");

    const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: text/plain\r\n\r\nokEXTRA";
    CLM_ASSERT(clm_http_utils_parse_response(response, strlen(response), false, headers, 8, &message) ==
                   CLM_HTTP_UTILS_SUCCESS && message.kind == CLM_HTTP_UTILS_RESPONSE &&
                   message.status == 200 && clm_http_utils_span_equals(message.reason, "OK") &&
                   clm_http_utils_span_equals(message.body, "ok") &&
                   message.consumed == strlen(response) - 5,
               "parse a response with Content-Length framing");
    const char no_reason[] = "HTTP/1.1 404 \r\n\r\nbody";
    const char bare[] = "HTTP/1.1 404\r\n\r\n";
    const char head[] = "HTTP/1.1 200 OK\r\nContent-Length: 99\r\n\r\n";
    const char no_content[] = "HTTP/1.1 204 No Content\r\n\r\n";
    CLM_ASSERT(clm_http_utils_parse_response_ex(no_reason, strlen(no_reason), false, false, true, headers, 8,
                       &message) == CLM_HTTP_UTILS_SUCCESS && message.status == 404 && message.reason.length == 0 &&
                   message.body_kind == CLM_HTTP_UTILS_BODY_UNTIL_CLOSE &&
                   clm_http_utils_span_equals(message.body, "body") &&
                   clm_http_utils_parse_response_ex(bare, strlen(bare), false, false, true, headers, 8, &message) ==
                       CLM_HTTP_UTILS_SUCCESS && message.reason.length == 0 && message.body.length == 0 &&
                   clm_http_utils_parse_response(no_reason, strlen(no_reason), false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_INCOMPLETE &&
                   clm_http_utils_parse_response(head, strlen(head), true, headers, 8, &message) ==
                       CLM_HTTP_UTILS_SUCCESS && message.body_kind == CLM_HTTP_UTILS_BODY_NONE &&
                   clm_http_utils_parse_response(head, strlen(head), false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_INCOMPLETE &&
                   clm_http_utils_parse_response_ex(head, strlen(head), false, false, true, headers, 8, &message) ==
                       CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_parse_response(no_content, strlen(no_content), false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_SUCCESS && message.body_kind == CLM_HTTP_UTILS_BODY_NONE,
               "handle empty reasons, until-close bodies only after EOF, HEAD responses, and 204");
    const char tunnel[] = "HTTP/1.1 200 Connection established\r\nContent-Length: 5\r\n\r\nabc";
    CLM_ASSERT(clm_http_utils_parse_response_ex(tunnel, strlen(tunnel), false, true, false, headers, 8, &message) ==
                   CLM_HTTP_UTILS_SUCCESS && message.body_kind == CLM_HTTP_UTILS_BODY_NONE &&
                   message.consumed == strstr(tunnel, "abc") - tunnel &&
                   clm_http_utils_parse_response(tunnel, strlen(tunnel), false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_INCOMPLETE,
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
        rejected += clm_http_utils_parse_request(bad[i], strlen(bad[i]), headers, 8, &message) ==
            CLM_HTTP_UTILS_FAILURE;
    CLM_ASSERT(rejected == (int)(sizeof(bad) / sizeof(bad[0])),
               "reject bare LF, folding, spaced names, controls, bad lines, and conflicting framing");
    CLM_ASSERT(clm_http_utils_parse_response("HTTP/1.1 99 X\r\n\r\n", 17, false, headers, 8, &message) ==
                   CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_parse_response("HTTP/1.1 2000 X\r\n\r\n", 19, false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_parse_response("HTTP/1.1 200X\r\n\r\n", 17, false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_FAILURE,
               "reject malformed status lines");
    CLM_ASSERT(clm_http_utils_parse_request(get, strlen(get), headers, 1, &message) ==
                   CLM_HTTP_UTILS_TOO_MANY_HEADERS &&
                   clm_http_utils_parse_request(get, strlen(get), NULL, 0, &message) ==
                       CLM_HTTP_UTILS_TOO_MANY_HEADERS &&
                   clm_http_utils_parse_request(NULL, 1, headers, 8, &message) ==
                       CLM_HTTP_UTILS_INVALID_ARGUMENT &&
                   clm_http_utils_parse_request(get, strlen(get), headers, 8, NULL) ==
                       CLM_HTTP_UTILS_INVALID_ARGUMENT,
               "enforce header capacity and reject invalid arguments");

    char text[512];
    size_t required = 0;
    CLM_ASSERT(clm_http_utils_parse_request(pipelined + 2, strlen(pipelined) - 2, headers, 8, &message) ==
                   CLM_HTTP_UTILS_SUCCESS &&
                   clm_http_utils_serialize(&message, NULL, 0, &required) == CLM_HTTP_UTILS_SUCCESS &&
                   required == strstr(pipelined, "GET") - (pipelined + 2) + 1 &&
                   clm_http_utils_serialize(&message, text, sizeof(text), &required) ==
                       CLM_HTTP_UTILS_SUCCESS &&
                   strncmp(text, pipelined + 2, required - 1) == 0 && text[required - 1] == '\0' &&
                   clm_http_utils_serialize(&message, text, required - 1, &required) ==
                       CLM_HTTP_UTILS_BUFFER_TOO_SMALL,
               "serialize a parsed request back to identical text and report short buffers");
    CLM_ASSERT(clm_http_utils_parse_response(response, strlen(response), false, headers, 8, &message) ==
                   CLM_HTTP_UTILS_SUCCESS &&
                   clm_http_utils_serialize(&message, text, sizeof(text), &required) ==
                       CLM_HTTP_UTILS_SUCCESS && strcmp(text, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
                       "Content-Type: text/plain\r\n\r\nok") == 0,
               "serialize a parsed response");

    clm_http_utils_header built[2] = {
        {{"Host", 4}, {"example.com", 11}},
        {{"Accept", 6}, {"*/*", 3}},
    };
    CLM_ASSERT(clm_http_utils_message_init(&message, CLM_HTTP_UTILS_REQUEST) == CLM_HTTP_UTILS_SUCCESS &&
                   (message.method = (clm_http_utils_span){"PUT", 3}, message.target = (clm_http_utils_span){"/r", 2},
                    message.headers = built, message.header_count = 2, message.header_capacity = 2,
                    message.body_kind = CLM_HTTP_UTILS_BODY_LENGTH, message.body = (clm_http_utils_span){"data", 4}, 1) &&
                   clm_http_utils_serialize(&message, text, sizeof(text), &required) ==
                       CLM_HTTP_UTILS_SUCCESS &&
                   strcmp(text, "PUT /r HTTP/1.1\r\nHost: example.com\r\nAccept: */*\r\n"
                       "Content-Length: 4\r\n\r\ndata") == 0,
               "build a request and add Content-Length automatically");

    char encoded[64];
    CLM_ASSERT(clm_http_utils_encode_chunked("hello", 5, encoded, sizeof(encoded), &required) ==
                   CLM_HTTP_UTILS_SUCCESS && required == 15 && memcmp(encoded, "5\r\nhello\r\n0\r\n\r\n", 15) == 0 &&
                   clm_http_utils_encode_chunked("", 0, encoded, sizeof(encoded), &required) ==
                       CLM_HTTP_UTILS_SUCCESS && required == 5 && memcmp(encoded, "0\r\n\r\n", 5) == 0 &&
                   clm_http_utils_encode_chunked("hello", 5, encoded, 14, &required) ==
                       CLM_HTTP_UTILS_BUFFER_TOO_SMALL && required == 15,
               "encode chunked bodies and report short buffers");
    CLM_ASSERT(clm_http_utils_message_init(&message, CLM_HTTP_UTILS_RESPONSE) == CLM_HTTP_UTILS_SUCCESS &&
                   clm_http_utils_encode_chunked("hello", 5, encoded, sizeof(encoded), &required) ==
                       CLM_HTTP_UTILS_SUCCESS &&
                   (message.reason = (clm_http_utils_span){"OK", 2},
                    message.body_kind = CLM_HTTP_UTILS_BODY_CHUNKED,
                    message.body = (clm_http_utils_span){encoded, required}, 1) &&
                   clm_http_utils_serialize(&message, text, sizeof(text), &required) ==
                       CLM_HTTP_UTILS_SUCCESS &&
                   strcmp(text, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                       "5\r\nhello\r\n0\r\n\r\n") == 0 &&
                   clm_http_utils_parse_response(text, strlen(text), false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_SUCCESS && message.body_kind == CLM_HTTP_UTILS_BODY_CHUNKED,
               "round-trip a chunked response through serialize and parse");

    clm_http_utils_header injected[1] = {{{"X", 1}, {"a\r\nEvil: 1", 10}}};
    clm_http_utils_header mismatch[1] = {{{"Content-Length", 14}, {"9", 1}}};
    clm_http_utils_header both[2] = {{{"Content-Length", 14}, {"4", 1}}, {{"Transfer-Encoding", 17}, {"chunked", 7}}};
    clm_http_utils_message_init(&message, CLM_HTTP_UTILS_REQUEST);
    message.method = (clm_http_utils_span){"GET", 3};
    message.target = (clm_http_utils_span){"/", 1};
    message.headers = injected;
    message.header_count = 1;
    CLM_ASSERT(clm_http_utils_serialize(&message, text, sizeof(text), &required) == CLM_HTTP_UTILS_FAILURE,
               "reject header values containing line breaks");
    message.headers = mismatch;
    message.body_kind = CLM_HTTP_UTILS_BODY_LENGTH;
    message.body = (clm_http_utils_span){"data", 4};
    CLM_ASSERT(clm_http_utils_serialize(&message, text, sizeof(text), &required) == CLM_HTTP_UTILS_FAILURE,
               "reject a Content-Length header that disagrees with the body");
    message.headers = both;
    message.header_count = 2;
    CLM_ASSERT(clm_http_utils_serialize(&message, text, sizeof(text), &required) == CLM_HTTP_UTILS_FAILURE,
               "reject messages with both Content-Length and Transfer-Encoding");
    message.headers = NULL;
    message.header_count = 0;
    message.method = (clm_http_utils_span){"GE T", 4};
    CLM_ASSERT(clm_http_utils_serialize(&message, text, sizeof(text), &required) == CLM_HTTP_UTILS_FAILURE,
               "reject invalid method tokens");
    message.method = (clm_http_utils_span){"GET", 3};
    message.body_kind = CLM_HTTP_UTILS_BODY_UNTIL_CLOSE;
    CLM_ASSERT(clm_http_utils_serialize(&message, text, sizeof(text), &required) == CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_serialize(NULL, text, sizeof(text), &required) ==
                       CLM_HTTP_UTILS_INVALID_ARGUMENT,
               "reject until-close request bodies and NULL messages");

    const clm_id source_id = 77300;
    const clm_id destination_id = 77301;
    CLM_ASSERT(clm_string_new(source_id, "HTTP/1.1 201 Created\r\nContent-Length: 3\r\n\r\nabc") &&
                   clm_string_new(destination_id, "") &&
                   clm_http_utils_loads_response(source_id, false, headers, 8, &message) ==
                       CLM_HTTP_UTILS_SUCCESS && message.status == 201 &&
                   clm_http_utils_span_equals(message.body, "abc") &&
                   clm_http_utils_dumps(&message, destination_id) == CLM_HTTP_UTILS_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(destination_id)->data,
                       "HTTP/1.1 201 Created\r\nContent-Length: 3\r\n\r\nabc") == 0 &&
                   clm_http_utils_loads_request(source_id, headers, 8, &message) == CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_loads_request(77399, headers, 8, &message) ==
                       CLM_HTTP_UTILS_INVALID_ARGUMENT &&
                   clm_http_utils_dumps(&message, 77399) == CLM_HTTP_UTILS_INVALID_ARGUMENT,
               "load from and dump to CLM string blocks");
    CLM_ASSERT(clm_string_delete(source_id) && clm_string_delete(destination_id), "release HTTP utils test strings");

    CLM_ASSERT(clm_http_utils_is_token("X-Custom_1") && clm_http_utils_is_token("GET") &&
                   !clm_http_utils_is_token("") && !clm_http_utils_is_token(NULL) &&
                   !clm_http_utils_is_token("Bad Name") && !clm_http_utils_is_token("a:b"),
               "validate header and method tokens");

    const clm_http_utils_span list = {"gzip;q=0, br ; q=0.5 , Deflate", 30};
    CLM_ASSERT(!clm_http_utils_header_has_token(list, "gzip") &&
                   clm_http_utils_header_has_token(list, "BR") &&
                   clm_http_utils_header_has_token(list, "deflate") &&
                   !clm_http_utils_header_has_token(list, "zstd") &&
                   !clm_http_utils_header_has_token(list, NULL),
               "match list tokens ignoring case and refuse q=0");

    CLM_ASSERT(strcmp(clm_http_utils_reason_phrase(404), "Not Found") == 0 &&
                   strcmp(clm_http_utils_reason_phrase(100), "Continue") == 0 &&
                   strcmp(clm_http_utils_reason_phrase(299), "Status") == 0,
               "map status codes to reason phrases");

    char date[64] = {0};
    CLM_ASSERT(clm_http_utils_format_date(date, sizeof(date)) && strlen(date) == 29 &&
                   strcmp(date + 26, "GMT") == 0 && !clm_http_utils_format_date(date, 5) &&
                   !clm_http_utils_format_date(NULL, 64),
               "format an IMF-fixdate and reject small buffers");

    size_t chunk_size = 0;
    CLM_ASSERT(clm_http_utils_parse_chunk_size("1aF;ext=1", 9, &chunk_size) == CLM_HTTP_UTILS_SUCCESS &&
                   chunk_size == 0x1af &&
                   clm_http_utils_parse_chunk_size("0", 1, &chunk_size) == CLM_HTTP_UTILS_SUCCESS &&
                   chunk_size == 0 &&
                   clm_http_utils_parse_chunk_size("5 ", 2, &chunk_size) == CLM_HTTP_UTILS_SUCCESS &&
                   chunk_size == 5 &&
                   clm_http_utils_parse_chunk_size("", 0, &chunk_size) == CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_parse_chunk_size(";x", 2, &chunk_size) == CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_parse_chunk_size("zz", 2, &chunk_size) == CLM_HTTP_UTILS_FAILURE &&
                   clm_http_utils_parse_chunk_size("fffffffffffffffffffff", 21, &chunk_size) ==
                       CLM_HTTP_UTILS_LIMIT_EXCEEDED,
               "parse chunk sizes with extensions, bad digits and overflow");

    clm_http_utils_header manual[1] = {{{"Content-Length", 14}, {"42", 2}}};
    clm_http_utils_message_init(&message, CLM_HTTP_UTILS_REQUEST);
    message.headers = manual;
    message.header_count = 1;
    size_t content_length = 99;
    CLM_ASSERT(clm_http_utils_content_length(&message, &content_length) == CLM_HTTP_UTILS_SUCCESS &&
                   content_length == 42,
               "read Content-Length");
    manual[0].value = (clm_http_utils_span){"4x", 2};
    CLM_ASSERT(clm_http_utils_content_length(&message, &content_length) == CLM_HTTP_UTILS_FAILURE,
               "reject a non-numeric Content-Length");
    manual[0].value = (clm_http_utils_span){"99999999999999999999999999", 26};
    CLM_ASSERT(clm_http_utils_content_length(&message, &content_length) ==
                   CLM_HTTP_UTILS_LIMIT_EXCEEDED,
               "report an overflowing Content-Length");
    message.header_count = 0;
    CLM_ASSERT(clm_http_utils_content_length(&message, &content_length) == CLM_HTTP_UTILS_SUCCESS &&
                   content_length == 0,
               "treat a missing Content-Length as zero");

    clm_http_utils_header expect_headers[2] = {
        {{"Expect", 6}, {"100-continue", 12}}, {{"Connection", 10}, {"close", 5}}};
    clm_http_utils_message_init(&message, CLM_HTTP_UTILS_REQUEST);
    message.headers = expect_headers;
    message.header_count = 2;
    message.version_major = 1;
    message.version_minor = 1;
    CLM_ASSERT(clm_http_utils_expects_continue(&message) && !clm_http_utils_keep_alive(&message),
               "detect Expect: 100-continue and Connection: close");
    message.version_minor = 0;
    CLM_ASSERT(!clm_http_utils_expects_continue(&message),
               "ignore Expect on HTTP/1.0 requests");
    message.header_count = 0;
    CLM_ASSERT(!clm_http_utils_keep_alive(&message), "close HTTP/1.0 by default");
    expect_headers[1].value = (clm_http_utils_span){"Keep-Alive", 10};
    message.headers = &expect_headers[1];
    message.header_count = 1;
    CLM_ASSERT(clm_http_utils_keep_alive(&message), "keep HTTP/1.0 alive on request");
    message.version_minor = 1;
    message.header_count = 0;
    CLM_ASSERT(clm_http_utils_keep_alive(&message), "keep HTTP/1.1 alive by default");

    const clm_http_utils_span target = {"/a/b?x=1#f", 10};
    clm_http_utils_span path = clm_http_utils_path_from_target(target);
    CLM_ASSERT(clm_http_utils_span_equals(path, "/a/b") &&
                   clm_http_utils_path_from_target((clm_http_utils_span){"/p", 2}).length == 2 &&
                   clm_http_utils_path_from_target((clm_http_utils_span){NULL, 0}).length == 0,
               "split the path from the query and fragment");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
