#include "multipart.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

static bool span_valid(clm_multipart_span span)
{
    return span.data != NULL || span.length == 0;
}

static unsigned char ascii_lower(unsigned char value)
{
    return value >= 'A' && value <= 'Z' ? (unsigned char)(value + ('a' - 'A')) : value;
}

bool clm_multipart_span_equals_ignore_case(clm_multipart_span span, const char* text)
{
    if (!text || (!span.data && span.length))
        return false;
    size_t text_length = strlen(text);
    if (span.length != text_length)
        return false;
    for (size_t i = 0; i < span.length; ++i)
        if (ascii_lower((unsigned char)span.data[i]) != ascii_lower((unsigned char)text[i]))
            return false;
    return true;
}

bool clm_multipart_boundary_is_valid(clm_multipart_span boundary)
{
    if (!boundary.data || boundary.length == 0 || boundary.length > 70 ||
        boundary.data[boundary.length - 1] == ' ')
        return false;
    for (size_t i = 0; i < boundary.length; ++i) {
        unsigned char c = (unsigned char)boundary.data[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || c == '\'' || c == '(' || c == ')' ||
            c == '+' || c == '_' || c == ',' || c == '-' || c == '.' ||
            c == '/' || c == ':' || c == '=' || c == '?' || c == ' ')
            continue;
        return false;
    }
    return true;
}

static bool is_token(unsigned char c)
{
    if (c == 0)
        return false;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z'))
        return true;
    return strchr("!#$%&'*+-.^_`|~", c) != NULL;
}

static bool add_size(size_t* total, size_t amount)
{
    if (amount > SIZE_MAX - *total)
        return false;
    *total += amount;
    return true;
}

static size_t skip_ows(const char* data, size_t length, size_t index)
{
    while (index < length && (data[index] == ' ' || data[index] == '\t'))
        ++index;
    return index;
}

static size_t trim_ows_end(const char* data, size_t begin, size_t end)
{
    while (end > begin && (data[end - 1] == ' ' || data[end - 1] == '\t'))
        --end;
    return end;
}

clm_int clm_multipart_boundary_from_content_type(
    clm_multipart_span content_type, char* dest, size_t capacity, size_t* required)
{
    if (!span_valid(content_type) || (!dest && capacity != 0))
        return CLM_MULTIPART_INVALID_ARGUMENT;

    const char* data = content_type.data ? content_type.data : "";
    size_t length = content_type.length;
    size_t type_begin = skip_ows(data, length, 0);
    size_t semicolon = type_begin;
    while (semicolon < length && data[semicolon] != ';')
        ++semicolon;
    size_t type_end = trim_ows_end(data, type_begin, semicolon);
    if (type_end - type_begin <= 10 ||
        !clm_multipart_span_equals_ignore_case(
            (clm_multipart_span){data + type_begin, 10}, "multipart/"))
        return CLM_MULTIPART_FAILURE;
    for (size_t i = type_begin + 10; i < type_end; ++i)
        if (!is_token((unsigned char)data[i]))
            return CLM_MULTIPART_FAILURE;

    char boundary_value[70];
    size_t boundary_length = 0;
    bool have_boundary = false;
    size_t index = semicolon;
    while (index < length) {
        if (data[index++] != ';')
            return CLM_MULTIPART_FAILURE;
        index = skip_ows(data, length, index);
        size_t name_begin = index;
        while (index < length && is_token((unsigned char)data[index]))
            ++index;
        if (index == name_begin)
            return CLM_MULTIPART_FAILURE;
        size_t name_length = index - name_begin;
        index = skip_ows(data, length, index);
        if (index >= length || data[index++] != '=')
            return CLM_MULTIPART_FAILURE;
        index = skip_ows(data, length, index);

        char value[71];
        size_t value_length = 0;
        if (index < length && data[index] == '"') {
            ++index;
            bool closed = false;
            while (index < length) {
                unsigned char c = (unsigned char)data[index++];
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\\') {
                    if (index == length)
                        return CLM_MULTIPART_FAILURE;
                    c = (unsigned char)data[index++];
                }
                if (c < 0x20 || c == 0x7f || value_length == sizeof(value))
                    return CLM_MULTIPART_FAILURE;
                value[value_length++] = (char)c;
            }
            if (!closed)
                return CLM_MULTIPART_FAILURE;
            index = skip_ows(data, length, index);
            if (index < length && data[index] != ';')
                return CLM_MULTIPART_FAILURE;
        } else {
            size_t value_begin = index;
            while (index < length && data[index] != ';' && data[index] != ' ' && data[index] != '\t')
                ++index;
            value_length = index - value_begin;
            if (value_length > sizeof(value))
                return CLM_MULTIPART_FAILURE;
            memcpy(value, data + value_begin, value_length);
            index = skip_ows(data, length, index);
            if (index < length && data[index] != ';')
                return CLM_MULTIPART_FAILURE;
        }
        if (name_length == 8 &&
            ascii_lower((unsigned char)data[name_begin]) == 'b' &&
            ascii_lower((unsigned char)data[name_begin + 1]) == 'o' &&
            ascii_lower((unsigned char)data[name_begin + 2]) == 'u' &&
            ascii_lower((unsigned char)data[name_begin + 3]) == 'n' &&
            ascii_lower((unsigned char)data[name_begin + 4]) == 'd' &&
            ascii_lower((unsigned char)data[name_begin + 5]) == 'a' &&
            ascii_lower((unsigned char)data[name_begin + 6]) == 'r' &&
            ascii_lower((unsigned char)data[name_begin + 7]) == 'y') {
            if (have_boundary ||
                !clm_multipart_boundary_is_valid((clm_multipart_span){value, value_length}))
                return CLM_MULTIPART_FAILURE;
            boundary_length = value_length;
            memcpy(boundary_value, value, boundary_length);
            have_boundary = true;
        }
    }
    if (!have_boundary)
        return CLM_MULTIPART_FAILURE;
    if (required)
        *required = boundary_length + 1;
    if (!dest)
        return CLM_MULTIPART_SUCCESS;
    if (capacity < boundary_length + 1)
        return CLM_MULTIPART_BUFFER_TOO_SMALL;
    memcpy(dest, boundary_value, boundary_length);
    dest[boundary_length] = '\0';
    return CLM_MULTIPART_SUCCESS;
}

static bool delimiter_at(const char* data, size_t length, size_t index,
                         clm_multipart_span boundary, bool* closing, size_t* after)
{
    const size_t marker_length = boundary.length + 2;
    if (index > length || marker_length > length - index ||
        memcmp(data + index, "--", 2) != 0 ||
        memcmp(data + index + 2, boundary.data, boundary.length) != 0)
        return false;
    size_t suffix = index + marker_length;
    if (suffix + 2 > length)
        return false;
    if (data[suffix] == '\r' && data[suffix + 1] == '\n') {
        *closing = false;
        *after = suffix + 2;
        return true;
    }
    if (data[suffix] == '-' && data[suffix + 1] == '-') {
        *closing = true;
        suffix += 2;
        if (suffix == length) {
            *after = suffix;
            return true;
        }
        if (suffix + 2 <= length && data[suffix] == '\r' && data[suffix + 1] == '\n') {
            *after = suffix + 2;
            return true;
        }
    }
    return false;
}

static clm_int find_delimiter(const char* data, size_t length, size_t start,
                              clm_multipart_span boundary, size_t* found,
                              bool* closing, size_t* after)
{
    const size_t marker_length = boundary.length + 2;
    for (size_t i = start; i + marker_length <= length; ++i) {
        if (i != 0 && (i < 2 || data[i - 2] != '\r' || data[i - 1] != '\n'))
            continue;
        if (delimiter_at(data, length, i, boundary, closing, after)) {
            *found = i;
            return CLM_MULTIPART_SUCCESS;
        }
    }
    return CLM_MULTIPART_INCOMPLETE;
}

static clm_int parse_part_headers(const char* data, size_t length, size_t* cursor,
                                  clm_multipart_header* headers, size_t header_capacity,
                                  size_t* header_count)
{
    size_t count = 0;
    for (;;) {
        size_t line_end = *cursor;
        while (line_end + 1 < length && !(data[line_end] == '\r' && data[line_end + 1] == '\n'))
            ++line_end;
        if (line_end + 1 >= length)
            return CLM_MULTIPART_INCOMPLETE;
        if (line_end == *cursor) {
            *cursor += 2;
            *header_count = count;
            return CLM_MULTIPART_SUCCESS;
        }
        size_t colon = *cursor;
        while (colon < line_end && data[colon] != ':')
            ++colon;
        if (colon == *cursor || colon == line_end)
            return CLM_MULTIPART_FAILURE;
        for (size_t i = *cursor; i < colon; ++i)
            if (!is_token((unsigned char)data[i]))
                return CLM_MULTIPART_FAILURE;
        size_t value_begin = colon + 1;
        while (value_begin < line_end && (data[value_begin] == ' ' || data[value_begin] == '\t'))
            ++value_begin;
        size_t value_end = trim_ows_end(data, value_begin, line_end);
        for (size_t i = value_begin; i < value_end; ++i) {
            unsigned char c = (unsigned char)data[i];
            if ((c < 0x20 && c != '\t') || c == 0x7f)
                return CLM_MULTIPART_FAILURE;
        }
        if (count == header_capacity)
            return CLM_MULTIPART_TOO_MANY_HEADERS;
        headers[count].name = (clm_multipart_span){data + *cursor, colon - *cursor};
        headers[count].value = (clm_multipart_span){data + value_begin, value_end - value_begin};
        ++count;
        *cursor = line_end + 2;
    }
}

clm_int clm_multipart_parse(
    const void* input, size_t length, clm_multipart_span boundary,
    clm_multipart_part* parts, size_t part_capacity,
    clm_multipart_header* headers, size_t header_capacity,
    clm_multipart_message* out)
{
    if ((!input && length) || !out || (!parts && part_capacity) || (!headers && header_capacity) ||
        !clm_multipart_boundary_is_valid(boundary))
        return CLM_MULTIPART_INVALID_ARGUMENT;

    const char* data = input ? (const char*)input : "";
    clm_multipart_message parsed = {0};
    parsed.parts = parts;
    parsed.part_capacity = part_capacity;
    parsed.headers = headers;
    parsed.header_capacity = header_capacity;

    size_t first = 0, after = 0;
    bool closing = false;
    clm_int status = find_delimiter(data, length, 0, boundary, &first, &closing, &after);
    if (status != CLM_MULTIPART_SUCCESS)
        return status;
    parsed.preamble = (clm_multipart_span){data, first ? first - 2 : 0};
    if (closing) {
        parsed.consumed = after;
        parsed.epilogue = (clm_multipart_span){data + after, length - after};
        *out = parsed;
        return CLM_MULTIPART_SUCCESS;
    }

    size_t cursor = after;
    for (;;) {
        if (parsed.part_count == part_capacity)
            return CLM_MULTIPART_TOO_MANY_PARTS;
        clm_multipart_part part = {0};
        part.header_offset = parsed.header_count;
        size_t part_headers = 0;
        status = parse_part_headers(data, length, &cursor,
                                    headers ? headers + parsed.header_count : NULL,
                                    header_capacity - parsed.header_count, &part_headers);
        if (status != CLM_MULTIPART_SUCCESS)
            return status;
        part.header_count = part_headers;
        parsed.header_count += part_headers;

        size_t delimiter = 0;
        status = find_delimiter(data, length, cursor, boundary, &delimiter, &closing, &after);
        if (status != CLM_MULTIPART_SUCCESS)
            return status == CLM_MULTIPART_FAILURE ? CLM_MULTIPART_INCOMPLETE : status;
        if (delimiter < cursor + 2)
            return CLM_MULTIPART_FAILURE;
        part.body = (clm_multipart_span){data + cursor, delimiter - cursor - 2};
        parts[parsed.part_count++] = part;
        cursor = after;
        if (closing) {
            parsed.consumed = after;
            parsed.epilogue = (clm_multipart_span){data + after, length - after};
            *out = parsed;
            return CLM_MULTIPART_SUCCESS;
        }
    }
}

bool clm_multipart_find_header(
    const clm_multipart_message* message, const clm_multipart_part* part,
    const char* name, clm_multipart_span* value)
{
    if (!message || !part || !name || !value ||
        part->header_offset > message->header_count ||
        part->header_count > message->header_count - part->header_offset)
        return false;
    for (size_t i = 0; i < part->header_count; ++i) {
        clm_multipart_header header = message->headers[part->header_offset + i];
        if (clm_multipart_span_equals_ignore_case(header.name, name)) {
            *value = header.value;
            return true;
        }
    }
    return false;
}

static bool valid_header(clm_multipart_header header)
{
    if (!span_valid(header.name) || !span_valid(header.value) || !header.name.length)
        return false;
    for (size_t i = 0; i < header.name.length; ++i)
        if (!is_token((unsigned char)header.name.data[i]))
            return false;
    for (size_t i = 0; i < header.value.length; ++i) {
        unsigned char c = (unsigned char)header.value.data[i];
        if ((c < 0x20 && c != '\t') || c == 0x7f)
            return false;
    }
    return true;
}

clm_int clm_multipart_generate(
    clm_bytes_buffer* destination, clm_multipart_span boundary,
    const clm_multipart_part* parts, size_t part_count,
    const clm_multipart_header* headers, size_t header_count)
{
    if (!destination || !clm_multipart_boundary_is_valid(boundary) ||
        (!parts && part_count) || (!headers && header_count))
        return CLM_MULTIPART_INVALID_ARGUMENT;

    size_t total = 0;
    for (size_t i = 0; i < part_count; ++i) {
        const clm_multipart_part* part = &parts[i];
        if ((!part->body.data && part->body.length) || part->header_offset > header_count ||
            part->header_count > header_count - part->header_offset)
            return CLM_MULTIPART_INVALID_ARGUMENT;
        if (!add_size(&total, boundary.length + 4))
            return CLM_MULTIPART_FAILURE;
        for (size_t j = 0; j < part->header_count; ++j) {
            clm_multipart_header header = headers[part->header_offset + j];
            if (!valid_header(header))
                return CLM_MULTIPART_FAILURE;
            if (!add_size(&total, header.name.length) || !add_size(&total, 2) ||
                !add_size(&total, header.value.length) || !add_size(&total, 2))
                return CLM_MULTIPART_FAILURE;
        }
        if (!add_size(&total, 2) || !add_size(&total, part->body.length) || !add_size(&total, 2))
            return CLM_MULTIPART_FAILURE;
    }
    if (!add_size(&total, boundary.length + 6))
        return CLM_MULTIPART_FAILURE;

    unsigned char* output = NULL;
    size_t free_space = 0;
    clm_int bytes_status = clm_bytes_buffer_prepare(destination, total, &output, &free_space);
    if (bytes_status == CLM_BYTES_NO_MEMORY)
        return CLM_MULTIPART_NO_MEMORY;
    if (bytes_status == CLM_BYTES_LIMIT_EXCEEDED)
        return CLM_MULTIPART_LIMIT_EXCEEDED;
    if (bytes_status != CLM_BYTES_SUCCESS)
        return CLM_MULTIPART_INVALID_ARGUMENT;
    if (free_space < total)
        return CLM_MULTIPART_BUFFER_TOO_SMALL;

    unsigned char* cursor = output;
    for (size_t i = 0; i < part_count; ++i) {
        const clm_multipart_part* part = &parts[i];
        memcpy(cursor, "--", 2); cursor += 2;
        memcpy(cursor, boundary.data, boundary.length); cursor += boundary.length;
        memcpy(cursor, "\r\n", 2); cursor += 2;
        for (size_t j = 0; j < part->header_count; ++j) {
            clm_multipart_header header = headers[part->header_offset + j];
            memcpy(cursor, header.name.data, header.name.length); cursor += header.name.length;
            memcpy(cursor, ": ", 2); cursor += 2;
            if (header.value.length) {
                memcpy(cursor, header.value.data, header.value.length);
                cursor += header.value.length;
            }
            memcpy(cursor, "\r\n", 2); cursor += 2;
        }
        memcpy(cursor, "\r\n", 2); cursor += 2;
        if (part->body.length) {
            memcpy(cursor, part->body.data, part->body.length);
            cursor += part->body.length;
        }
        memcpy(cursor, "\r\n", 2); cursor += 2;
    }
    memcpy(cursor, "--", 2); cursor += 2;
    memcpy(cursor, boundary.data, boundary.length); cursor += boundary.length;
    memcpy(cursor, "--\r\n", 4); cursor += 4;
    return clm_bytes_buffer_commit(destination, (size_t)(cursor - output)) == CLM_BYTES_SUCCESS
        ? CLM_MULTIPART_SUCCESS : CLM_MULTIPART_FAILURE;
}