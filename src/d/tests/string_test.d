module tests.string_test;

import clm.bpa;
import clm.string;
import tests.test;

private bool stringEquals(ulong id, string expected)
{
    auto block = clm_bpa_find_block(id);
    return block !is null && block.type == CLM_BPA_TYPE_STRING &&
        cast(const(char)[])block.data[0 .. block.length] == expected;
}

int clmStringTests()
{
    ClmTestResult result;
    enum utf8Initial = "A\u00E9\U0001F431";
    enum utf8AfterInsert = "A\u4E2D\u00E9\U0001F431!";
    enum utf8AfterRemove = "A\u00E9\U0001F431!";
    enum utf8AfterReplace = "A\u4E2D\U0001F431!";
    enum ulong stringId = 90001;
    enum ulong otherStringId = 90002;
    enum ulong arrayId = 90003;
    enum ulong firstValueId = 90004;
    enum ulong secondValueId = 90005;
    enum ulong thirdValueId = 90006;
    enum ulong scratchId = 90007;

    clmTestBegin("CLM - String");

    clmAssert(result, clm_string_new(stringId, utf8Initial), "create a UTF-8 string");
    clmAssert(result, clm_string_length(stringId) == 3, "string length counts code points");
    clmAssert(result, clm_bpa_find_block(stringId).encoding == CLM_STRING_ENCODING_UTF8,
        "string block records UTF-8 encoding");
    clmAssert(result, clm_string_append(stringId, "!") == 1, "append text");
    clmAssert(result, clm_string_insert(stringId, 1, "\u4E2D") == 1, "insert a UTF-8 character");
    clmAssert(result, stringEquals(stringId, utf8AfterInsert), "insert respects UTF-8 code-point index");
    clmAssert(result, clm_string_remove(stringId, 1, 1) == 1, "remove a character");
    clmAssert(result, stringEquals(stringId, utf8AfterRemove), "remove updates content");
    clmAssert(result, clm_string_replace(stringId, "\u00E9", "\u4E2D") == 1, "replace a UTF-8 match");
    clmAssert(result, stringEquals(stringId, utf8AfterReplace), "replace updates content");
    clmAssert(result, clm_string_replace(stringId, "!", "") == 1, "allow an empty replacement");
    clmAssert(result, clm_string_contains(stringId, "\U0001F431"), "contains finds UTF-8 text");

    clmAssert(result, clm_string_append_line(stringId, "!") == 2, "append text and newline");
    clmAssert(result, clm_string_contains(stringId, "\n"), "contains finds newline");
    clmAssert(result, clm_string_length(stringId) > 0, "length reports nonempty string");
    clmAssert(result, clm_string_remove(stringId, -1, 1) == CLM_BPA_FAILURE, "remove rejects a negative index");
    clmAssert(result, clm_string_insert(stringId, int.max, "x") == CLM_BPA_FAILURE,
        "insert rejects an out-of-range index");
    clmAssert(result, clm_string_replace(stringId, "", "x") == CLM_BPA_FAILURE, "replace rejects an empty search");
    clmAssert(result, !clm_string_new(90008, "\xC0\xAF"), "new rejects invalid UTF-8");

    {
        enum ulong latinId = 90100;
        enum ulong latinCopyId = 90101;
        enum ulong unicodeId = 90102;
        enum ulong unicodeCopyId = 90103;
        enum ulong invalidId = 90104;
        enum latinText = "caf\u00E9";
        enum unicodeText = "A\u00E9\U0001F431";
        immutable ubyte[] expectedLatin = ['c', 'a', 'f', 0xE9];
        immutable ubyte[] expectedUtf16 = [0x41, 0x00, 0xE9, 0x00, 0x3D, 0xD8, 0x31, 0xDC];
        immutable ubyte[] malformedUtf16 = [0x00, 0xD8];
        immutable ubyte[] embeddedNul = ['A', 0, 'B'];
        ubyte[16] encoded;
        string borrowedText;
        size_t borrowedBytes;

        clmAssert(result, clm_string_get_default_encoding() == CLM_STRING_ENCODING_UTF8,
            "runtime default starts as UTF-8");
        clmAssert(result, clm_string_set_default_encoding(CLM_STRING_ENCODING_LATIN1) == CLM_BPA_SUCCESS,
            "set Latin-1 byte-export default");
        clmAssert(result, clm_string_new(latinId, latinText), "create text with Latin-1 export preference");
        clmAssert(result, clm_bpa_find_block(latinId).encoding == CLM_STRING_ENCODING_LATIN1,
            "new string records current export preference");
        clmAssert(result, clm_string_get_bytes(latinId, null) == 4, "query encoded Latin-1 byte count");
        encoded[] = 0xA5;
        clmAssert(result, clm_string_get_bytes(latinId, encoded[0 .. 3]) == CLM_BPA_FAILURE && encoded[0] == 0xA5,
            "undersized encoding output fails without writing");
        clmAssert(result, clm_string_get_bytes(latinId, encoded[]) == 4 && encoded[0 .. 4] == expectedLatin,
            "export Latin-1 bytes exactly");
        clmAssert(result, clm_string_from_bytes(latinCopyId, CLM_STRING_ENCODING_LATIN1, expectedLatin) == CLM_BPA_SUCCESS &&
            stringEquals(latinCopyId, latinText), "decode Latin-1 bytes into UTF-8 storage");
        clmAssert(result, clm_bpa_find_block(latinCopyId).encoding == CLM_STRING_ENCODING_LATIN1,
            "from_bytes records source encoding preference");
        clmAssert(result, clm_string_from_bytes(latinCopyId, CLM_STRING_ENCODING_UTF8, cast(immutable(ubyte)[])"\xC0\xAF") ==
            CLM_BPA_FAILURE && stringEquals(latinCopyId, latinText),
            "invalid UTF-8 input preserves existing string");
        clmAssert(result, clm_string_from_bytes(invalidId, CLM_STRING_ENCODING_UTF16LE, malformedUtf16) == CLM_BPA_FAILURE &&
            !clm_bpa_is_existing(invalidId), "unpaired UTF-16 surrogate does not create a string");
        clmAssert(result, clm_string_from_bytes(invalidId, CLM_STRING_ENCODING_UTF8, embeddedNul) == CLM_BPA_FAILURE &&
            !clm_bpa_is_existing(invalidId), "embedded NUL input is rejected");
        clmAssert(result, clm_string_from_bytes(invalidId, CLM_STRING_ENCODING_UTF16LE, expectedUtf16[0 .. 1]) ==
            CLM_BPA_FAILURE, "odd-length UTF-16 input is rejected");
        clmAssert(result, clm_string_set_default_encoding(0xFF) == CLM_BPA_FAILURE &&
            clm_string_get_default_encoding() == CLM_STRING_ENCODING_LATIN1,
            "unsupported default encoding is rejected");
        clmAssert(result, clm_string_new(unicodeId, unicodeText), "create Unicode string for UTF-16LE and slicing");
        clmAssert(result, clm_string_change_encoding(unicodeId, CLM_STRING_ENCODING_UTF16LE) == CLM_BPA_SUCCESS,
            "change representable export encoding to UTF-16LE");
        clmAssert(result, clm_string_get_bytes(unicodeId, null) == cast(int)expectedUtf16.length,
            "query UTF-16LE byte count");
        clmAssert(result, clm_string_get_bytes(unicodeId, encoded[]) == cast(int)expectedUtf16.length &&
            encoded[0 .. expectedUtf16.length] == expectedUtf16, "export UTF-16LE including surrogate pair");
        clmAssert(result, clm_string_from_bytes(unicodeCopyId, CLM_STRING_ENCODING_UTF16LE, expectedUtf16) == CLM_BPA_SUCCESS &&
            stringEquals(unicodeCopyId, unicodeText), "round-trip UTF-16LE through UTF-8 storage");
        clmAssert(result, clm_string_change_encoding(unicodeId, CLM_STRING_ENCODING_LATIN1) == CLM_BPA_FAILURE &&
            clm_bpa_find_block(unicodeId).encoding == CLM_STRING_ENCODING_UTF16LE,
            "unrepresentable encoding change fails and rolls back");
        clmAssert(result, clm_string_slice_borrow(unicodeId, 1, 2, borrowedText, borrowedBytes) == CLM_BPA_SUCCESS &&
            borrowedBytes == 6 && borrowedText[0 .. borrowedBytes] == "\u00E9\U0001F431",
            "borrow Unicode slice by code-point range");
        clmAssert(result, clm_string_slice(unicodeId, 1, 2, 90105) == CLM_BPA_SUCCESS &&
            stringEquals(90105, "\u00E9\U0001F431"), "copy Unicode slice into independent BPA string");
        clmAssert(result, clm_bpa_find_block(90105).encoding == CLM_STRING_ENCODING_UTF16LE,
            "copied slice inherits preferred export encoding");
        clmAssert(result, clm_string_slice_borrow(unicodeId, 3, 0, borrowedText, borrowedBytes) == CLM_BPA_SUCCESS &&
            borrowedBytes == 0, "empty borrowed slice at end succeeds");
        clmAssert(result, clm_string_slice(unicodeId, 3, 0, 90107) == CLM_BPA_SUCCESS && stringEquals(90107, ""),
            "empty copied slice at end succeeds");
        clmAssert(result, clm_string_slice(unicodeId, 4, 0, 90106) == CLM_BPA_FAILURE &&
            clm_string_slice(unicodeId, -1, 1, 90106) == CLM_BPA_FAILURE,
            "copy slice rejects out-of-range and negative positions");
        clmAssert(result, clm_string_slice_borrow(unicodeId, 4, 0, borrowedText, borrowedBytes) == CLM_BPA_FAILURE,
            "borrowed slice rejects out-of-range position");
        clmAssert(result, clm_string_slice(unicodeId, 0, 1, latinId) == CLM_BPA_FAILURE,
            "copy slice rejects an existing destination ID");
        clmAssert(result, clm_string_set(unicodeId, "changed") == 7 && stringEquals(90105, "\u00E9\U0001F431"),
            "copied slice remains independent after source mutation");
        clmAssert(result, clm_string_set_default_encoding(CLM_STRING_ENCODING_UTF8) == CLM_BPA_SUCCESS,
            "restore UTF-8 byte-export default");
        clmAssert(result, clm_bpa_find_block(latinId).encoding == CLM_STRING_ENCODING_LATIN1,
            "changing runtime default does not alter existing preference");
        clmAssert(result, clm_string_delete(latinId), "delete Latin-1 test string");
        clmAssert(result, clm_string_delete(latinCopyId), "delete Latin-1 round-trip string");
        clmAssert(result, clm_string_delete(unicodeId), "delete UTF-16LE test string");
        clmAssert(result, clm_string_delete(unicodeCopyId), "delete UTF-16LE round-trip string");
        clmAssert(result, clm_string_delete(90105), "delete copied Unicode slice");
        clmAssert(result, clm_string_delete(90107), "delete empty copied slice");
    }

    clmAssert(result, clm_string_set(stringId, "red red") == 7, "set replaces string content");
    clmAssert(result, clm_string_replace(stringId, "red", "blue") == 2, "replace replaces every match");
    clmAssert(result, stringEquals(stringId, "blue blue"), "replace-all result is correct");
    clmAssert(result, clm_string_new(otherStringId, "scratch"), "create scratch string");
    clmAssert(result, clm_string_delete(otherStringId), "delete removes a string block");
    clmAssert(result, clm_bpa_create(90008, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS,
        "create a non-string block for delete type safety");
    clmAssert(result, !clm_string_delete(90008) && clm_bpa_is_existing(90008),
        "delete preserves non-string blocks");
    clmAssert(result, clm_bpa_delete(90008) == CLM_BPA_SUCCESS, "delete non-string test block");

    clmAssert(result, clm_string_new(firstValueId, "pear"), "create first array string");
    clmAssert(result, clm_string_new(secondValueId, "apple"), "create second array string");
    clmAssert(result, clm_string_new(thirdValueId, "banana"), "create third array string");
    clmAssert(result, clm_string_new(scratchId, ""), "create array output string");
    clmAssert(result, clm_string_array_new(arrayId, 0), "create string array");
    clmAssert(result, clm_string_array_append(arrayId, firstValueId) == CLM_BPA_SUCCESS, "append first string");
    clmAssert(result, clm_string_array_append(arrayId, secondValueId) == CLM_BPA_SUCCESS, "append second string");
    clmAssert(result, clm_string_array_insert(arrayId, 1, thirdValueId) == CLM_BPA_SUCCESS, "insert string");
    clmAssert(result, clm_string_array_count(arrayId) == 3, "array count is 3");
    clmAssert(result, clm_string_array_find(arrayId, thirdValueId) == 1, "find inserted item");
    clmAssert(result, clm_string_array_get(arrayId, 1, scratchId) == CLM_BPA_GET_SUCCESS,
        "get array item into BPA string");
    clmAssert(result, stringEquals(scratchId, "banana"), "get returns expected string");
    clmAssert(result, clm_string_array_length_at(arrayId, 1) == 6, "length of indexed array string");
    clmAssert(result, clm_string_array_contains_at(arrayId, 1, "ana"), "contains finds substring");
    clmAssert(result, clm_string_array_append_at(arrayId, 1, "!") == 1, "append to indexed array string");
    clmAssert(result, clm_string_array_append_line_at(arrayId, 1, ".") == 2, "append line to indexed array string");
    clmAssert(result, clm_string_array_insert_at(arrayId, 1, 0, "!") == 1, "insert into indexed array string");
    clmAssert(result, clm_string_array_remove_at(arrayId, 1, 0, 1) == 1, "remove from indexed array string");
    clmAssert(result, clm_string_array_replace_at(arrayId, 1, "a", "A") == 3,
        "replace all matches in indexed array string");
    clmAssert(result, clm_string_array_get(arrayId, 1, scratchId) == CLM_BPA_GET_SUCCESS, "get mutated array item");
    clmAssert(result, stringEquals(scratchId, "bAnAnA!.\n"), "indexed string operations produce expected content");
    clmAssert(result, clm_string_array_set(arrayId, 0, secondValueId) == CLM_BPA_SET_SUCCESS,
        "set array item from BPA string");
    clmAssert(result, clm_string_array_set(arrayId, 1, thirdValueId) == CLM_BPA_SET_SUCCESS,
        "reset mutated array item");
    clmAssert(result, clm_string_array_sort(arrayId) == CLM_BPA_SUCCESS, "sort string array");
    clmAssert(result, clm_string_array_move_last(arrayId) == CLM_BPA_SUCCESS, "move cursor to last");
    clmAssert(result, clm_string_array_get_current(arrayId, scratchId) == CLM_BPA_GET_SUCCESS,
        "get current array item");
    clmAssert(result, stringEquals(scratchId, "banana"), "sort and cursor return final string");
    clmAssert(result, clm_string_array_move_first(arrayId) == CLM_BPA_SUCCESS, "move cursor to first");
    clmAssert(result, clm_string_array_move_next(arrayId) == CLM_BPA_SUCCESS, "move cursor next");
    clmAssert(result, clm_string_array_move_previous(arrayId) == CLM_BPA_SUCCESS, "move cursor previous");
    clmAssert(result, clm_string_array_move_to(arrayId, 2) == CLM_BPA_SUCCESS, "move cursor to index");
    clmAssert(result, clm_string_array_remove(arrayId, 1) == CLM_BPA_SUCCESS, "remove array item");
    clmAssert(result, clm_string_array_count(arrayId) == 2, "array count updates after removal");
    clmAssert(result, clm_string_array_shuffle(arrayId) == CLM_BPA_SUCCESS, "shuffle string array");
    clmAssert(result, clm_string_array_draw_at(arrayId, 0, scratchId) == CLM_BPA_GET_SUCCESS,
        "draw indexed string from deck");
    clmAssert(result, clm_string_array_draw(arrayId, scratchId) == CLM_BPA_GET_SUCCESS, "draw next string from deck");
    clmAssert(result, clm_string_array_draw(arrayId, scratchId) == CLM_BPA_GET_SUCCESS,
        "draw reshuffles exhausted deck");
    clmAssert(result, clm_string_array_draw_at(arrayId, 99, scratchId) == CLM_BPA_FAILURE,
        "draw rejects out-of-range deck index");
    clmAssert(result, clm_string_array_clear(arrayId) == CLM_BPA_SUCCESS, "clear string array");
    clmAssert(result, clm_string_array_count(arrayId) == 0, "cleared array has zero items");
    clmAssert(result, clm_string_array_delete(arrayId), "delete string array");
    clmAssert(result, clm_string_delete(firstValueId), "delete first array string");
    clmAssert(result, clm_string_delete(secondValueId), "delete second array string");
    clmAssert(result, clm_string_delete(thirdValueId), "delete third array string");
    clmAssert(result, clm_string_delete(scratchId), "delete array output string");
    clmAssert(result, clm_string_delete(stringId), "delete test string");

    clmTestSummary(result.passed, result.failed);
    return result.failed;
}
