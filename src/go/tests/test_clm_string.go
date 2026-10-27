package tests

import (
	"bytes"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

func stringEquals(id uint64, expected string) bool {
	actual, status := clm.StringValue(id)
	return status == 1 && actual == expected
}

func StringTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - String")
	defer clm.BpaShutdown()
	initial := "A\xc3\xa9\xf0\x9f\x90\xb1"
	result.Assert(clm.StringNew(90001, initial) && clm.StringLength(90001) == 3, "UTF-8 length counts code points")
	result.Assert(clm.StringAppend(90001, "!") == 1 && clm.StringInsert(90001, 1, "\xe4\xb8\xad") == 1 && stringEquals(90001, "A\xe4\xb8\xad\xc3\xa9\xf0\x9f\x90\xb1!"), "insert at code-point index")
	result.Assert(clm.StringRemove(90001, 1, 1) == 1 && clm.StringReplace(90001, "\xc3\xa9", "\xe4\xb8\xad") == 1, "remove and replace Unicode")
	result.Assert(clm.StringReplace(90001, "!", "") == 1 && clm.StringAppendLine(90001, "!") == 2 && clm.StringContains(90001, "\n"), "empty replacement and append line")
	result.Assert(clm.StringRemove(90001, -1, 1) == -1 && clm.StringInsert(90001, 999, "x") == -1 && clm.StringReplace(90001, "", "x") == -1 && !clm.StringNew(90008, "\xc0\xaf"), "reject invalid indexes and UTF-8")
	result.Assert(clm.StringGetDefaultEncoding() == clm.StringEncodingUTF8 && clm.StringSetDefaultEncoding(clm.StringEncodingLatin1) == 0, "configure Latin-1 export default")
	result.Assert(clm.StringNew(90100, "caf\xc3\xa9") && clm.BpaFindBlock(90100).Encoding == clm.StringEncodingLatin1 && clm.StringGetBytes(90100, nil) == 4, "record preferred encoding")
	encoded := bytes.Repeat([]byte{0xa5}, 16)
	result.Assert(clm.StringGetBytes(90100, encoded[:3]) == -1 && encoded[0] == 0xa5, "undersized encoding output is unchanged")
	result.Assert(clm.StringGetBytes(90100, encoded) == 4 && bytes.Equal(encoded[:4], []byte{'c', 'a', 'f', 0xe9}), "exact Latin-1 export")
	result.Assert(clm.StringFromBytes(90101, clm.StringEncodingLatin1, encoded[:4]) == 0 && stringEquals(90101, "caf\xc3\xa9"), "Latin-1 round trip")
	result.Assert(clm.StringFromBytes(90101, clm.StringEncodingUTF8, []byte{0xc0, 0xaf}) == -1 && stringEquals(90101, "caf\xc3\xa9"), "invalid decode preserves existing text")
	for _, data := range [][]byte{{0, 0xd8}, {0}, {0x31, 0xdc}} {
		result.Assert(clm.StringFromBytes(90104, clm.StringEncodingUTF16LE, data) == -1 && !clm.BpaIsExisting(90104), "reject malformed UTF-16")
	}
	result.Assert(clm.StringFromBytes(90104, clm.StringEncodingUTF8, []byte{'A', 0, 'B'}) == -1, "reject embedded NUL")
	result.Assert(clm.StringSetDefaultEncoding(255) == -1 && clm.StringGetDefaultEncoding() == clm.StringEncodingLatin1, "invalid default leaves state unchanged")
	result.Assert(clm.StringNew(90102, initial) && clm.StringChangeEncoding(90102, clm.StringEncodingUTF16LE) == 0, "select UTF-16LE export")
	expected := []byte{0x41, 0, 0xe9, 0, 0x3d, 0xd8, 0x31, 0xdc}
	result.Assert(clm.StringGetBytes(90102, encoded) == 8 && bytes.Equal(encoded[:8], expected), "UTF-16LE surrogate pair export")
	result.Assert(clm.StringFromBytes(90103, clm.StringEncodingUTF16LE, expected) == 0 && stringEquals(90103, initial), "UTF-16LE round trip")
	result.Assert(clm.StringChangeEncoding(90102, clm.StringEncodingLatin1) == -1 && clm.BpaFindBlock(90102).Encoding == clm.StringEncodingUTF16LE, "unrepresentable encoding rolls back")
	borrowed, status := clm.StringSliceBorrow(90102, 1, 2)
	result.Assert(status == 0 && len(borrowed) == 6 && string(borrowed) == initial[1:], "borrow code-point slice")
	result.Assert(clm.StringSlice(90102, 1, 2, 90105) == 0 && clm.BpaFindBlock(90105).Encoding == clm.StringEncodingUTF16LE, "copy inherits encoding")
	borrowed, status = clm.StringSliceBorrow(90102, 3, 0)
	result.Assert(status == 0 && len(borrowed) == 0 && clm.StringSlice(90102, 3, 0, 90107) == 0, "empty end slices")
	result.Assert(clm.StringSlice(90102, 4, 0, 90106) == -1 && clm.StringSlice(90102, -1, 1, 90106) == -1 && clm.StringSlice(90102, 0, 1, 90100) == -1, "reject invalid slice bounds and existing destination")
	result.Assert(clm.StringSet(90102, "changed") == 7 && stringEquals(90105, initial[1:]), "copied string independent")
	result.Assert(clm.StringSetDefaultEncoding(clm.StringEncodingUTF8) == 0 && clm.BpaFindBlock(90100).Encoding == clm.StringEncodingLatin1, "new default preserves old preference")
	result.Assert(clm.StringSet(90001, "red red") == 7 && clm.StringReplace(90001, "red", "blue") == 2 && stringEquals(90001, "blue blue"), "replace every occurrence")
	result.Assert(clm.BpaCreate(90008, clm.BpaTypeInt) == 0 && !clm.StringDelete(90008) && clm.BpaIsExisting(90008), "string delete preserves other types")
	for index, text := range []string{"pear", "apple", "banana", ""} {
		result.Assert(clm.StringNew(uint64(90004+index), text), "create string-array value")
	}
	result.Assert(clm.StringArrayNew(90003, 0) && clm.StringArrayAppend(90003, 90004) == 0 && clm.StringArrayAppend(90003, 90005) == 0 && clm.StringArrayInsert(90003, 1, 90006) == 0, "append and insert strings")
	result.Assert(clm.StringArrayCount(90003) == 3 && clm.StringArrayFind(90003, 90006) == 1 && clm.StringArrayLengthAt(90003, 1) == 6 && clm.StringArrayContainsAt(90003, 1, "ana"), "array queries")
	result.Assert(clm.StringArrayAppendAt(90003, 1, "!") == 1 && clm.StringArrayAppendLineAt(90003, 1, ".") == 2 && clm.StringArrayInsertAt(90003, 1, 0, "!") == 1 && clm.StringArrayRemoveAt(90003, 1, 0, 1) == 1 && clm.StringArrayReplaceAt(90003, 1, "a", "A") == 3, "indexed text edits")
	result.Assert(clm.StringArrayGet(90003, 1, 90007) == 1 && stringEquals(90007, "bAnAnA!.\n"), "indexed edit result")
	result.Assert(clm.StringArraySet(90003, 0, 90005) == 2 && clm.StringArraySet(90003, 1, 90006) == 2 && clm.StringArraySort(90003) == 0 && clm.StringArrayMoveLast(90003) == 0 && clm.StringArrayGetCurrent(90003, 90007) == 1 && stringEquals(90007, "banana"), "sort and cursor")
	result.Assert(clm.StringArrayMoveFirst(90003) == 0 && clm.StringArrayMoveNext(90003) == 0 && clm.StringArrayMovePrevious(90003) == 0 && clm.StringArrayMoveTo(90003, 2) == 0, "array cursor operations")
	result.Assert(clm.StringArrayRemove(90003, 1) == 0 && clm.StringArrayShuffle(90003) == 0 && clm.StringArrayDrawAt(90003, 0, 90007) == 1 && clm.StringArrayDraw(90003, 90007) == 1 && clm.StringArrayDraw(90003, 90007) == 1 && clm.StringArrayDrawAt(90003, 99, 90007) == -1, "draw and restart string deck")
	result.Assert(clm.StringArrayClear(90003) == 0 && clm.StringArrayCount(90003) == 0 && clm.StringArrayDelete(90003), "clear and delete array")
	for _, id := range []uint64{90001, 90004, 90005, 90006, 90007, 90100, 90101, 90102, 90103, 90105, 90107} {
		result.Assert(clm.StringDelete(id), "delete string resource")
	}
	CLMTestSummary("String Tests Summary", result)
	return result
}
