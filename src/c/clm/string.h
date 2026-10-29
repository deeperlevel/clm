#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "bpa.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Unicode text is ALWAYS stored internally as validated UTF-8.
 * Encoding values describe external byte representations only.
 * UTF-16LE exports are not NUL-terminated. */
#define CLM_STRING_ENCODING_UTF8 1u
#define CLM_STRING_ENCODING_LATIN1 2u
#define CLM_STRING_ENCODING_UTF16LE 3u

/* Shared runtime default, defined once via CLM_STRING_IMPLEMENTATION.
 * It affects new strings' preferred output encoding; existing strings retain theirs.
 * Not thread-safe: configure before concurrent use. */
extern clm_byte clm_string_global_default_encoding;

static inline bool clm_string_encoding_supported(const clm_byte encoding)
{
    return encoding == CLM_STRING_ENCODING_UTF8 ||
           encoding == CLM_STRING_ENCODING_LATIN1 ||
           encoding == CLM_STRING_ENCODING_UTF16LE;
}

/** Changes the preferred byte-export encoding for subsequently created strings.
 * Does not change Unicode text or existing strings' preferences.
 */
static inline clm_int clm_string_set_default_encoding(const clm_byte encoding)
{
    if (!clm_string_encoding_supported(encoding)) return CLM_BPA_FAILURE;
    clm_string_global_default_encoding = encoding;
    return CLM_BPA_SUCCESS;
}

/** Returns the runtime default byte-export encoding (initially UTF-8). */
static inline clm_byte clm_string_get_default_encoding(void)
{
    return clm_string_global_default_encoding;
}

static inline bool clm_string_utf8_next(
    const unsigned char* text,
    const size_t length,
    size_t* offset)
{
    const size_t start = *offset;
    size_t sequence_length;
    unsigned int codepoint;

    if (start >= length)
        return false;
    const unsigned char first = text[start];
    if (first < 0x80) {
        *offset = start + 1;
        return true;
    }
    if (first >= 0xC2 && first <= 0xDF) {
        sequence_length = 2;
        codepoint = first & 0x1F;
    } else if (first >= 0xE0 && first <= 0xEF) {
        sequence_length = 3;
        codepoint = first & 0x0F;
    } else if (first >= 0xF0 && first <= 0xF4) {
        sequence_length = 4;
        codepoint = first & 0x07;
    } else {
        return false;
    }
    if (sequence_length > length - start)
        return false;
    for (size_t index = 1; index < sequence_length; ++index) {
        const unsigned char continuation = text[start + index];
        if ((continuation & 0xC0) != 0x80)
            return false;
        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }
    if ((sequence_length == 3 && codepoint < 0x800) ||
        (sequence_length == 4 && codepoint < 0x10000) ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
        codepoint > 0x10FFFF)
        return false;
    *offset = start + sequence_length;
    return true;
}

static inline clm_int clm_string_measure(
    const char* text,
    const size_t byte_length,
    const clm_byte encoding,
    size_t* character_count)
{
    size_t count = 0;
    size_t offset = 0;

    if (text == NULL || character_count == NULL ||
        encoding != CLM_STRING_ENCODING_UTF8)
        return CLM_BPA_FAILURE;
    while (offset < byte_length) {
        if (!clm_string_utf8_next((const unsigned char*)text, byte_length, &offset))
            return CLM_BPA_FAILURE;
        ++count;
        if (count > INT_MAX)
            return CLM_BPA_FAILURE;
    }
    *character_count = count;
    return CLM_BPA_SUCCESS;
}

static inline clm_int clm_string_offset(
    const char* text,
    const size_t byte_length,
    const clm_byte encoding,
    const size_t character_index,
    size_t* byte_offset)
{
    size_t offset = 0;
    size_t current_index = 0;

    if (text == NULL || byte_offset == NULL)
        return CLM_BPA_FAILURE;
    if (encoding != CLM_STRING_ENCODING_UTF8)
        return CLM_BPA_FAILURE;
    while (current_index < character_index && offset < byte_length) {
        if (!clm_string_utf8_next((const unsigned char*)text, byte_length, &offset))
            return CLM_BPA_FAILURE;
        ++current_index;
    }
    if (current_index != character_index)
        return CLM_BPA_FAILURE;
    *byte_offset = offset;
    return CLM_BPA_SUCCESS;
}

static inline clm_int clm_string_get_block(
    const clm_id id,
    clm_bpa_block_node** block,
    const char** text,
    size_t* byte_length,
    size_t* character_count)
{
    clm_bpa_block_node* found = clm_bpa_find_block(id);
    if (found == NULL || found->type != CLM_BPA_TYPE_STRING ||
        found->length > INT_MAX || (found->length != 0 && found->data == NULL))
        return CLM_BPA_FAILURE;
    if (clm_string_measure(
            found->data != NULL ? (const char*)found->data : "",
            found->length,
            CLM_STRING_ENCODING_UTF8,
            character_count) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    *block = found;
    *text = found->data != NULL ? (const char*)found->data : "";
    *byte_length = found->length;
    return CLM_BPA_SUCCESS;
}

static inline clm_int clm_string_replace_bytes(
    const clm_id id,
    const char* text,
    const size_t byte_length)
{
    if (text == NULL || byte_length >= UINT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpa_replace(id, text, (clm_uint)(byte_length + 1)) < 0
        ? CLM_BPA_FAILURE : CLM_BPA_SUCCESS;
}

static inline char* clm_string_temp_alloc(const size_t bytes)
{
    return bytes == 0 ? NULL : (char*)clm_bpa_pool_alloc(bytes);
}

static inline void clm_string_temp_free(char* text, const size_t bytes)
{
    clm_bpa_pool_free(text, bytes);
}

/** Creates a string BPA block and initializes it with a UTF-8 string.
 * @return true on success; false for invalid text, duplicate ID, or allocation failure.
 */
static inline bool clm_string_new(const clm_id id, const char* text)
{
    size_t character_count;

    if (text == NULL ||
        clm_string_measure(
            text, strlen(text), CLM_STRING_ENCODING_UTF8,
            &character_count) != CLM_BPA_SUCCESS ||
        clm_bpa_create(id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS)
        return false;
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL) {
        (void)clm_bpa_delete(id);
        return false;
    }
    block->encoding = clm_string_get_default_encoding();
    if (strlen(text) >= UINT_MAX ||
        clm_bpa_set_string(id, text, (clm_uint)(strlen(text) + 1)) < 0) {
        (void)clm_bpa_delete(id);
        return false;
    }
    return true;
}

/** Deletes a string block without affecting blocks of other types.
 * @return true when deleted; false if missing or not a string block.
 */
static inline bool clm_string_delete(const clm_id id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    return block != NULL && block->type == CLM_BPA_TYPE_STRING &&
           clm_bpa_delete(id) == CLM_BPA_SUCCESS;
}

/** Replaces an existing string's value with valid UTF-8 text.
 * @param dest_id ID of the string block to update; it must already exist.
 * @param text NUL-terminated UTF-8 text to copy.
 * @return Unicode code-point count on success, or CLM_BPA_FAILURE.
 */
static inline clm_int clm_string_set(const clm_id dest_id, const char* text)
{
    size_t character_count;
    if (text == NULL)
        return CLM_BPA_FAILURE;
    const clm_bpa_block_node* block = clm_bpa_find_block(dest_id);
    const size_t byte_length = strlen(text);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        byte_length >= UINT_MAX ||
        clm_string_measure(text, byte_length, CLM_STRING_ENCODING_UTF8,
                           &character_count) != CLM_BPA_SUCCESS ||
        clm_bpa_replace(dest_id, text, (clm_uint)(byte_length + 1)) < 0)
        return CLM_BPA_FAILURE;
    return (clm_int)character_count;
}

/** Replaces a string block's value; returns its character count or failure. */
static inline clm_int clm_string_set_from(const clm_id dest_id, const char* text)
{
    return clm_string_set(dest_id, text);
}

/** Appends text and returns the number of characters appended, or CLM_BPA_FAILURE.
 * Character counts and indices are Unicode code points.
 */
static inline clm_int clm_string_append(const clm_id id, const char* suffix)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters, suffix_characters;

    if (suffix == NULL ||
        clm_string_get_block(id, &block, &current, &current_bytes,
                             &current_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const size_t suffix_bytes = strlen(suffix);
    if (clm_string_measure(suffix, suffix_bytes, CLM_STRING_ENCODING_UTF8,
                           &suffix_characters) != CLM_BPA_SUCCESS ||
        suffix_bytes > (size_t)INT_MAX - current_bytes)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes + suffix_bytes;
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, current_bytes);
    memcpy(result + current_bytes, suffix, suffix_bytes + 1);
    if (clm_string_replace_bytes(id, result, result_bytes) != CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return (clm_int)suffix_characters;
}

/** Appends text followed by a newline; returns appended character count or failure. */
static inline clm_int clm_string_append_line(const clm_id id, const char* suffix)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters, suffix_characters;

    if (suffix == NULL ||
        clm_string_get_block(id, &block, &current, &current_bytes,
                             &current_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const size_t suffix_bytes = strlen(suffix);
    if (clm_string_measure(suffix, suffix_bytes, CLM_STRING_ENCODING_UTF8,
                           &suffix_characters) != CLM_BPA_SUCCESS ||
        current_bytes >= (size_t)INT_MAX ||
        suffix_bytes > (size_t)INT_MAX - current_bytes - 1)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes + suffix_bytes + 1;
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, current_bytes);
    memcpy(result + current_bytes, suffix, suffix_bytes);
    result[current_bytes + suffix_bytes] = '\n';
    result[result_bytes] = '\0';
    if (clm_string_replace_bytes(id, result, result_bytes) != CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return (clm_int)(suffix_characters + 1);
}

/** Inserts text at a character index; returns inserted character count or failure. */
static inline clm_int clm_string_insert(
    const clm_id id,
    const clm_int index,
    const char* inserted)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters, inserted_characters;
    size_t byte_index;

    if (index < 0 || inserted == NULL ||
        clm_string_get_block(id, &block, &current, &current_bytes,
                             &current_characters) != CLM_BPA_SUCCESS ||
        (size_t)index > current_characters)
        return CLM_BPA_FAILURE;
    const size_t inserted_bytes = strlen(inserted);
    if (clm_string_measure(inserted, inserted_bytes, CLM_STRING_ENCODING_UTF8,
                           &inserted_characters) != CLM_BPA_SUCCESS ||
        clm_string_offset(current, current_bytes, CLM_STRING_ENCODING_UTF8, (size_t)index,
                          &byte_index) != CLM_BPA_SUCCESS ||
        inserted_bytes > (size_t)INT_MAX - current_bytes)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes + inserted_bytes;
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, byte_index);
    memcpy(result + byte_index, inserted, inserted_bytes);
    memcpy(result + byte_index + inserted_bytes, current + byte_index,
           current_bytes - byte_index);
    result[result_bytes] = '\0';
    if (clm_string_replace_bytes(id, result, result_bytes) != CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return (clm_int)inserted_characters;
}

/** Removes a character range; returns the number removed, or CLM_BPA_FAILURE. */
static inline clm_int clm_string_remove(
    const clm_id id,
    const clm_int index,
    const clm_int length)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters, start, end;

    if (index < 0 || length < 0 ||
        clm_string_get_block(id, &block, &current, &current_bytes,
                             &current_characters) != CLM_BPA_SUCCESS ||
        (size_t)index > current_characters ||
        (size_t)length > current_characters - (size_t)index)
        return CLM_BPA_FAILURE;
    if (length == 0)
        return 0;
    if (clm_string_offset(current, current_bytes, CLM_STRING_ENCODING_UTF8, (size_t)index,
                          &start) != CLM_BPA_SUCCESS ||
        clm_string_offset(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)index + (size_t)length, &end) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes - (end - start);
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, start);
    memcpy(result + start, current + end, current_bytes - end);
    result[result_bytes] = '\0';
    if (clm_string_replace_bytes(id, result, result_bytes) != CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return length;
}

/** Returns whether a string block contains a valid search string. */
static inline bool clm_string_contains(const clm_id id, const char* search)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters, search_characters;
    if (search == NULL ||
        clm_string_get_block(id, &block, &current, &current_bytes,
                             &current_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(search, strlen(search), CLM_STRING_ENCODING_UTF8,
                           &search_characters) != CLM_BPA_SUCCESS)
        return false;
    return strstr(current, search) != NULL;
}

/** Returns string length in Unicode code points, or failure. */
static inline clm_int clm_string_length(const clm_id id)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters;
    return clm_string_get_block(id, &block, &current, &current_bytes,
                                &current_characters) == CLM_BPA_SUCCESS
        ? (clm_int)current_characters : CLM_BPA_FAILURE;
}

/** Replaces every non-overlapping occurrence; returns replacements made or failure.
 * An empty search string is invalid. The replacement may be empty.
 */
static inline clm_int clm_string_replace_from(
    const clm_id id,
    const char* search,
    const char* replacement)
{
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters, search_characters, replacement_characters;
    const size_t search_bytes = search != NULL ? strlen(search) : 0;
    const size_t replacement_bytes = replacement != NULL ? strlen(replacement) : 0;
    const char* match;
    size_t count = 0, output_bytes;

    if (search == NULL || replacement == NULL || search_bytes == 0 ||
        clm_string_get_block(id, &block, &current, &current_bytes,
                             &current_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(search, search_bytes, CLM_STRING_ENCODING_UTF8,
                           &search_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(replacement, replacement_bytes, CLM_STRING_ENCODING_UTF8,
                           &replacement_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    const char* source = current;
    while ((match = strstr(source, search)) != NULL) {
        ++count;
        source = match + search_bytes;
    }
    if (count == 0)
        return 0;
    if (replacement_bytes >= search_bytes) {
        const size_t growth = replacement_bytes - search_bytes;
        if (growth != 0 && count > ((size_t)INT_MAX - current_bytes) / growth)
            return CLM_BPA_FAILURE;
        output_bytes = current_bytes + count * growth;
    } else {
        output_bytes = current_bytes - count * (search_bytes - replacement_bytes);
    }
    char* result = clm_string_temp_alloc(output_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    source = current;
    char* destination = result;
    while ((match = strstr(source, search)) != NULL) {
        const size_t prefix_bytes = (size_t)(match - source);
        memcpy(destination, source, prefix_bytes);
        destination += prefix_bytes;
        memcpy(destination, replacement, replacement_bytes);
        destination += replacement_bytes;
        source = match + search_bytes;
    }
    memcpy(destination, source, strlen(source) + 1);
    if (clm_string_replace_bytes(id, result, output_bytes) != CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, output_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, output_bytes + 1);
    return (clm_int)count;
}

/** Replaces every non-overlapping occurrence in a string.
 * Returns the replacement count, zero when there are no matches, or
 * CLM_BPA_FAILURE for invalid UTF-8, an empty search, or an invalid string ID.
 */
static inline clm_int clm_string_replace(
    const clm_id id,
    const char* search,
    const char* replacement)
{
    return clm_string_replace_from(id, search, replacement);
}

/** Decodes one validated UTF-8 scalar value. */
static inline clm_int clm_string_utf8_decode_one(const unsigned char* data, size_t size, size_t* offset, uint32_t* scalar)
{
    size_t begin = *offset, end = begin;
    if (!clm_string_utf8_next(data, size, &end)) return CLM_BPA_FAILURE;
    uint32_t cp = data[begin];
    if (end - begin > 1) {
        cp &= (1u << (7u - (unsigned int)(end - begin))) - 1u;
        for (size_t j = begin + 1; j < end; ++j)
            cp = (cp << 6) | (data[j] & 0x3fu);
    }
    *scalar = cp;
    *offset = end;
    return CLM_BPA_SUCCESS;
}

/** Encodes one Unicode scalar as UTF-8. Caller provides 4 writable bytes. */
static inline size_t clm_string_utf8_encode_one(uint32_t cp, unsigned char* out)
{
    if (cp < 0x80) { out[0] = (unsigned char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (unsigned char)(0xc0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3f)); return 2;
    }
    if (cp < 0x10000) {
        out[0] = (unsigned char)(0xe0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (unsigned char)(0x80 | (cp & 0x3f)); return 3;
    }
    out[0] = (unsigned char)(0xf0 | (cp >> 18));
    out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (unsigned char)(0x80 | (cp & 0x3f)); return 4;
}

/** Convert bytes to validated UTF-8 in an allocated temporary buffer.
 * Rejects embedded U+0000 because BPA currently stores NUL-terminated strings.
 * Call free() on the returned buffer. */
static inline clm_int clm_string_decode_bytes(const clm_byte encoding,
    const void* input, size_t size, char** output, size_t* output_length)
{
    if (!clm_string_encoding_supported(encoding) || output == NULL || output_length == NULL ||
        (size != 0 && input == NULL) || size > INT_MAX) return CLM_BPA_FAILURE;
    const unsigned char* in = input;
    if (encoding == CLM_STRING_ENCODING_UTF16LE && (size % 2) != 0)
        return CLM_BPA_FAILURE;
    if (encoding == CLM_STRING_ENCODING_UTF8 && size != 0) {
        size_t off = 0;
        while (off < size) {
            uint32_t cp;
            if (clm_string_utf8_decode_one(in, size, &off, &cp) != CLM_BPA_SUCCESS || cp == 0)
                return CLM_BPA_FAILURE;
        }
    }
    if (encoding == CLM_STRING_ENCODING_LATIN1 && size != 0 && memchr(in, 0, size))
        return CLM_BPA_FAILURE;
    size_t allocation;
    if (encoding == CLM_STRING_ENCODING_UTF8) allocation = size + 1;
    else if (encoding == CLM_STRING_ENCODING_LATIN1) allocation = size * 2 + 1;
    else allocation = (size / 2) * 3 + 1;
    if (allocation == 0 || allocation > (size_t)INT_MAX + 1)
        return CLM_BPA_FAILURE;
    char* result = malloc(allocation);
    if (result == NULL) return CLM_BPA_FAILURE;
    size_t used = 0;
    if (encoding == CLM_STRING_ENCODING_UTF8) {
        if (size) memcpy(result, in, size);
        used = size;
    } else if (encoding == CLM_STRING_ENCODING_LATIN1) {
        for (size_t i = 0; i < size; ++i) {
            unsigned char encoded[4];
            size_t n = clm_string_utf8_encode_one(in[i], encoded);
            memcpy(result + used, encoded, n); used += n;
        }
    } else {
        for (size_t i = 0; i < size;) {
            uint32_t cp = (uint32_t)in[i] | ((uint32_t)in[i+1] << 8);
            i += 2;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 1 >= size) { free(result); return CLM_BPA_FAILURE; }
                uint32_t tail = (uint32_t)in[i] | ((uint32_t)in[i+1] << 8);
                if (tail < 0xdc00 || tail > 0xdfff) { free(result); return CLM_BPA_FAILURE; }
                cp = 0x10000u + ((cp - 0xd800u) << 10) + (tail - 0xdc00u);
                i += 2;
            } else if (cp >= 0xdc00 && cp <= 0xdfff) {
                free(result); return CLM_BPA_FAILURE;
            }
            if (cp == 0) { free(result); return CLM_BPA_FAILURE; }
            unsigned char encoded[4];
            size_t n = clm_string_utf8_encode_one(cp, encoded);
            memcpy(result + used, encoded, n); used += n;
        }
    }
    result[used] = '\0';
    *output = result;
    *output_length = used;
    return CLM_BPA_SUCCESS;
}

/** Exports the Unicode text into bytes in its preferred external encoding.
 * No trailing NUL or BOM is included. Returns byte count, or -1 on failure.
 * NULL buffer with zero capacity queries required byte count (without writing).
 * Fails without writing when output capacity is insufficient.
 */
static inline clm_int clm_string_get_bytes(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        !clm_string_encoding_supported(block->encoding) ||
        (block->length > 0 && block->data == NULL)) return CLM_BPA_FAILURE;
    const unsigned char* in = (const unsigned char*)(block->data ? block->data : "");
    const size_t bytes = block->length;
    size_t offset = 0, required = 0;
    while (offset < bytes) {
        uint32_t cp;
        if (clm_string_utf8_decode_one(in, bytes, &offset, &cp) != CLM_BPA_SUCCESS || cp == 0)
            return CLM_BPA_FAILURE;
        size_t step;
        if (block->encoding == CLM_STRING_ENCODING_UTF8) step = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        else if (block->encoding == CLM_STRING_ENCODING_LATIN1) {
            if (cp > 255) return CLM_BPA_FAILURE;
            step = 1;
        } else step = cp < 0x10000 ? 2 : 4;
        if (required > INT_MAX - step) return CLM_BPA_FAILURE;
        required += step;
    }
    if (buffer == NULL) return buffer_size == 0 ? (clm_int)required : CLM_BPA_FAILURE;
    if (buffer_size < required) return CLM_BPA_FAILURE;
    unsigned char* out = buffer;
    if (block->encoding == CLM_STRING_ENCODING_UTF8) {
        if (required) memcpy(out, in, required);
    } else {
        offset = 0;
        size_t pos = 0;
        while (offset < bytes) {
            uint32_t cp;
            if (clm_string_utf8_decode_one(in, bytes, &offset, &cp) != CLM_BPA_SUCCESS)
                return CLM_BPA_FAILURE;
            if (block->encoding == CLM_STRING_ENCODING_LATIN1) out[pos++] = (unsigned char)cp;
            else if (cp < 0x10000) {
                out[pos++] = (unsigned char)cp;
                out[pos++] = (unsigned char)(cp >> 8);
            } else {
                cp -= 0x10000;
                uint16_t hi = (uint16_t)(0xd800 | (cp >> 10));
                uint16_t lo = (uint16_t)(0xdc00 | (cp & 0x3ff));
                out[pos++] = (unsigned char)hi; out[pos++] = (unsigned char)(hi >> 8);
                out[pos++] = (unsigned char)lo; out[pos++] = (unsigned char)(lo >> 8);
            }
        }
    }
    return (clm_int)required;
}

/** Decode external byte sequence into an existing string, or create a new string.
 * Buffer length is exact (not NUL-terminated). Internal storage remains UTF-8.
 * If replacing, preserve resource identity; update preferred export encoding.
 * Fails on invalid UTF-8, UTF-16, embedded U+0000, or allocation failure.
 */
static inline clm_int clm_string_from_bytes(const clm_id id, const clm_byte encoding, const void* buffer, const clm_uint buffer_size)
{
    char* converted = NULL;
    size_t len = 0, chars = 0;
    if (clm_string_decode_bytes(encoding, buffer, buffer_size, &converted, &len) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_string_measure(converted, len, CLM_STRING_ENCODING_UTF8, &chars) != CLM_BPA_SUCCESS) {
        free(converted); return CLM_BPA_FAILURE;
    }
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    int created = 0;
    if (block == NULL) {
        if (clm_bpa_create(id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS) {
            free(converted); return CLM_BPA_FAILURE;
        }
        created = 1;
        block = clm_bpa_find_block(id);
    }
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        clm_bpa_set_string(id, converted, (clm_uint)(len + 1)) < 0) {
        if (created) (void)clm_bpa_delete(id);
        free(converted); return CLM_BPA_FAILURE;
    }
    block->encoding = encoding;
    free(converted);
    return CLM_BPA_SUCCESS;
}

/** Changes the preferred encoding for subsequent byte exports.
 * Text remains unchanged and stored as UTF-8. Does NOT transcode in place.
 * Fails if the current text cannot be represented in the new encoding.
 */
static inline clm_int clm_string_change_encoding(const clm_id id, const clm_byte new_encoding)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        !clm_string_encoding_supported(new_encoding)) return CLM_BPA_FAILURE;
    clm_byte old = block->encoding;
    block->encoding = new_encoding;
    if (clm_string_get_bytes(id, NULL, 0) < 0) {
        block->encoding = old;
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}


/** Creates an independent CLM string containing a Unicode code-point slice.
 * The range is [start, start + length), with strict bounds. A zero-length
 * range at the end is valid. Output is UTF-8 internally, and inherits the
 * source's preferred byte-export encoding. The destination ID must be unused.
 * @return CLM_BPA_SUCCESS on success; CLM_BPA_FAILURE otherwise.
 */
static inline clm_int clm_string_slice(const clm_id source_id, const clm_int start,
                                   const clm_int length, const clm_id destination_id)
{
    clm_bpa_block_node* source;
    const char* text;
    size_t bytes, characters, first, last;
    if (start < 0 || length < 0 ||
        clm_string_get_block(source_id, &source, &text, &bytes, &characters) != CLM_BPA_SUCCESS ||
        (size_t)start > characters || (size_t)length > characters - (size_t)start ||
        clm_bpa_is_existing(destination_id))
        return CLM_BPA_FAILURE;
    if (clm_string_offset(text, bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)start, &first) != CLM_BPA_SUCCESS ||
        clm_string_offset(text, bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)start + (size_t)length, &last) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = last - first;
    char* copy = clm_string_temp_alloc(result_bytes + 1);
    if (copy == NULL) return CLM_BPA_FAILURE;
    memcpy(copy, text + first, result_bytes);
    copy[result_bytes] = '\0';
    const clm_byte preference = source->encoding;
    const bool created = clm_string_new(destination_id, copy);
    clm_string_temp_free(copy, result_bytes + 1);
    if (!created) return CLM_BPA_FAILURE;
    /* This is a substring, so any encoding representable by source remains valid. */
    if (clm_string_change_encoding(destination_id, preference) != CLM_BPA_SUCCESS) {
        (void)clm_string_delete(destination_id);
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}

/** Borrows a UTF-8 byte view of a Unicode code-point range.
 * The view is not NUL-terminated at its end and remains valid only while the
 * source string is live and unmodified. It does not retain the source block.
 * @param source_id ID of the source string.
 * @param start Zero-based first Unicode code point to include.
 * @param length Number of Unicode code points to borrow.
 * @param text Receives the first byte of the range; may point to the final NUL
 *        for an empty range.
 * @param byte_length Receives the view length in bytes, excluding any terminator.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range.
 */
static inline clm_int clm_string_slice_borrow(
    const clm_id source_id,
    const clm_int start,
    const clm_int length,
    const char** text,
    size_t* byte_length)
{
    clm_bpa_block_node* source;
    const char* source_text;
    size_t source_bytes, characters, first, last;
    if (text == NULL || byte_length == NULL)
        return CLM_BPA_FAILURE;
    *text = NULL;
    *byte_length = 0;
    if (start < 0 || length < 0 ||
        clm_string_get_block(source_id, &source, &source_text,
                             &source_bytes, &characters) != CLM_BPA_SUCCESS ||
        (size_t)start > characters ||
        (size_t)length > characters - (size_t)start ||
        clm_string_offset(source_text, source_bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)start, &first) != CLM_BPA_SUCCESS ||
        clm_string_offset(source_text, source_bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)start + (size_t)length, &last) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    *text = source_text + first;
    *byte_length = last - first;
    (void)source;
    return CLM_BPA_SUCCESS;
}

/** Copies a string block into a destination string, creating it if needed.
 * An existing destination must be a string block. The source must contain
 * valid UTF-8 and have a supported, representable export encoding; that
 * encoding preference is copied to the destination.
 * @param dest_id ID to create or update.
 * @param source_id ID of the source string block.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE on invalid input or allocation failure.
 */
static inline clm_int clm_string_from(const clm_id dest_id, const clm_id source_id)
{
    clm_bpa_block_node* source;
    const char* text;
    size_t byte_length, character_count;
    if (clm_string_get_block(source_id, &source, &text, &byte_length,
                             &character_count) != CLM_BPA_SUCCESS ||
        clm_string_get_bytes(source_id, NULL, 0) < 0)
        return CLM_BPA_FAILURE;

    clm_bpa_block_node* destination = clm_bpa_find_block(dest_id);
    bool created = false;
    if (destination == NULL) {
        if (clm_bpa_create(dest_id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        created = true;
        destination = clm_bpa_find_block(dest_id);
    }
    if (destination == NULL || destination->type != CLM_BPA_TYPE_STRING ||
        byte_length >= UINT_MAX ||
        clm_bpa_set_string(dest_id, text, (clm_uint)(byte_length + 1)) < 0) {
        if (created)
            (void)clm_bpa_delete(dest_id);
        return CLM_BPA_FAILURE;
    }
    destination->encoding = source->encoding;
    return CLM_BPA_SUCCESS;
}

/** Initializes a string from NUL-terminated UTF-8 text, creating the
 * destination with the current default encoding if it does not yet exist.
 * An existing destination must be a string block and retains its encoding.
 * @param dest_id ID to create or update.
 * @param source Valid NUL-terminated UTF-8 text.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE on invalid input or allocation failure.
 */
static inline clm_int clm_string_from_from(const clm_id dest_id, const char* source)
{
    if (source == NULL)
        return CLM_BPA_FAILURE;
    clm_bpa_block_node* destination = clm_bpa_find_block(dest_id);
    if (destination == NULL)
        return clm_string_new(dest_id, source) ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
    if (destination->type != CLM_BPA_TYPE_STRING ||
        clm_string_set(dest_id, source) < 0)
        return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

/** Decodes an externally encoded byte sequence and initializes a string from
 * the resulting UTF-8 text. New destinations use the global default encoding;
 * existing string destinations retain their encoding preference.
 * @param dest_id ID to create or update.
 * @param encoding Encoding of the input bytes.
 * @param source Exact input byte sequence; may be NULL only when source_size is zero.
 * @param source_size Number of input bytes, excluding any terminator.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input or allocation failure.
 */
static inline clm_int clm_string_from_enc(
    const clm_id dest_id,
    const clm_byte encoding,
    const void* source,
    const clm_uint source_size)
{
    char* converted = NULL;
    size_t converted_length;
    if (clm_string_decode_bytes(encoding, source, source_size,
                                &converted, &converted_length) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_int result = clm_string_from_from(dest_id, converted);
    free(converted);
    return result;
}

/** Alias of clm_string_from_enc for callers using the from_from naming form. */
static inline clm_int clm_string_from_from_enc(
    const clm_id dest_id,
    const clm_byte encoding,
    const void* source,
    const clm_uint source_size)
{
    return clm_string_from_enc(dest_id, encoding, source, source_size);
}

static inline clm_int clm_string_array_get_node(
    const clm_id id,
    clm_bpaa_array_node** array)
{
    clm_bpaa_array_node* found = clm_bpaa_find_array(id);
    if (found == NULL || found->type != CLM_BPA_TYPE_STRING ||
        found->encoding != CLM_STRING_ENCODING_UTF8)
        return CLM_BPA_FAILURE;
    *array = found;
    return CLM_BPA_SUCCESS;
}

static inline clm_int clm_string_array_validate_block(
    const clm_bpaa_array_node* array,
    const clm_id block_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    (void)array;
    size_t characters;
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        block->length > INT_MAX || (block->length != 0 && block->data == NULL))
        return CLM_BPA_FAILURE;
    return clm_string_measure(
        block->data != NULL ? (const char*)block->data : "",
        block->length, CLM_STRING_ENCODING_UTF8, &characters);
}

static inline clm_int clm_string_array_replace_bytes(
    clm_bpaa_array_node* array,
    const clm_int index,
    const char* text,
    const size_t byte_length)
{
    char* copy;
    clm_uint length;
    size_t characters;
    if (index < 0 || (clm_uint)index >= array->count ||
        text == NULL || byte_length > INT_MAX ||
        clm_string_measure(text, byte_length, CLM_STRING_ENCODING_UTF8,
                           &characters) != CLM_BPA_SUCCESS ||
        clm_bpa_copy_string(text, (clm_uint)(byte_length + 1),
                            &copy, &length) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    clm_bpa_pool_free(
        array->items.strings[index],
        strlen(array->items.strings[index]) + 1);
    array->items.strings[index] = copy;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Creates a string array; strings use the canonical UTF-8 internal encoding. */
static inline bool clm_string_array_new(const clm_id id, const clm_int reserved)
{
    return clm_bpaa_create(
        id, CLM_BPA_TYPE_STRING, reserved, CLM_STRING_ENCODING_UTF8) ==
        CLM_BPA_SUCCESS;
}

/** Returns the indexed string's character count, or CLM_BPA_FAILURE. */
static inline clm_int clm_string_array_length_at(
    const clm_id id,
    const clm_int index)
{
    clm_bpaa_array_node* array;
    size_t characters;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count ||
        clm_string_measure(array->items.strings[index],
                           strlen(array->items.strings[index]),
                           CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return (clm_int)characters;
}

/** Returns whether the indexed string contains a valid search string. */
static inline bool clm_string_array_contains_at(
    const clm_id id,
    const clm_int index,
    const char* search)
{
    clm_bpaa_array_node* array;
    size_t characters;
    if (search == NULL ||
        clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count ||
        clm_string_measure(search, strlen(search), CLM_STRING_ENCODING_UTF8,
                           &characters) != CLM_BPA_SUCCESS)
        return false;
    return strstr(array->items.strings[index], search) != NULL;
}

/** Appends text to one array string; returns appended character count. */
static inline clm_int clm_string_array_append_at(
    const clm_id id,
    const clm_int index,
    const char* suffix)
{
    clm_bpaa_array_node* array;
    size_t suffix_characters, current_characters;
    if (suffix == NULL ||
        clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    const char* current = array->items.strings[index];
    size_t current_bytes = strlen(current);
    size_t suffix_bytes = strlen(suffix);
    if (clm_string_measure(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                           &current_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(suffix, suffix_bytes, CLM_STRING_ENCODING_UTF8,
                           &suffix_characters) != CLM_BPA_SUCCESS ||
        current_bytes > (size_t)INT_MAX ||
        suffix_bytes > (size_t)INT_MAX - current_bytes)
        return CLM_BPA_FAILURE;
    size_t result_bytes = current_bytes + suffix_bytes;
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, current_bytes);
    memcpy(result + current_bytes, suffix, suffix_bytes + 1);
    if (clm_string_array_replace_bytes(array, index, result, result_bytes) !=
        CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return (clm_int)suffix_characters;
}

/** Appends text and a newline to one array string. */
static inline clm_int clm_string_array_append_line_at(
    const clm_id id,
    const clm_int index,
    const char* suffix)
{
    clm_bpaa_array_node* array;
    size_t suffix_characters, current_characters;
    if (suffix == NULL ||
        clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    const char* current = array->items.strings[index];
    const size_t current_bytes = strlen(current);
    const size_t suffix_bytes = strlen(suffix);
    if (clm_string_measure(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                           &current_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(suffix, suffix_bytes, CLM_STRING_ENCODING_UTF8,
                           &suffix_characters) != CLM_BPA_SUCCESS ||
        current_bytes >= (size_t)INT_MAX ||
        suffix_bytes > (size_t)INT_MAX - current_bytes - 1)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes + suffix_bytes + 1;
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, current_bytes);
    memcpy(result + current_bytes, suffix, suffix_bytes);
    result[current_bytes + suffix_bytes] = '\n';
    result[result_bytes] = '\0';
    if (clm_string_array_replace_bytes(array, index, result, result_bytes) !=
        CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return (clm_int)(suffix_characters + 1);
}

/** Inserts text at a character index in one array string. */
static inline clm_int clm_string_array_insert_at(
    const clm_id id,
    const clm_int index,
    const clm_int character_index,
    const char* inserted)
{
    clm_bpaa_array_node* array;
    size_t current_characters, inserted_characters;
    size_t byte_index;
    if (inserted == NULL || character_index < 0 ||
        clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    const char* current = array->items.strings[index];
    const size_t current_bytes = strlen(current);
    const size_t inserted_bytes = strlen(inserted);
    if (clm_string_measure(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                           &current_characters) != CLM_BPA_SUCCESS ||
        (size_t)character_index > current_characters ||
        current_bytes > (size_t)INT_MAX ||
        clm_string_measure(inserted, inserted_bytes, CLM_STRING_ENCODING_UTF8,
                           &inserted_characters) != CLM_BPA_SUCCESS ||
        clm_string_offset(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)character_index, &byte_index) != CLM_BPA_SUCCESS ||
        inserted_bytes > (size_t)INT_MAX - current_bytes)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes + inserted_bytes;
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, byte_index);
    memcpy(result + byte_index, inserted, inserted_bytes);
    memcpy(result + byte_index + inserted_bytes, current + byte_index,
           current_bytes - byte_index);
    result[result_bytes] = '\0';
    if (clm_string_array_replace_bytes(array, index, result, result_bytes) !=
        CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return (clm_int)inserted_characters;
}

/** Removes a character range from one array string. */
static inline clm_int clm_string_array_remove_at(
    const clm_id id,
    const clm_int index,
    const clm_int character_index,
    const clm_int length)
{
    clm_bpaa_array_node* array;
    size_t current_characters, start, end;
    if (character_index < 0 || length < 0 ||
        clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    const char* current = array->items.strings[index];
    const size_t current_bytes = strlen(current);
    if (clm_string_measure(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                           &current_characters) != CLM_BPA_SUCCESS ||
        current_bytes > (size_t)INT_MAX ||
        (size_t)character_index > current_characters ||
        (size_t)length > current_characters - (size_t)character_index)
        return CLM_BPA_FAILURE;
    if (length == 0)
        return 0;
    if (clm_string_offset(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)character_index, &start) != CLM_BPA_SUCCESS ||
        clm_string_offset(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                          (size_t)character_index + (size_t)length,
                          &end) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const size_t result_bytes = current_bytes - (end - start);
    char* result = clm_string_temp_alloc(result_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    memcpy(result, current, start);
    memcpy(result + start, current + end, current_bytes - end);
    result[result_bytes] = '\0';
    if (clm_string_array_replace_bytes(array, index, result, result_bytes) !=
        CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, result_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, result_bytes + 1);
    return length;
}

/** Replaces every non-overlapping match in one array string; returns match count. */
static inline clm_int clm_string_array_replace_at(
    const clm_id id,
    const clm_int index,
    const char* search,
    const char* replacement)
{
    clm_bpaa_array_node* array;
    const char* match;
    size_t current_characters, search_characters, replacement_characters;
    size_t count = 0, output_bytes;
    if (search == NULL || replacement == NULL || search[0] == '\0' ||
        clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    const char* current = array->items.strings[index];
    const size_t current_bytes = strlen(current);
    const size_t search_bytes = strlen(search);
    const size_t replacement_bytes = strlen(replacement);
    if (clm_string_measure(current, current_bytes, CLM_STRING_ENCODING_UTF8,
                           &current_characters) != CLM_BPA_SUCCESS ||
        current_bytes > (size_t)INT_MAX ||
        clm_string_measure(search, search_bytes, CLM_STRING_ENCODING_UTF8,
                           &search_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(replacement, replacement_bytes, CLM_STRING_ENCODING_UTF8,
                           &replacement_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const char* source = current;
    while ((match = strstr(source, search)) != NULL) {
        ++count;
        source = match + search_bytes;
    }
    if (count == 0)
        return 0;
    if (replacement_bytes >= search_bytes) {
        const size_t growth = replacement_bytes - search_bytes;
        if (growth != 0 && count > ((size_t)INT_MAX - current_bytes) / growth)
            return CLM_BPA_FAILURE;
        output_bytes = current_bytes + count * growth;
    } else {
        output_bytes = current_bytes - count * (search_bytes - replacement_bytes);
    }
    char* result = clm_string_temp_alloc(output_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    source = current;
    char* destination = result;
    while ((match = strstr(source, search)) != NULL) {
        const size_t prefix_bytes = (size_t)(match - source);
        memcpy(destination, source, prefix_bytes);
        destination += prefix_bytes;
        memcpy(destination, replacement, replacement_bytes);
        destination += replacement_bytes;
        source = match + search_bytes;
    }
    memcpy(destination, source, strlen(source) + 1);
    if (clm_string_array_replace_bytes(array, index, result, output_bytes) !=
        CLM_BPA_SUCCESS) {
        clm_string_temp_free(result, output_bytes + 1);
        return CLM_BPA_FAILURE;
    }
    clm_string_temp_free(result, output_bytes + 1);
    return (clm_int)count;
}

/** Deletes a string array without affecting arrays of other item types. */
static inline bool clm_string_array_delete(const clm_id id)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS &&
           clm_bpaa_delete(id) == CLM_BPA_SUCCESS;
}

/** Returns array item count, or CLM_BPA_FAILURE if missing/too large. */
static inline clm_int clm_string_array_count(const clm_id id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        array->count > INT_MAX)
        return CLM_BPA_FAILURE;
    return (clm_int)array->count;
}

/** Copies an array item into an existing string BPA block. */
static inline int clm_string_array_get(
    const clm_id id,
    const clm_int index,
    const clm_id destination_string_id)
{
    clm_bpaa_array_node* array;
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_string_id);
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        destination == NULL || destination->type != CLM_BPA_TYPE_STRING ||
        index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    if (clm_bpaa_get_string(id, index, destination_string_id) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    destination->encoding = array->encoding;
    return CLM_BPA_GET_SUCCESS;
}

/** Replaces an array item with an existing string BPA block's value. */
static inline clm_int clm_string_array_set(
    const clm_id id,
    const clm_int index,
    const clm_id source_string_id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_string(id, index, source_string_id);
}

/** Inserts a string block at an array index. */
static inline clm_int clm_string_array_insert(
    const clm_id id,
    const clm_int index,
    const clm_id source_string_id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_string(id, index, source_string_id);
}

/** Appends a string block to an array. */
static inline clm_int clm_string_array_append(const clm_id id, const clm_id source_string_id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_append_string(id, source_string_id);
}

/** Removes an array item by zero-based index. */
static inline clm_int clm_string_array_remove(const clm_id id, const clm_int index)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_remove(id, index) : CLM_BPA_FAILURE;
}

/** Clears array items while preserving its reserved capacity. */
static inline clm_int clm_string_array_clear(const clm_id id)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_clear(id) : CLM_BPA_FAILURE;
}

/** Finds the first exact string match; returns its index or CLM_BPA_FAILURE. */
static inline clm_int clm_string_array_find(
    const clm_id id,
    const clm_id source_string_id)
{
    clm_bpaa_array_node* array;
    const clm_bpa_block_node* source = clm_bpa_find_block(source_string_id);
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS ||
        source == NULL)
        return CLM_BPA_FAILURE;
    const char* source_text = source->data != NULL ? (const char*)source->data : "";
    for (clm_uint index = 0; index < array->count; ++index)
        if (strcmp(array->items.strings[index], source_text) == 0)
            return index <= INT_MAX ? (clm_int)index : CLM_BPA_FAILURE;
    return CLM_BPA_FAILURE;
}

static inline clm_int clm_string_array_compare(const void* left, const void* right)
{
    const char* const* left_string = (const char* const*)left;
    const char* const* right_string = (const char* const*)right;
    return strcmp(*left_string, *right_string);
}

/** Sorts strings lexicographically by their encoded bytes. */
static inline clm_int clm_string_array_sort(const clm_id id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (array->count > 1)
        qsort(array->items.strings, array->count, sizeof(char*),
              clm_string_array_compare);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Randomly shuffles string items and resets the draw deck. */
static inline clm_int clm_string_array_shuffle(const clm_id id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    for (clm_uint index = array->count; index > 1; --index) {
        const clm_uint other = (clm_uint)((unsigned long)rand() % index);
        char* temporary = array->items.strings[index - 1];
        array->items.strings[index - 1] = array->items.strings[other];
        array->items.strings[other] = temporary;
    }
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Moves the array cursor to the first item. */
static inline clm_int clm_string_array_move_first(const clm_id id)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_first(id) : CLM_BPA_FAILURE;
}

/** Moves the array cursor to the previous item. */
static inline clm_int clm_string_array_move_previous(const clm_id id)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_previous(id) : CLM_BPA_FAILURE;
}

/** Moves the array cursor to the next item. */
static inline clm_int clm_string_array_move_next(const clm_id id)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_next(id) : CLM_BPA_FAILURE;
}

/** Moves the array cursor to the last item. */
static inline clm_int clm_string_array_move_last(const clm_id id)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_last(id) : CLM_BPA_FAILURE;
}

/** Moves the array cursor to an item. */
static inline clm_int clm_string_array_move_to(const clm_id id, const clm_int index)
{
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_to(id, index) : CLM_BPA_FAILURE;
}

/** Copies the current array item into a string BPA block. */
static inline clm_int clm_string_array_get_current(
    const clm_id id,
    const clm_id destination_string_id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        array->index > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_string_array_get(id, (clm_int)array->index,
                                destination_string_id);
}

/** Draws an indexed item from the active deck into a string BPA block. */
static inline clm_int clm_string_array_draw_at(
    const clm_id id,
    const clm_int deck_index,
    const clm_id destination_string_id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        deck_index < 0 || (clm_uint)deck_index >= array->deck_size)
        return CLM_BPA_FAILURE;
    if (clm_string_array_get(id, deck_index, destination_string_id) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_uint last = array->deck_size - 1;
    char* temporary = array->items.strings[deck_index];
    array->items.strings[deck_index] = array->items.strings[last];
    array->items.strings[last] = temporary;
    --array->deck_size;
    return CLM_BPA_GET_SUCCESS;
}

/** Draws the next item, shuffling again when the active deck is exhausted. */
static inline clm_int clm_string_array_draw(
    const clm_id id,
    const clm_id destination_string_id)
{
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (array->deck_size == 0 &&
        clm_string_array_shuffle(id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_array_draw_at(id, 0, destination_string_id);
}

/* Define once in exactly one translation unit, next to CLM_BPA_IMPLEMENTATION. */
#ifdef CLM_STRING_IMPLEMENTATION
clm_byte clm_string_global_default_encoding = CLM_STRING_ENCODING_UTF8;
#endif


