#pragma once

#include "test.hpp"
#include "../clm/string.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

static int test_clm_string()
{
    clm_test_result result = {0, 0};
    using clm::string::encoding;
    constexpr clm_id string_id = 90001;
    constexpr clm_id other_string_id = 90002;
    constexpr clm_id array_id = 90003;
    constexpr clm_id first_value_id = 90004;
    constexpr clm_id second_value_id = 90005;
    constexpr clm_id third_value_id = 90006;
    constexpr clm_id scratch_id = 90007;
    constexpr clm_id wrong_type_array_id = 90009;
    const std::string utf8_initial = "A\xC3\xA9\xF0\x9F\x90\xB1";
    const std::string utf8_after_insert = "A\xE4\xB8\xAD\xC3\xA9\xF0\x9F\x90\xB1!";
    const std::string utf8_after_remove = "A\xC3\xA9\xF0\x9F\x90\xB1!";
    const std::string utf8_after_replace = "A\xE4\xB8\xAD\xF0\x9F\x90\xB1!";

    clm_test_begin("CLM - String");

    CLM_ASSERT(clm::string::get_default_encoding() == encoding::utf8,
               "string runtime default starts as UTF-8");
    CLM_ASSERT(clm::string::create(string_id, utf8_initial), "create UTF-8 string");
    CLM_ASSERT(clm::string::length(string_id) == 3, "length counts Unicode code points");
    CLM_ASSERT(clm::string::preferred_encoding(string_id) == encoding::utf8,
               "new string records current export preference");
    CLM_ASSERT(clm::string::append(string_id, "!") == 1, "append returns character count");
    CLM_ASSERT(clm::string::insert(string_id, 1, "\xE4\xB8\xAD") == 1,
               "insert uses a Unicode code-point index");
    CLM_ASSERT(*clm::string::value(string_id) == utf8_after_insert,
               "insert respects UTF-8 code-point index");
    CLM_ASSERT(clm::string::remove(string_id, 1, 1) == 1, "remove returns removed count");
    CLM_ASSERT(*clm::string::value(string_id) == utf8_after_remove,
               "remove updates string content");
    CLM_ASSERT(clm::string::replace(string_id, "\xC3\xA9", "\xE4\xB8\xAD") == 1,
               "replace updates a UTF-8 match");
    CLM_ASSERT(*clm::string::value(string_id) == utf8_after_replace,
               "replace-all result is correct");
    CLM_ASSERT(clm::string::replace(string_id, "!", "") == 1,
               "replace permits an empty replacement");
    CLM_ASSERT(clm::string::contains(string_id, "\xF0\x9F\x90\xB1"),
               "contains finds UTF-8 text");
    CLM_ASSERT(clm::string::append_line(string_id, "!") == 2,
               "append_line appends text and a newline");
    CLM_ASSERT(clm::string::contains(string_id, "\n"), "contains finds newline");
    CLM_ASSERT(clm::string::length(string_id) > 0, "length reports nonempty string");
    CLM_ASSERT(clm::string::remove(string_id, -1, 1) == clm::string::failure,
               "remove rejects a negative index");
    CLM_ASSERT(clm::string::insert(string_id, INT_MAX, "x") == clm::string::failure,
               "insert rejects an out-of-range index");
    CLM_ASSERT(clm::string::replace(string_id, "", "x") == clm::string::failure,
               "replace rejects an empty search");
    CLM_ASSERT(!clm::string::create(90008, "\xC0\xAF"),
               "create rejects invalid UTF-8");

    constexpr clm_id latin_id = 90100;
    constexpr clm_id latin_copy_id = 90101;
    constexpr clm_id unicode_id = 90102;
    constexpr clm_id unicode_copy_id = 90103;
    constexpr clm_id invalid_id = 90104;
    const std::string latin_text = "caf\xC3\xA9";
    const std::string unicode_text = "A\xC3\xA9\xF0\x9F\x90\xB1";
    constexpr std::array expected_latin{std::byte{'c'}, std::byte{'a'}, std::byte{'f'}, std::byte{0xe9}};
    constexpr std::array expected_utf16{std::byte{0x41}, std::byte{0}, std::byte{0xe9}, std::byte{0},
                                        std::byte{0x3d}, std::byte{0xd8}, std::byte{0x31}, std::byte{0xdc}};
    constexpr std::array malformed_utf16{std::byte{0}, std::byte{0xd8}};
    constexpr std::array embedded_nul{std::byte{'A'}, std::byte{0}, std::byte{'B'}};
    std::array<std::byte, 16> encoded{};

    CLM_ASSERT(clm::string::set_default_encoding(encoding::latin1) == clm::string::success,
               "set Latin-1 byte-export default");
    CLM_ASSERT(clm::string::create(latin_id, latin_text),
               "create Latin-1-export string");
    CLM_ASSERT(clm::string::preferred_encoding(latin_id) == encoding::latin1,
               "new string keeps current export preference");
    CLM_ASSERT(clm::string::get_bytes(latin_id, std::span<std::byte>{}) == 4,
               "query encoded Latin-1 byte count");
    encoded.fill(std::byte{0xa5});
    CLM_ASSERT(clm::string::get_bytes(latin_id, std::span<std::byte>(encoded).first(3)) == clm::string::failure && encoded[0] == std::byte{0xa5},
               "undersized output fails without writing");
    CLM_ASSERT(clm::string::get_bytes(latin_id, encoded) == 4 &&
               std::equal(expected_latin.begin(), expected_latin.end(), encoded.begin()),
               "export exact Latin-1 bytes");
    CLM_ASSERT(clm::string::from_bytes(latin_copy_id, encoding::latin1, expected_latin) == clm::string::success &&
               *clm::string::value(latin_copy_id) == latin_text,
               "decode Latin-1 bytes to UTF-8 storage");
    CLM_ASSERT(clm::string::preferred_encoding(latin_copy_id) == encoding::latin1,
               "from_bytes records source encoding preference");
    constexpr std::array invalid_utf8{
        std::byte{0xc0}, std::byte{0xaf}};
    CLM_ASSERT(clm::string::from_bytes(latin_copy_id, encoding::utf8, invalid_utf8) == clm::string::failure &&
               *clm::string::value(latin_copy_id) == latin_text,
               "invalid input preserves an existing string");
    CLM_ASSERT(clm::string::from_bytes(invalid_id, encoding::utf16le, malformed_utf16) == clm::string::failure &&
               clm::string::value(invalid_id) == nullptr,
               "unpaired UTF-16 surrogate does not create a string");
    CLM_ASSERT(clm::string::from_bytes(invalid_id, encoding::utf8, embedded_nul) == clm::string::failure &&
               clm::string::value(invalid_id) == nullptr,
               "embedded NUL input is rejected");
    CLM_ASSERT(clm::string::from_bytes(invalid_id, encoding::utf16le,
               std::span<const std::byte>(expected_utf16).first(1)) == clm::string::failure,
               "odd-length UTF-16 input is rejected");
    CLM_ASSERT(clm::string::set_default_encoding(static_cast<encoding>(0xff)) == clm::string::failure &&
               clm::string::get_default_encoding() == encoding::latin1,
               "unsupported default encoding is rejected");
    CLM_ASSERT(clm::string::create(unicode_id, unicode_text),
               "create Unicode string for UTF-16LE and slicing");
    CLM_ASSERT(clm::string::change_encoding(unicode_id, encoding::utf16le) == clm::string::success,
               "change representable string export encoding");
    CLM_ASSERT(clm::string::get_bytes(unicode_id, std::span<std::byte>{}) == static_cast<clm_int>(expected_utf16.size()),
               "query UTF-16LE byte count");
    CLM_ASSERT(clm::string::get_bytes(unicode_id, encoded) == static_cast<clm_int>(expected_utf16.size()) &&
               std::equal(expected_utf16.begin(), expected_utf16.end(), encoded.begin()),
               "export UTF-16LE including surrogate pair");
    CLM_ASSERT(clm::string::from_bytes(unicode_copy_id, encoding::utf16le,
               expected_utf16) == clm::string::success &&
               *clm::string::value(unicode_copy_id) == unicode_text,
               "round-trip UTF-16LE through UTF-8 storage");
    CLM_ASSERT(clm::string::change_encoding(unicode_id, encoding::latin1) == clm::string::failure &&
               clm::string::preferred_encoding(unicode_id) == encoding::utf16le,
               "unrepresentable encoding change fails and rolls back");

    const auto borrowed = clm::string::slice_borrow(unicode_id, 1, 2);
    CLM_ASSERT(borrowed && borrowed->size() == 6 &&
               *borrowed == "\xC3\xA9\xF0\x9F\x90\xB1",
               "borrow a Unicode slice by code-point range");
    CLM_ASSERT(clm::string::slice(unicode_id, 1, 2, 90105) == clm::string::success &&
               *clm::string::value(90105) == "\xC3\xA9\xF0\x9F\x90\xB1",
               "copy Unicode slice into an independent string");
    CLM_ASSERT(clm::string::preferred_encoding(90105) == encoding::utf16le,
               "copied slice inherits preferred export encoding");
    const auto empty_borrow = clm::string::slice_borrow(unicode_id, 3, 0);
    CLM_ASSERT(empty_borrow && empty_borrow->empty(),
               "empty borrowed slice at end succeeds");
    CLM_ASSERT(clm::string::slice(unicode_id, 3, 0, 90107) == clm::string::success &&
               clm::string::value(90107)->empty(),
               "empty copied slice at end succeeds");
    CLM_ASSERT(clm::string::slice(unicode_id, 4, 0, 90106) == clm::string::failure,
               "copy slice rejects a position past the end");
    CLM_ASSERT(clm::string::slice(unicode_id, -1, 1, 90106) == clm::string::failure,
               "copy slice rejects a negative position");
    CLM_ASSERT(!clm::string::slice_borrow(unicode_id, 4, 0),
               "borrowed slice rejects out-of-range position");
    CLM_ASSERT(clm::string::slice(unicode_id, 0, 1, latin_id) == clm::string::failure,
               "copy slice rejects an existing destination ID");
    CLM_ASSERT(clm::string::set(unicode_id, "changed") == 7 &&
               *clm::string::value(90105) == "\xC3\xA9\xF0\x9F\x90\xB1",
               "copied slice remains independent after source mutation");
    CLM_ASSERT(clm::string::set_default_encoding(encoding::utf8) == clm::string::success &&
               clm::string::preferred_encoding(latin_id) == encoding::latin1,
               "default changes do not affect existing string preferences");
    CLM_ASSERT(clm::string::erase(latin_id), "delete Latin-1 test string");
    CLM_ASSERT(clm::string::erase(latin_copy_id), "delete Latin-1 round-trip string");
    CLM_ASSERT(clm::string::erase(unicode_id), "delete UTF-16LE test string");
    CLM_ASSERT(clm::string::erase(unicode_copy_id), "delete UTF-16LE round-trip string");
    CLM_ASSERT(clm::string::erase(90105), "delete copied Unicode slice");
    CLM_ASSERT(clm::string::erase(90107), "delete empty copied string slice");

    CLM_ASSERT(clm::string::set(string_id, "red red") == 7, "set replaces string content");
    CLM_ASSERT(clm::string::replace(string_id, "red", "blue") == 2,
               "replace handles every non-overlapping match");
    CLM_ASSERT(*clm::string::value(string_id) == "blue blue", "replace result is correct");
    CLM_ASSERT(clm::string::create(other_string_id, "scratch"),
               "create another string");
    CLM_ASSERT(clm::string::erase(other_string_id), "delete another string");

    CLM_ASSERT(clm::string::create(first_value_id, "pear"),
               "create first array source string");
    CLM_ASSERT(clm::string::create(second_value_id, "apple"),
               "create second array source string");
    CLM_ASSERT(clm::string::create(third_value_id, "banana"),
               "create third array source string");
    CLM_ASSERT(clm::string::create(scratch_id, ""),
               "create array output string");
    CLM_ASSERT(clm::string::array_create(wrong_type_array_id),
               "create a non-string array for erase type safety");
    CLM_ASSERT(!clm::string::erase(wrong_type_array_id) &&
               clm::string::array_count(wrong_type_array_id) == 0,
               "string erase rejects an array ID and preserves the array");
    CLM_ASSERT(clm::string::array_erase(wrong_type_array_id),
               "delete non-string array test fixture");
    CLM_ASSERT(clm::string::array_create(array_id), "create string array");
    CLM_ASSERT(clm::string::array_append(array_id, first_value_id) == clm::string::success,
               "append first string to array");
    CLM_ASSERT(clm::string::array_append(array_id, second_value_id) == clm::string::success,
               "append second string to array");
    CLM_ASSERT(clm::string::array_insert(array_id, 1, third_value_id) ==
               clm::string::success,
               "insert string into array");
    CLM_ASSERT(clm::string::array_count(array_id) == 3, "array count");
    CLM_ASSERT(clm::string::array_find(array_id, third_value_id) == 1,
               "find inserted array item");
    CLM_ASSERT(clm::string::array_get(array_id, 1, scratch_id) == clm::string::get_success,
               "get array item into existing string");
    CLM_ASSERT(*clm::string::value(scratch_id) == "banana",
               "array get returns the expected string");
    CLM_ASSERT(clm::string::array_length_at(array_id, 1) == 6,
               "query indexed string length");
    CLM_ASSERT(clm::string::array_contains_at(array_id, 1, "ana"),
               "find substring in indexed string");
    CLM_ASSERT(clm::string::array_append_at(array_id, 1, "!") == 1,
               "append text to indexed array string");
    CLM_ASSERT(clm::string::array_append_line_at(array_id, 1, ".") == 2,
               "append line to indexed array string");
    CLM_ASSERT(clm::string::array_insert_at(array_id, 1, 0, "!") == 1,
               "insert into indexed array string");
    CLM_ASSERT(clm::string::array_remove_at(array_id, 1, 0, 1) == 1,
               "remove from indexed array string");
    CLM_ASSERT(clm::string::array_replace_at(array_id, 1, "a", "A") == 3,
               "replace all matches in indexed array string");
    CLM_ASSERT(clm::string::array_get(array_id, 1, scratch_id) == clm::string::get_success,
               "get mutated string array item");
    CLM_ASSERT(*clm::string::value(scratch_id) == "bAnAnA!.\n",
               "indexed string operations produce expected content");
    CLM_ASSERT(clm::string::array_set(array_id, 0, second_value_id) == clm::string::set_success,
               "set first array item from a source string");
    CLM_ASSERT(clm::string::array_set(array_id, 1, third_value_id) == clm::string::set_success,
               "set second array item from a source string");
    CLM_ASSERT(clm::string::array_sort(array_id) == clm::string::success,
               "sort string array");
    CLM_ASSERT(clm::string::array_move_last(array_id) == clm::string::success,
               "move array cursor to last item");
    CLM_ASSERT(clm::string::array_get_current(array_id, scratch_id) == clm::string::get_success,
               "get current string array item");
    CLM_ASSERT(*clm::string::value(scratch_id) == "banana",
               "sort and cursor return final string");
    CLM_ASSERT(clm::string::array_move_first(array_id) == clm::string::success,
               "move array cursor to first item");
    CLM_ASSERT(clm::string::array_move_next(array_id) == clm::string::success,
               "move array cursor to next item");
    CLM_ASSERT(clm::string::array_move_previous(array_id) == clm::string::success,
               "move array cursor to previous item");
    CLM_ASSERT(clm::string::array_move_to(array_id, 2) == clm::string::success,
               "move array cursor to an index");
    CLM_ASSERT(clm::string::array_remove(array_id, 1) == clm::string::success,
               "remove an array item");
    CLM_ASSERT(clm::string::array_count(array_id) == 2,
               "array count updates after removal");
    CLM_ASSERT(clm::string::array_shuffle(array_id) == clm::string::success,
               "shuffle string array");
    CLM_ASSERT(clm::string::array_draw_at(array_id, 0, scratch_id) == clm::string::get_success,
               "draw indexed string from deck");
    CLM_ASSERT(clm::string::array_draw(array_id, scratch_id) == clm::string::get_success,
               "draw next string from deck");
    CLM_ASSERT(clm::string::array_draw(array_id, scratch_id) == clm::string::get_success,
               "draw reshuffles exhausted deck");
    CLM_ASSERT(clm::string::array_draw_at(array_id, 99, scratch_id) == clm::string::failure,
               "draw rejects an out-of-range deck index");
    CLM_ASSERT(clm::string::array_clear(array_id) == clm::string::success,
               "clear string array");
    CLM_ASSERT(clm::string::array_count(array_id) == 0,
               "cleared array has zero items");
    CLM_ASSERT(clm::string::array_erase(array_id),
               "delete string array");
    CLM_ASSERT(clm::string::erase(first_value_id), "delete first array string");
    CLM_ASSERT(clm::string::erase(second_value_id), "delete second array string");
    CLM_ASSERT(clm::string::erase(third_value_id), "delete third array string");
    CLM_ASSERT(clm::string::erase(scratch_id), "delete array output string");
    CLM_ASSERT(clm::string::erase(string_id), "delete test string");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
