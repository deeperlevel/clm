package tests

import (
	"bytes"
	"slices"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

func DictionaryTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - Dictionary")
	defer clm.BpaShutdown()
	result.Assert(clm.DictionaryNew(71000, clm.BpaTypeInt, 0) == 0 && clm.DictionaryCount(71000) == 0, "create empty integer dictionary")
	result.Assert(clm.DictionaryNew(71000, clm.BpaTypeInt, 0) == -1 && clm.DictionaryNew(71003, clm.BpaTypeString, 1) == -1, "reject duplicate ID and invalid size")
	for index := 0; index < 40; index++ {
		result.Assert(clm.DictionarySetFrom(71000, uint64(1000+index), intBytes(int32(index*10))) == 2, "insert and grow dictionary")
	}
	output := make([]byte, 32)
	result.Assert(clm.DictionarySetFrom(71000, 1005, intBytes(9876)) == 2 && clm.DictionaryGetTo(71000, 1005, output) == 1 && bytes.Equal(output[:4], intBytes(9876)) && clm.DictionaryCount(71000) == 40, "replace without adding key")
	result.Assert(clm.BpaCreate(71001, clm.BpaTypeInt) == 0 && clm.BpaSet(71001, intBytes(9876)) == 2 && clm.DictionarySet(71000, 0, 71001) == 2, "copy BPA block under key zero")
	result.Assert(clm.BpaCreate(71002, clm.BpaTypeInt) == 0 && clm.DictionaryGet(71000, 0, 71002) == 1, "retrieve into BPA block")
	result.Assert(clm.DictionaryRemove(71000, 1005) == 0 && !clm.DictionaryContains(71000, 1005) && clm.DictionaryGetTo(71000, 1005, output) == -1, "remove key")
	result.Assert(clm.DictionarySetFrom(71000, 99999, intBytes(12345)) == 2 && clm.DictionaryCount(71000) == 41, "insert after removal")
	clm.DictionaryIterateReset(71000)
	seen := map[uint64]bool{}
	for {
		key, status := clm.DictionaryIterateNext(71000)
		if status < 0 {
			break
		}
		result.Assert(!seen[key] && clm.DictionaryGetTo(71000, key, output) == 1, "iterate unique live keys")
		seen[key] = true
	}
	result.Assert(len(seen) == 41, "iterate every key")
	result.Assert(clm.DictionaryNew(71010, clm.BpaTypeString, 0) == 0 && clm.DictionarySetFrom(71010, 5, []byte("dictionary value\x00")) == 2 && clm.DictionaryGetTo(71010, 5, output) == 1 && string(output[:16]) == "dictionary value", "string dictionary round trip")
	result.Assert(clm.DictionarySetFrom(71010, 6, []byte{0}) == 2 && clm.DictionaryGetTo(71010, 6, output) == 1 && output[0] == 0, "empty string value")
	result.Assert(clm.DictionaryNew(71020, clm.BpaTypeStruct, 8) == 0 && clm.DictionarySetFrom(71020, 7, append(intBytes(11), intBytes(22)...)) == 2, "struct dictionary")
	result.Assert(clm.DictionaryNew(71050, clm.BpaTypeInt, 0) == 0, "create ordered dictionary")
	ordered := []uint64{900, 2, 77, 0}
	for _, key := range ordered {
		result.Assert(clm.DictionaryAddFrom(71050, key, intBytes(100)) == 2, "insert distinct ordered key")
	}
	result.Assert(clm.DictionaryAddFrom(71050, 2, intBytes(1)) == -1 && clm.DictionaryUpdateFrom(71050, 333, intBytes(1)) == -1, "insert-only and update-only semantics")
	result.Assert(clm.DictionaryUpdateFrom(71050, 2, intBytes(9)) == 2, "update existing key")
	keys := make([]uint64, 4)
	result.Assert(clm.DictionaryKeysTo(71050, keys) == 1 && slices.Equal(keys, ordered), "preserve insertion order")
	result.Assert(clm.DictionaryRemove(71050, 2) == 0 && clm.DictionaryAddFrom(71050, 2, intBytes(9)) == 2 && clm.DictionaryKeysTo(71050, keys) == 1 && slices.Equal(keys, []uint64{900, 77, 0, 2}), "reinsert moves key to end")
	result.Assert(clm.DictionaryClear(71050) == 0 && clm.DictionaryCount(71050) == 0, "clear dictionary")
	for _, id := range []uint64{71000, 71010, 71020, 71050} {
		result.Assert(clm.DictionaryDelete(id) == 0 && clm.DictionaryCount(id) == -1, "delete dictionary and backing array")
	}
	CLMTestSummary("Dictionary Tests Summary", result)
	return result
}
