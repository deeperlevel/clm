#pragma once

#include "test.hpp"
#include "../clm/string.hpp"

static int clm_string_test_equals(const clm_id id, const char* expected)
{
    const std::string* actual = clm::string::value(id);
    return actual != nullptr && *actual == expected;
}

static int clm_string_array_test_equals(
    const clm_id id,
    const clm_int index,
    const char* expected)
{
    const auto actual = clm::string::array_value(id, index);
    return actual.has_value() && *actual == expected;
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

    CLM_ASSERT(clm::string::create(string_id, utf8_initial), "clm::string::create(string_id, utf8_initial)");
    CLM_ASSERT(clm::string::length(string_id) == 3, "clm::string::length(string_id) == 3");
    CLM_ASSERT(clm::string::preferred_encoding(string_id).value() == clm::string::encoding::utf8, "string block records UTF-8 encoding");
    CLM_ASSERT(clm::string::append(string_id, "!") == 1, "clm::string::append(string_id, \"!\") == 1");
    CLM_ASSERT(clm::string::insert(string_id, 1, "\xE4\xB8\xAD") == 1, "clm::string::insert(string_id, 1, UTF-8 character) == 1");
    CLM_ASSERT(clm_string_test_equals(string_id, utf8_after_insert), "string insert respects UTF-8 code-point index");
    CLM_ASSERT(clm::string::remove(string_id, 1, 1) == 1, "clm::string::remove(string_id, 1, 1) == 1");
    CLM_ASSERT(clm_string_test_equals(string_id, utf8_after_remove), "string remove updates content");
    CLM_ASSERT(clm::string::replace(string_id, "\xC3\xA9", "\xE4\xB8\xAD") == 1, "clm::string::replace replaces a UTF-8 match");
    CLM_ASSERT(clm_string_test_equals(string_id, utf8_after_replace), "string replace updates content");
    CLM_ASSERT(clm::string::replace(string_id, "!", "") == 1, "clm::string::replace allows an empty replacement");
    CLM_ASSERT(clm::string::contains(string_id, "\xF0\x9F\x90\xB1"), "clm::string::contains finds UTF-8 text");

    CLM_ASSERT(clm::string::append_line(string_id, "!") == 2, "clm::string::append_line appends text and newline");
    CLM_ASSERT(clm::string::contains(string_id, "\n"), "clm::string::contains finds newline");
    CLM_ASSERT(clm::string::length(string_id) > 0, "clm::string::length reports nonempty string");
    CLM_ASSERT(clm::string::remove(string_id, -1, 1) == CLM_BPA_FAILURE, "clm::string::remove rejects a negative index");
    CLM_ASSERT(clm::string::insert(string_id, INT_MAX, "x") == CLM_BPA_FAILURE, "clm::string::insert rejects an out-of-range index");
    CLM_ASSERT(clm::string::replace(string_id, "", "x") == CLM_BPA_FAILURE, "clm::string::replace rejects an empty search");
    CLM_ASSERT(!clm::string::create(UINT64_C(90008), "\xC0\xAF"), "clm::string::create rejects invalid UTF-8");

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
        CLM_ASSERT(clm::string::get_default_encoding() == clm::string::encoding::utf8, "string runtime default starts as UTF-8");
        CLM_ASSERT(clm::string::set_default_encoding(clm::string::encoding::latin1) == CLM_BPA_SUCCESS, "set Latin-1 byte-export default");
        CLM_ASSERT(clm::string::create(latin_id, latin_text), "create UTF-8 text with Latin-1 export preference");
        CLM_ASSERT(clm::string::preferred_encoding(latin_id).value() == clm::string::encoding::latin1, "new string records current export preference");
        CLM_ASSERT(clm::string::get_bytes(latin_id, {}) == 4, "query encoded Latin-1 byte count");
        memset(encoded, 0xA5, sizeof(encoded));
        CLM_ASSERT(clm::string::get_bytes(latin_id, std::as_writable_bytes(std::span(encoded).first(3))) == CLM_BPA_FAILURE && encoded[0] == 0xA5, "undersized encoding output fails without writing");
        CLM_ASSERT(clm::string::get_bytes(latin_id, std::as_writable_bytes(std::span(encoded))) == 4 &&
                   memcmp(encoded, expected_latin, sizeof(expected_latin)) == 0, "export Latin-1 bytes exactly");
        CLM_ASSERT(clm::string::from_bytes(latin_copy_id, clm::string::encoding::latin1,
                        std::as_bytes(std::span(expected_latin))) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(latin_copy_id, latin_text), "decode Latin-1 bytes into UTF-8 storage");
        CLM_ASSERT(clm::string::preferred_encoding(latin_copy_id).value() == clm::string::encoding::latin1, "from_bytes records source encoding preference");
        CLM_ASSERT(clm::string::from_bytes(latin_copy_id, clm::string::encoding::utf8,
                        std::as_bytes(std::span<const char>("\xC0\xAF", 2))) == CLM_BPA_FAILURE &&
                   clm_string_test_equals(latin_copy_id, latin_text), "invalid UTF-8 input preserves existing string");
        CLM_ASSERT(clm::string::from_bytes(invalid_id, clm::string::encoding::utf16le,
                        std::as_bytes(std::span(malformed_utf16))) == CLM_BPA_FAILURE &&
                   !clm::string::value(invalid_id), "unpaired UTF-16 surrogate does not create a string");
        CLM_ASSERT(clm::string::from_bytes(invalid_id, clm::string::encoding::utf8,
                        std::as_bytes(std::span(embedded_nul))) == CLM_BPA_FAILURE &&
                   !clm::string::value(invalid_id), "embedded NUL input is rejected");
        CLM_ASSERT(clm::string::from_bytes(invalid_id, clm::string::encoding::utf16le,
                        std::as_bytes(std::span(expected_utf16).first(1))) == CLM_BPA_FAILURE, "odd-length UTF-16 input is rejected");
        CLM_ASSERT(clm::string::set_default_encoding(
                   static_cast<clm::string::encoding>(0xFF)) == CLM_BPA_FAILURE &&
                   clm::string::get_default_encoding() == clm::string::encoding::latin1, "unsupported default encoding is rejected");
        CLM_ASSERT(clm::string::create(unicode_id, unicode_text), "create Unicode string for UTF-16LE and slicing");
        CLM_ASSERT(clm::string::change_encoding(unicode_id, clm::string::encoding::utf16le) == CLM_BPA_SUCCESS, "change representable string export encoding to UTF-16LE");
        CLM_ASSERT(clm::string::get_bytes(unicode_id, {}) == (clm_int)sizeof(expected_utf16), "query UTF-16LE byte count");
        CLM_ASSERT(clm::string::get_bytes(unicode_id, std::as_writable_bytes(std::span(encoded))) == (clm_int)sizeof(expected_utf16) &&
                   memcmp(encoded, expected_utf16, sizeof(expected_utf16)) == 0, "export UTF-16LE including surrogate pair");
        CLM_ASSERT(clm::string::from_bytes(unicode_copy_id, clm::string::encoding::utf16le,
                        std::as_bytes(std::span(expected_utf16))) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(unicode_copy_id, unicode_text), "round-trip UTF-16LE through UTF-8 storage");
        CLM_ASSERT(clm::string::change_encoding(unicode_id, clm::string::encoding::latin1) == CLM_BPA_FAILURE &&
                   clm::string::preferred_encoding(unicode_id).value() == clm::string::encoding::utf16le, "unrepresentable encoding change fails and rolls back");
        const auto borrowed_text = clm::string::slice_borrow(unicode_id, 1, 2);
        CLM_ASSERT(borrowed_text.has_value() && borrowed_text->size() == 6 &&
               *borrowed_text == "\xC3\xA9\xF0\x9F\x90\xB1",
                   "borrow Unicode string slice by code-point range");
        CLM_ASSERT(clm::string::slice(unicode_id, 1, 2, UINT64_C(90105)) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(UINT64_C(90105), "\xC3\xA9\xF0\x9F\x90\xB1"),
                   "copy Unicode string slice into independent BPA string");
        CLM_ASSERT(clm::string::preferred_encoding(UINT64_C(90105)).value() == clm::string::encoding::utf16le,
                   "copied string slice inherits preferred export encoding");
        const auto empty_borrowed_text = clm::string::slice_borrow(unicode_id, 3, 0);
        CLM_ASSERT(empty_borrowed_text.has_value() && empty_borrowed_text->empty(),
               "empty borrowed string slice at end succeeds");
        CLM_ASSERT(clm::string::slice(unicode_id, 3, 0, UINT64_C(90107)) == CLM_BPA_SUCCESS &&
                   clm_string_test_equals(UINT64_C(90107), ""), "empty copied string slice at end succeeds");
        CLM_ASSERT(clm::string::slice(unicode_id, 4, 0, UINT64_C(90106)) == CLM_BPA_FAILURE &&
               clm::string::slice(unicode_id, -1, 1, UINT64_C(90106)) == CLM_BPA_FAILURE,
                   "string copy slice rejects out-of-range and negative positions");
        CLM_ASSERT(!clm::string::slice_borrow(unicode_id, 4, 0),
                   "borrowed string slice rejects out-of-range position");
        CLM_ASSERT(clm::string::slice(unicode_id, 0, 1, latin_id) == CLM_BPA_FAILURE,
                   "string copy slice rejects an existing destination ID");
        CLM_ASSERT(clm::string::set(unicode_id, "changed") == 7 &&
                   clm_string_test_equals(UINT64_C(90105), "\xC3\xA9\xF0\x9F\x90\xB1"),
                   "copied string slice remains independent after source mutation");
        CLM_ASSERT(clm::string::set_default_encoding(clm::string::encoding::utf8) == CLM_BPA_SUCCESS,
                   "restore UTF-8 byte-export default");
        CLM_ASSERT(clm::string::preferred_encoding(latin_id).value() == clm::string::encoding::latin1,
                   "changing runtime default does not alter existing string preference");
        CLM_ASSERT(clm::string::erase(latin_id), "delete Latin-1 test string");
        CLM_ASSERT(clm::string::erase(latin_copy_id), "delete Latin-1 round-trip string");
        CLM_ASSERT(clm::string::erase(unicode_id), "delete UTF-16LE test string");
        CLM_ASSERT(clm::string::erase(unicode_copy_id), "delete UTF-16LE round-trip string");
        CLM_ASSERT(clm::string::erase(UINT64_C(90105)), "delete copied Unicode slice");
        CLM_ASSERT(clm::string::erase(UINT64_C(90107)), "delete empty copied string slice");
    }

    CLM_ASSERT(clm::string::set(string_id, "red red") == 7, "clm::string::set replaces string content");
    CLM_ASSERT(clm::string::replace(string_id, "red", "blue") == 2, "clm::string::replace replaces every match");
    CLM_ASSERT(clm_string_test_equals(string_id, "blue blue"), "replace-all result is correct");
    CLM_ASSERT(clm::string::create(other_string_id, "scratch"), "clm::string::create(other_string_id, \"scratch\")");
    CLM_ASSERT(clm::string::erase(other_string_id), "clm::string::erase removes a string block");

    {
        const clm_id compare_left_id = UINT64_C(90301);
        const clm_id compare_right_id = UINT64_C(90302);
        const clm_id trim_id = UINT64_C(90303);
        const clm_id raw_trim_id = UINT64_C(90304);
        const clm_id split_source_id = UINT64_C(90305);
        const clm_id split_delimiter_id = UINT64_C(90306);
        const clm_id split_array_id = UINT64_C(90307);
        const clm_id lines_array_id = UINT64_C(90308);
        const clm_id trim_array_id = UINT64_C(90309);
        const clm_id joined_id = UINT64_C(90310);
        const clm_id array_compare_id = UINT64_C(90311);
        const clm_id line_source_id = UINT64_C(90312);
        const clm_id line_source_array_id = UINT64_C(90313);
        const clm_id left_trim_array_id = UINT64_C(90314);
        const clm_id right_trim_array_id = UINT64_C(90315);

        CLM_ASSERT(clm::string::create(compare_left_id, "CAF\xC3\x89") &&
                   clm::string::create(compare_right_id, "caf\xC3\xA9"), "create compare operands");
        CLM_ASSERT(clm::string::compare(compare_left_id, compare_right_id) != 0 &&
                   clm::string::compare_ignore_case(compare_left_id, compare_right_id) == 0,
                   "string ID compare supports case-sensitive and case-insensitive modes");
        CLM_ASSERT(clm::string::compare_from("AbC", "aBc") != 0 &&
                   clm::string::compare_ignore_case_from("AbC", "aBc") == 0,
                   "raw UTF-8 compare variants support both case modes");

        CLM_ASSERT(clm::string::create(trim_id, " \tvalue\xE3\x80\x80"), "create Unicode-trim input");
        CLM_ASSERT(clm::string::trim(trim_id) == 3 &&
                   clm_string_test_equals(trim_id, "value"), "string trim removes Unicode whitespace on both sides");
        CLM_ASSERT(clm::string::left_trim_from(raw_trim_id, " \tvalue\n") == 2 &&
                   clm_string_test_equals(raw_trim_id, "value\n"), "raw left-trim writes trimmed text to destination ID");
        CLM_ASSERT(clm::string::right_trim(raw_trim_id) == 1 &&
                   clm_string_test_equals(raw_trim_id, "value"), "string right-trim mutates destination ID");

        CLM_ASSERT(clm::string::create(split_source_id, "a::b::") &&
                   clm::string::create(split_delimiter_id, "::"), "create split input and delimiter");
        CLM_ASSERT(clm::string::split(split_array_id, split_source_id,
                                    split_delimiter_id) == CLM_BPA_SUCCESS &&
                   clm::string::array_count(split_array_id) == 3, "split by string retains trailing empty fields");
        CLM_ASSERT(clm_string_array_test_equals(split_array_id, 0, "a") &&
               clm_string_array_test_equals(split_array_id, 1, "b") &&
               clm_string_array_test_equals(split_array_id, 2, ""),
                   "split by string produces expected elements");
        CLM_ASSERT(clm::string::array_join_from(joined_id, split_array_id, "-") == 4 &&
                   clm_string_test_equals(joined_id, "a-b-"), "join array using raw separator");
        CLM_ASSERT(clm::string::array_join(joined_id, split_array_id,
                                         split_delimiter_id) == 6 &&
                   clm_string_test_equals(joined_id, "a::b::"),
                   "join array using a separator string ID");
        CLM_ASSERT(clm::string::split_lines_from(lines_array_id, "one\r\ntwo\n") == CLM_BPA_SUCCESS &&
                   clm::string::array_count(lines_array_id) == 3 &&
                   clm_string_array_test_equals(lines_array_id, 2, ""),
                   "split lines handles CRLF and retains trailing empty line");
        CLM_ASSERT(clm::string::create(line_source_id, "x\r\ny") &&
                   clm::string::split_lines(line_source_array_id, line_source_id) ==
                       CLM_BPA_SUCCESS &&
                   clm::string::array_count(line_source_array_id) == 2,
                   "split lines accepts a string ID");

        CLM_ASSERT(clm::string::split_from(trim_array_id, " \tone \n \ttwo  ", "\n") ==
                   CLM_BPA_SUCCESS, "split raw UTF-8 text");
        CLM_ASSERT(clm::string::array_trim(trim_array_id) == 7 &&
               clm_string_array_test_equals(trim_array_id, 0, "one") &&
               clm_string_array_test_equals(trim_array_id, 1, "two"),
                   "array trim removes whitespace from every element");
        CLM_ASSERT(clm::string::array_compare_from(trim_array_id, 0, "one") == 0 &&
                   clm::string::array_compare_ignore_case_from(trim_array_id, 1, "TWO") == 0,
                   "array compare supports index and raw text");
        CLM_ASSERT(clm::string::create(array_compare_id, "ONE") &&
                   clm::string::array_compare(trim_array_id, 0,
                                            array_compare_id) != 0 &&
                   clm::string::array_compare_ignore_case(trim_array_id, 0,
                                                       array_compare_id) == 0,
                   "array compare supports string IDs");
        CLM_ASSERT(clm::string::split_from(left_trim_array_id, " \ta\n b", "\n") ==
                       CLM_BPA_SUCCESS &&
                   clm::string::array_left_trim(left_trim_array_id) == 3 &&
                     clm_string_array_test_equals(left_trim_array_id, 0, "a"),
                   "array left-trim removes leading whitespace in each item");
        CLM_ASSERT(clm::string::split_from(right_trim_array_id, "a \t\nb  ", "\n") ==
                       CLM_BPA_SUCCESS &&
                   clm::string::array_right_trim(right_trim_array_id) == 4 &&
                     clm_string_array_test_equals(right_trim_array_id, 1, "b"),
                   "array right-trim removes trailing whitespace in each item");

        CLM_ASSERT(clm::string::array_erase(split_array_id) &&
                   clm::string::array_erase(lines_array_id) &&
                   clm::string::array_erase(trim_array_id) &&
                   clm::string::array_erase(line_source_array_id) &&
                   clm::string::array_erase(left_trim_array_id) &&
                   clm::string::array_erase(right_trim_array_id), "delete new test arrays");
        CLM_ASSERT(clm::string::erase(compare_left_id) &&
                   clm::string::erase(compare_right_id) &&
                   clm::string::erase(trim_id) &&
                   clm::string::erase(raw_trim_id) &&
                   clm::string::erase(split_source_id) &&
                   clm::string::erase(split_delimiter_id) &&
                   clm::string::erase(joined_id) &&
                   clm::string::erase(array_compare_id) &&
                   clm::string::erase(line_source_id), "delete new test strings");
    }

    CLM_ASSERT(clm::string::create(first_value_id, "pear"), "create first array string");
    CLM_ASSERT(clm::string::create(second_value_id, "apple"), "create second array string");
    CLM_ASSERT(clm::string::create(third_value_id, "banana"), "create third array string");
    CLM_ASSERT(clm::string::create(scratch_id, ""), "create array output string");
    CLM_ASSERT(clm::string::array_create(array_id, 0), "clm::string::array_create(array_id, 0)");
    CLM_ASSERT(clm::string::array_append(array_id, first_value_id) == CLM_BPA_SUCCESS, "append first string to array");
    CLM_ASSERT(clm::string::array_append(array_id, second_value_id) == CLM_BPA_SUCCESS, "append second string to array");
    CLM_ASSERT(clm::string::array_insert(array_id, 1, third_value_id) == CLM_BPA_SUCCESS, "insert string into array");
    CLM_ASSERT(clm::string::array_count(array_id) == 3, "clm::string::array_count(array_id) == 3");
    CLM_ASSERT(clm::string::array_find(array_id, third_value_id) == 1, "clm::string::array_find finds inserted item");
    CLM_ASSERT(clm::string::array_get(array_id, 1, scratch_id) == CLM_BPA_GET_SUCCESS, "get string array item into BPA string");
    CLM_ASSERT(clm_string_test_equals(scratch_id, "banana"), "get returns expected string");
    CLM_ASSERT(clm::string::array_length_at(array_id, 1) == 6, "clm::string::array_length_at(array_id, 1) == 6");
    CLM_ASSERT(clm::string::array_contains_at(array_id, 1, "ana"), "clm::string::array_contains_at finds substring");
    CLM_ASSERT(clm::string::array_append_at(array_id, 1, "!") == 1, "append to indexed array string");
    CLM_ASSERT(clm::string::array_append_line_at(array_id, 1, ".") == 2, "append line to indexed array string");
    CLM_ASSERT(clm::string::array_insert_at(array_id, 1, 0, "!") == 1, "insert into indexed array string");
    CLM_ASSERT(clm::string::array_remove_at(array_id, 1, 0, 1) == 1, "remove from indexed array string");
    CLM_ASSERT(clm::string::array_replace_at(array_id, 1, "a", "A") == 3, "replace all matches in indexed array string");
    CLM_ASSERT(clm::string::array_get(array_id, 1, scratch_id) == CLM_BPA_GET_SUCCESS, "get mutated string array item");
    CLM_ASSERT(clm_string_test_equals(scratch_id, "bAnAnA!.\n"), "indexed string operations produce expected content");
    CLM_ASSERT(clm::string::array_set(array_id, 0, second_value_id) == CLM_BPA_SET_SUCCESS, "set string array item from BPA string");
    CLM_ASSERT(clm::string::array_set(array_id, 1, third_value_id) == CLM_BPA_SET_SUCCESS, "reset mutated array item");
    CLM_ASSERT(clm::string::array_sort(array_id) == CLM_BPA_SUCCESS, "sort string array");
    CLM_ASSERT(clm::string::array_move_last(array_id) == CLM_BPA_SUCCESS, "move string array cursor to last");
    CLM_ASSERT(clm::string::array_get_current(array_id, scratch_id) == CLM_BPA_GET_SUCCESS, "get current string array item");
    CLM_ASSERT(clm_string_test_equals(scratch_id, "banana"), "sort and cursor return final string");
    CLM_ASSERT(clm::string::array_move_first(array_id) == CLM_BPA_SUCCESS, "move string array cursor to first");
    CLM_ASSERT(clm::string::array_move_next(array_id) == CLM_BPA_SUCCESS, "move string array cursor next");
    CLM_ASSERT(clm::string::array_move_previous(array_id) == CLM_BPA_SUCCESS, "move string array cursor previous");
    CLM_ASSERT(clm::string::array_move_to(array_id, 2) == CLM_BPA_SUCCESS, "move string array cursor to index");
    CLM_ASSERT(clm::string::array_remove(array_id, 1) == CLM_BPA_SUCCESS, "remove string array item");
    CLM_ASSERT(clm::string::array_count(array_id) == 2, "array count updates after removal");
    CLM_ASSERT(clm::string::array_shuffle(array_id) == CLM_BPA_SUCCESS, "shuffle string array");
    CLM_ASSERT(clm::string::array_draw_at(array_id, 0, scratch_id) == CLM_BPA_GET_SUCCESS, "draw indexed string from deck");
    CLM_ASSERT(clm::string::array_draw(array_id, scratch_id) == CLM_BPA_GET_SUCCESS, "draw next string from deck");
    CLM_ASSERT(clm::string::array_draw(array_id, scratch_id) == CLM_BPA_GET_SUCCESS, "draw reshuffles exhausted deck");
    CLM_ASSERT(clm::string::array_draw_at(array_id, 99, scratch_id) == CLM_BPA_FAILURE, "draw rejects out-of-range deck index");
    CLM_ASSERT(clm::string::array_clear(array_id) == CLM_BPA_SUCCESS, "clear string array");
    CLM_ASSERT(clm::string::array_count(array_id) == 0, "cleared array has zero items");
    CLM_ASSERT(clm::string::array_erase(array_id), "delete string array");
    CLM_ASSERT(clm::string::erase(first_value_id), "delete first array string");
    CLM_ASSERT(clm::string::erase(second_value_id), "delete second array string");
    CLM_ASSERT(clm::string::erase(third_value_id), "delete third array string");
    CLM_ASSERT(clm::string::erase(scratch_id), "delete array output string");
    CLM_ASSERT(clm::string::erase(string_id), "delete test string");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}


static int test_clm_string(void)
{
    return clm_string_tests();
}

