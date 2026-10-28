//! Tests for Unicode string storage, byte encodings, slices, and string arrays.

const std = @import("std");
const string = @import("clm_string");
const bpa = @import("clm_bpa");
const test_report = @import("test.zig");

fn assert(runner: *test_report.TestRunner, result: *test_report.clm_test_result, condition: bool, description: []const u8) void {
    test_report.clm_test_assert(runner, result, condition, description);
}

fn equals(id: u64, expected: []const u8) bool {
    return std.mem.eql(u8, bpa.clm_bpa_string(id) orelse return false, expected);
}

pub fn clm_string_tests(runner: *test_report.TestRunner) void {
    var result: test_report.clm_test_result = .{};
    const initial = "Aé🐱";
    const id: u64 = 90001;
    const array: u64 = 90003;
    const first: u64 = 90004;
    const second: u64 = 90005;
    const third: u64 = 90006;
    const scratch: u64 = 90007;
    const ok = bpa.CLM_BPA_SUCCESS;
    const fail = bpa.CLM_BPA_FAILURE;
    const get_ok = bpa.CLM_BPA_GET_SUCCESS;
    const set_ok = bpa.CLM_BPA_SET_SUCCESS;
    test_report.clm_test_begin(runner, "CLM - String");

    assert(runner, &result, string.clm_string_new(id, initial), "create UTF-8 string");
    assert(runner, &result, string.clm_string_length(id) == 3, "length counts Unicode code points");
    assert(runner, &result, bpa.clm_bpa_find_block(id).?.encoding == string.CLM_STRING_ENCODING_UTF8, "string records UTF-8 encoding");
    assert(runner, &result, string.clm_string_append(id, "!") == 1, "append returns character count");
    assert(runner, &result, string.clm_string_insert(id, 1, "中") == 1, "insert UTF-8 character");
    assert(runner, &result, equals(id, "A中é🐱!"), "insert respects code-point index");
    assert(runner, &result, string.clm_string_remove(id, 1, 1) == 1, "remove UTF-8 character");
    assert(runner, &result, equals(id, "Aé🐱!"), "remove updates content");
    assert(runner, &result, string.clm_string_replace(id, "é", "中") == 1, "replace UTF-8 match");
    assert(runner, &result, equals(id, "A中🐱!"), "replace updates content");
    assert(runner, &result, string.clm_string_replace(id, "!", "") == 1, "allow empty replacement");
    assert(runner, &result, string.clm_string_contains(id, "🐱"), "contains UTF-8 text");
    assert(runner, &result, string.clm_string_append_line(id, "!") == 2, "append line returns text and newline count");
    assert(runner, &result, string.clm_string_contains(id, "\n"), "contains newline");
    assert(runner, &result, string.clm_string_length(id) > 0, "length reports nonempty string");
    assert(runner, &result, string.clm_string_remove(id, -1, 1) == fail, "reject negative removal index");
    assert(runner, &result, string.clm_string_insert(id, std.math.maxInt(i32), "x") == fail, "reject out-of-range insertion");
    assert(runner, &result, string.clm_string_replace(id, "", "x") == fail, "reject empty search");
    assert(runner, &result, !string.clm_string_new(90008, "\xc0\xaf"), "reject invalid UTF-8");

    {
        const latin: u64 = 90100;
        const latin_copy: u64 = 90101;
        const unicode: u64 = 90102;
        const unicode_copy: u64 = 90103;
        const invalid: u64 = 90104;
        const expected_latin = "caf\xe9";
        const expected_utf16 = [_]u8{ 0x41, 0, 0xe9, 0, 0x3d, 0xd8, 0x31, 0xdc };
        var encoded: [16]u8 = @splat(0xa5);

        assert(runner, &result, string.clm_string_get_default_encoding() == string.CLM_STRING_ENCODING_UTF8, "runtime default starts as UTF-8");
        assert(runner, &result, string.clm_string_set_default_encoding(string.CLM_STRING_ENCODING_LATIN1) == ok, "set Latin-1 export default");
        assert(runner, &result, string.clm_string_new(latin, "café"), "create UTF-8 text with Latin-1 preference");
        assert(runner, &result, bpa.clm_bpa_find_block(latin).?.encoding == string.CLM_STRING_ENCODING_LATIN1, "new string records export preference");
        assert(runner, &result, string.clm_string_get_bytes(latin, null) == 4, "query Latin-1 byte count");
        assert(runner, &result, string.clm_string_get_bytes(latin, encoded[0..3]) == fail and encoded[0] == 0xa5, "undersized output fails without writing");
        assert(runner, &result, string.clm_string_get_bytes(latin, &encoded) == 4 and std.mem.eql(u8, encoded[0..4], expected_latin), "export exact Latin-1 bytes");
        assert(runner, &result, string.clm_string_from_bytes(latin_copy, string.CLM_STRING_ENCODING_LATIN1, expected_latin) == ok and equals(latin_copy, "café"), "decode Latin-1 into UTF-8");
        assert(runner, &result, bpa.clm_bpa_find_block(latin_copy).?.encoding == string.CLM_STRING_ENCODING_LATIN1, "from_bytes records source preference");
        assert(runner, &result, string.clm_string_from_bytes(latin_copy, string.CLM_STRING_ENCODING_UTF8, "\xc0\xaf") == fail and equals(latin_copy, "café"), "invalid input preserves existing string");
        assert(runner, &result, string.clm_string_from_bytes(invalid, string.CLM_STRING_ENCODING_UTF16LE, &.{ 0, 0xd8 }) == fail and !bpa.clm_bpa_is_existing(invalid), "unpaired surrogate does not create string");
        assert(runner, &result, string.clm_string_from_bytes(invalid, string.CLM_STRING_ENCODING_UTF8, "A\x00B") == fail and !bpa.clm_bpa_is_existing(invalid), "embedded NUL is rejected");
        assert(runner, &result, string.clm_string_from_bytes(invalid, string.CLM_STRING_ENCODING_UTF16LE, expected_utf16[0..1]) == fail, "odd UTF-16 length is rejected");
        assert(runner, &result, string.clm_string_set_default_encoding(0xff) == fail and string.clm_string_get_default_encoding() == string.CLM_STRING_ENCODING_LATIN1, "unsupported default leaves preference unchanged");
        assert(runner, &result, string.clm_string_new(unicode, initial), "create Unicode string for UTF-16 and slicing");
        assert(runner, &result, string.clm_string_change_encoding(unicode, string.CLM_STRING_ENCODING_UTF16LE) == ok, "change export encoding to UTF-16LE");
        assert(runner, &result, string.clm_string_get_bytes(unicode, null) == expected_utf16.len, "query UTF-16LE size");
        assert(runner, &result, string.clm_string_get_bytes(unicode, &encoded) == expected_utf16.len and std.mem.eql(u8, encoded[0..expected_utf16.len], &expected_utf16), "export UTF-16LE surrogate pair");
        assert(runner, &result, string.clm_string_from_bytes(unicode_copy, string.CLM_STRING_ENCODING_UTF16LE, &expected_utf16) == ok and equals(unicode_copy, initial), "round-trip UTF-16LE");
        assert(runner, &result, string.clm_string_change_encoding(unicode, string.CLM_STRING_ENCODING_LATIN1) == fail and bpa.clm_bpa_find_block(unicode).?.encoding == string.CLM_STRING_ENCODING_UTF16LE, "unrepresentable encoding fails and rolls back");
        const borrowed = string.clm_string_slice_borrow(unicode, 1, 2);
        assert(runner, &result, borrowed != null and std.mem.eql(u8, borrowed.?, "é🐱"), "borrow slice by code-point range");
        assert(runner, &result, string.clm_string_slice(unicode, 1, 2, 90105) == ok and equals(90105, "é🐱"), "copy independent Unicode slice");
        assert(runner, &result, bpa.clm_bpa_find_block(90105).?.encoding == string.CLM_STRING_ENCODING_UTF16LE, "copy inherits export preference");
        const empty = string.clm_string_slice_borrow(unicode, 3, 0);
        assert(runner, &result, empty != null and empty.?.len == 0, "empty borrowed slice at end succeeds");
        assert(runner, &result, string.clm_string_slice(unicode, 3, 0, 90107) == ok and equals(90107, ""), "empty copied slice at end succeeds");
        assert(runner, &result, string.clm_string_slice(unicode, 4, 0, 90106) == fail and string.clm_string_slice(unicode, -1, 1, 90106) == fail, "copy rejects invalid positions");
        assert(runner, &result, string.clm_string_slice_borrow(unicode, 4, 0) == null, "borrow rejects invalid position");
        assert(runner, &result, string.clm_string_slice(unicode, 0, 1, latin) == fail, "copy rejects existing destination");
        assert(runner, &result, string.clm_string_set(unicode, "changed") == 7 and equals(90105, "é🐱"), "copy remains independent after mutation");
        assert(runner, &result, string.clm_string_set_default_encoding(string.CLM_STRING_ENCODING_UTF8) == ok, "restore UTF-8 default");
        assert(runner, &result, bpa.clm_bpa_find_block(latin).?.encoding == string.CLM_STRING_ENCODING_LATIN1, "default changes preserve existing preference");
        for ([_]u64{ latin, latin_copy, unicode, unicode_copy, 90105, 90107 }) |resource| {
            assert(runner, &result, string.clm_string_delete(resource), "delete encoding and slice test string");
        }
    }

    assert(runner, &result, string.clm_string_set(id, "red red") == 7, "set string content");
    assert(runner, &result, string.clm_string_replace(id, "red", "blue") == 2, "replace every match");
    assert(runner, &result, equals(id, "blue blue"), "replace-all result");
    assert(runner, &result, string.clm_string_new(90002, "scratch"), "create other string");
    assert(runner, &result, string.clm_string_delete(90002), "delete other string");
    assert(runner, &result, bpa.clm_bpa_create(90008, bpa.CLM_BPA_TYPE_INT) == ok, "create non-string block");
    assert(runner, &result, !string.clm_string_delete(90008) and bpa.clm_bpa_is_existing(90008), "delete preserves other block types");
    assert(runner, &result, bpa.clm_bpa_delete(90008) == ok, "delete non-string test block");
    assert(runner, &result, string.clm_string_new(first, "pear"), "create first array string");
    assert(runner, &result, string.clm_string_new(second, "apple"), "create second array string");
    assert(runner, &result, string.clm_string_new(third, "banana"), "create third array string");
    assert(runner, &result, string.clm_string_new(scratch, ""), "create array output string");
    assert(runner, &result, string.clm_string_array_new(array, 0), "create string array");
    assert(runner, &result, string.clm_string_array_append(array, first) == ok, "append first string");
    assert(runner, &result, string.clm_string_array_append(array, second) == ok, "append second string");
    assert(runner, &result, string.clm_string_array_insert(array, 1, third) == ok, "insert third string");
    assert(runner, &result, string.clm_string_array_count(array) == 3, "array count");
    assert(runner, &result, string.clm_string_array_find(array, third) == 1, "find inserted string");
    assert(runner, &result, string.clm_string_array_get(array, 1, scratch) == get_ok, "get array string into block");
    assert(runner, &result, equals(scratch, "banana"), "array get content");
    assert(runner, &result, string.clm_string_array_length_at(array, 1) == 6, "indexed string length");
    assert(runner, &result, string.clm_string_array_contains_at(array, 1, "ana"), "indexed contains");
    assert(runner, &result, string.clm_string_array_append_at(array, 1, "!") == 1, "indexed append");
    assert(runner, &result, string.clm_string_array_append_line_at(array, 1, ".") == 2, "indexed append line");
    assert(runner, &result, string.clm_string_array_insert_at(array, 1, 0, "!") == 1, "indexed insert");
    assert(runner, &result, string.clm_string_array_remove_at(array, 1, 0, 1) == 1, "indexed remove");
    assert(runner, &result, string.clm_string_array_replace_at(array, 1, "a", "A") == 3, "indexed replace all");
    assert(runner, &result, string.clm_string_array_get(array, 1, scratch) == get_ok, "get mutated array string");
    assert(runner, &result, equals(scratch, "bAnAnA!.\n"), "indexed mutation content");
    assert(runner, &result, string.clm_string_array_set(array, 0, second) == set_ok, "set array item");
    assert(runner, &result, string.clm_string_array_set(array, 1, third) == set_ok, "reset mutated array item");
    assert(runner, &result, string.clm_string_array_sort(array) == ok, "sort array");
    assert(runner, &result, string.clm_string_array_move_last(array) == ok, "move cursor last");
    assert(runner, &result, string.clm_string_array_get_current(array, scratch) == get_ok, "get current array item");
    assert(runner, &result, equals(scratch, "banana"), "sorted final string");
    assert(runner, &result, string.clm_string_array_move_first(array) == ok, "move cursor first");
    assert(runner, &result, string.clm_string_array_move_next(array) == ok, "move cursor next");
    assert(runner, &result, string.clm_string_array_move_previous(array) == ok, "move cursor previous");
    assert(runner, &result, string.clm_string_array_move_to(array, 2) == ok, "move cursor to index");
    assert(runner, &result, string.clm_string_array_remove(array, 1) == ok, "remove array item");
    assert(runner, &result, string.clm_string_array_count(array) == 2, "count after removal");
    assert(runner, &result, string.clm_string_array_shuffle(array) == ok, "shuffle string array");
    assert(runner, &result, string.clm_string_array_draw_at(array, 0, scratch) == get_ok, "draw indexed string");
    assert(runner, &result, string.clm_string_array_draw(array, scratch) == get_ok, "draw next string");
    assert(runner, &result, string.clm_string_array_draw(array, scratch) == get_ok, "draw reshuffles exhausted deck");
    assert(runner, &result, string.clm_string_array_draw_at(array, 99, scratch) == fail, "draw rejects out-of-range deck index");
    assert(runner, &result, string.clm_string_array_clear(array) == ok, "clear array");
    assert(runner, &result, string.clm_string_array_count(array) == 0, "cleared array count");
    assert(runner, &result, string.clm_string_array_delete(array), "delete string array");
    for ([_]u64{ first, second, third, scratch, id }) |resource| {
        assert(runner, &result, string.clm_string_delete(resource), "delete string test resource");
    }
    test_report.clm_test_summary(runner, result);
}

test "ported C string suite" {
    var output: [32768]u8 = undefined;
    var writer = std.Io.Writer.fixed(&output);
    var runner = test_report.TestRunner.init(&writer, null);
    clm_string_tests(&runner);
    if (runner.total_failed != 0) std.debug.print("{s}", .{writer.buffered()});
    try std.testing.expectEqual(@as(usize, 0), runner.total_failed);
    try std.testing.expectEqual(@as(usize, 110), runner.total_passed);
}
