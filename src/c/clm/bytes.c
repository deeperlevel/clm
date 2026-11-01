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

#include "bytes.h"
#include "crypto.h"
#include "file.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool bytes_view_valid(const clm_bytes_view view)
{
    return view.length == 0 || view.data != NULL;
}

/* Checks the buffer cap before growth; the buffer is not changed. */
static clm_int bytes_check_growth(const clm_bytes_buffer* buffer, const size_t additional)
{
    if (additional > SIZE_MAX - buffer->length) return CLM_BYTES_NO_MEMORY;
    if (buffer->limit != 0 && buffer->length + additional > buffer->limit)
        return CLM_BYTES_LIMIT_EXCEEDED;
    return CLM_BYTES_SUCCESS;
}

static clm_int bytes_ensure_tail(clm_bytes_buffer* buffer, const size_t needed)
{
    if (buffer->capacity - buffer->offset - buffer->length >= needed) return CLM_BYTES_SUCCESS;
    if (buffer->offset > 0) {
        memmove(buffer->storage, buffer->storage + buffer->offset, buffer->length);
        buffer->offset = 0;
        if (buffer->capacity - buffer->length >= needed) return CLM_BYTES_SUCCESS;
    }
    const size_t required = buffer->length + needed;
    size_t grown = buffer->capacity != 0 ? buffer->capacity : 64;
    while (grown < required) {
        if (grown > SIZE_MAX / 2) {
            grown = required;
            break;
        }
        grown *= 2;
    }
    if (buffer->limit != 0 && grown > buffer->limit) grown = buffer->limit;
    if (grown < required) grown = required;
    unsigned char* resized = (unsigned char*)realloc(buffer->storage, grown);
    if (resized == NULL) return CLM_BYTES_NO_MEMORY;
    buffer->storage = resized;
    buffer->capacity = grown;
    return CLM_BYTES_SUCCESS;
}

/* Prepares room for additional bytes after validating the limit. */
static clm_int bytes_grow(clm_bytes_buffer* buffer, const size_t additional)
{
    const clm_int status = bytes_check_growth(buffer, additional);
    return status != CLM_BYTES_SUCCESS ? status : bytes_ensure_tail(buffer, additional);
}

/* Source bytes that live inside the buffer are copied first because growth
 * and shifting would move them. */
static clm_int bytes_stable_source(
    const clm_bytes_buffer* buffer, const void* data, const size_t length,
    const unsigned char** out_source, unsigned char** out_copy)
{
    *out_source = (const unsigned char*)data;
    *out_copy = NULL;
    if (length == 0 || buffer->storage == NULL) return CLM_BYTES_SUCCESS;
    const uintptr_t start = (uintptr_t)data;
    const uintptr_t low = (uintptr_t)buffer->storage;
    if (start >= low + buffer->capacity || start + length <= low) return CLM_BYTES_SUCCESS;
    unsigned char* copy = (unsigned char*)malloc(length);
    if (copy == NULL) return CLM_BYTES_NO_MEMORY;
    memcpy(copy, data, length);
    *out_source = copy;
    *out_copy = copy;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_init(clm_bytes_buffer* buffer, const size_t initial_capacity)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    memset(buffer, 0, sizeof(*buffer));
    if (initial_capacity == 0) return CLM_BYTES_SUCCESS;
    buffer->storage = (unsigned char*)malloc(initial_capacity);
    if (buffer->storage == NULL) return CLM_BYTES_NO_MEMORY;
    buffer->capacity = initial_capacity;
    return CLM_BYTES_SUCCESS;
}

void clm_bytes_buffer_dispose(clm_bytes_buffer* buffer)
{
    if (buffer == NULL) return;
    free(buffer->storage);
    memset(buffer, 0, sizeof(*buffer));
}

clm_int clm_bytes_buffer_clear(clm_bytes_buffer* buffer)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    buffer->offset = 0;
    buffer->length = 0;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_wipe(clm_bytes_buffer* buffer)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (buffer->storage != NULL &&
        clm_crypto_secure_zero(buffer->storage, buffer->capacity) != CLM_CRYPTO_SUCCESS)
        return CLM_BYTES_FAILURE;
    return clm_bytes_buffer_clear(buffer);
}

clm_int clm_bytes_buffer_set_limit(clm_bytes_buffer* buffer, const size_t limit)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (limit != 0 && buffer->length > limit) return CLM_BYTES_LIMIT_EXCEEDED;
    buffer->limit = limit;
    return CLM_BYTES_SUCCESS;
}

clm_bytes_view clm_bytes_buffer_view(const clm_bytes_buffer* buffer)
{
    clm_bytes_view view = {NULL, 0};
    if (buffer != NULL && buffer->storage != NULL) {
        view.data = buffer->storage + buffer->offset;
        view.length = buffer->length;
    }
    return view;
}

size_t clm_bytes_buffer_length(const clm_bytes_buffer* buffer)
{
    return buffer != NULL ? buffer->length : 0;
}

clm_int clm_bytes_buffer_reserve(clm_bytes_buffer* buffer, const size_t additional)
{
    return buffer == NULL ? CLM_BYTES_INVALID_ARGUMENT : bytes_grow(buffer, additional);
}

clm_int clm_bytes_buffer_prepare(
    clm_bytes_buffer* buffer, const size_t min_free, unsigned char** out_tail, size_t* out_free)
{
    if (buffer == NULL || out_tail == NULL || out_free == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    const clm_int status = bytes_grow(buffer, min_free);
    if (status != CLM_BYTES_SUCCESS) return status;
    size_t available = buffer->capacity - buffer->offset - buffer->length;
    if (buffer->limit != 0 && available > buffer->limit - buffer->length)
        available = buffer->limit - buffer->length;
    *out_tail = available != 0 ? buffer->storage + buffer->offset + buffer->length : NULL;
    *out_free = available;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_commit(clm_bytes_buffer* buffer, const size_t written)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (written > buffer->capacity - buffer->offset - buffer->length ||
        bytes_check_growth(buffer, written) != CLM_BYTES_SUCCESS)
        return CLM_BYTES_OUT_OF_RANGE;
    buffer->length += written;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_consume(clm_bytes_buffer* buffer, const size_t count)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (count > buffer->length) return CLM_BYTES_OUT_OF_RANGE;
    buffer->offset += count;
    buffer->length -= count;
    if (buffer->length == 0) buffer->offset = 0;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_read(clm_bytes_buffer* buffer, void* dest, const size_t count)
{
    if (buffer == NULL || (dest == NULL && count != 0)) return CLM_BYTES_INVALID_ARGUMENT;
    if (count > buffer->length) return CLM_BYTES_OUT_OF_RANGE;
    if (count != 0) memcpy(dest, buffer->storage + buffer->offset, count);
    return clm_bytes_buffer_consume(buffer, count);
}

clm_int clm_bytes_buffer_insert(
    clm_bytes_buffer* buffer, const size_t index, const void* data, const size_t length)
{
    if (buffer == NULL || (data == NULL && length != 0)) return CLM_BYTES_INVALID_ARGUMENT;
    if (index > buffer->length) return CLM_BYTES_OUT_OF_RANGE;
    if (length == 0) return CLM_BYTES_SUCCESS;
    const unsigned char* source;
    unsigned char* copy;
    clm_int status = bytes_stable_source(buffer, data, length, &source, &copy);
    if (status != CLM_BYTES_SUCCESS) return status;
    status = bytes_grow(buffer, length);
    if (status == CLM_BYTES_SUCCESS) {
        unsigned char* base = buffer->storage + buffer->offset;
        memmove(base + index + length, base + index, buffer->length - index);
        memcpy(base + index, source, length);
        buffer->length += length;
    }
    free(copy);
    return status;
}

clm_int clm_bytes_buffer_append(clm_bytes_buffer* buffer, const void* data, const size_t length)
{
    return buffer == NULL ? CLM_BYTES_INVALID_ARGUMENT
        : clm_bytes_buffer_insert(buffer, buffer->length, data, length);
}

clm_int clm_bytes_buffer_append_view(clm_bytes_buffer* buffer, const clm_bytes_view view)
{
    return clm_bytes_buffer_append(buffer, view.data, view.length);
}

clm_int clm_bytes_buffer_append_byte(clm_bytes_buffer* buffer, const unsigned char value)
{
    return clm_bytes_buffer_append_fill(buffer, value, 1);
}

clm_int clm_bytes_buffer_append_fill(
    clm_bytes_buffer* buffer, const unsigned char value, const size_t count)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    const clm_int status = bytes_grow(buffer, count);
    if (status != CLM_BYTES_SUCCESS) return status;
    if (count != 0) memset(buffer->storage + buffer->offset + buffer->length, value, count);
    buffer->length += count;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_append_string(clm_bytes_buffer* buffer, const char* text)
{
    return text == NULL ? CLM_BYTES_INVALID_ARGUMENT : clm_bytes_buffer_append(buffer, text, strlen(text));
}

clm_int clm_bytes_buffer_remove(clm_bytes_buffer* buffer, const size_t index, const size_t count)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (index > buffer->length || count > buffer->length - index) return CLM_BYTES_OUT_OF_RANGE;
    unsigned char* base = buffer->storage + buffer->offset;
    if (count != 0) memmove(base + index, base + index + count, buffer->length - index - count);
    buffer->length -= count;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_buffer_resize(
    clm_bytes_buffer* buffer, const size_t new_length, const unsigned char fill)
{
    if (buffer == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (new_length <= buffer->length) {
        buffer->length = new_length;
        return CLM_BYTES_SUCCESS;
    }
    return clm_bytes_buffer_append_fill(buffer, fill, new_length - buffer->length);
}

clm_int clm_bytes_buffer_load_file(clm_bytes_buffer* buffer, const char* path)
{
    clm_ulong file_size = 0;
    if (buffer == NULL || path == NULL) return CLM_BYTES_INVALID_ARGUMENT;
    if (clm_file_get_size_as(path, &file_size) != CLM_FILE_OPERATION_SUCCESS) return CLM_BYTES_FAILURE;
    if (file_size > SIZE_MAX) return CLM_BYTES_NO_MEMORY;
    const size_t size = (size_t)file_size;
    if (size == 0) return CLM_BYTES_SUCCESS;
    unsigned char* tail = NULL;
    size_t available = 0;
    clm_int status = clm_bytes_buffer_prepare(buffer, size, &tail, &available);
    if (status != CLM_BYTES_SUCCESS) return status;
    size_t read = 0;
    if (clm_file_read_all_to(path, tail, size, &read) != CLM_FILE_OPERATION_SUCCESS) return CLM_BYTES_FAILURE;
    return clm_bytes_buffer_commit(buffer, read) == CLM_BYTES_SUCCESS ? CLM_BYTES_SUCCESS : CLM_BYTES_FAILURE;
}

clm_bytes_view clm_bytes_view_from_string(const char* text)
{
    clm_bytes_view view = {NULL, 0};
    if (text != NULL) {
        view.data = (const unsigned char*)text;
        view.length = strlen(text);
    }
    return view;
}

clm_int clm_bytes_view_slice(
    const clm_bytes_view view, const size_t offset, const size_t length, clm_bytes_view* out)
{
    if (out == NULL || !bytes_view_valid(view)) return CLM_BYTES_INVALID_ARGUMENT;
    if (offset > view.length || length > view.length - offset) return CLM_BYTES_OUT_OF_RANGE;
    out->data = view.data != NULL ? view.data + offset : NULL;
    out->length = length;
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_view_copy(
    const clm_bytes_view view, void* dest, const size_t capacity, size_t* copied)
{
    if (!bytes_view_valid(view) || (dest == NULL && capacity != 0)) return CLM_BYTES_INVALID_ARGUMENT;
    if (copied != NULL) *copied = view.length;
    if (view.length > capacity) return CLM_BYTES_BUFFER_TOO_SMALL;
    if (view.length != 0) memcpy(dest, view.data, view.length);
    return CLM_BYTES_SUCCESS;
}

int clm_bytes_view_compare(const clm_bytes_view a, const clm_bytes_view b)
{
    const size_t shared = a.length < b.length ? a.length : b.length;
    const int order = shared != 0 ? memcmp(a.data, b.data, shared) : 0;
    if (order != 0) return order < 0 ? -1 : 1;
    return a.length == b.length ? 0 : a.length < b.length ? -1 : 1;
}

bool clm_bytes_view_equals(const clm_bytes_view a, const clm_bytes_view b)
{
    return bytes_view_valid(a) && bytes_view_valid(b) && a.length == b.length &&
        (a.length == 0 || memcmp(a.data, b.data, a.length) == 0);
}

bool clm_bytes_view_equals_constant_time(const clm_bytes_view a, const clm_bytes_view b)
{
    if (!bytes_view_valid(a) || !bytes_view_valid(b) || a.length != b.length) return false;
    volatile unsigned char difference = 0;
    for (size_t i = 0; i < a.length; ++i) difference |= (unsigned char)(a.data[i] ^ b.data[i]);
    return difference == 0;
}

bool clm_bytes_view_starts_with(const clm_bytes_view view, const clm_bytes_view prefix)
{
    return bytes_view_valid(view) && bytes_view_valid(prefix) && prefix.length <= view.length &&
        (prefix.length == 0 || memcmp(view.data, prefix.data, prefix.length) == 0);
}

bool clm_bytes_view_ends_with(const clm_bytes_view view, const clm_bytes_view suffix)
{
    return bytes_view_valid(view) && bytes_view_valid(suffix) && suffix.length <= view.length &&
        (suffix.length == 0 || memcmp(view.data + view.length - suffix.length, suffix.data, suffix.length) == 0);
}

clm_int clm_bytes_view_find_byte(
    const clm_bytes_view view, const unsigned char value, size_t* out_index)
{
    if (!bytes_view_valid(view)) return CLM_BYTES_INVALID_ARGUMENT;
    const unsigned char* match = view.length != 0 ? (const unsigned char*)memchr(view.data, value, view.length) : NULL;
    if (match == NULL) return CLM_BYTES_NOT_FOUND;
    if (out_index != NULL) *out_index = (size_t)(match - view.data);
    return CLM_BYTES_SUCCESS;
}

clm_int clm_bytes_view_find(
    const clm_bytes_view view, const clm_bytes_view needle, size_t* out_index)
{
    if (!bytes_view_valid(view) || !bytes_view_valid(needle)) return CLM_BYTES_INVALID_ARGUMENT;
    if (needle.length == 0) {
        if (out_index != NULL) *out_index = 0;
        return CLM_BYTES_SUCCESS;
    }
    if (needle.length > view.length) return CLM_BYTES_NOT_FOUND;
    const size_t last_start = view.length - needle.length;
    size_t start = 0;
    while (start <= last_start) {
        const unsigned char* match = (const unsigned char*)memchr(
            view.data + start, needle.data[0], last_start - start + 1);
        if (match == NULL) break;
        start = (size_t)(match - view.data);
        if (memcmp(match, needle.data, needle.length) == 0) {
            if (out_index != NULL) *out_index = start;
            return CLM_BYTES_SUCCESS;
        }
        ++start;
    }
    return CLM_BYTES_NOT_FOUND;
}

clm_int clm_bytes_view_save_file(const clm_bytes_view view, const char* path)
{
    if (path == NULL || !bytes_view_valid(view)) return CLM_BYTES_INVALID_ARGUMENT;
    return clm_file_write_all_from(path, view.data, view.length) == CLM_FILE_OPERATION_SUCCESS
        ? CLM_BYTES_SUCCESS : CLM_BYTES_FAILURE;
}

clm_int clm_bytes_view_append_to_file(const clm_bytes_view view, const char* path)
{
    if (path == NULL || !bytes_view_valid(view)) return CLM_BYTES_INVALID_ARGUMENT;
    return clm_file_append_from(path, view.data, view.length) == CLM_FILE_OPERATION_SUCCESS
        ? CLM_BYTES_SUCCESS : CLM_BYTES_FAILURE;
}
