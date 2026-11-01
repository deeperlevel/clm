#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

/* HTTP/1.x message parser and serializer (RFC 9112 framing).
 *
 * Parsing is zero-copy and allocation-free: a parsed message holds spans that
 * point into the input and header entries stored in caller-owned memory. The
 * spans stay valid only while the input is unchanged. Parsing is strict:
 * CRLF line endings only, no obsolete line folding, no whitespace before the
 * header colon, and conflicting framing (Content-Length with Transfer-Encoding,
 * or repeated Content-Length values) is rejected to prevent request smuggling.
 * For simplicity and predictable framing, Transfer-Encoding supports only a
 * single 'chunked' field; other transfer codings are rejected. HTTP/1.1
 * requests require exactly one nonempty Host field.
 */

#include "base.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLM_STRING_HTTPP_SUCCESS ((clm_int)0)
/** More input is needed; retry with more bytes. */
#define CLM_STRING_HTTPP_INCOMPLETE ((clm_int)1)
/** Malformed or unsafe message, or a message that cannot be serialized. */
#define CLM_STRING_HTTPP_FAILURE ((clm_int)-1)
#define CLM_STRING_HTTPP_INVALID_ARGUMENT ((clm_int)-2)
/** Output capacity is too small; the required size is still reported. */
#define CLM_STRING_HTTPP_BUFFER_TOO_SMALL ((clm_int)-3)
/** The message has more header fields than the supplied storage can hold. */
#define CLM_STRING_HTTPP_TOO_MANY_HEADERS ((clm_int)-4)

/** Borrowed, not NUL-terminated bytes; data may be NULL only when length is 0. */
typedef struct {
    const char* data;
    size_t length;
} clm_string_httpp_span;

typedef enum {
    CLM_STRING_HTTPP_REQUEST = 1,
    CLM_STRING_HTTPP_RESPONSE = 2
} clm_string_httpp_kind;

/** How the body of a message is framed. */
typedef enum {
    /** No body. */
    CLM_STRING_HTTPP_BODY_NONE = 0,
    /** body holds exactly Content-Length bytes. */
    CLM_STRING_HTTPP_BODY_LENGTH,
    /** body holds the raw chunked encoding, including terminator and trailers. */
    CLM_STRING_HTTPP_BODY_CHUNKED,
    /** Response only: body is every remaining input byte; the end is the close. */
    CLM_STRING_HTTPP_BODY_UNTIL_CLOSE
} clm_string_httpp_body_kind;

typedef struct {
    clm_string_httpp_span name;
    clm_string_httpp_span value;
} clm_string_httpp_header;

/** Parsed or to-be-serialized message. Header names keep their original case. */
typedef struct {
    clm_string_httpp_kind kind;
    /** Request only. */
    clm_string_httpp_span method;
    clm_string_httpp_span target;
    /** Response only; reason may be empty. */
    clm_uint status;
    clm_string_httpp_span reason;
    clm_uint version_major;
    clm_uint version_minor;
    /** Caller-owned storage; header_count entries are valid in order. */
    clm_string_httpp_header* headers;
    size_t header_count;
    size_t header_capacity;
    clm_string_httpp_body_kind body_kind;
    clm_string_httpp_span body;
    /** Parse only: input bytes used by this message, including a skipped
     * leading empty line; the next pipelined message starts after them. */
    size_t consumed;
} clm_string_httpp_message;

/** Zeroes a message for building and sets HTTP/1.1. Responses default to
 * status 200 and BODY_LENGTH with an empty body, so serialization adds
 * Content-Length: 0 rather than producing an ambiguous close-delimited 200.
 * Callers can change the body kind explicitly for HEAD, 204, 304, etc.
 * @param message Message to initialize.
 * @param kind Request or response.
 * @return SUCCESS, or INVALID_ARGUMENT for NULL or an unknown kind.
 */
clm_int clm_string_httpp_message_init(
    clm_string_httpp_message* message, clm_string_httpp_kind kind);

/** Parses one request from the start of data.
 * Leading empty lines are skipped. HTTP/1.1 requires exactly one Host field.
 * Body framing: a single Transfer-Encoding: chunked field, else Content-Length,
 * else no body. Conflicting framing and unsupported codings are rejected.
 * @param data Input bytes; need not be NUL-terminated.
 * @param length Input byte count.
 * @param headers Storage for header entries; may be NULL when capacity is 0.
 * @param header_capacity Number of entries in headers.
 * @param out Receives the message; spans point into data. Unchanged unless
 *        the result is SUCCESS.
 * @return SUCCESS, INCOMPLETE, FAILURE, TOO_MANY_HEADERS, or INVALID_ARGUMENT.
 */
clm_int clm_string_httpp_parse_request(
    const char* data, size_t length,
    clm_string_httpp_header* headers, size_t header_capacity,
    clm_string_httpp_message* out);

/** Parses one response from the start of data.
 * No body for HEAD responses, 1xx, 204 and 304. Otherwise framing is
 * chunked, Content-Length, or until close. Until-close returns INCOMPLETE
 * until the caller signals EOF using clm_string_httpp_parse_response_ex.
 * @param head_request True when answering a HEAD request.
 * @return Same codes as clm_string_httpp_parse_request.
 */
clm_int clm_string_httpp_parse_response(
    const char* data, size_t length, bool head_request,
    clm_string_httpp_header* headers, size_t header_capacity,
    clm_string_httpp_message* out);

/** Parse a response with the request method and connection state supplied.
 * @param head_request True if this responds to a HEAD request.
 * @param connect_request True if this responds to CONNECT; successful 2xx
 *        responses end after headers, leaving remaining bytes for the tunnel.
 * @param input_closed True only after the transport reports EOF. Required to
 *        complete close-delimited response bodies. A truncated framed message
 *        at EOF reports FAILURE rather than INCOMPLETE.
 * @return SUCCESS for a complete response; INCOMPLETE if more data/EOF is
 *         required; FAILURE for malformed or truncated input.
 */
clm_int clm_string_httpp_parse_response_ex(
    const char* data, size_t length,
    bool head_request, bool connect_request, bool input_closed,
    clm_string_httpp_header* headers, size_t header_capacity,
    clm_string_httpp_message* out);

/** Parses a request from a CLM string block; spans point into the block. */
clm_int clm_string_httpp_loads_request(
    clm_id source_id,
    clm_string_httpp_header* headers, size_t header_capacity,
    clm_string_httpp_message* out);

/** Parses a response from a CLM string block; spans point into the block. */
clm_int clm_string_httpp_loads_response(
    clm_id source_id, bool head_request,
    clm_string_httpp_header* headers, size_t header_capacity,
    clm_string_httpp_message* out);

/** Like loads_response, but also supplies CONNECT context and input EOF state. */
clm_int clm_string_httpp_loads_response_ex(
    clm_id source_id,
    bool head_request, bool connect_request, bool input_closed,
    clm_string_httpp_header* headers, size_t header_capacity,
    clm_string_httpp_message* out);

/** Finds the first header whose name equals name, ignoring ASCII case.
 * @return true and sets value when found.
 */
bool clm_string_httpp_find_header(
    const clm_string_httpp_message* message, const char* name,
    clm_string_httpp_span* value);

/** Compares a span with a NUL-terminated string byte for byte. */
bool clm_string_httpp_span_equals(
    clm_string_httpp_span span, const char* text);

/** Compares a span with a NUL-terminated string, ignoring ASCII case. */
bool clm_string_httpp_span_equals_ignore_case(
    clm_string_httpp_span span, const char* text);

/** Serializes a message as HTTP/1.x text with CRLF line endings.
 * Validates tokens, status, version and field values (no CR, LF or NUL), so
 * header injection is rejected. Content-Length is added when the body kind is
 * LENGTH and the header is absent; Transfer-Encoding: chunked is added for
 * CHUNKED when absent. Existing framing headers must agree with the body.
 * A CHUNKED body must already be valid chunked encoding (see encode_chunked).
 * The message's header_count must not exceed header_capacity.
 * The output is NUL-terminated; a NULL dest with capacity 0 only measures.
 * @param required Receives the output size including the terminating NUL.
 * @return SUCCESS, FAILURE for an invalid message, BUFFER_TOO_SMALL (dest is
 *         unchanged), or INVALID_ARGUMENT.
 */
clm_int clm_string_httpp_serialize(
    const clm_string_httpp_message* message,
    char* dest, size_t capacity, size_t* required);

/** Serializes into an existing CLM string block.
 * Fails when the result is not valid UTF-8 or the body holds a NUL byte.
 * @return SUCCESS, FAILURE, or INVALID_ARGUMENT for a missing/non-string block.
 */
clm_int clm_string_httpp_dumps(
    const clm_string_httpp_message* message, clm_id destination_id);

/** Decodes a chunked body, validating extensions and trailer field syntax.
 * Framing-sensitive trailer fields are rejected; allowed trailers are skipped.
 * @param dest Output; NULL with capacity 0 only measures.
 * @param out_length Receives the decoded size (the needed size when too small).
 * @param out_consumed Receives the encoded bytes used; may be NULL.
 * @return SUCCESS, INCOMPLETE, FAILURE, BUFFER_TOO_SMALL, or INVALID_ARGUMENT.
 *         BUFFER_TOO_SMALL leaves dest unchanged.
 */
clm_int clm_string_httpp_decode_chunked(
    const char* data, size_t length,
    char* dest, size_t capacity,
    size_t* out_length, size_t* out_consumed);

/** Encodes bytes as a single chunk followed by the terminating zero chunk.
 * @param required Receives the exact output size; no NUL is appended.
 * @return SUCCESS, BUFFER_TOO_SMALL (dest unchanged), or INVALID_ARGUMENT.
 */
clm_int clm_string_httpp_encode_chunked(
    const void* data, size_t length,
    char* dest, size_t capacity, size_t* required);

#ifdef __cplusplus
}
#endif
