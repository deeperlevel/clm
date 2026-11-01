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

#include "string_httpp.h"
#include "bpa.h"
#include "string.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTPP_OK CLM_STRING_HTTPP_SUCCESS
#define HTTPP_INCOMPLETE CLM_STRING_HTTPP_INCOMPLETE
#define HTTPP_FAIL CLM_STRING_HTTPP_FAILURE

static unsigned char httpp_lower(const unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c;
}

static bool httpp_is_digit(const char c)
{
    return c >= '0' && c <= '9';
}

static int httpp_hex(const char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool httpp_is_token_char(const unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c != 0 && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}

static bool httpp_span_ok(const clm_string_httpp_span span)
{
    return span.length == 0 || span.data != NULL;
}

static bool httpp_is_token(const clm_string_httpp_span span)
{
    if (span.length == 0 || span.data == NULL) return false;
    for (size_t i = 0; i < span.length; ++i)
        if (!httpp_is_token_char((unsigned char)span.data[i])) return false;
    return true;
}

/* Field values and reasons: HTAB, SP, VCHAR and obs-text; no other controls. */
static bool httpp_is_field_text(const clm_string_httpp_span span)
{
    if (!httpp_span_ok(span)) return false;
    for (size_t i = 0; i < span.length; ++i) {
        const unsigned char c = (unsigned char)span.data[i];
        if (c != '\t' && (c < 0x20 || c == 0x7f)) return false;
    }
    return true;
}

static bool httpp_is_target(const clm_string_httpp_span span)
{
    if (span.length == 0 || span.data == NULL) return false;
    for (size_t i = 0; i < span.length; ++i) {
        const unsigned char c = (unsigned char)span.data[i];
        if (c <= 0x20 || c == 0x7f) return false;
    }
    return true;
}

bool clm_string_httpp_span_equals(const clm_string_httpp_span span, const char* text)
{
    if (text == NULL) return false;
    const size_t length = strlen(text);
    return span.length == length &&
        (length == 0 || (span.data != NULL && memcmp(span.data, text, length) == 0));
}

bool clm_string_httpp_span_equals_ignore_case(const clm_string_httpp_span span, const char* text)
{
    if (text == NULL) return false;
    const size_t length = strlen(text);
    if (span.length != length || (length != 0 && span.data == NULL)) return false;
    for (size_t i = 0; i < length; ++i)
        if (httpp_lower((unsigned char)span.data[i]) != httpp_lower((unsigned char)text[i]))
            return false;
    return true;
}

bool clm_string_httpp_find_header(
    const clm_string_httpp_message* message, const char* name, clm_string_httpp_span* value)
{
    if (message == NULL || name == NULL || value == NULL ||
        (message->header_count != 0 && message->headers == NULL))
        return false;
    for (size_t i = 0; i < message->header_count; ++i) {
        if (clm_string_httpp_span_equals_ignore_case(message->headers[i].name, name)) {
            *value = message->headers[i].value;
            return true;
        }
    }
    return false;
}

/* Finds the CRLF ending the line at pos; bare CR or LF is malformed. */
static clm_int httpp_find_line(
    const char* data, const size_t length, const size_t pos, size_t* line_end)
{
    for (size_t i = pos; i < length; ++i) {
        if (data[i] == '\n') return HTTPP_FAIL;
        if (data[i] == '\r') {
            if (i + 1 >= length) return HTTPP_INCOMPLETE;
            if (data[i + 1] != '\n') return HTTPP_FAIL;
            *line_end = i;
            return HTTPP_OK;
        }
    }
    return HTTPP_INCOMPLETE;
}

static bool httpp_parse_version(
    const char* text, const size_t length, clm_uint* major, clm_uint* minor)
{
    if (length != 8 || memcmp(text, "HTTP/", 5) != 0 || !httpp_is_digit(text[5]) ||
        text[6] != '.' || !httpp_is_digit(text[7]))
        return false;
    *major = (clm_uint)(text[5] - '0');
    *minor = (clm_uint)(text[7] - '0');
    return *major == 1 && *minor <= 1;
}

static bool httpp_parse_uint(const clm_string_httpp_span span, size_t* out)
{
    if (span.length == 0) return false;
    size_t value = 0;
    for (size_t i = 0; i < span.length; ++i) {
        if (!httpp_is_digit(span.data[i])) return false;
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
static bool httpp_is_chunked_transfer_encoding(const clm_string_httpp_span span)
{
    return clm_string_httpp_span_equals_ignore_case(span, "chunked");
}

typedef struct {
    bool has_length;
    size_t length;
    bool has_encoding;
    bool encoding_chunked;
    size_t host_count;
} httpp_framing;

/* A conservative Host check. HTTP authority validation can be strengthened in
 * the server once request-target and virtual-host rules are applied.
 */
static bool httpp_valid_host(const clm_string_httpp_span host)
{
    if (host.length == 0 || host.data == NULL) return false;

    for (size_t i = 0; i < host.length; ++i) {
        const unsigned char c = (unsigned char)host.data[i];
        if (c <= 0x20 || c >= 0x7f || c == ',' || c == '/' || c == '@' || c == '\\')
            return false;
    }
    return true;
}

static clm_int httpp_scan_framing(
    const clm_string_httpp_header* headers, const size_t count, httpp_framing* framing)
{
    memset(framing, 0, sizeof(*framing));

    for (size_t i = 0; i < count; ++i) {
        const clm_string_httpp_header* header = &headers[i];

        if (clm_string_httpp_span_equals_ignore_case(header->name, "Content-Length")) {
            size_t value;
            /* Reject duplicate fields, including matching duplicates. */
            if (framing->has_length || !httpp_parse_uint(header->value, &value))
                return HTTPP_FAIL;
            framing->has_length = true;
            framing->length = value;
        } else if (clm_string_httpp_span_equals_ignore_case(header->name, "Transfer-Encoding")) {
            if (framing->has_encoding ||
                !httpp_is_chunked_transfer_encoding(header->value))
                return HTTPP_FAIL;
            framing->has_encoding = true;
            framing->encoding_chunked = true;
        } else if (clm_string_httpp_span_equals_ignore_case(header->name, "Host")) {
            ++framing->host_count;
            if (framing->host_count != 1 || !httpp_valid_host(header->value))
                return HTTPP_FAIL;
        }
    }

    if (framing->has_length && framing->has_encoding)
        return HTTPP_FAIL;
    return HTTPP_OK;
}

/* Header syntax is also used to validate (discarded) chunked trailers. */
static clm_int httpp_parse_header_line(
    const char* line, size_t length, clm_string_httpp_header* header);

/* Parse the optional chunk extension list. Values may be tokens or quoted
 * strings, with ordinary quoted-pair escaping. No control bytes are allowed.
 */
static bool httpp_chunk_extensions(const char* line, size_t pos, size_t end)
{
    while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

    while (pos < end) {
        if (line[pos++] != ';') return false;
        while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

        const size_t name_start = pos;
        while (pos < end && httpp_is_token_char((unsigned char)line[pos])) ++pos;
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
                while (pos < end && httpp_is_token_char((unsigned char)line[pos])) ++pos;
                if (pos == value_start) return false;
            }
            while (pos < end && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        }
    }
    return true;
}

static bool httpp_forbidden_trailer(const clm_string_httpp_span name)
{
    static const char* forbidden[] = {
        "Content-Length", "Transfer-Encoding", "Host", "Connection",
        "TE", "Trailer", "Upgrade", "Expect", "Authorization",
        "Proxy-Authorization", "Content-Encoding", "Content-Type",
        "Content-Range", "Content-Location"
    };

    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        if (clm_string_httpp_span_equals_ignore_case(name, forbidden[i]))
            return true;
    }
    return false;
}

/* Walks a chunked body; with dest == NULL it only validates and measures. */
static clm_int httpp_chunked(
    const char* data, const size_t length, char* dest, const size_t capacity,
    size_t* out_length, size_t* out_consumed)
{
    size_t pos = 0;
    size_t decoded = 0;
    bool too_small = false;
    for (;;) {
        size_t line_end;
        clm_int status = httpp_find_line(data, length, pos, &line_end);
        if (status != HTTPP_OK) return status;
        size_t size = 0;
        size_t index = pos;
        while (index < line_end && httpp_hex(data[index]) >= 0) {
            if (size > (SIZE_MAX >> 4)) return HTTPP_FAIL;
            size = (size << 4) | (size_t)httpp_hex(data[index]);
            ++index;
        }
        if (index == pos || !httpp_chunk_extensions(data, index, line_end))
            return HTTPP_FAIL;
        pos = line_end + 2;
        if (size == 0) {
            for (;;) {
                status = httpp_find_line(data, length, pos, &line_end);
                if (status != HTTPP_OK) return status;
                const bool last = line_end == pos;
                if (!last) {
                    clm_string_httpp_header trailer;
                    if (httpp_parse_header_line(data + pos, line_end - pos, &trailer) != HTTPP_OK ||
                        httpp_forbidden_trailer(trailer.name))
                        return HTTPP_FAIL;
                }
                pos = line_end + 2;
                if (last) break;
            }
            break;
        }
        if (size > length - pos || length - pos - size < 2) return HTTPP_INCOMPLETE;
        if (data[pos + size] != '\r' || data[pos + size + 1] != '\n') return HTTPP_FAIL;
        if (size > SIZE_MAX - decoded) return HTTPP_FAIL;
        if (dest != NULL) {
            if (decoded > capacity || size > capacity - decoded) too_small = true;
            else memcpy(dest + decoded, data + pos, size);
        }
        decoded += size;
        pos += size + 2;
    }
    if (out_length != NULL) *out_length = decoded;
    if (out_consumed != NULL) *out_consumed = pos;
    return too_small ? CLM_STRING_HTTPP_BUFFER_TOO_SMALL : HTTPP_OK;
}

clm_int clm_string_httpp_decode_chunked(
    const char* data, const size_t length, char* dest, const size_t capacity,
    size_t* out_length, size_t* out_consumed)
{
    if ((data == NULL && length != 0) || (dest == NULL && capacity != 0))
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;
    /* Validate and measure first so BUFFER_TOO_SMALL leaves dest unchanged. */
    size_t decoded_length = 0;
    size_t consumed = 0;
    clm_int status = httpp_chunked(data, length, NULL, 0,
        &decoded_length, &consumed);
    if (status != HTTPP_OK) return status;

    if (out_length != NULL) *out_length = decoded_length;
    if (out_consumed != NULL) *out_consumed = consumed;
    if (dest == NULL) return HTTPP_OK;
    if (capacity < decoded_length) return CLM_STRING_HTTPP_BUFFER_TOO_SMALL;

    return httpp_chunked(data, length, dest, capacity, NULL, NULL);
}

clm_int clm_string_httpp_encode_chunked(
    const void* data, const size_t length, char* dest, const size_t capacity, size_t* required)
{
    if (required == NULL || (data == NULL && length != 0) || (dest == NULL && capacity != 0))
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;
    static const char terminator[] = "0\r\n\r\n";
    char head[2 * sizeof(unsigned long long) + 3];
    int head_length = 0;
    size_t total = sizeof(terminator) - 1;
    if (length != 0) {
        head_length = snprintf(head, sizeof(head), "%llx\r\n", (unsigned long long)length);
        if (head_length < 0 || length > SIZE_MAX - (size_t)head_length - total - 2)
            return CLM_STRING_HTTPP_INVALID_ARGUMENT;
        total += (size_t)head_length + length + 2;
    }
    *required = total;
    if (dest == NULL) return HTTPP_OK;
    if (capacity < total) return CLM_STRING_HTTPP_BUFFER_TOO_SMALL;
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
    return HTTPP_OK;
}

clm_int clm_string_httpp_message_init(
    clm_string_httpp_message* message, const clm_string_httpp_kind kind)
{
    if (message == NULL || (kind != CLM_STRING_HTTPP_REQUEST && kind != CLM_STRING_HTTPP_RESPONSE))
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;
    memset(message, 0, sizeof(*message));
    message->kind = kind;
    message->version_major = 1;
    message->version_minor = 1;
    if (kind == CLM_STRING_HTTPP_RESPONSE) {
        message->status = 200;
        /* An empty 200 response must still be unambiguously framed. */
        message->body_kind = CLM_STRING_HTTPP_BODY_LENGTH;
    }
    return HTTPP_OK;
}

static clm_int httpp_parse_request_line(
    const char* line, const size_t length, clm_string_httpp_message* message)
{
    const char* first = memchr(line, ' ', length);
    if (first == NULL) return HTTPP_FAIL;
    const size_t method_length = (size_t)(first - line);
    const char* target = first + 1;
    const size_t rest = length - method_length - 1;
    const char* second = memchr(target, ' ', rest);
    if (second == NULL) return HTTPP_FAIL;
    const size_t target_length = (size_t)(second - target);
    message->method = (clm_string_httpp_span){line, method_length};
    message->target = (clm_string_httpp_span){target, target_length};
    if (!httpp_is_token(message->method) || !httpp_is_target(message->target) ||
        !httpp_parse_version(second + 1, rest - target_length - 1,
            &message->version_major, &message->version_minor))
        return HTTPP_FAIL;
    return HTTPP_OK;
}

static clm_int httpp_parse_status_line(
    const char* line, const size_t length, clm_string_httpp_message* message)
{
    if (length < 12 || line[8] != ' ' || !httpp_parse_version(line, 8,
            &message->version_major, &message->version_minor) ||
        !httpp_is_digit(line[9]) || !httpp_is_digit(line[10]) || !httpp_is_digit(line[11]))
        return HTTPP_FAIL;
    message->status = (clm_uint)((line[9] - '0') * 100 + (line[10] - '0') * 10 + (line[11] - '0'));
    if (message->status < 100) return HTTPP_FAIL;
    if (length == 12) return HTTPP_OK;
    if (line[12] != ' ') return HTTPP_FAIL;
    message->reason = (clm_string_httpp_span){line + 13, length - 13};
    return httpp_is_field_text(message->reason) ? HTTPP_OK : HTTPP_FAIL;
}

static clm_int httpp_parse_header_line(
    const char* line, const size_t length, clm_string_httpp_header* header)
{
    const char* colon = memchr(line, ':', length);
    if (colon == NULL || line[0] == ' ' || line[0] == '\t') return HTTPP_FAIL;
    header->name = (clm_string_httpp_span){line, (size_t)(colon - line)};
    if (!httpp_is_token(header->name)) return HTTPP_FAIL;
    size_t start = (size_t)(colon - line) + 1;
    size_t end = length;
    while (start < end && (line[start] == ' ' || line[start] == '\t')) ++start;
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) --end;
    header->value = (clm_string_httpp_span){line + start, end - start};
    return httpp_is_field_text(header->value) ? HTTPP_OK : HTTPP_FAIL;
}

static clm_int httpp_parse(
    const clm_string_httpp_kind kind,
    const char* data, const size_t length,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    if (out == NULL || (data == NULL && length != 0) || (headers == NULL && header_capacity != 0))
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;
    clm_string_httpp_message message;
    memset(&message, 0, sizeof(message));
    message.kind = kind;
    message.headers = headers;
    message.header_capacity = header_capacity;

    size_t pos = 0;
    if (kind == CLM_STRING_HTTPP_REQUEST)
        while (length - pos >= 2 && data[pos] == '\r' && data[pos + 1] == '\n') pos += 2;
    size_t line_end;
    clm_int status = httpp_find_line(data, length, pos, &line_end);
    if (status != HTTPP_OK) return status;
    status = kind == CLM_STRING_HTTPP_REQUEST
        ? httpp_parse_request_line(data + pos, line_end - pos, &message)
        : httpp_parse_status_line(data + pos, line_end - pos, &message);
    if (status != HTTPP_OK) return status;
    pos = line_end + 2;

    for (;;) {
        status = httpp_find_line(data, length, pos, &line_end);
        if (status != HTTPP_OK) return status;
        if (line_end == pos) {
            pos += 2;
            break;
        }
        if (message.header_count == header_capacity) return CLM_STRING_HTTPP_TOO_MANY_HEADERS;
        status = httpp_parse_header_line(data + pos, line_end - pos, &headers[message.header_count]);
        if (status != HTTPP_OK) return status;
        ++message.header_count;
        pos = line_end + 2;
    }

    httpp_framing framing;
    /* RFC 9112: a 2xx CONNECT response establishes the tunnel at the empty
     * header line. Ignore Content-Length and Transfer-Encoding in that case.
     */
    const bool tunnel_established = kind == CLM_STRING_HTTPP_RESPONSE &&
        connect_request && message.status >= 200 && message.status < 300;
    if (tunnel_established) {
        memset(&framing, 0, sizeof(framing));
    } else if (httpp_scan_framing(headers, message.header_count, &framing) != HTTPP_OK) {
        return HTTPP_FAIL;
    }
    if (kind == CLM_STRING_HTTPP_REQUEST &&
        message.version_major == 1 && message.version_minor == 1 &&
        framing.host_count != 1)
        return HTTPP_FAIL;
    if (framing.has_encoding && message.version_minor == 0)
        return HTTPP_FAIL;

    const size_t available = length - pos;
    clm_string_httpp_body_kind body_kind;
    if (kind == CLM_STRING_HTTPP_REQUEST) {
        if (framing.has_encoding) {
            if (!framing.encoding_chunked || framing.has_length) return HTTPP_FAIL;
            body_kind = CLM_STRING_HTTPP_BODY_CHUNKED;
        } else {
            body_kind = framing.has_length ? CLM_STRING_HTTPP_BODY_LENGTH : CLM_STRING_HTTPP_BODY_NONE;
        }
    } else if (head_request ||
               tunnel_established || message.status < 200 ||
               message.status == 204 || message.status == 304) {
        body_kind = CLM_STRING_HTTPP_BODY_NONE;
    } else if (framing.has_encoding) {
        if (framing.has_length) return HTTPP_FAIL;
        body_kind = framing.encoding_chunked ? CLM_STRING_HTTPP_BODY_CHUNKED : CLM_STRING_HTTPP_BODY_UNTIL_CLOSE;
    } else {
        body_kind = framing.has_length ? CLM_STRING_HTTPP_BODY_LENGTH : CLM_STRING_HTTPP_BODY_UNTIL_CLOSE;
    }

    size_t body_length = 0;
    if (body_kind == CLM_STRING_HTTPP_BODY_LENGTH) {
        if (framing.length > available)
            return input_closed ? HTTPP_FAIL : HTTPP_INCOMPLETE;
        body_length = framing.length;
    } else if (body_kind == CLM_STRING_HTTPP_BODY_CHUNKED) {
        status = httpp_chunked(data + pos, available, NULL, 0, NULL, &body_length);
        if (status != HTTPP_OK)
            return input_closed && status == HTTPP_INCOMPLETE ? HTTPP_FAIL : status;
    } else if (body_kind == CLM_STRING_HTTPP_BODY_UNTIL_CLOSE) {
        if (!input_closed) return HTTPP_INCOMPLETE;
        body_length = available;
    }
    message.body_kind = body_kind;
    message.body = (clm_string_httpp_span){data + pos, body_length};
    message.consumed = pos + body_length;
    *out = message;
    return HTTPP_OK;
}

clm_int clm_string_httpp_parse_request(
    const char* data, const size_t length,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    return httpp_parse(CLM_STRING_HTTPP_REQUEST, data, length,
        false, false, false, headers, header_capacity, out);
}

clm_int clm_string_httpp_parse_response_ex(
    const char* data, const size_t length,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    const clm_int status = httpp_parse(CLM_STRING_HTTPP_RESPONSE, data, length,
        head_request, connect_request, input_closed, headers, header_capacity, out);
    return input_closed && status == HTTPP_INCOMPLETE ? HTTPP_FAIL : status;
}

clm_int clm_string_httpp_parse_response(
    const char* data, const size_t length, const bool head_request,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    return clm_string_httpp_parse_response_ex(data, length,
        head_request, false, false, headers, header_capacity, out);
}

static clm_int httpp_loads(
    const clm_string_httpp_kind kind, const clm_id source_id,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(source_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        (block->length != 0 && block->data == NULL))
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;

    return httpp_parse(kind, (const char*)block->data, block->length,
        head_request, connect_request, input_closed, headers, header_capacity, out);
}

clm_int clm_string_httpp_loads_request(
    const clm_id source_id,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    return httpp_loads(CLM_STRING_HTTPP_REQUEST, source_id,
        false, false, false, headers, header_capacity, out);
}

clm_int clm_string_httpp_loads_response_ex(
    const clm_id source_id,
    const bool head_request, const bool connect_request, const bool input_closed,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    const clm_int status = httpp_loads(CLM_STRING_HTTPP_RESPONSE, source_id,
        head_request, connect_request, input_closed, headers, header_capacity, out);
    return input_closed && status == HTTPP_INCOMPLETE ? HTTPP_FAIL : status;
}

clm_int clm_string_httpp_loads_response(
    const clm_id source_id, const bool head_request,
    clm_string_httpp_header* headers, const size_t header_capacity,
    clm_string_httpp_message* out)
{
    return clm_string_httpp_loads_response_ex(source_id,
        head_request, false, false, headers, header_capacity, out);
}

/* Output sink: with dest == NULL it only counts, so one emit pass can measure. */
typedef struct {
    char* dest;
    size_t capacity;
    size_t length;
    bool overflow;
} httpp_sink;

static void httpp_put(httpp_sink* sink, const char* text, const size_t length)
{
    if (length > SIZE_MAX - sink->length) {
        sink->overflow = true;
        return;
    }
    if (sink->dest != NULL && sink->length + length <= sink->capacity && length != 0)
        memcpy(sink->dest + sink->length, text, length);
    sink->length += length;
}

static void httpp_put_span(httpp_sink* sink, const clm_string_httpp_span span)
{
    httpp_put(sink, span.data, span.length);
}

static void httpp_put_cstr(httpp_sink* sink, const char* text)
{
    httpp_put(sink, text, strlen(text));
}

static clm_int httpp_emit(const clm_string_httpp_message* message, httpp_sink* sink)
{
    if (message->version_major != 1 || message->version_minor > 1 ||
        message->header_count > message->header_capacity ||
        (message->header_count != 0 && message->headers == NULL) || !httpp_span_ok(message->body))
        return HTTPP_FAIL;
    char version[10];
    snprintf(version, sizeof(version), "HTTP/1.%u", (unsigned)message->version_minor);

    if (message->kind == CLM_STRING_HTTPP_REQUEST) {
        if (!httpp_is_token(message->method) || !httpp_is_target(message->target)) return HTTPP_FAIL;
        httpp_put_span(sink, message->method);
        httpp_put_cstr(sink, " ");
        httpp_put_span(sink, message->target);
        httpp_put_cstr(sink, " ");
        httpp_put_cstr(sink, version);
    } else if (message->kind == CLM_STRING_HTTPP_RESPONSE) {
        if (message->status < 100 || message->status > 599 || !httpp_is_field_text(message->reason))
            return HTTPP_FAIL;
        char code[8];
        snprintf(code, sizeof(code), " %u ", (unsigned)message->status);
        httpp_put_cstr(sink, version);
        httpp_put_cstr(sink, code);
        httpp_put_span(sink, message->reason);
    } else {
        return HTTPP_FAIL;
    }
    httpp_put_cstr(sink, "\r\n");

    for (size_t i = 0; i < message->header_count; ++i) {
        const clm_string_httpp_header* header = &message->headers[i];
        if (!httpp_is_token(header->name) || !httpp_is_field_text(header->value)) return HTTPP_FAIL;
        httpp_put_span(sink, header->name);
        httpp_put_cstr(sink, ": ");
        httpp_put_span(sink, header->value);
        httpp_put_cstr(sink, "\r\n");
    }

    httpp_framing framing;
    if (httpp_scan_framing(message->headers, message->header_count, &framing) != HTTPP_OK)
        return HTTPP_FAIL;
    if (message->kind == CLM_STRING_HTTPP_REQUEST && message->version_minor == 1 &&
        framing.host_count != 1)
        return HTTPP_FAIL;
    if (framing.has_encoding && message->version_minor == 0)
        return HTTPP_FAIL;
    if (message->version_minor == 0 &&
        message->body_kind == CLM_STRING_HTTPP_BODY_CHUNKED)
        return HTTPP_FAIL;

    /* These responses end after the header section, not after content bytes. */
    const bool status_has_no_body = message->kind == CLM_STRING_HTTPP_RESPONSE &&
        (message->status < 200 || message->status == 204 || message->status == 304);
    if (status_has_no_body && message->body_kind != CLM_STRING_HTTPP_BODY_NONE)
        return HTTPP_FAIL;
    if (message->kind == CLM_STRING_HTTPP_RESPONSE &&
        (message->status < 200 || message->status == 204) && framing.has_length)
        return HTTPP_FAIL;

    switch (message->body_kind) {
    case CLM_STRING_HTTPP_BODY_NONE:
        if (message->body.length != 0 || framing.has_encoding ||
            (message->kind == CLM_STRING_HTTPP_REQUEST &&
                framing.has_length && framing.length != 0))
            return HTTPP_FAIL;
        break;
    case CLM_STRING_HTTPP_BODY_LENGTH:
        if (framing.has_encoding || (framing.has_length && framing.length != message->body.length))
            return HTTPP_FAIL;
        if (!framing.has_length) {
            char field[40];
            snprintf(field, sizeof(field), "Content-Length: %llu\r\n", (unsigned long long)message->body.length);
            httpp_put_cstr(sink, field);
        }
        break;
    case CLM_STRING_HTTPP_BODY_CHUNKED: {
        size_t consumed = 0;
        if (framing.has_length || (framing.has_encoding && !framing.encoding_chunked) ||
            httpp_chunked(message->body.data, message->body.length, NULL, 0, NULL, &consumed) != HTTPP_OK ||
            consumed != message->body.length)
            return HTTPP_FAIL;
        if (!framing.has_encoding) httpp_put_cstr(sink, "Transfer-Encoding: chunked\r\n");
        break;
    }
    case CLM_STRING_HTTPP_BODY_UNTIL_CLOSE:
        if (message->kind != CLM_STRING_HTTPP_RESPONSE ||
            framing.has_length || framing.has_encoding)
            return HTTPP_FAIL;
        break;
    default:
        return HTTPP_FAIL;
    }
    httpp_put_cstr(sink, "\r\n");
    httpp_put_span(sink, message->body);
    httpp_put(sink, "", 1);
    return sink->overflow ? HTTPP_FAIL : HTTPP_OK;
}

clm_int clm_string_httpp_serialize(
    const clm_string_httpp_message* message, char* dest, const size_t capacity, size_t* required)
{
    if (message == NULL || required == NULL || (dest == NULL && capacity != 0))
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;
    httpp_sink measure = {NULL, 0, 0, false};
    const clm_int status = httpp_emit(message, &measure);
    if (status != HTTPP_OK) return status;
    *required = measure.length;
    if (dest == NULL) return HTTPP_OK;
    if (capacity < measure.length) return CLM_STRING_HTTPP_BUFFER_TOO_SMALL;
    httpp_sink write = {dest, capacity, 0, false};
    return httpp_emit(message, &write);
}

clm_int clm_string_httpp_dumps(const clm_string_httpp_message* message, const clm_id destination_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination_id);
    if (message == NULL || block == NULL || block->type != CLM_BPA_TYPE_STRING)
        return CLM_STRING_HTTPP_INVALID_ARGUMENT;
    size_t required = 0;
    clm_int status = clm_string_httpp_serialize(message, NULL, 0, &required);
    if (status != HTTPP_OK) return status;
    char* text = (char*)malloc(required);
    if (text == NULL) return HTTPP_FAIL;
    status = clm_string_httpp_serialize(message, text, required, &required);
    if (status == HTTPP_OK && (memchr(text, '\0', required - 1) != NULL ||
        clm_string_set(destination_id, text) == CLM_BPA_FAILURE))
        status = HTTPP_FAIL;
    free(text);
    return status;
}
