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

/* MIME multipart body parsing and generation.
 *
 * Parsing is zero-copy: part bodies and header spans borrow the input bytes.
 * The caller supplies storage for parts and headers. Only CRLF framing is
 * accepted; header folding and malformed delimiter lines are rejected.
 */

#include "base.h"
#include "bytes.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLM_MULTIPART_SUCCESS ((clm_int)0)
#define CLM_MULTIPART_INCOMPLETE ((clm_int)1)
#define CLM_MULTIPART_FAILURE ((clm_int)-1)
#define CLM_MULTIPART_INVALID_ARGUMENT ((clm_int)-2)
#define CLM_MULTIPART_BUFFER_TOO_SMALL ((clm_int)-3)
#define CLM_MULTIPART_TOO_MANY_PARTS ((clm_int)-4)
#define CLM_MULTIPART_TOO_MANY_HEADERS ((clm_int)-5)
#define CLM_MULTIPART_NO_MEMORY ((clm_int)-6)
#define CLM_MULTIPART_LIMIT_EXCEEDED ((clm_int)-7)

typedef struct {
    const char* data;
    size_t length;
} clm_multipart_span;

typedef struct {
    clm_multipart_span name;
    clm_multipart_span value;
} clm_multipart_header;

typedef struct {
    size_t header_offset;
    size_t header_count;
    clm_multipart_span body;
} clm_multipart_part;

typedef struct {
    clm_multipart_part* parts;
    size_t part_count;
    size_t part_capacity;
    clm_multipart_header* headers;
    size_t header_count;
    size_t header_capacity;
    clm_multipart_span preamble;
    clm_multipart_span epilogue;
    size_t consumed;
} clm_multipart_message;

/** Validates the RFC 2046 boundary character set and 1-70 byte length. */
bool clm_multipart_boundary_is_valid(clm_multipart_span boundary);

/** Extracts a boundary parameter from a multipart Content-Type value.
 * Quoted values and quoted-pair escapes are supported. dest is NUL-terminated;
 * required includes that terminator. A NULL dest with capacity 0 measures.
 */
clm_int clm_multipart_boundary_from_content_type(
    clm_multipart_span content_type, char* dest, size_t capacity,
    size_t* required);

/** Parses one complete multipart body. Parts and headers point into data;
 * preamble and epilogue are also borrowed. The part/header arrays are caller
 * owned; their contents are unspecified unless SUCCESS is returned.
 * @return SUCCESS, INCOMPLETE, FAILURE, capacity error, or INVALID_ARGUMENT.
 */
clm_int clm_multipart_parse(
    const void* data, size_t length, clm_multipart_span boundary,
    clm_multipart_part* parts, size_t part_capacity,
    clm_multipart_header* headers, size_t header_capacity,
    clm_multipart_message* out);

/** Finds the first part header by ASCII case-insensitive name. */
bool clm_multipart_find_header(
    const clm_multipart_message* message, const clm_multipart_part* part,
    const char* name, clm_multipart_span* value);

/** Appends a complete multipart body. The destination is unchanged on failure.
 * Headers are stored in one flat array; each part selects a range by offset.
 */
clm_int clm_multipart_generate(
    clm_bytes_buffer* destination, clm_multipart_span boundary,
    const clm_multipart_part* parts, size_t part_count,
    const clm_multipart_header* headers, size_t header_count);

bool clm_multipart_span_equals_ignore_case(
    clm_multipart_span span, const char* text);

#ifdef __cplusplus
}
#endif