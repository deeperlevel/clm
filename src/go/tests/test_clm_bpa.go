package tests

import (
	"bytes"
	"encoding/binary"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

func intBytes(value int32) []byte { return binary.LittleEndian.AppendUint32(nil, uint32(value)) }

func BpaTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - Block Pool Allocation (BPA)")
	defer clm.BpaShutdown()
	result.Assert(clm.BpaTypeStruct == 0 && clm.BpaTypeDatetime == 13, "BPA type codes match C")
	result.Assert(!clm.BpaIsExisting(1) && clm.BpaCreate(1, clm.BpaTypeInt) == 0, "create scalar block")
	result.Assert(clm.BpaCreate(1, clm.BpaTypeInt) == -1, "reject duplicate block")
	result.Assert(clm.BpaSet(1, intBytes(9)) == 2, "set scalar block")
	output := make([]byte, 4)
	result.Assert(clm.BpaGet(1, output) == 1 && bytes.Equal(output, intBytes(9)), "get scalar block")
	result.Assert(clm.BpaGet(1, output[:1]) == -1, "reject undersized output")
	result.Assert(clm.BpaRef(1) == 0 && clm.BpaUnref(1) == 0 && clm.BpaIsExisting(1), "retain referenced block")
	result.Assert(clm.BpaUnref(1) == 0 && !clm.BpaIsExisting(1), "release final block reference")
	result.Assert(clm.BpaCreate(2, clm.BpaTypeStruct) == -1 && clm.BpaCreateSized(2, clm.BpaTypeStruct, 8) == 0, "require struct width")
	result.Assert(clm.BpaSet(2, append(intBytes(123), intBytes(456)...)) == 2, "set struct bytes")
	result.Assert(clm.BpaDelete(2) == 0, "delete struct")
	result.Assert(clm.BpaCreate(3, clm.BpaTypeString) == 0 && clm.BpaSetString(3, []byte("hello\x00")) == 5, "create string block")
	text := make([]byte, 32)
	result.Assert(clm.BpaGetString(3, text) == 5 && string(text[:5]) == "hello", "read string block")
	result.Assert(clm.BpaGetString(3, text[:5]) == -1 && clm.BpaSetString(3, []byte("unterminated")) == -1, "require string terminator and output capacity")
	result.Assert(clm.BpaReplace(3, []byte("world! updated\x00")) == 14, "replace string payload")
	result.Assert(clm.BpaDelete(3) == 0, "delete string")
	result.Assert(clm.BpaaCreate(10, clm.BpaTypeInt, 1, 0) == 0, "create integer array")
	for _, value := range []int32{4, 1, 3, 2} {
		result.Assert(clm.BpaaAppendRaw(10, intBytes(value)) == 0, "append and grow array")
	}
	array := clm.BpaaFindArray(10)
	result.Assert(array.Count == 4 && array.Capacity >= 4, "array count and capacity")
	result.Assert(clm.BpaaGetRaw(10, -1, output) == -1 && clm.BpaaGetRaw(10, 4, output) == -1, "reject invalid array indexes")
	result.Assert(clm.BpaaInsertRaw(10, 0, array.Data[4:8]) == 0 && clm.BpaaGetFirstRaw(10, output) == 1 && bytes.Equal(output, intBytes(1)), "insert from aliased array payload")
	result.Assert(clm.BpaaMoveLast(10) == 0 && clm.BpaaMoveNext(10) == -1 && clm.BpaaMovePrevious(10) == 0, "array cursor boundaries")
	result.Assert(clm.BpaaSortWith(10, func(left, right []byte) int {
		leftValue, rightValue := int32(binary.LittleEndian.Uint32(left)), int32(binary.LittleEndian.Uint32(right))
		if leftValue < rightValue {
			return -1
		}
		if leftValue > rightValue {
			return 1
		}
		return 0
	}) == 0, "sort with comparator")
	result.Assert(clm.BpaaSlice(10, 1, 2, 11) == 0, "copy array slice")
	borrowed, status := clm.BpaaSliceBorrow(10, 1, 2)
	result.Assert(status == 0 && len(borrowed) == 8, "borrow contiguous array slice")
	copyBefore := bytes.Clone(clm.BpaaFindArray(11).Data)
	result.Assert(clm.BpaaSetRaw(10, 1, intBytes(99)) == 2 && bytes.Equal(copyBefore, clm.BpaaFindArray(11).Data), "copied slice is independent")
	result.Assert(clm.BpaaSlice(10, 5, 0, 12) == 0 && clm.BpaaSlice(10, 4, 2, 13) == -1, "empty and invalid slices")
	result.Assert(clm.BpaaSeal(10) == 0, "seal array capacity")
	for array.Count < array.Capacity {
		result.Assert(clm.BpaaAppendRaw(10, intBytes(4)) == 0, "sealed array accepts reserved capacity")
	}
	result.Assert(clm.BpaaAppendRaw(10, intBytes(4)) == -1, "sealed array cannot grow")
	result.Assert(clm.BpaaClear(10) == 0 && clm.BpaaAppendRaw(10, intBytes(7)) == 0, "clear retains reserved capacity")
	result.Assert(clm.BpaaDrawRaw(10, output) == 1 && clm.BpaaDrawRaw(10, output) == 1, "exhausted deck restarts")
	advanced, status := clm.BpaaAdvancedBorrow(10)
	result.Assert(status == 0 && clm.BpaaUnref(10) == 0 && clm.BpaaIsExisting(10), "advanced borrow retains array")
	result.Assert(clm.BpaaAdvancedRelease(&advanced) == 0 && advanced == nil && !clm.BpaaIsExisting(10), "release final advanced borrow")
	result.Assert(clm.BpaaAdvancedRelease(&advanced) == -1, "reject double release")
	result.Assert(clm.BpaaCreate(20, clm.BpaTypeString, 0, 0) == 0 && clm.BpaaAppendRaw(20, []byte("hello\x00")) == 0, "create string array")
	result.Assert(clm.BpaaSlice(20, 0, 1, 21) == 0 && clm.BpaaSetStringRaw(20, 0, []byte("changed\x00")) == 7, "copy and mutate string array")
	result.Assert(clm.BpaaGetStringRaw(21, 0, text) == 5 && string(text[:5]) == "hello", "string slice is independent")
	_, status = clm.BpaaSliceBorrow(20, 0, 0)
	result.Assert(status == -1, "reject raw string-array borrow")
	for index := uint64(0); index < 64; index++ {
		result.Assert(clm.BpaCreate(0x100000001+index, clm.BpaTypeInt) == 0, "support wide IDs and registry growth")
	}
	activeBefore, capacityBefore, status := clm.BpaPoolStats(4)
	result.Assert(status == 0, "read pool statistics")
	result.Assert(clm.BpaCreate(50001, clm.BpaTypeInt) == 0, "allocate pooled block")
	activeAfter, capacityAfter, status := clm.BpaPoolStats(4)
	result.Assert(status == 0 && activeAfter == activeBefore+1 && capacityAfter >= capacityBefore && clm.BpaPoolCleanup() == -1, "track live pooled allocations")
	result.Assert(clm.BpaSet(50001, intBytes(777)) == 2 && clm.BpaaCreateSized(50002, clm.BpaTypeStruct, 0, 4, 0) == 0 && clm.BpaaAppend(50002, 50001, 4) == 0 && clm.BpaaAppend(50002, 50001, 5) == -1 && clm.BpaaGet(50002, 0, 50001, 4) == 1, "struct arrays transfer compatible scalar blocks")
	result.Assert(clm.BpaDelete(50001) == 0 && clm.BpaCreate(50001, clm.BpaTypeInt) == 0 && clm.BpaGet(50001, output) == 1 && bytes.Equal(output, intBytes(0)), "reused pool slot is zeroed")
	advanced, status = clm.BpaaAdvancedBorrow(50002)
	result.Assert(status == 0 && clm.BpaaAdvancedSetRaw(advanced, 0, intBytes(4)) == 2 && clm.BpaaAdvancedExtend(advanced, 1) == 0 && clm.BpaaAdvancedAppendRaw(advanced, intBytes(6)) == 0 && clm.BpaaAdvancedInsertRaw(advanced, 1, intBytes(2)) == 0, "advanced fixed-width mutations")
	result.Assert(clm.BpaaAdvancedRemove(advanced, 0) == 0 && clm.BpaaAdvancedShrink(advanced, advanced.Count) == 0 && clm.BpaaAdvancedGetRaw(advanced, 0, output) == 1 && bytes.Equal(output, intBytes(2)), "advanced removal and shrink")
	result.Assert(clm.BpaaAdvancedClear(advanced) == 0 && clm.BpaaAdvancedRelease(&advanced) == 0, "clear and release advanced handle")
	result.Assert(clm.BpaaCreate(53000, clm.BpaTypeInt, 0, 0) == 0, "create rotation array")
	for _, value := range []int32{4, 1, 3, 2} {
		clm.BpaaAppendRaw(53000, intBytes(value))
	}
	before := bytes.Clone(clm.BpaaFindArray(53000).Data[:16])
	result.Assert(clm.BpaaShiftLeft(53000, 1, 1) == 0 && clm.BpaaShiftRight(53000, 1, 1) == 0 && bytes.Equal(before, clm.BpaaFindArray(53000).Data[:16]), "suffix rotations are inverse")
	result.Assert(clm.BpaaSort(53000, func(left, right []byte) bool {
		return int32(binary.LittleEndian.Uint32(left)) > int32(binary.LittleEndian.Uint32(right))
	}) == 0 && clm.BpaaGetFirstRaw(53000, output) == 1 && bytes.Equal(output, intBytes(1)), "predicate reports out-of-order pairs")
	result.Assert(clm.BpaCreate(53001, clm.BpaTypeUnsignedInt) == 0 && clm.BpaaGetCount(53000, 53001) == 1 && clm.BpaGet(53001, output) == 1 && binary.LittleEndian.Uint32(output) == 4, "metadata transfer to typed BPA block")
	result.Assert(clm.BpaaGetCount(53000, 50001) == -1, "metadata rejects wrong destination type")
	payload, borrowStatus := clm.BpaBorrow(53001)
	result.Assert(borrowStatus == clm.BpaGetSuccess && len(payload) == 4, "block borrow returns GET_SUCCESS")
	result.Assert(clm.BpaShutdown() == 0 && clm.BpaPoolCleanup() == 0, "shutdown releases pools and registries")
	CLMTestSummary("BPA Tests Summary", result)
	return result
}
