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

#include "string.h"

clm_byte clm_string_global_default_encoding = CLM_STRING_ENCODING_UTF8;

bool clm_string_encoding_supported(const clm_byte encoding) {
    return encoding == CLM_STRING_ENCODING_UTF8 ||
           encoding == CLM_STRING_ENCODING_LATIN1 ||
           encoding == CLM_STRING_ENCODING_UTF16LE;
}

clm_int clm_string_set_default_encoding(const clm_byte encoding) {
    if (!clm_string_encoding_supported(encoding)) return CLM_BPA_FAILURE;
    clm_string_global_default_encoding = encoding;
    return CLM_BPA_SUCCESS;
}

clm_byte clm_string_get_default_encoding(void) {
    return clm_string_global_default_encoding;
}

bool clm_string_utf8_next(
    const unsigned char* text,
    const size_t length,
    size_t* offset) {
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

clm_int clm_string_measure(
    const char* text,
    const size_t byte_length,
    const clm_byte encoding,
    size_t* character_count) {
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

clm_int clm_string_offset(
    const char* text,
    const size_t byte_length,
    const clm_byte encoding,
    const size_t character_index,
    size_t* byte_offset) {
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

clm_int clm_string_utf8_decode_one(
    const unsigned char* data,
    size_t size,
    size_t* offset,
    uint32_t* scalar) {
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

bool clm_string_is_whitespace(const uint32_t codepoint) {
    return (codepoint >= 0x0009 && codepoint <= 0x000D) ||
           codepoint == 0x0020 || codepoint == 0x0085 ||
           codepoint == 0x00A0 || codepoint == 0x1680 ||
           (codepoint >= 0x2000 && codepoint <= 0x200A) ||
           codepoint == 0x2028 || codepoint == 0x2029 ||
           codepoint == 0x202F || codepoint == 0x205F ||
           codepoint == 0x3000;
}

uint32_t clm_string_casefold_codepoint(const uint32_t codepoint) {
    if ((codepoint >= 'A' && codepoint <= 'Z') ||
        (codepoint >= 0x00C0 && codepoint <= 0x00D6) ||
        (codepoint >= 0x00D8 && codepoint <= 0x00DE) ||
        (codepoint >= 0x0391 && codepoint <= 0x03A1) ||
        (codepoint >= 0x03A3 && codepoint <= 0x03AB) ||
        (codepoint >= 0x0410 && codepoint <= 0x042F))
        return codepoint + 0x20;
    if (codepoint >= 0x0400 && codepoint <= 0x040F)
        return codepoint + 0x50;
    if (codepoint == 0x0178)
        return 0x00FF;
    if (codepoint == 0x03C2)
        return 0x03C3;
    if ((codepoint >= 0x0100 && codepoint <= 0x012F &&
         (codepoint & 1u) == 0) ||
        (codepoint >= 0x0132 && codepoint <= 0x0137 &&
         (codepoint & 1u) == 0) ||
        (codepoint >= 0x014A && codepoint <= 0x0177 &&
         (codepoint & 1u) == 0))
        return codepoint + 1;
    if (codepoint >= 0x0139 && codepoint <= 0x0148 &&
        (codepoint & 1u) == 1)
        return codepoint + 1;
    return codepoint;
}

clm_int clm_string_compare_text(
    const char* left,
    const size_t left_bytes,
    const char* right,
    const size_t right_bytes,
    const bool ignore_case) {
    size_t left_offset = 0, right_offset = 0;
    while (left_offset < left_bytes && right_offset < right_bytes) {
        uint32_t left_codepoint, right_codepoint;
        if (clm_string_utf8_decode_one((const unsigned char*)left, left_bytes,
                                       &left_offset, &left_codepoint) != CLM_BPA_SUCCESS ||
            clm_string_utf8_decode_one((const unsigned char*)right, right_bytes,
                                       &right_offset, &right_codepoint) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        if (ignore_case) {
            left_codepoint = clm_string_casefold_codepoint(left_codepoint);
            right_codepoint = clm_string_casefold_codepoint(right_codepoint);
        }
        if (left_codepoint != right_codepoint)
            return left_codepoint < right_codepoint ? -1 : 1;
    }
    if (left_offset < left_bytes) return 1;
    if (right_offset < right_bytes) return -1;
    return 0;
}

clm_int clm_string_trim_bounds(
    const char* text,
    const size_t byte_length,
    const bool trim_left,
    const bool trim_right,
    size_t* begin,
    size_t* end,
    size_t* removed) {
    size_t first = 0, last = byte_length, count = 0;
    if (text == NULL || begin == NULL || end == NULL || removed == NULL)
        return CLM_BPA_FAILURE;
    if (trim_left) {
        while (first < last) {
            size_t next = first;
            uint32_t codepoint;
            if (clm_string_utf8_decode_one((const unsigned char*)text, last,
                                           &next, &codepoint) != CLM_BPA_SUCCESS)
                return CLM_BPA_FAILURE;
            if (!clm_string_is_whitespace(codepoint))
                break;
            first = next;
            ++count;
        }
    }
    if (trim_right) {
        size_t offset = first, last_non_whitespace = first;
        size_t trailing_count = 0;
        while (offset < last) {
            size_t next = offset;
            uint32_t codepoint;
            if (clm_string_utf8_decode_one((const unsigned char*)text, last,
                                           &next, &codepoint) != CLM_BPA_SUCCESS)
                return CLM_BPA_FAILURE;
            if (clm_string_is_whitespace(codepoint)) {
                ++trailing_count;
            } else {
                last_non_whitespace = next;
                trailing_count = 0;
            }
            offset = next;
        }
        last = last_non_whitespace;
        count += trailing_count;
    }
    *begin = first;
    *end = last;
    *removed = count;
    return CLM_BPA_SUCCESS;
}

clm_int clm_string_get_block(
    const clm_id id,
    clm_bpa_block_node** block,
    const char** text,
    size_t* byte_length,
    size_t* character_count) {
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

clm_int clm_string_replace_bytes(
    const clm_id id,
    const char* text,
    const size_t byte_length) {
    if (text == NULL || byte_length >= UINT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpa_replace(id, text, (clm_uint)(byte_length + 1)) < 0
        ? CLM_BPA_FAILURE : CLM_BPA_SUCCESS;
}

char* clm_string_temp_alloc(const size_t bytes) {
    return bytes == 0 ? NULL : (char*)clm_bpa_pool_alloc(bytes);
}

void clm_string_temp_free(char* text, const size_t bytes) {
    clm_bpa_pool_free(text, bytes);
}

bool clm_string_new(const clm_id id, const char* text) {
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

bool clm_string_delete(const clm_id id) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    return block != NULL && block->type == CLM_BPA_TYPE_STRING &&
           clm_bpa_delete(id) == CLM_BPA_SUCCESS;
}

clm_int clm_string_set(const clm_id dest_id, const char* text) {
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

clm_int clm_string_from_from(const clm_id dest_id, const char* source) {
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

clm_int clm_string_set_from(const clm_id dest_id, const char* text) {
    return clm_string_set(dest_id, text);
}

clm_int clm_string_append(const clm_id id, const char* suffix) {
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

clm_int clm_string_append_line(const clm_id id, const char* suffix) {
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

clm_int clm_string_insert(
    const clm_id id,
    const clm_int index,
    const char* inserted) {
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

clm_int clm_string_remove(
    const clm_id id,
    const clm_int index,
    const clm_int length) {
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

bool clm_string_contains(const clm_id id, const char* search) {
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

clm_int clm_string_length(const clm_id id) {
    clm_bpa_block_node* block;
    const char* current;
    size_t current_bytes, current_characters;
    return clm_string_get_block(id, &block, &current, &current_bytes,
                                &current_characters) == CLM_BPA_SUCCESS
        ? (clm_int)current_characters : CLM_BPA_FAILURE;
}

clm_int clm_string_compare_case(
    const clm_id left_id,
    const clm_id right_id,
    const bool ignore_case) {
    clm_bpa_block_node* left_block;
    clm_bpa_block_node* right_block;
    const char* left;
    const char* right;
    size_t left_bytes, right_bytes, left_characters, right_characters;
    if (clm_string_get_block(left_id, &left_block, &left, &left_bytes,
                             &left_characters) != CLM_BPA_SUCCESS ||
        clm_string_get_block(right_id, &right_block, &right, &right_bytes,
                             &right_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    (void)left_block;
    (void)right_block;
    return clm_string_compare_text(left, left_bytes, right, right_bytes,
                                   ignore_case);
}

clm_int clm_string_compare(
    const clm_id left_id,
    const clm_id right_id) {
    return clm_string_compare_case(left_id, right_id, false);
}

clm_int clm_string_compare_ignore_case(
    const clm_id left_id,
    const clm_id right_id) {
    return clm_string_compare_case(left_id, right_id, true);
}

clm_int clm_string_compare_from(
    const char* left,
    const char* right) {
    size_t left_characters, right_characters;
    if (left == NULL || right == NULL ||
        clm_string_measure(left, strlen(left), CLM_STRING_ENCODING_UTF8,
                           &left_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(right, strlen(right), CLM_STRING_ENCODING_UTF8,
                           &right_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_compare_text(left, strlen(left), right, strlen(right), false);
}

clm_int clm_string_compare_ignore_case_from(
    const char* left,
    const char* right) {
    size_t left_characters, right_characters;
    if (left == NULL || right == NULL ||
        clm_string_measure(left, strlen(left), CLM_STRING_ENCODING_UTF8,
                           &left_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(right, strlen(right), CLM_STRING_ENCODING_UTF8,
                           &right_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_compare_text(left, strlen(left), right, strlen(right), true);
}

clm_int clm_string_array_append_bytes(
    const clm_id array_id,
    const char* text,
    const size_t byte_length) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->type != CLM_BPA_TYPE_STRING ||
        array->count > INT_MAX || text == NULL || byte_length > INT_MAX)
        return CLM_BPA_FAILURE;
    char* terminated = clm_string_temp_alloc(byte_length + 1);
    if (terminated == NULL)
        return CLM_BPA_FAILURE;
    memcpy(terminated, text, byte_length);
    terminated[byte_length] = '\0';
    const clm_int result = clm_bpaa_insert_raw(
        array_id, (clm_int)array->count, terminated, (clm_uint)(byte_length + 1));
    clm_string_temp_free(terminated, byte_length + 1);
    return result;
}

clm_int clm_string_split_text(
    const clm_id destination_array_id,
    const char* text,
    const size_t text_bytes,
    const char* delimiter,
    const size_t delimiter_bytes) {
    size_t characters;
    if (text == NULL || delimiter == NULL || delimiter_bytes == 0 ||
        text_bytes > INT_MAX ||
        clm_string_measure(text, text_bytes, CLM_STRING_ENCODING_UTF8,
                           &characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(delimiter, delimiter_bytes,
                           CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS ||
        clm_bpaa_create(destination_array_id, CLM_BPA_TYPE_STRING, 0,
                        CLM_STRING_ENCODING_UTF8) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    size_t start = 0;
    while (start <= text_bytes) {
        const char* match = strstr(text + start, delimiter);
        const size_t end = match != NULL ? (size_t)(match - text) : text_bytes;
        if (clm_string_array_append_bytes(destination_array_id, text + start,
                                          end - start) != CLM_BPA_SUCCESS) {
            (void)clm_bpaa_delete(destination_array_id);
            return CLM_BPA_FAILURE;
        }
        if (match == NULL)
            break;
        start = end + delimiter_bytes;
    }
    return CLM_BPA_SUCCESS;
}

clm_int clm_string_split(
    const clm_id destination_array_id,
    const clm_id source_string_id,
    const clm_id delimiter_string_id) {
    clm_bpa_block_node* source_block;
    clm_bpa_block_node* delimiter_block;
    const char* source;
    const char* delimiter;
    size_t source_bytes, delimiter_bytes, source_characters, delimiter_characters;
    if (clm_string_get_block(source_string_id, &source_block, &source,
                             &source_bytes, &source_characters) != CLM_BPA_SUCCESS ||
        clm_string_get_block(delimiter_string_id, &delimiter_block, &delimiter,
                             &delimiter_bytes, &delimiter_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    (void)source_block;
    (void)delimiter_block;
    return clm_string_split_text(destination_array_id, source, source_bytes,
                                 delimiter, delimiter_bytes);
}

clm_int clm_string_split_from(
    const clm_id destination_array_id,
    const char* text,
    const char* delimiter) {
    if (text == NULL || delimiter == NULL)
        return CLM_BPA_FAILURE;
    return clm_string_split_text(destination_array_id, text, strlen(text),
                                 delimiter, strlen(delimiter));
}

clm_int clm_string_split_lines_text(
    const clm_id destination_array_id,
    const char* text,
    const size_t text_bytes) {
    size_t characters;
    if (text == NULL || text_bytes > INT_MAX ||
        clm_string_measure(text, text_bytes, CLM_STRING_ENCODING_UTF8,
                           &characters) != CLM_BPA_SUCCESS ||
        clm_bpaa_create(destination_array_id, CLM_BPA_TYPE_STRING, 0,
                        CLM_STRING_ENCODING_UTF8) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    size_t start = 0, offset = 0;
    while (offset < text_bytes) {
        const size_t character_start = offset;
        uint32_t codepoint;
        if (clm_string_utf8_decode_one((const unsigned char*)text, text_bytes,
                                       &offset, &codepoint) != CLM_BPA_SUCCESS) {
            (void)clm_bpaa_delete(destination_array_id);
            return CLM_BPA_FAILURE;
        }
        if (codepoint == '\r' || codepoint == '\n' || codepoint == 0x85 ||
            codepoint == 0x2028 || codepoint == 0x2029) {
            const size_t line_end = character_start;
            if (codepoint == '\r' && offset < text_bytes && text[offset] == '\n')
                ++offset;
            if (clm_string_array_append_bytes(destination_array_id, text + start,
                                              line_end - start) != CLM_BPA_SUCCESS) {
                (void)clm_bpaa_delete(destination_array_id);
                return CLM_BPA_FAILURE;
            }
            start = offset;
        }
    }
    if (clm_string_array_append_bytes(destination_array_id, text + start,
                                      text_bytes - start) != CLM_BPA_SUCCESS) {
        (void)clm_bpaa_delete(destination_array_id);
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}

clm_int clm_string_split_lines(
    const clm_id destination_array_id,
    const clm_id source_string_id) {
    clm_bpa_block_node* block;
    const char* text;
    size_t byte_length, characters;
    if (clm_string_get_block(source_string_id, &block, &text, &byte_length,
                             &characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    (void)block;
    return clm_string_split_lines_text(destination_array_id, text, byte_length);
}

clm_int clm_string_split_lines_from(
    const clm_id destination_array_id,
    const char* text) {
    if (text == NULL)
        return CLM_BPA_FAILURE;
    return clm_string_split_lines_text(destination_array_id, text, strlen(text));
}

clm_int clm_string_trim_id(
    const clm_id id,
    const bool trim_left,
    const bool trim_right) {
    clm_bpa_block_node* block;
    const char* text;
    size_t byte_length, characters, begin, end, removed;
    if (clm_string_get_block(id, &block, &text, &byte_length, &characters) != CLM_BPA_SUCCESS ||
        clm_string_trim_bounds(text, byte_length, trim_left, trim_right,
                               &begin, &end, &removed) != CLM_BPA_SUCCESS ||
        removed > INT_MAX)
        return CLM_BPA_FAILURE;
    if (removed != 0) {
        char* trimmed = clm_string_temp_alloc(end - begin + 1);
        if (trimmed == NULL)
            return CLM_BPA_FAILURE;
        memcpy(trimmed, text + begin, end - begin);
        trimmed[end - begin] = '\0';
        const clm_int status = clm_string_replace_bytes(id, trimmed, end - begin);
        clm_string_temp_free(trimmed, end - begin + 1);
        if (status != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
    }
    return (clm_int)removed;
}

clm_int clm_string_trim_text_to_id(
    const clm_id destination_string_id,
    const char* text,
    const bool trim_left,
    const bool trim_right) {
    size_t text_characters, begin, end, removed;
    if (text == NULL ||
        clm_string_measure(text, strlen(text), CLM_STRING_ENCODING_UTF8,
                           &text_characters) != CLM_BPA_SUCCESS ||
        clm_string_trim_bounds(text, strlen(text), trim_left, trim_right,
                               &begin, &end, &removed) != CLM_BPA_SUCCESS ||
        removed > INT_MAX)
        return CLM_BPA_FAILURE;
    char* trimmed = clm_string_temp_alloc(end - begin + 1);
    if (trimmed == NULL)
        return CLM_BPA_FAILURE;
    memcpy(trimmed, text + begin, end - begin);
    trimmed[end - begin] = '\0';
    const clm_int result = clm_string_from_from(destination_string_id, trimmed);
    clm_string_temp_free(trimmed, end - begin + 1);
    return result < 0 ? CLM_BPA_FAILURE : (clm_int)removed;
}

clm_int clm_string_left_trim(const clm_id id) {
    return clm_string_trim_id(id, true, false);
}

clm_int clm_string_right_trim(const clm_id id) {
    return clm_string_trim_id(id, false, true);
}

clm_int clm_string_trim(const clm_id id) {
    return clm_string_trim_id(id, true, true);
}

clm_int clm_string_left_trim_from(
    const clm_id destination_string_id,
    const char* text) {
    return clm_string_trim_text_to_id(destination_string_id, text, true, false);
}

clm_int clm_string_right_trim_from(
    const clm_id destination_string_id,
    const char* text) {
    return clm_string_trim_text_to_id(destination_string_id, text, false, true);
}

clm_int clm_string_trim_from(
    const clm_id destination_string_id,
    const char* text) {
    return clm_string_trim_text_to_id(destination_string_id, text, true, true);
}

clm_int clm_string_replace_from(
    const clm_id id,
    const char* search,
    const char* replacement) {
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

clm_int clm_string_replace(
    const clm_id id,
    const char* search,
    const char* replacement) {
    return clm_string_replace_from(id, search, replacement);
}

size_t clm_string_utf8_encode_one(uint32_t cp, unsigned char* out) {
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

clm_int clm_string_decode_bytes(const clm_byte encoding,
    const void* input, size_t size, char** output, size_t* output_length) {
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

clm_int clm_string_get_bytes(const clm_id id, void* buffer, const clm_uint buffer_size) {
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

clm_int clm_string_from_bytes(const clm_id id, const clm_byte encoding, const void* buffer, const clm_uint buffer_size) {
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

clm_int clm_string_change_encoding(const clm_id id, const clm_byte new_encoding) {
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

clm_int clm_string_slice(const clm_id source_id, const clm_int start,
                                   const clm_int length, const clm_id destination_id) {
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

clm_int clm_string_slice_borrow(
    const clm_id source_id,
    const clm_int start,
    const clm_int length,
    const char** text,
    size_t* byte_length) {
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

clm_int clm_string_from(const clm_id dest_id, const clm_id source_id) {
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

clm_int clm_string_from_enc(
    const clm_id dest_id,
    const clm_byte encoding,
    const void* source,
    const clm_uint source_size) {
    char* converted = NULL;
    size_t converted_length;
    if (clm_string_decode_bytes(encoding, source, source_size,
                                &converted, &converted_length) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_int result = clm_string_from_from(dest_id, converted);
    free(converted);
    return result;
}

clm_int clm_string_from_from_enc(
    const clm_id dest_id,
    const clm_byte encoding,
    const void* source,
    const clm_uint source_size) {
    return clm_string_from_enc(dest_id, encoding, source, source_size);
}

clm_int clm_string_array_get_node(
    const clm_id id,
    clm_bpaa_array_node** array) {
    clm_bpaa_array_node* found = clm_bpaa_find_array(id);
    if (found == NULL || found->type != CLM_BPA_TYPE_STRING ||
        found->encoding != CLM_STRING_ENCODING_UTF8)
        return CLM_BPA_FAILURE;
    *array = found;
    return CLM_BPA_SUCCESS;
}

clm_int clm_string_array_validate_block(
    const clm_bpaa_array_node* array,
    const clm_id block_id) {
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

clm_int clm_string_array_replace_bytes(
    clm_bpaa_array_node* array,
    const clm_int index,
    const char* text,
    const size_t byte_length) {
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

clm_int clm_string_array_compare_text(
    const clm_id array_id,
    const clm_int index,
    const char* text,
    const size_t text_bytes,
    const bool ignore_case) {
    clm_bpaa_array_node* array;
    size_t array_characters, text_characters;
    if (text == NULL ||
        clm_string_array_get_node(array_id, &array) != CLM_BPA_SUCCESS ||
        index < 0 || (clm_uint)index >= array->count ||
        clm_string_measure(array->items.strings[index],
                           strlen(array->items.strings[index]),
                           CLM_STRING_ENCODING_UTF8,
                           &array_characters) != CLM_BPA_SUCCESS ||
        clm_string_measure(text, text_bytes, CLM_STRING_ENCODING_UTF8,
                           &text_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_compare_text(array->items.strings[index],
                                   strlen(array->items.strings[index]),
                                   text, text_bytes, ignore_case);
}

clm_int clm_string_array_compare(
    const clm_id array_id,
    const clm_int index,
    const clm_id string_id) {
    clm_bpa_block_node* block;
    const char* text;
    size_t byte_length, characters;
    if (clm_string_get_block(string_id, &block, &text, &byte_length,
                             &characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    (void)block;
    return clm_string_array_compare_text(array_id, index, text, byte_length, false);
}

clm_int clm_string_array_compare_ignore_case(
    const clm_id array_id,
    const clm_int index,
    const clm_id string_id) {
    clm_bpa_block_node* block;
    const char* text;
    size_t byte_length, characters;
    if (clm_string_get_block(string_id, &block, &text, &byte_length,
                             &characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    (void)block;
    return clm_string_array_compare_text(array_id, index, text, byte_length, true);
}

clm_int clm_string_array_compare_from(
    const clm_id array_id,
    const clm_int index,
    const char* text) {
    return text != NULL
        ? clm_string_array_compare_text(array_id, index, text, strlen(text), false)
        : CLM_BPA_FAILURE;
}

clm_int clm_string_array_compare_ignore_case_from(
    const clm_id array_id,
    const clm_int index,
    const char* text) {
    return text != NULL
        ? clm_string_array_compare_text(array_id, index, text, strlen(text), true)
        : CLM_BPA_FAILURE;
}

clm_int clm_string_array_trim_all(
    const clm_id id,
    const bool trim_left,
    const bool trim_right) {
    clm_bpaa_array_node* array;
    size_t total_removed = 0;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    for (clm_uint index = 0; index < array->count; ++index) {
        size_t begin, end, removed;
        const char* text = array->items.strings[index];
        if (clm_string_trim_bounds(text, strlen(text), trim_left, trim_right,
                                   &begin, &end, &removed) != CLM_BPA_SUCCESS ||
            removed > (size_t)INT_MAX - total_removed)
            return CLM_BPA_FAILURE;
        total_removed += removed;
    }
    if (total_removed == 0)
        return 0;
    for (clm_uint index = 0; index < array->count; ++index) {
        size_t begin, end, removed;
        const char* text = array->items.strings[index];
        const size_t byte_length = strlen(text);
        if (clm_string_trim_bounds(text, byte_length, trim_left, trim_right,
                                   &begin, &end, &removed) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        if (removed != 0) {
            char* trimmed = clm_string_temp_alloc(end - begin + 1);
            if (trimmed == NULL)
                return CLM_BPA_FAILURE;
            memcpy(trimmed, text + begin, end - begin);
            trimmed[end - begin] = '\0';
            const clm_int status = clm_string_array_replace_bytes(
                array, (clm_int)index, trimmed, end - begin);
            clm_string_temp_free(trimmed, end - begin + 1);
            if (status != CLM_BPA_SUCCESS)
                return CLM_BPA_FAILURE;
        }
    }
    return (clm_int)total_removed;
}

clm_int clm_string_array_left_trim(const clm_id id) {
    return clm_string_array_trim_all(id, true, false);
}

clm_int clm_string_array_right_trim(const clm_id id) {
    return clm_string_array_trim_all(id, false, true);
}

clm_int clm_string_array_trim(const clm_id id) {
    return clm_string_array_trim_all(id, true, true);
}

clm_int clm_string_array_join_text(
    const clm_id destination_string_id,
    const clm_id array_id,
    const char* separator,
    const size_t separator_bytes) {
    clm_bpaa_array_node* array;
    size_t separator_characters, output_bytes = 0, output_characters = 0;
    if (separator == NULL ||
        clm_string_array_get_node(array_id, &array) != CLM_BPA_SUCCESS ||
        clm_string_measure(separator, separator_bytes, CLM_STRING_ENCODING_UTF8,
                           &separator_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    for (clm_uint index = 0; index < array->count; ++index) {
        const char* item = array->items.strings[index];
        size_t item_characters;
        const size_t item_bytes = strlen(item);
        if (clm_string_measure(item, item_bytes, CLM_STRING_ENCODING_UTF8,
                               &item_characters) != CLM_BPA_SUCCESS ||
            item_bytes > (size_t)INT_MAX - output_bytes ||
            item_characters > (size_t)INT_MAX - output_characters)
            return CLM_BPA_FAILURE;
        output_bytes += item_bytes;
        output_characters += item_characters;
        if (index + 1 < array->count) {
            if (separator_bytes > (size_t)INT_MAX - output_bytes ||
                separator_characters > (size_t)INT_MAX - output_characters)
                return CLM_BPA_FAILURE;
            output_bytes += separator_bytes;
            output_characters += separator_characters;
        }
    }
    char* result = clm_string_temp_alloc(output_bytes + 1);
    if (result == NULL)
        return CLM_BPA_FAILURE;
    size_t offset = 0;
    for (clm_uint index = 0; index < array->count; ++index) {
        const size_t item_bytes = strlen(array->items.strings[index]);
        memcpy(result + offset, array->items.strings[index], item_bytes);
        offset += item_bytes;
        if (index + 1 < array->count) {
            memcpy(result + offset, separator, separator_bytes);
            offset += separator_bytes;
        }
    }
    result[output_bytes] = '\0';
    const clm_int status = clm_string_from_from(destination_string_id, result);
    clm_string_temp_free(result, output_bytes + 1);
    return status < 0 ? CLM_BPA_FAILURE : (clm_int)output_characters;
}

clm_int clm_string_array_join(
    const clm_id destination_string_id,
    const clm_id array_id,
    const clm_id separator_string_id) {
    clm_bpa_block_node* block;
    const char* separator;
    size_t separator_bytes, separator_characters;
    if (clm_string_get_block(separator_string_id, &block, &separator,
                             &separator_bytes, &separator_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    (void)block;
    return clm_string_array_join_text(destination_string_id, array_id,
                                      separator, separator_bytes);
}

clm_int clm_string_array_join_from(
    const clm_id destination_string_id,
    const clm_id array_id,
    const char* separator) {
    size_t separator_characters;
    if (separator == NULL ||
        clm_string_measure(separator, strlen(separator), CLM_STRING_ENCODING_UTF8,
                           &separator_characters) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_array_join_text(destination_string_id, array_id,
                                      separator, strlen(separator));
}

bool clm_string_array_new(const clm_id id, const clm_int reserved) {
    return clm_bpaa_create(
        id, CLM_BPA_TYPE_STRING, reserved, CLM_STRING_ENCODING_UTF8) ==
        CLM_BPA_SUCCESS;
}

clm_int clm_string_array_length_at(
    const clm_id id,
    const clm_int index) {
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

bool clm_string_array_contains_at(
    const clm_id id,
    const clm_int index,
    const char* search) {
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

clm_int clm_string_array_append_at(
    const clm_id id,
    const clm_int index,
    const char* suffix) {
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

clm_int clm_string_array_append_line_at(
    const clm_id id,
    const clm_int index,
    const char* suffix) {
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

clm_int clm_string_array_insert_at(
    const clm_id id,
    const clm_int index,
    const clm_int character_index,
    const char* inserted) {
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

clm_int clm_string_array_remove_at(
    const clm_id id,
    const clm_int index,
    const clm_int character_index,
    const clm_int length) {
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

clm_int clm_string_array_replace_at(
    const clm_id id,
    const clm_int index,
    const char* search,
    const char* replacement) {
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

bool clm_string_array_delete(const clm_id id) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS &&
           clm_bpaa_delete(id) == CLM_BPA_SUCCESS;
}

clm_int clm_string_array_count(const clm_id id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        array->count > INT_MAX)
        return CLM_BPA_FAILURE;
    return (clm_int)array->count;
}

int clm_string_array_get(
    const clm_id id,
    const clm_int index,
    const clm_id destination_string_id) {
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

clm_int clm_string_array_set(
    const clm_id id,
    const clm_int index,
    const clm_id source_string_id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_string(id, index, source_string_id);
}

clm_int clm_string_array_insert(
    const clm_id id,
    const clm_int index,
    const clm_id source_string_id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_string(id, index, source_string_id);
}

clm_int clm_string_array_append(const clm_id id, const clm_id source_string_id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        clm_string_array_validate_block(array, source_string_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_append_string(id, source_string_id);
}

clm_int clm_string_array_remove(const clm_id id, const clm_int index) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_remove(id, index) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_clear(const clm_id id) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_clear(id) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_find(
    const clm_id id,
    const clm_id source_string_id) {
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

int clm_string_array_sort_compare(const void* left, const void* right) {
    const char* const* left_string = (const char* const*)left;
    const char* const* right_string = (const char* const*)right;
    return strcmp(*left_string, *right_string);
}

clm_int clm_string_array_sort(const clm_id id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (array->count > 1)
        qsort(array->items.strings, array->count, sizeof(char*),
              clm_string_array_sort_compare);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

clm_int clm_string_array_shuffle(const clm_id id) {
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

clm_int clm_string_array_move_first(const clm_id id) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_first(id) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_move_previous(const clm_id id) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_previous(id) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_move_next(const clm_id id) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_next(id) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_move_last(const clm_id id) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_last(id) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_move_to(const clm_id id, const clm_int index) {
    clm_bpaa_array_node* array;
    return clm_string_array_get_node(id, &array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_to(id, index) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_get_current(
    const clm_id id,
    const clm_id destination_string_id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS ||
        array->index > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_string_array_get(id, (clm_int)array->index,
                                destination_string_id);
}

clm_int clm_string_array_draw_at(
    const clm_id id,
    const clm_int deck_index,
    const clm_id destination_string_id) {
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

clm_int clm_string_array_draw(
    const clm_id id,
    const clm_id destination_string_id) {
    clm_bpaa_array_node* array;
    if (clm_string_array_get_node(id, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (array->deck_size == 0 &&
        clm_string_array_shuffle(id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_array_draw_at(id, 0, destination_string_id);
}
