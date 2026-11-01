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

#include "http_utils.h"
#include "bpa.h"
#include "string.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HTTP_UTILS_OK CLM_HTTP_UTILS_SUCCESS
#define HTTP_UTILS_INCOMPLETE CLM_HTTP_UTILS_INCOMPLETE
#define HTTP_UTILS_FAIL CLM_HTTP_UTILS_FAILURE

static unsigned char http_utils_lower(const unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c;
}

static bool http_utils_is_digit(const char c)
{
    return c >= '0' && c <= '9';
}

static int http_utils_hex(const char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool http_utils_is_token_char(const unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c != 0 && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}

static bool http_utils_span_ok(const clm_http_utils_span span)
{
    return span.length == 0 || span.data != NULL;
}

static bool http_utils_is_token(const clm_http_utils_span span)
{
    if (span.length == 0 || span.data == NULL) return false;
    for (size_t i = 0; i < span.length; ++i)
        if (!http_utils_is_token_char((unsigned char)span.data[i])) return false;
    return true;
}

/* Field values and reasons: HTAB, SP, VCHAR and obs-text; no other controls. */
static bool http_utils_is_field_text(const clm_http_utils_span span)
{
    if (!http_utils_span_ok(span)) return false;
    for (size_t i = 0; i < span.length; ++i) {
        const unsigned char c = (unsigned char)span.data[i];
        if (c != '\t' && (c < 0x20 || c == 0x7f)) return false;
    }
    return true;
}

static bool http_utils_is_target(const clm_http_utils_span span)
{
    if (span.length == 0 || span.data == NULL) return false;
    for (size_t i = 0; i < span.length; ++i) {
        const unsigned char c = (unsigned char)span.data[i];
        if (c <= 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool clm_http_utils_span_equals(const clm_http_utils_span span, const char* text)
{
    if (text == NULL) return false;
    const size_t length = strlen(text);
    return span.length == length &&
        (length == 0 || (span.data != NULL && memcmp(span.data, text, length) == 0));
}

bool clm_http_utils_span_equals_ignore_case(const clm_http_utils_span span, const char* text)
{
    if (text == NULL) return false;
    const size_t length = strlen(text);
    if (span.length != length || (length != 0 && span.data == NULL)) return false;
    for (size_t i = 0; i < length; ++i)
        if (http_utils_lower((unsigned char)span.data[i]) != http_utils_lower((unsigned char)text[i]))
            return false;
    return true;
}

bool clm_http_utils_find_header(
    const clm_http_utils_message* message, const char* name, clm_http_utils_span* value)
{
    if (message == NULL || name == NULL || value == NULL ||
        (message->header_count != 0 && message->headers == NULL))
        return false;
    for (size_t i = 0; i < message->header_count; ++i) {
        if (clm_http_utils_span_equals_ignore_case(message->headers[i].name, name)) {
            *value = message->headers[i].value;
            return true;
        }
    }
    return false;
}

/* Finds the CRLF ending the line at pos; bare CR or LF is malformed. */
static clm_int http_utils_find_line(
    const char* data, const size_t length, const size_t pos, size_t* line_end)
{
    for (size_t i = pos; i < length; ++i) {
        if (data[i] == '\n') return HTTP_UTILS_FAIL;
        if (data[i] == '\r') {
            if (i + 1 >= length) return HTTP_UTILS_INCOMPLETE;
            if (data[i + 1] != '\n') return HTTP_UTILS_FAIL;
            *line_end = i;
            return HTTP_UTILS_OK;
        }
    }
    return HTTP_UTILS_INCOMPLETE;
}

static bool http_utils_parse_version(
    const char* text, const size_t length, clm_uint* major, clm_uint* minor)
{
    if (length != 8 || memcmp(text, "HTTP/", 5) != 0 || !http_utils_is_digit(text[5]) ||
        text[6] != '.' || !http_utils_is_digit(text[7]))
        return false;
    *major = (clm_uint)(text[5] - '0');
    *minor = (clm_uint)(text[7] - '0');
    return *major == 1 && *minor <= 1;
}

static bool http_utils_parse_uint(const clm_http_utils_span span, size_t* out)
{
    if (span.length == 0) return false;
    size_t value = 0;
    for (size_t i = 0; i < span.length; ++i) {
        if (!http_utils_is_digit(span.data[i])) return false;
        const size_t digit = (size_t)(span.data[i] - '0');
        if (value > (SIZE_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }
    *out = value;
    return true;
}

/* This initial HTTP/1.1 implementation supports the chunked transfer coding only.
 * Rejecting other transfer codings and repeated fields prevents peers with
 * different framing rules from disagreeing on message boundaries.
 */
static bool http_utils_is_chunked_transfer_encoding(const clm_http_utils_span span)
{
    return clm_http_utils_span_equals_ignore_case(span, "chunked");
}

typedef struct {
    bool has_length;
    size_t length;
    bool has_encoding;
    bool encoding_chunked;
    size_t host_count;
} http_utils_framing;

/* A conservative Host check. HTTP authority validation can be strengthened in
 * the server once request-target and virtual-host rules are applied.
 */
static bool http_utils_valid_host(const clm_http_utils_span host)
{
    if (host.length == 0 || host.data == NULL) return false;

    for (size_t i = 0; i < host.length; ++i) {
        const unsigned char c = (unsigned char)host.data[i];
        if (c <= 0x20 || c >= 0x7f || c == ',' || c == '/' || c == '@' || c == '\\')
            return false;
    }
    return true;
}

static clm_int http_utils_scan_framing(
    const clm_http_utils_header* headers, const size_t count, http_utils_framing* framing)
{
    memset(framing, 0, sizeof(*framing));

    for (size_t i = 0; i < count; ++i) {
        const clm_http_utils_header* header = &headers[i];

        if (clm_http_utils_span_equals_ignore_case(header->name, "Content-Length")) {
            size_t value;
            /* Reject duplicate fields, including matching duplicates. */
            if (framing->has_length || !http_utils_parse_uint(header->value, &value))
                return HTTP_UTILS_FAIL;
            framing->has_length = true;
            framing->length = value;
        } else if (clm_http_utils_span_equals_ignore_case(header->name, "Transfer-Encoding")) {
            if (framing->has_encoding ||
                !http_utils_is_chunked_transfer_encoding(header->value))
                return HTTP_UTILS_FAIL;
            framing->has_encoding = true;
            framing->encoding_chunked = true;
        } else if (clm_http_utils_span_equals_ignore_case(header->name, "Host")) {
            ++framing->host_count;
            if (framing->host_count != 1 || !http_utils_valid_host(header->value))
                return HTTP_UTILS_FAIL;
        }
    }

    if (framing->has_length && framing->has_encoding)
        return HTTP_UTILS_FAIL;
    return HTTP_UTILS_OK;
}

/* Header syntax is also used to validate (discarded) chunked trailers. */
static clm_int http_utils_parse_header_line(
    const char* line, size_t length, clm_http_utils_header* header);

/* Parse the optional chunk extension list. Values may be tokens or quoted
 * strings, with ordinary quoted-pair escaping. No control bytes are allowed.
 */
static bool http_utils_chunk_extensions(const char* line, size_t pos, size_t end)
{
    while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

    while (pos < end) {
        if (line[pos++] != ';') return false;
        while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

        const size_t name_start = pos;
        while (pos < end && http_utils_is_token_char((unsigned char)line[pos])) ++pos;
        if (pos == name_start) return false;
        while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

        if (pos < end && line[pos] == '=') {
            ++pos;
            while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

            if (pos < end && line[pos] == '"') {
                ++pos;
                bool closed = false;
                while (pos < end) {
                    const unsigned char c = (unsigned char)line[pos++];
                    if (c == '"') { closed = true; break; }
                    if (c == '\\') {
                        if (pos == end) return false;
                        const unsigned char escaped = (unsigned char)line[pos++];
                        if (escaped < 0x20 || escaped == 0x7f) return false;
                    } else if (c != '\t' && (c < 0x20 || c == 0x7f)) {
                        return false;
                    }
                }
                if (!closed) return false;
            } else {
                const size_t value_start = pos;
                while (pos < end && http_utils_is_token_char((unsigned char)line[pos])) ++pos;
                if (pos == value_start) return false;
            }
            while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        }
    }
    return true;
}

static bool http_utils_forbidden_trailer(const clm_http_utils_span name)
{
    static const char* forbidden[] = {
        "Content-Length", "Transfer-Encoding", "Host", "Connection",
        "TE", "Trailer", "Upgrade", "Expect", "Authorization",
        "Proxy-Authorization", "Content-Encoding", "Content-Type",
        "Content-Range", "Content-Location"
    };

    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        if (clm_http_utils_span_equals_ignore_case(name, forbidden[i]))
            return true;
    }
    return false;
}

/* Walks a chunked body; with dest == NULL it only validates and measures. */
static clm_int http_utils_chunked(
    const char* data, const size_t length, char* dest, const size_t capacity,
    size_t* out_length, size_t* out_consumed)
{
    size_t pos = 0;
    size_t decoded = 0;
    bool too_small = false;
    for (;;) {
        size_t line_end;
        clm_int status = http_utils_find_line(data, length, pos, &line_end);
        if (status != HTTP_UTILS_OK) return status;
        size_t size = 0;
        size_t index = pos;
        while (index < line_end && http_utils_hex(data[index]) >= 0) {
            if (size > (SIZE_MAX >> 4)) return HTTP_UTILS_FAIL;
            size = (size << 4) | (size_t)http_utils_hex(data[index]);
            ++index;
        }
        if (index == pos || !http_utils_chunk_extensions(data, index, line_end))
            return HTTP_UTILS_FAIL;
        pos = line_end + 2;
        if (size == 0) {
            for (;;) {
                status = http_utils_find_line(data, length, pos, &line_end);
                if (status != HTTP_UTILS_OK) return status;
                const bool last = line_end == pos;
                if (!last) {
                    clm_http_utils_header trailer;
                    if (http_utils_parse_header_line(data + pos, line_end - pos, &trailer) != HTTP_UTILS_OK ||
                        http_utils_forbidden_trailer(trailer.name))
                        return HTTP_UTILS_FAIL;
                }
                pos = line_end + 2;
                if (last) break;
            }
            break;
        }
        if (size > length - pos || length - pos - size < 2) return HTTP_UTILS_INCOMPLETE;
        if (data[pos + size] != '\r' || data[pos + size + 1] != '\n') return HTTP_UTILS_FAIL;
        if (size > SIZE_MAX - decoded) return HTTP_UTILS_FAIL;
        if (dest != NULL) {
            if (decoded > capacity || size > capacity - decoded) too_small = true;
            else memcpy(dest + decoded, data + pos, size);
        }
        decoded += size;
        pos += size + 2;
    }
    if (out_length != NULL) *out_length = decoded;
    if (out_consumed != NULL) *out_consumed = pos;
    return too_small ? CLM_HTTP_UTILS_BUFFER_TOO_SMALL : HTTP_UTILS_OK;
}

clm_int clm_http_utils_decode_chunked(
    const char* data, const size_t length, char* dest, const size_t capacity,
    size_t* out_length, size_t* out_consumed)
{
    if ((data == NULL && length != 0) || (dest == NULL && capacity != 0))
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    /* Validate and measure first so BUFFER_TOO_SMALL leaves dest unchanged. */
    size_t decoded_length = 0;
    size_t consumed = 0;
    clm_int status = http_utils_chunked(data, length, NULL, 0,
        &decoded_length, &consumed);
    if (status != HTTP_UTILS_OK) return status;

    if (out_length != NULL) *out_length = decoded_length;
    if (out_consumed != NULL) *out_consumed = consumed;
    if (dest == NULL) return HTTP_UTILS_OK;
    if (capacity < decoded_length) return CLM_HTTP_UTILS_BUFFER_TOO_SMALL;

    return http_utils_chunked(data, length, dest, capacity, NULL, NULL);
}

clm_int clm_http_utils_encode_chunked(
    const void* data, const size_t length, char* dest, const size_t capacity, size_t* required)
{
    if (required == NULL || (data == NULL && length != 0) || (dest == NULL && capacity != 0))
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    static const char terminator[] = "0\r\n\r\n";
    char head[2 * sizeof(unsigned long long) + 3];
    int head_length = 0;
    size_t total = sizeof(terminator) - 1;
    if (length != 0) {
        head_length = snprintf(head, sizeof(head), "%llx\r\n", (unsigned long long)length);
        if (head_length < 0 || length > SIZE_MAX - (size_t)head_length - total - 2)
            return CLM_HTTP_UTILS_INVALID_ARGUMENT;
        total += (size_t)head_length + length + 2;
    }
    *required = total;
    if (dest == NULL) return HTTP_UTILS_OK;
    if (capacity < total) return CLM_HTTP_UTILS_BUFFER_TOO_SMALL;
    size_t pos = 0;
    if (length != 0) {
        memcpy(dest, head, (size_t)head_length);
        pos = (size_t)head_length;
        memcpy(dest + pos, data, length);
        pos += length;
        memcpy(dest + pos, "\r\n", 2);
        pos += 2;
    }
    memcpy(dest + pos, terminator, sizeof(terminator) - 1);
    return HTTP_UTILS_OK;
}

clm_int clm_http_utils_message_init(
    clm_http_utils_message* message, const clm_http_utils_kind kind)
{
    if (message == NULL || (kind != CLM_HTTP_UTILS_REQUEST && kind != CLM_HTTP_UTILS_RESPONSE))
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    memset(message, 0, sizeof(*message));
    message->kind = kind;
    message->version_major = 1;
    message->version_minor = 1;
    if (kind == CLM_HTTP_UTILS_RESPONSE) {
        message->status = 200;
        /* An empty 200 response must still be unambiguously framed. */
        message->body_kind = CLM_HTTP_UTILS_BODY_LENGTH;
    }
    return HTTP_UTILS_OK;
}

static clm_int http_utils_parse_request_line(
    const char* line, const size_t length, clm_http_utils_message* message)
{
    const char* first = memchr(line, ' ', length);
    if (first == NULL) return HTTP_UTILS_FAIL;
    const size_t method_length = (size_t)(first - line);
    const char* target = first + 1;
    const size_t rest = length - method_length - 1;
    const char* second = memchr(target, ' ', rest);
    if (second == NULL) return HTTP_UTILS_FAIL;
    const size_t target_length = (size_t)(second - target);
    message->method = (clm_http_utils_span){line, method_length};
    message->target = (clm_http_utils_span){target, target_length};
    if (!http_utils_is_token(message->method) || !http_utils_is_target(message->target) ||
        !http_utils_parse_version(second + 1, rest - target_length - 1,
            &message->version_major, &message->version_minor))
        return HTTP_UTILS_FAIL;
    return HTTP_UTILS_OK;
}

static clm_int http_utils_parse_status_line(
    const char* line, const size_t length, clm_http_utils_message* message)
{
    if (length < 12 || line[8] != ' ' || !http_utils_parse_version(line, 8,
            &message->version_major, &message->version_minor) ||
        !http_utils_is_digit(line[9]) || !http_utils_is_digit(line[10]) || !http_utils_is_digit(line[11]))
        return HTTP_UTILS_FAIL;
    message->status = (clm_uint)((line[9] - '0') * 100 + (line[10] - '0') * 10 + (line[11] - '0'));
    if (message->status < 100) return HTTP_UTILS_FAIL;
    if (length == 12) return HTTP_UTILS_OK;
    if (line[12] != ' ') return HTTP_UTILS_FAIL;
    message->reason = (clm_http_utils_span){line + 13, length - 13};
    return http_utils_is_field_text(message->reason) ? HTTP_UTILS_OK : HTTP_UTILS_FAIL;
}

static clm_int http_utils_parse_header_line(
    const char* line, const size_t length, clm_http_utils_header* header)
{
    const char* colon = memchr(line, ':', length);
    if (colon == NULL || line[0] == ' ' || line[0] == '\t') return HTTP_UTILS_FAIL;
    header->name = (clm_http_utils_span){line, (size_t)(colon - line)};
    if (!http_utils_is_token(header->name)) return HTTP_UTILS_FAIL;
    size_t start = (size_t)(colon - line) + 1;
    size_t end = length;
    while (start < end && (line[start] == ' ' || line[start] == '\t')) ++start;
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) --end;
    header->value = (clm_http_utils_span){line + start, end - start};
    return http_utils_is_field_text(header->value) ? HTTP_UTILS_OK : HTTP_UTILS_FAIL;
}

static clm_int http_utils_parse(
    const clm_http_utils_kind kind,
    const char* data, const size_t length,
    const bool head_request, const bool connect_request, const bool input_closed,
    const bool headers_only,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    if (out == NULL || (data == NULL && length != 0) || (headers == NULL && header_capacity != 0))
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    clm_http_utils_message message;
    memset(&message, 0, sizeof(message));
    message.kind = kind;
    message.headers = headers;
    message.header_capacity = header_capacity;

    size_t pos = 0;
    if (kind == CLM_HTTP_UTILS_REQUEST)
        while (length - pos >= 2 && data[pos] == '\r' && data[pos + 1] == '\n') pos += 2;
    size_t line_end;
    clm_int status = http_utils_find_line(data, length, pos, &line_end);
    if (status != HTTP_UTILS_OK) return status;
    status = kind == CLM_HTTP_UTILS_REQUEST
        ? http_utils_parse_request_line(data + pos, line_end - pos, &message)
        : http_utils_parse_status_line(data + pos, line_end - pos, &message);
    if (status != HTTP_UTILS_OK) return status;
    pos = line_end + 2;

    for (;;) {
        status = http_utils_find_line(data, length, pos, &line_end);
        if (status != HTTP_UTILS_OK) return status;
        if (line_end == pos) {
            pos += 2;
            break;
        }
        if (message.header_count == header_capacity) return CLM_HTTP_UTILS_TOO_MANY_HEADERS;
        status = http_utils_parse_header_line(data + pos, line_end - pos, &headers[message.header_count]);
        if (status != HTTP_UTILS_OK) return status;
        ++message.header_count;
        pos = line_end + 2;
    }

    http_utils_framing framing;
    /* RFC 9112: a 2xx CONNECT response establishes the tunnel at the empty
     * header line. Ignore Content-Length and Transfer-Encoding in that case.
     */
    const bool tunnel_established = kind == CLM_HTTP_UTILS_RESPONSE &&
        connect_request && message.status >= 200 && message.status < 300;
    if (tunnel_established) {
        memset(&framing, 0, sizeof(framing));
    } else if (http_utils_scan_framing(headers, message.header_count, &framing) != HTTP_UTILS_OK) {
        return HTTP_UTILS_FAIL;
    }
    if (kind == CLM_HTTP_UTILS_REQUEST &&
        message.version_major == 1 && message.version_minor == 1 &&
        framing.host_count != 1)
        return HTTP_UTILS_FAIL;
    if (framing.has_encoding && message.version_minor == 0)
        return HTTP_UTILS_FAIL;

    const size_t available = length - pos;
    clm_http_utils_body_kind body_kind;
    if (kind == CLM_HTTP_UTILS_REQUEST) {
        if (framing.has_encoding) {
            if (!framing.encoding_chunked || framing.has_length) return HTTP_UTILS_FAIL;
            body_kind = CLM_HTTP_UTILS_BODY_CHUNKED;
        } else {
            body_kind = framing.has_length ? CLM_HTTP_UTILS_BODY_LENGTH : CLM_HTTP_UTILS_BODY_NONE;
        }
    } else if (head_request ||
               tunnel_established || message.status < 200 ||
               message.status == 204 || message.status == 304) {
        body_kind = CLM_HTTP_UTILS_BODY_NONE;
    } else if (framing.has_encoding) {
        if (framing.has_length) return HTTP_UTILS_FAIL;
        body_kind = framing.encoding_chunked ? CLM_HTTP_UTILS_BODY_CHUNKED : CLM_HTTP_UTILS_BODY_UNTIL_CLOSE;
    } else {
        body_kind = framing.has_length ? CLM_HTTP_UTILS_BODY_LENGTH : CLM_HTTP_UTILS_BODY_UNTIL_CLOSE;
    }

    if (headers_only) {
        message.body_kind = body_kind;
        message.body = (clm_http_utils_span){data + pos, 0};
        message.consumed = pos;
        *out = message;
        return HTTP_UTILS_OK;
    }

    size_t body_length = 0;
    if (body_kind == CLM_HTTP_UTILS_BODY_LENGTH) {
        if (framing.length > available)
            return input_closed ? HTTP_UTILS_FAIL : HTTP_UTILS_INCOMPLETE;
        body_length = framing.length;
    } else if (body_kind == CLM_HTTP_UTILS_BODY_CHUNKED) {
        status = http_utils_chunked(data + pos, available, NULL, 0, NULL, &body_length);
        if (status != HTTP_UTILS_OK)
            return input_closed && status == HTTP_UTILS_INCOMPLETE ? HTTP_UTILS_FAIL : status;
    } else if (body_kind == CLM_HTTP_UTILS_BODY_UNTIL_CLOSE) {
        if (!input_closed) return HTTP_UTILS_INCOMPLETE;
        body_length = available;
    }
    message.body_kind = body_kind;
    message.body = (clm_http_utils_span){data + pos, body_length};
    message.consumed = pos + body_length;
    *out = message;
    return HTTP_UTILS_OK;
}

clm_int clm_http_utils_parse_request(
    const char* data, const size_t length,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    return http_utils_parse(CLM_HTTP_UTILS_REQUEST, data, length,
        false, false, false, false, headers, header_capacity, out);
}

clm_int clm_http_utils_parse_request_headers(
    const char* data, const size_t length,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    return http_utils_parse(CLM_HTTP_UTILS_REQUEST, data, length,
        false, false, false, true, headers, header_capacity, out);
}

clm_int clm_http_utils_parse_response_ex(
    const char* data, const size_t length,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    const clm_int status = http_utils_parse(CLM_HTTP_UTILS_RESPONSE, data, length,
        head_request, connect_request, input_closed, false, headers, header_capacity, out);
    return input_closed && status == HTTP_UTILS_INCOMPLETE ? HTTP_UTILS_FAIL : status;
}

clm_int clm_http_utils_parse_response(
    const char* data, const size_t length, const bool head_request,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    return clm_http_utils_parse_response_ex(data, length,
        head_request, false, false, headers, header_capacity, out);
}

static clm_int http_utils_loads(
    const clm_http_utils_kind kind, const clm_id source_id,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(source_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        (block->length != 0 && block->data == NULL))
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;

    return http_utils_parse(kind, (const char*)block->data, block->length,
        head_request, connect_request, input_closed, false, headers, header_capacity, out);
}

clm_int clm_http_utils_loads_request(
    const clm_id source_id,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    return http_utils_loads(CLM_HTTP_UTILS_REQUEST, source_id,
        false, false, false, headers, header_capacity, out);
}

clm_int clm_http_utils_loads_response_ex(
    const clm_id source_id,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    const clm_int status = http_utils_loads(CLM_HTTP_UTILS_RESPONSE, source_id,
        head_request, connect_request, input_closed, headers, header_capacity, out);
    return input_closed && status == HTTP_UTILS_INCOMPLETE ? HTTP_UTILS_FAIL : status;
}

clm_int clm_http_utils_loads_response(
    const clm_id source_id, const bool head_request,
    clm_http_utils_header* headers, const size_t header_capacity,
    clm_http_utils_message* out)
{
    return clm_http_utils_loads_response_ex(source_id,
        head_request, false, false, headers, header_capacity, out);
}

/* Output sink: with dest == NULL it only counts, so one emit pass can measure. */
typedef struct {
    char* dest;
    size_t capacity;
    size_t length;
    bool overflow;
} http_utils_sink;

static void http_utils_put(http_utils_sink* sink, const char* text, const size_t length)
{
    if (length > SIZE_MAX - sink->length) {
        sink->overflow = true;
        return;
    }
    if (sink->dest != NULL && sink->length + length <= sink->capacity && length != 0)
        memcpy(sink->dest + sink->length, text, length);
    sink->length += length;
}

static void http_utils_put_span(http_utils_sink* sink, const clm_http_utils_span span)
{
    http_utils_put(sink, span.data, span.length);
}

static void http_utils_put_cstr(http_utils_sink* sink, const char* text)
{
    http_utils_put(sink, text, strlen(text));
}

static clm_int http_utils_emit(const clm_http_utils_message* message, http_utils_sink* sink)
{
    if (message->version_major != 1 || message->version_minor > 1 ||
        message->header_count > message->header_capacity ||
        (message->header_count != 0 && message->headers == NULL) || !http_utils_span_ok(message->body))
        return HTTP_UTILS_FAIL;
    char version[10];
    snprintf(version, sizeof(version), "HTTP/1.%u", (unsigned)message->version_minor);

    if (message->kind == CLM_HTTP_UTILS_REQUEST) {
        if (!http_utils_is_token(message->method) || !http_utils_is_target(message->target)) return HTTP_UTILS_FAIL;
        http_utils_put_span(sink, message->method);
        http_utils_put_cstr(sink, " ");
        http_utils_put_span(sink, message->target);
        http_utils_put_cstr(sink, " ");
        http_utils_put_cstr(sink, version);
    } else if (message->kind == CLM_HTTP_UTILS_RESPONSE) {
        if (message->status < 100 || message->status > 599 || !http_utils_is_field_text(message->reason))
            return HTTP_UTILS_FAIL;
        char code[8];
        snprintf(code, sizeof(code), " %u ", (unsigned)message->status);
        http_utils_put_cstr(sink, version);
        http_utils_put_cstr(sink, code);
        http_utils_put_span(sink, message->reason);
    } else {
        return HTTP_UTILS_FAIL;
    }
    http_utils_put_cstr(sink, "\r\n");

    for (size_t i = 0; i < message->header_count; ++i) {
        const clm_http_utils_header* header = &message->headers[i];
        if (!http_utils_is_token(header->name) || !http_utils_is_field_text(header->value)) return HTTP_UTILS_FAIL;
        http_utils_put_span(sink, header->name);
        http_utils_put_cstr(sink, ": ");
        http_utils_put_span(sink, header->value);
        http_utils_put_cstr(sink, "\r\n");
    }

    http_utils_framing framing;
    if (http_utils_scan_framing(message->headers, message->header_count, &framing) != HTTP_UTILS_OK)
        return HTTP_UTILS_FAIL;
    if (message->kind == CLM_HTTP_UTILS_REQUEST && message->version_minor == 1 &&
        framing.host_count != 1)
        return HTTP_UTILS_FAIL;
    if (framing.has_encoding && message->version_minor == 0)
        return HTTP_UTILS_FAIL;
    if (message->version_minor == 0 &&
        message->body_kind == CLM_HTTP_UTILS_BODY_CHUNKED)
        return HTTP_UTILS_FAIL;

    /* These responses end after the header section, not after content bytes. */
    const bool status_has_no_body = message->kind == CLM_HTTP_UTILS_RESPONSE &&
        (message->status < 200 || message->status == 204 || message->status == 304);
    if (status_has_no_body && message->body_kind != CLM_HTTP_UTILS_BODY_NONE)
        return HTTP_UTILS_FAIL;
    if (message->kind == CLM_HTTP_UTILS_RESPONSE &&
        (message->status < 200 || message->status == 204) && framing.has_length)
        return HTTP_UTILS_FAIL;

    switch (message->body_kind) {
    case CLM_HTTP_UTILS_BODY_NONE:
        if (message->body.length != 0 || framing.has_encoding ||
            (message->kind == CLM_HTTP_UTILS_REQUEST &&
                framing.has_length && framing.length != 0))
            return HTTP_UTILS_FAIL;
        break;
    case CLM_HTTP_UTILS_BODY_LENGTH:
        if (framing.has_encoding || (framing.has_length && framing.length != message->body.length))
            return HTTP_UTILS_FAIL;
        if (!framing.has_length) {
            char field[40];
            snprintf(field, sizeof(field), "Content-Length: %llu\r\n", (unsigned long long)message->body.length);
            http_utils_put_cstr(sink, field);
        }
        break;
    case CLM_HTTP_UTILS_BODY_CHUNKED: {
        size_t consumed = 0;
        if (framing.has_length || (framing.has_encoding && !framing.encoding_chunked) ||
            http_utils_chunked(message->body.data, message->body.length, NULL, 0, NULL, &consumed) != HTTP_UTILS_OK ||
            consumed != message->body.length)
            return HTTP_UTILS_FAIL;
        if (!framing.has_encoding) http_utils_put_cstr(sink, "Transfer-Encoding: chunked\r\n");
        break;
    }
    case CLM_HTTP_UTILS_BODY_UNTIL_CLOSE:
        if (message->kind != CLM_HTTP_UTILS_RESPONSE ||
            framing.has_length || framing.has_encoding)
            return HTTP_UTILS_FAIL;
        break;
    default:
        return HTTP_UTILS_FAIL;
    }
    http_utils_put_cstr(sink, "\r\n");
    http_utils_put_span(sink, message->body);
    http_utils_put(sink, "", 1);
    return sink->overflow ? HTTP_UTILS_FAIL : HTTP_UTILS_OK;
}

clm_int clm_http_utils_serialize(
    const clm_http_utils_message* message, char* dest, const size_t capacity, size_t* required)
{
    if (message == NULL || required == NULL || (dest == NULL && capacity != 0))
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    http_utils_sink measure = {NULL, 0, 0, false};
    const clm_int status = http_utils_emit(message, &measure);
    if (status != HTTP_UTILS_OK) return status;
    *required = measure.length;
    if (dest == NULL) return HTTP_UTILS_OK;
    if (capacity < measure.length) return CLM_HTTP_UTILS_BUFFER_TOO_SMALL;
    http_utils_sink write = {dest, capacity, 0, false};
    return http_utils_emit(message, &write);
}

clm_int clm_http_utils_dumps(const clm_http_utils_message* message, const clm_id destination_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination_id);
    if (message == NULL || block == NULL || block->type != CLM_BPA_TYPE_STRING)
        return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    size_t required = 0;
    clm_int status = clm_http_utils_serialize(message, NULL, 0, &required);
    if (status != HTTP_UTILS_OK) return status;
    char* text = (char*)malloc(required);
    if (text == NULL) return HTTP_UTILS_FAIL;
    status = clm_http_utils_serialize(message, text, required, &required);
    if (status == HTTP_UTILS_OK && (memchr(text, '\0', required - 1) != NULL ||
        clm_string_set(destination_id, text) == CLM_BPA_FAILURE))
        status = HTTP_UTILS_FAIL;
    free(text);
    return status;
}

bool clm_http_utils_is_token(const char* name)
{
    return name != NULL &&
        http_utils_is_token((clm_http_utils_span){name, strlen(name)});
}

static bool http_utils_is_blank(const char c)
{
    return c == ' ' || c == '\t';
}

bool clm_http_utils_header_has_token(const clm_http_utils_span value, const char* token)
{
    if (token == NULL || !http_utils_span_ok(value)) return false;
    size_t index = 0;
    while (index < value.length) {
        while (index < value.length && (http_utils_is_blank(value.data[index]) || value.data[index] == ','))
            ++index;
        const size_t begin = index;
        while (index < value.length && value.data[index] != ',') ++index;
        size_t end = index;
        while (end > begin && http_utils_is_blank(value.data[end - 1])) --end;
        size_t name_end = begin;
        while (name_end < end && value.data[name_end] != ';') ++name_end;
        while (name_end > begin && http_utils_is_blank(value.data[name_end - 1])) --name_end;
        const clm_http_utils_span name = {value.data + begin, name_end - begin};
        if (!clm_http_utils_span_equals_ignore_case(name, token)) continue;

        /* A matching token still counts as absent when its quality value is zero. */
        size_t param = name_end;
        while (param < end) {
            if (value.data[param++] != ';') break;
            while (param < end && http_utils_is_blank(value.data[param])) ++param;
            if (param + 2 <= end && http_utils_lower((unsigned char)value.data[param]) == 'q' &&
                value.data[param + 1] == '=') {
                param += 2;
                while (param < end && http_utils_is_blank(value.data[param])) ++param;
                bool nonzero = false;
                while (param < end && value.data[param] != ';') {
                    if (value.data[param] >= '1' && value.data[param] <= '9') nonzero = true;
                    ++param;
                }
                if (!nonzero) return false;
            }
            while (param < end && value.data[param] != ';') ++param;
        }
        return true;
    }
    return false;
}

const char* clm_http_utils_reason_phrase(const clm_uint status)
{
    switch (status) {
    case 100: return "Continue";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 413: return "Content Too Large";
    case 415: return "Unsupported Media Type";
    case 417: return "Expectation Failed";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default: return "Status";
    }
}

bool clm_http_utils_format_date(char* out, const size_t capacity)
{
    if (out == NULL || capacity == 0) return false;
    const time_t now = time(NULL);
    struct tm value;
#ifdef _WIN32
    if (gmtime_s(&value, &now) != 0) return false;
#else
    if (!gmtime_r(&now, &value)) return false;
#endif
    return strftime(out, capacity, "%a, %d %b %Y %H:%M:%S GMT", &value) != 0;
}

clm_int clm_http_utils_parse_chunk_size(const char* line, const size_t length, size_t* out_size)
{
    if (line == NULL || out_size == NULL) return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    size_t size = 0;
    size_t digits = 0;
    for (; digits < length && line[digits] != ';' && !http_utils_is_blank(line[digits]); ++digits) {
        const int value = http_utils_hex(line[digits]);
        if (value < 0) return HTTP_UTILS_FAIL;
        if (size > (SIZE_MAX >> 4)) return CLM_HTTP_UTILS_LIMIT_EXCEEDED;
        size = (size << 4) | (size_t)value;
    }
    if (!digits) return HTTP_UTILS_FAIL;
    if (!http_utils_chunk_extensions(line, digits, length)) return HTTP_UTILS_FAIL;
    *out_size = size;
    return HTTP_UTILS_OK;
}

clm_int clm_http_utils_validate_trailer(const char* line, const size_t length)
{
    if (line == NULL) return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    clm_http_utils_header trailer;
    if (http_utils_parse_header_line(line, length, &trailer) != HTTP_UTILS_OK ||
        http_utils_forbidden_trailer(trailer.name))
        return HTTP_UTILS_FAIL;
    return HTTP_UTILS_OK;
}

clm_int clm_http_utils_content_length(const clm_http_utils_message* message, size_t* out_length)
{
    if (message == NULL || out_length == NULL) return CLM_HTTP_UTILS_INVALID_ARGUMENT;
    clm_http_utils_span value;
    if (!clm_http_utils_find_header(message, "Content-Length", &value)) {
        *out_length = 0;
        return HTTP_UTILS_OK;
    }
    if (!value.length) return HTTP_UTILS_FAIL;
    size_t length = 0;
    for (size_t i = 0; i < value.length; ++i) {
        if (!http_utils_is_digit(value.data[i])) return HTTP_UTILS_FAIL;
        const size_t digit = (size_t)(value.data[i] - '0');
        if (length > (SIZE_MAX - digit) / 10) return CLM_HTTP_UTILS_LIMIT_EXCEEDED;
        length = length * 10 + digit;
    }
    *out_length = length;
    return HTTP_UTILS_OK;
}

bool clm_http_utils_expects_continue(const clm_http_utils_message* message)
{
    clm_http_utils_span expect;
    return message != NULL && message->version_major == 1 && message->version_minor >= 1 &&
        clm_http_utils_find_header(message, "Expect", &expect) &&
        clm_http_utils_header_has_token(expect, "100-continue");
}

bool clm_http_utils_keep_alive(const clm_http_utils_message* message)
{
    if (message == NULL) return false;
    clm_http_utils_span connection;
    if (clm_http_utils_find_header(message, "Connection", &connection)) {
        if (clm_http_utils_header_has_token(connection, "close")) return false;
        if (message->version_minor == 0)
            return clm_http_utils_header_has_token(connection, "keep-alive");
    }
    return message->version_major == 1 && message->version_minor >= 1;
}

clm_http_utils_span clm_http_utils_path_from_target(const clm_http_utils_span target)
{
    size_t length = 0;
    while (length < target.length && target.data[length] != '?' && target.data[length] != '#')
        ++length;
    return (clm_http_utils_span){target.data, length};
}
