#pragma once

#include "test.h"
#include "../clm/multipart.h"

static int clm_multipart_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Multipart");

    const char body[] = "preamble\r\n--AaB03x\r\nContent-Type: text/plain\r\nX-Test: yes\r\n\r\nhello\0world\r\n"
                        "--AaB03x\r\nContent-Disposition: form-data; name=\"file\"\r\n\r\n\xff\x00\r\n"
                        "--AaB03x--\r\nepilogue";
    clm_multipart_part parts[3];
    clm_multipart_header headers[8];
    clm_multipart_message message;
    clm_multipart_span boundary = {"AaB03x", 6};
    clm_int status = clm_multipart_parse(body, sizeof(body) - 1, boundary, parts, 3,
                                         headers, 8, &message);
    CLM_ASSERT(status == CLM_MULTIPART_SUCCESS && message.part_count == 2 &&
                   message.header_count == 3 && message.preamble.length == 8 &&
                   message.epilogue.length == 8 && parts[0].body.length == 11 &&
                   memcmp(parts[0].body.data, "hello\0world", 11) == 0 &&
                   parts[1].body.length == 2 && (unsigned char)parts[1].body.data[0] == 0xff &&
                   parts[1].body.data[1] == 0,
               "parse binary multipart parts with preamble and epilogue");
    clm_multipart_span header_value;
    CLM_ASSERT(clm_multipart_find_header(&message, &parts[0], "x-test", &header_value) &&
                   clm_multipart_span_equals_ignore_case(header_value, "yes") &&
                   clm_multipart_find_header(&message, &parts[1], "content-disposition", &header_value),
               "find part headers without regard to ASCII case");

    size_t required = 0;
    char boundary_text[72];
    const char content_type[] = "Multipart/form-data; charset=utf-8; boundary=\"AaB03x\"";
    CLM_ASSERT(clm_multipart_boundary_from_content_type(
                   (clm_multipart_span){content_type, sizeof(content_type) - 1},
                   boundary_text, sizeof(boundary_text), &required) == CLM_MULTIPART_SUCCESS &&
                   strcmp(boundary_text, "AaB03x") == 0 && required == 7 &&
                   clm_multipart_boundary_from_content_type(
                       (clm_multipart_span){content_type, sizeof(content_type) - 1},
                       boundary_text, 3, &required) == CLM_MULTIPART_BUFFER_TOO_SMALL && required == 7,
               "extract and measure a quoted Content-Type boundary");
    CLM_ASSERT(clm_multipart_boundary_is_valid((clm_multipart_span){"a b", 3}) &&
                   !clm_multipart_boundary_is_valid((clm_multipart_span){"a ", 2}) &&
                   !clm_multipart_boundary_is_valid((clm_multipart_span){"bad\r\n", 5}) &&
                   clm_multipart_parse(body, 14, boundary, parts, 3, headers, 8, &message) ==
                       CLM_MULTIPART_INCOMPLETE &&
                   clm_multipart_parse(body, sizeof(body) - 1, boundary, parts, 1, headers, 8, &message) ==
                       CLM_MULTIPART_TOO_MANY_PARTS,
               "validate boundaries and report truncated input and capacity limits");

    clm_bytes_buffer generated = {0};
    clm_multipart_part generated_parts[2] = {
        {0, 2, {"hello\0world", 11}},
        {2, 1, {"\xff\0", 2}},
    };
    clm_multipart_header generated_headers[3] = {
        {{"Content-Type", 12}, {"text/plain", 10}},
        {{"X-Test", 6}, {"yes", 3}},
        {{"Content-Disposition", 19}, {"form-data; name=\"file\"", 22}},
    };
    const char expected[] = "--AaB03x\r\nContent-Type: text/plain\r\nX-Test: yes\r\n\r\nhello\0world\r\n"
                            "--AaB03x\r\nContent-Disposition: form-data; name=\"file\"\r\n\r\n\xff\0\r\n"
                            "--AaB03x--\r\n";
    CLM_ASSERT(clm_bytes_buffer_init(&generated, 0) == CLM_BYTES_SUCCESS &&
                   clm_multipart_generate(&generated, boundary, generated_parts, 2,
                                          generated_headers, 3) == CLM_MULTIPART_SUCCESS &&
                   generated.length == sizeof(expected) - 1 &&
                   memcmp(generated.storage + generated.offset, expected, sizeof(expected) - 1) == 0,
               "generate a binary-safe multipart body");
    clm_bytes_buffer_dispose(&generated);

    clm_multipart_header injected[1] = {{{"X-Test", 6}, {"ok\r\nBad: x", 10}}};
    clm_multipart_part one[1] = {{0, 1, {"x", 1}}};
    clm_bytes_buffer_init(&generated, 0);
    CLM_ASSERT(clm_multipart_generate(&generated, boundary, one, 1, injected, 1) ==
                   CLM_MULTIPART_FAILURE && generated.length == 0 &&
                   clm_multipart_parse("--b\r\n\r\nx\r\n--b--\r\n", 17,
                       (clm_multipart_span){"b", 1}, parts, 3, headers, 8, &message) ==
                       CLM_MULTIPART_SUCCESS && parts[0].body.length == 1,
               "reject generated header injection and parse an empty-header part");
    clm_bytes_buffer_dispose(&generated);

    clm_bytes_buffer_init(&generated, 0);
    clm_bytes_buffer_append(&generated, "keep", 4);
    clm_bytes_buffer_set_limit(&generated, 4);
    CLM_ASSERT(clm_multipart_generate(&generated, boundary, generated_parts, 2,
                                      generated_headers, 3) == CLM_MULTIPART_LIMIT_EXCEEDED &&
                   generated.length == 4 && memcmp(generated.storage + generated.offset, "keep", 4) == 0,
               "preserve existing destination bytes when its size limit is reached");
    clm_bytes_buffer_dispose(&generated);

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}