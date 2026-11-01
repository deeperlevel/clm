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

/* HTTP/1.x utilities: message parsing, serialization and chunked coding (RFC 9112 framing).
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

#define CLM_HTTP_UTILS_SUCCESS ((clm_int)0)
/** More input is needed; retry with more bytes. */
#define CLM_HTTP_UTILS_INCOMPLETE ((clm_int)1)
/** Malformed or unsafe message, or a message that cannot be serialized. */
#define CLM_HTTP_UTILS_FAILURE ((clm_int)-1)
#define CLM_HTTP_UTILS_INVALID_ARGUMENT ((clm_int)-2)
/** Output capacity is too small; the required size is still reported. */
#define CLM_HTTP_UTILS_BUFFER_TOO_SMALL ((clm_int)-3)
/** The message has more header fields than the supplied storage can hold. */
#define CLM_HTTP_UTILS_TOO_MANY_HEADERS ((clm_int)-4)
/** A numeric field does not fit in size_t. */
#define CLM_HTTP_UTILS_LIMIT_EXCEEDED ((clm_int)-5)

/** Borrowed, not NUL-terminated bytes; data may be NULL only when length is 0. */
typedef struct {
    const char* data;
    size_t length;
} clm_http_utils_span;

typedef enum {
    CLM_HTTP_UTILS_REQUEST = 1,
    CLM_HTTP_UTILS_RESPONSE = 2
} clm_http_utils_kind;

/** How the body of a message is framed. */
typedef enum {
    /** No body. */
    CLM_HTTP_UTILS_BODY_NONE = 0,
    /** body holds exactly Content-Length bytes. */
    CLM_HTTP_UTILS_BODY_LENGTH,
    /** body holds the raw chunked encoding, including terminator and trailers. */
    CLM_HTTP_UTILS_BODY_CHUNKED,
    /** Response only: body is every remaining input byte; the end is the close. */
    CLM_HTTP_UTILS_BODY_UNTIL_CLOSE
} clm_http_utils_body_kind;

typedef struct {
    clm_http_utils_span name;
    clm_http_utils_span value;
} clm_http_utils_header;

/** Parsed or to-be-serialized message. Header names keep their original case. */
typedef struct {
    clm_http_utils_kind kind;
    /** Request only. */
    clm_http_utils_span method;
    clm_http_utils_span target;
    /** Response only; reason may be empty. */
    clm_uint status;
    clm_http_utils_span reason;
    clm_uint version_major;
    clm_uint version_minor;
    /** Caller-owned storage; header_count entries are valid in order. */
    clm_http_utils_header* headers;
    size_t header_count;
    size_t header_capacity;
    clm_http_utils_body_kind body_kind;
    clm_http_utils_span body;
    /** Parse only: input bytes used by this message, including a skipped
     * leading empty line; the next pipelined message starts after them. */
    size_t consumed;
} clm_http_utils_message;

/** Zeroes a message for building and sets HTTP/1.1. Responses default to
 * status 200 and BODY_LENGTH with an empty body, so serialization adds
 * Content-Length: 0 rather than producing an ambiguous close-delimited 200.
 * Callers can change the body kind explicitly for HEAD, 204, 304, etc.
 * @param message Message to initialize.
 * @param kind Request or response.
 * @return SUCCESS, or INVALID_ARGUMENT for NULL or an unknown kind.
 */
clm_int clm_http_utils_message_init(
    clm_http_utils_message* message, clm_http_utils_kind kind);

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
clm_int clm_http_utils_parse_request(
    const char* data, size_t length,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

/** Parses and validates a request line and complete header section without
 * waiting for the framed body. body is empty and consumed is the header byte
 * count; the same strict framing/smuggling checks as parse_request apply.
 */
clm_int clm_http_utils_parse_request_headers(
    const char* data, size_t length,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

/** Parses one response from the start of data.
 * No body for HEAD responses, 1xx, 204 and 304. Otherwise framing is
 * chunked, Content-Length, or until close. Until-close returns INCOMPLETE
 * until the caller signals EOF using clm_http_utils_parse_response_ex.
 * @param head_request True when answering a HEAD request.
 * @return Same codes as clm_http_utils_parse_request.
 */
clm_int clm_http_utils_parse_response(
    const char* data, size_t length, bool head_request,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

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
clm_int clm_http_utils_parse_response_ex(
    const char* data, size_t length,
    bool head_request, bool connect_request, bool input_closed,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

/** Parses a request from a CLM string block; spans point into the block. */
clm_int clm_http_utils_loads_request(
    clm_id source_id,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

/** Parses a response from a CLM string block; spans point into the block. */
clm_int clm_http_utils_loads_response(
    clm_id source_id, bool head_request,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

/** Like loads_response, but also supplies CONNECT context and input EOF state. */
clm_int clm_http_utils_loads_response_ex(
    clm_id source_id,
    bool head_request, bool connect_request, bool input_closed,
    clm_http_utils_header* headers, size_t header_capacity,
    clm_http_utils_message* out);

/** Finds the first header whose name equals name, ignoring ASCII case.
 * @return true and sets value when found.
 */
bool clm_http_utils_find_header(
    const clm_http_utils_message* message, const char* name,
    clm_http_utils_span* value);

/** Compares a span with a NUL-terminated string byte for byte. */
bool clm_http_utils_span_equals(
    clm_http_utils_span span, const char* text);

/** Compares a span with a NUL-terminated string, ignoring ASCII case. */
bool clm_http_utils_span_equals_ignore_case(
    clm_http_utils_span span, const char* text);

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
clm_int clm_http_utils_serialize(
    const clm_http_utils_message* message,
    char* dest, size_t capacity, size_t* required);

/** Serializes into an existing CLM string block.
 * Fails when the result is not valid UTF-8 or the body holds a NUL byte.
 * @return SUCCESS, FAILURE, or INVALID_ARGUMENT for a missing/non-string block.
 */
clm_int clm_http_utils_dumps(
    const clm_http_utils_message* message, clm_id destination_id);

/** Decodes a chunked body, validating extensions and trailer field syntax.
 * Framing-sensitive trailer fields are rejected; allowed trailers are skipped.
 * @param dest Output; NULL with capacity 0 only measures.
 * @param out_length Receives the decoded size (the needed size when too small).
 * @param out_consumed Receives the encoded bytes used; may be NULL.
 * @return SUCCESS, INCOMPLETE, FAILURE, BUFFER_TOO_SMALL, or INVALID_ARGUMENT.
 *         BUFFER_TOO_SMALL leaves dest unchanged.
 */
clm_int clm_http_utils_decode_chunked(
    const char* data, size_t length,
    char* dest, size_t capacity,
    size_t* out_length, size_t* out_consumed);

/** Encodes bytes as a single chunk followed by the terminating zero chunk.
 * @param required Receives the exact output size; no NUL is appended.
 * @return SUCCESS, BUFFER_TOO_SMALL (dest unchanged), or INVALID_ARGUMENT.
 */
clm_int clm_http_utils_encode_chunked(
    const void* data, size_t length,
    char* dest, size_t capacity, size_t* required);

/** True when name is a non-empty RFC 9110 token, i.e. a valid header or method name. */
bool clm_http_utils_is_token(const char* name);

/** Case-insensitively looks for token in a comma-separated list value such as Connection
 * or Accept-Encoding. Parameters are ignored except q=0, which marks the token as refused. */
bool clm_http_utils_header_has_token(clm_http_utils_span value, const char* token);

/** Standard reason phrase for a status code; "Status" for codes without one. */
const char* clm_http_utils_reason_phrase(clm_uint status);

/** Writes the current time as an IMF-fixdate (for example "Sun, 06 Nov 1994 08:49:37 GMT").
 * Returns false when capacity is too small (at least 30 bytes are needed). */
bool clm_http_utils_format_date(char* out, size_t capacity);

/** Parses the hexadecimal size on a chunk-size line (without the CRLF) and validates any
 * chunk extension with the same strict grammar as clm_http_utils_decode_chunked().
 * Returns SUCCESS, FAILURE for a malformed size or extension, or LIMIT_EXCEEDED on overflow. */
clm_int clm_http_utils_parse_chunk_size(const char* line, size_t length, size_t* out_size);

/** Validates one chunked trailer field line (without the CRLF): header syntax, and not a
 * field that must not appear in a trailer (Content-Length, Transfer-Encoding, Host...).
 * Returns SUCCESS or FAILURE. */
clm_int clm_http_utils_validate_trailer(const char* line, size_t length);

/** Reads Content-Length; an absent header yields 0. Returns SUCCESS, FAILURE for a value
 * that is empty or not all digits, or LIMIT_EXCEEDED when it does not fit in size_t. */
clm_int clm_http_utils_content_length(const clm_http_utils_message* message, size_t* out_length);

/** True for an HTTP/1.1 (or later) request carrying "Expect: 100-continue". */
bool clm_http_utils_expects_continue(const clm_http_utils_message* message);

/** Whether the connection should stay open after this message, per version and Connection. */
bool clm_http_utils_keep_alive(const clm_http_utils_message* message);

/** The path part of a request target: everything before the first '?' or '#'. */
clm_http_utils_span clm_http_utils_path_from_target(clm_http_utils_span target);

#ifdef __cplusplus
}
#endif
