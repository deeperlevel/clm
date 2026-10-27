#pragma once

#include "test.h"
#include "../clm/string.h"

static int clm_string_test_equals(const clm_id id, const char* expected)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING)
        return 0;
    const char* actual = block->data != NULL ? (const char*)block->data : "";
    return strcmp(actual, expected) == 0;
}

static int clm_string_tests(void)
{
    clm_test_result result = {0};
    const char utf8_initial[] = "A\xC3\xA9\xF0\x9F\x90\xB1";
    const char utf8_after_insert[] =
        "A\xE4\xB8\xAD\xC3\xA9\xF0\x9F\x90\xB1!";
    const char utf8_after_remove[] =
        "A\xC3\xA9\xF0\x9F\x90\xB1!";
    const char utf8_after_replace[] =
        "A\xE4\xB8\xAD\xF0\x9F\x90\xB1!";
    const clm_id string_id = UINT64_C(90001);
    const clm_id other_string_id = UINT64_C(90002);
    const clm_id array_id = UINT64_C(90003);
    const clm_id first_value_id = UINT64_C(90004);
    const clm_id second_value_id = UINT64_C(90005);
    const clm_id third_value_id = UINT64_C(90006);
    const clm_id scratch_id = UINT64_C(90007);

    clm_test_begin("CLM - String");

    CLM_ASSERT(clm_string_new(string_id, utf8_initial), "clm_string_new(string_id, utf8_initial)");
    CLM_ASSERT(clm_string_length(string_id) == 3, "clm_string_length(string_id) == 3");
    CLM_ASSERT(clm_bpa_find_block(string_id)->encoding == CLM_STRING_ENCODING_UTF8, "string block records UTF-8 encoding");
    CLM_ASSERT(clm_string_append(string_id, "!") == 1, "clm_string_append(string_id, \"!\") == 1");
    CLM_ASSERT(clm_string_insert(string_id, 1, "\xE4\xB8\xAD") == 1, "clm_string_insert(string_id, 1, UTF-8 character) == 1");
    CLM_ASSERT(clm_string_test_equals(string_id, utf8_after_insert), "string insert respects UTF-8 code-point index");
    CLM_ASSERT(clm_string_remove(string_id, 1, 1) == 1, "clm_string_remove(string_id, 1, 1) == 1");
    CLM_ASSERT(clm_string_test_equals(string_id, utf8_after_remove), "string remove updates content");
    CLM_ASSERT(clm_string_replace(string_id, "\xC3\xA9", "\xE4\xB8\xAD") == 1, "clm_string_replace replaces a UTF-8 match");
    CLM_ASSERT(clm_string_test_equals(string_id, utf8_after_replace), "string replace updates content");
    CLM_ASSERT(clm_string_replace(string_id, "!", "") == 1, "clm_string_replace allows an empty replacement");
    CLM_ASSERT(clm_string_contains(string_id, "\xF0\x9F\x90\xB1"), "clm_string_contains finds UTF-8 text");

    CLM_ASSERT(clm_string_append_line(string_id, "!") == 2, "clm_string_append_line appends text and newline");
    CLM_ASSERT(clm_string_contains(string_id, "\n"), "clm_string_contains finds newline");
    CLM_ASSERT(clm_string_length(string_id) > 0, "clm_string_length reports nonempty string");
    CLM_ASSERT(clm_string_remove(string_id, -1, 1) == CLM_BPA_FAILURE, "clm_string_remove rejects a negative index");
    CLM_ASSERT(clm_string_insert(string_id, INT_MAX, "x") == CLM_BPA_FAILURE, "clm_string_insert rejects an out-of-range index");
    CLM_ASSERT(clm_string_replace(string_id, "", "x") == CLM_BPA_FAILURE, "clm_string_replace rejects an empty search");
    CLM_ASSERT(!clm_string_new(UINT64_C(90008), "\xC0\xAF"), "clm_string_new rejects invalid UTF-8");

    {
        const clm_id latin_id = UINT64_C(90100);
        const clm_id latin_copy_id = UINT64_C(90101);
        const clm_id unicode_id = UINT64_C(90102);
        const clm_id unicode_copy_id = UINT64_C(90103);
        const clm_id invalid_id = UINT64_C(90104);
        const char latin_text[] = "caf\xC3\xA9";
        const char unicode_text[] = "A\xC3\xA9\xF0\x9F\x90\xB1";
        const unsigned char expected_latin[] = {'c', 'a', 'f', 0xE9};
        const unsigned char expected_utf16[] = {
            0x41, 0x00, 0xE9, 0x00, 0x3D, 0xD8, 0x31, 0xDC
        };
        const unsigned char malformed_utf16[] = {0x00, 0xD8};
        const unsigned char embedded_nul[] = {'A', 0, 'B'};
        unsigned char encoded[16];
        const char* borrowed_text = NULL;
        size_t borrowed_bytes = 0;

        CLM_ASSERT(clm_string_get_default_encoding() == CLM_STRING_ENCODING_UTF8, "string runtime default starts as UTF-8");
        CLM_ASSERT(clm_string_set_default_encoding(CLM_STRING_ENCODING_LATIN1) == CLM_BPA_SUCCESS, "set Latin-1 byte-export default");
        CLM_ASSERT(clm_string_new(latin_id, latin_text), "create UTF-8 text with Latin-1 export preference");
        CLM_ASSERT(clm_bpa_find_block(latin_id)->encoding == CLM_STRING_ENCODING_LATIN1, "new string records current export preference");
        CLM_ASSERT(clm_string_get_bytes(latin_id, NULL, 0) == 4, "query encoded Latin-1 byte count");
        memset(encoded, 0xA5, sizeof(encoded));
        CLM_ASSERT(clm_string_get_bytes(latin_id, encoded, 3) == CLM_BPA_FAILURE && encoded[0] == 0xA5, "undersized encoding output fails without writing");
        CLM_ASSERT(clm_string_get_bytes(latin_id, encoded, sizeof(encoded)) == 4 &&
                   memcmp(encoded, expected_latin, sizeof(expected_latin)) == 0, "export Latin-1 bytes exactly");
        CLM_ASSERT(clm_string_from_bytes(latin_copy_id, CLM_STRING_ENCODING_LATIN1,
                                        expected_latin, sizeof(expected_latin)) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(latin_copy_id, latin_text), "decode Latin-1 bytes into UTF-8 storage");
        CLM_ASSERT(clm_bpa_find_block(latin_copy_id)->encoding == CLM_STRING_ENCODING_LATIN1, "from_bytes records source encoding preference");
        CLM_ASSERT(clm_string_from_bytes(latin_copy_id, CLM_STRING_ENCODING_UTF8,
                                        "\xC0\xAF", 2) == CLM_BPA_FAILURE &&
                   clm_string_test_equals(latin_copy_id, latin_text), "invalid UTF-8 input preserves existing string");
        CLM_ASSERT(clm_string_from_bytes(invalid_id, CLM_STRING_ENCODING_UTF16LE,
                                        malformed_utf16, sizeof(malformed_utf16)) == CLM_BPA_FAILURE &&
                   !clm_bpa_is_existing(invalid_id), "unpaired UTF-16 surrogate does not create a string");
        CLM_ASSERT(clm_string_from_bytes(invalid_id, CLM_STRING_ENCODING_UTF8,
                                        embedded_nul, sizeof(embedded_nul)) == CLM_BPA_FAILURE &&
                   !clm_bpa_is_existing(invalid_id), "embedded NUL input is rejected");
        CLM_ASSERT(clm_string_from_bytes(invalid_id, CLM_STRING_ENCODING_UTF16LE,
                                        expected_utf16, 1) == CLM_BPA_FAILURE, "odd-length UTF-16 input is rejected");
        CLM_ASSERT(clm_string_set_default_encoding(0xFF) == CLM_BPA_FAILURE &&
                   clm_string_get_default_encoding() == CLM_STRING_ENCODING_LATIN1, "unsupported default encoding is rejected");
        CLM_ASSERT(clm_string_new(unicode_id, unicode_text), "create Unicode string for UTF-16LE and slicing");
        CLM_ASSERT(clm_string_change_encoding(unicode_id, CLM_STRING_ENCODING_UTF16LE) == CLM_BPA_SUCCESS, "change representable string export encoding to UTF-16LE");
        CLM_ASSERT(clm_string_get_bytes(unicode_id, NULL, 0) == (clm_int)sizeof(expected_utf16), "query UTF-16LE byte count");
        CLM_ASSERT(clm_string_get_bytes(unicode_id, encoded, sizeof(encoded)) == (clm_int)sizeof(expected_utf16) &&
                   memcmp(encoded, expected_utf16, sizeof(expected_utf16)) == 0, "export UTF-16LE including surrogate pair");
        CLM_ASSERT(clm_string_from_bytes(unicode_copy_id, CLM_STRING_ENCODING_UTF16LE,
                                        expected_utf16, sizeof(expected_utf16)) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(unicode_copy_id, unicode_text), "round-trip UTF-16LE through UTF-8 storage");
        CLM_ASSERT(clm_string_change_encoding(unicode_id, CLM_STRING_ENCODING_LATIN1) == CLM_BPA_FAILURE &&
                   clm_bpa_find_block(unicode_id)->encoding == CLM_STRING_ENCODING_UTF16LE, "unrepresentable encoding change fails and rolls back");
        CLM_ASSERT(clm_string_slice_borrow(unicode_id, 1, 2, &borrowed_text, &borrowed_bytes) == CLM_BPA_SUCCESS &&
                   borrowed_bytes == 6 && memcmp(borrowed_text, "\xC3\xA9\xF0\x9F\x90\xB1", borrowed_bytes) == 0,
                   "borrow Unicode string slice by code-point range");
        CLM_ASSERT(clm_string_slice(unicode_id, 1, 2, UINT64_C(90105)) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(UINT64_C(90105), "\xC3\xA9\xF0\x9F\x90\xB1"),
                   "copy Unicode string slice into independent BPA string");
        CLM_ASSERT(clm_bpa_find_block(UINT64_C(90105))->encoding == CLM_STRING_ENCODING_UTF16LE,
                   "copied string slice inherits preferred export encoding");
        CLM_ASSERT(clm_string_slice_borrow(unicode_id, 3, 0, &borrowed_text, &borrowed_bytes) == CLM_BPA_SUCCESS &&
                   borrowed_text != NULL && borrowed_bytes == 0, "empty borrowed string slice at end succeeds");
        CLM_ASSERT(clm_string_slice(unicode_id, 3, 0, UINT64_C(90107)) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(UINT64_C(90107), ""), "empty copied string slice at end succeeds");
        CLM_ASSERT(clm_string_slice(unicode_id, 4, 0, UINT64_C(90106)) == CLM_BPA_FAILURE &&
                   clm_string_slice(unicode_id, -1, 1, UINT64_C(90106)) == CLM_BPA_FAILURE,
                   "string copy slice rejects out-of-range and negative positions");
        CLM_ASSERT(clm_string_slice_borrow(unicode_id, 4, 0, &borrowed_text, &borrowed_bytes) == CLM_BPA_FAILURE,
                   "borrowed string slice rejects out-of-range position");
        CLM_ASSERT(clm_string_slice(unicode_id, 0, 1, latin_id) == CLM_BPA_FAILURE,
                   "string copy slice rejects an existing destination ID");
        CLM_ASSERT(clm_string_set(unicode_id, "changed") == 7 &&
                   clm_string_test_equals(UINT64_C(90105), "\xC3\xA9\xF0\x9F\x90\xB1"),
                   "copied string slice remains independent after source mutation");
        CLM_ASSERT(clm_string_set_default_encoding(CLM_STRING_ENCODING_UTF8) == CLM_BPA_SUCCESS,
                   "restore UTF-8 byte-export default");
        CLM_ASSERT(clm_bpa_find_block(latin_id)->encoding == CLM_STRING_ENCODING_LATIN1,
                   "changing runtime default does not alter existing string preference");
        CLM_ASSERT(clm_string_delete(latin_id), "delete Latin-1 test string");
        CLM_ASSERT(clm_string_delete(latin_copy_id), "delete Latin-1 round-trip string");
        CLM_ASSERT(clm_string_delete(unicode_id), "delete UTF-16LE test string");
        CLM_ASSERT(clm_string_delete(unicode_copy_id), "delete UTF-16LE round-trip string");
        CLM_ASSERT(clm_string_delete(UINT64_C(90105)), "delete copied Unicode slice");
        CLM_ASSERT(clm_string_delete(UINT64_C(90107)), "delete empty copied string slice");
    }

    CLM_ASSERT(clm_string_set(string_id, "red red") == 7, "clm_string_set replaces string content");
    CLM_ASSERT(clm_string_replace(string_id, "red", "blue") == 2, "clm_string_replace replaces every match");
    CLM_ASSERT(clm_string_test_equals(string_id, "blue blue"), "replace-all result is correct");
    CLM_ASSERT(clm_string_new(other_string_id, "scratch"), "clm_string_new(other_string_id, \"scratch\")");
    CLM_ASSERT(clm_string_delete(other_string_id), "clm_string_delete removes a string block");
    CLM_ASSERT(clm_bpa_create(UINT64_C(90008), CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS, "create a non-string block for delete type safety");
    CLM_ASSERT(!clm_string_delete(UINT64_C(90008)) && clm_bpa_is_existing(UINT64_C(90008)), "clm_string_delete preserves non-string blocks");
    CLM_ASSERT(clm_bpa_delete(UINT64_C(90008)) == CLM_BPA_SUCCESS, "delete non-string test block");

    CLM_ASSERT(clm_string_new(first_value_id, "pear"), "create first array string");
    CLM_ASSERT(clm_string_new(second_value_id, "apple"), "create second array string");
    CLM_ASSERT(clm_string_new(third_value_id, "banana"), "create third array string");
    CLM_ASSERT(clm_string_new(scratch_id, ""), "create array output string");
    CLM_ASSERT(clm_string_array_new(array_id, 0), "clm_string_array_new(array_id, 0)");
    CLM_ASSERT(clm_string_array_append(array_id, first_value_id) == CLM_BPA_SUCCESS, "append first string to array");
    CLM_ASSERT(clm_string_array_append(array_id, second_value_id) == CLM_BPA_SUCCESS, "append second string to array");
    CLM_ASSERT(clm_string_array_insert(array_id, 1, third_value_id) == CLM_BPA_SUCCESS, "insert string into array");
    CLM_ASSERT(clm_string_array_count(array_id) == 3, "clm_string_array_count(array_id) == 3");
    CLM_ASSERT(clm_string_array_find(array_id, third_value_id) == 1, "clm_string_array_find finds inserted item");
    CLM_ASSERT(clm_string_array_get(array_id, 1, scratch_id) == CLM_BPA_GET_SUCCESS, "get string array item into BPA string");
    CLM_ASSERT(clm_string_test_equals(scratch_id, "banana"), "get returns expected string");
    CLM_ASSERT(clm_string_array_length_at(array_id, 1) == 6, "clm_string_array_length_at(array_id, 1) == 6");
    CLM_ASSERT(clm_string_array_contains_at(array_id, 1, "ana"), "clm_string_array_contains_at finds substring");
    CLM_ASSERT(clm_string_array_append_at(array_id, 1, "!") == 1, "append to indexed array string");
    CLM_ASSERT(clm_string_array_append_line_at(array_id, 1, ".") == 2, "append line to indexed array string");
    CLM_ASSERT(clm_string_array_insert_at(array_id, 1, 0, "!") == 1, "insert into indexed array string");
    CLM_ASSERT(clm_string_array_remove_at(array_id, 1, 0, 1) == 1, "remove from indexed array string");
    CLM_ASSERT(clm_string_array_replace_at(array_id, 1, "a", "A") == 3, "replace all matches in indexed array string");
    CLM_ASSERT(clm_string_array_get(array_id, 1, scratch_id) == CLM_BPA_GET_SUCCESS, "get mutated string array item");
    CLM_ASSERT(clm_string_test_equals(scratch_id, "bAnAnA!.\n"), "indexed string operations produce expected content");
    CLM_ASSERT(clm_string_array_set(array_id, 0, second_value_id) == CLM_BPA_SET_SUCCESS, "set string array item from BPA string");
    CLM_ASSERT(clm_string_array_set(array_id, 1, third_value_id) == CLM_BPA_SET_SUCCESS, "reset mutated array item");
    CLM_ASSERT(clm_string_array_sort(array_id) == CLM_BPA_SUCCESS, "sort string array");
    CLM_ASSERT(clm_string_array_move_last(array_id) == CLM_BPA_SUCCESS, "move string array cursor to last");
    CLM_ASSERT(clm_string_array_get_current(array_id, scratch_id) == CLM_BPA_GET_SUCCESS, "get current string array item");
    CLM_ASSERT(clm_string_test_equals(scratch_id, "banana"), "sort and cursor return final string");
    CLM_ASSERT(clm_string_array_move_first(array_id) == CLM_BPA_SUCCESS, "move string array cursor to first");
    CLM_ASSERT(clm_string_array_move_next(array_id) == CLM_BPA_SUCCESS, "move string array cursor next");
    CLM_ASSERT(clm_string_array_move_previous(array_id) == CLM_BPA_SUCCESS, "move string array cursor previous");
    CLM_ASSERT(clm_string_array_move_to(array_id, 2) == CLM_BPA_SUCCESS, "move string array cursor to index");
    CLM_ASSERT(clm_string_array_remove(array_id, 1) == CLM_BPA_SUCCESS, "remove string array item");
    CLM_ASSERT(clm_string_array_count(array_id) == 2, "array count updates after removal");
    CLM_ASSERT(clm_string_array_shuffle(array_id) == CLM_BPA_SUCCESS, "shuffle string array");
    CLM_ASSERT(clm_string_array_draw_at(array_id, 0, scratch_id) == CLM_BPA_GET_SUCCESS, "draw indexed string from deck");
    CLM_ASSERT(clm_string_array_draw(array_id, scratch_id) == CLM_BPA_GET_SUCCESS, "draw next string from deck");
    CLM_ASSERT(clm_string_array_draw(array_id, scratch_id) == CLM_BPA_GET_SUCCESS, "draw reshuffles exhausted deck");
    CLM_ASSERT(clm_string_array_draw_at(array_id, 99, scratch_id) == CLM_BPA_FAILURE, "draw rejects out-of-range deck index");
    CLM_ASSERT(clm_string_array_clear(array_id) == CLM_BPA_SUCCESS, "clear string array");
    CLM_ASSERT(clm_string_array_count(array_id) == 0, "cleared array has zero items");
    CLM_ASSERT(clm_string_array_delete(array_id), "delete string array");
    CLM_ASSERT(clm_string_delete(first_value_id), "delete first array string");
    CLM_ASSERT(clm_string_delete(second_value_id), "delete second array string");
    CLM_ASSERT(clm_string_delete(third_value_id), "delete third array string");
    CLM_ASSERT(clm_string_delete(scratch_id), "delete array output string");
    CLM_ASSERT(clm_string_delete(string_id), "delete test string");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
