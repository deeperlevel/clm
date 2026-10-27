package clm

import "slices"

const DictionaryInitialCapacity = 16

type dictionaryState struct {
	block    *BpaBlockNode
	values   IDKey
	keys     []IDKey
	indexes  map[IDKey]int
	iterator int
}

var dictionaries = map[IDKey]*dictionaryState{}

func allocateArrayID(forbidden IDKey) IDKey {
	for candidate := IDKeyMax; ; candidate-- {
		if candidate != forbidden && !BpaaIsExisting(candidate) {
			return candidate
		}
	}
}
func dictionaryRead(id IDKey) *dictionaryState {
	state := dictionaries[id]
	if state == nil || bpaBlocks[id] != state.block || bpaaArrays[state.values] == nil {
		return nil
	}
	return state
}
func DictionaryNew(id IDKey, kind byte, size int) int {
	if BpaIsExisting(id) || BpaaIsExisting(id) || bpaWidth(kind, size) < 0 {
		return BpaFailure
	}
	values := allocateArrayID(id)
	if BpaaCreateSized(values, kind, 0, size, 0) < 0 {
		return BpaFailure
	}
	if BpaCreateSized(id, BpaTypeStruct, 8) < 0 {
		BpaaDelete(values)
		return BpaFailure
	}
	dictionaries[id] = &dictionaryState{block: bpaBlocks[id], values: values, indexes: map[IDKey]int{}}
	return BpaSuccess
}
func DictionaryDelete(id IDKey) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	BpaaDelete(state.values)
	BpaDelete(id)
	delete(dictionaries, id)
	return BpaSuccess
}
func DictionarySetFrom(id, key IDKey, value []byte) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	array := bpaaArrays[state.values]
	if index, found := state.indexes[key]; found {
		status := BpaFailure
		if array.Type == BpaTypeString {
			status = BpaaSetStringRaw(state.values, index, value)
		} else {
			status = BpaaSetRaw(state.values, index, value)
		}
		if status < 0 {
			return BpaFailure
		}
		return BpaSetSuccess
	}
	if BpaaAppendRaw(state.values, value) < 0 {
		return BpaFailure
	}
	state.indexes[key] = len(state.keys)
	state.keys = append(state.keys, key)
	state.iterator = 0
	return BpaSetSuccess
}
func DictionarySet(id, key, source IDKey) int {
	state := dictionaryRead(id)
	if state == nil || !bpaaCompatible(bpaaArrays[state.values], bpaBlocks[source], bpaaArrays[state.values].BlockSize) {
		return BpaFailure
	}
	return DictionarySetFrom(id, key, bpaBlocks[source].Data)
}
func DictionaryGetTo(id, key IDKey, output []byte) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	index, found := state.indexes[key]
	if !found {
		return BpaFailure
	}
	status := BpaFailure
	if bpaaArrays[state.values].Type == BpaTypeString {
		status = BpaaGetStringRaw(state.values, index, output)
	} else {
		status = BpaaGetRaw(state.values, index, output)
	}
	if status < 0 {
		return BpaFailure
	}
	return BpaGetSuccess
}
func DictionaryGet(id, key, destination IDKey) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	index, found := state.indexes[key]
	if !found {
		return BpaFailure
	}
	return BpaaGet(state.values, index, destination, bpaaArrays[state.values].BlockSize)
}
func DictionaryContains(id, key IDKey) bool {
	state := dictionaryRead(id)
	if state == nil {
		return false
	}
	_, found := state.indexes[key]
	return found
}
func DictionaryCount(id IDKey) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	return len(state.keys)
}
func DictionaryRemove(id, key IDKey) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	index, found := state.indexes[key]
	if !found || BpaaRemove(state.values, index) < 0 {
		return BpaFailure
	}
	state.keys = slices.Delete(state.keys, index, index+1)
	delete(state.indexes, key)
	for position := index; position < len(state.keys); position++ {
		state.indexes[state.keys[position]] = position
	}
	state.iterator = 0
	return BpaSuccess
}
func DictionaryIterateReset(id IDKey) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	state.iterator = 0
	return BpaSuccess
}
func DictionaryIterateNext(id IDKey) (IDKey, int) {
	state := dictionaryRead(id)
	if state == nil || state.iterator >= len(state.keys) {
		return 0, BpaFailure
	}
	key := state.keys[state.iterator]
	state.iterator++
	return key, BpaGetSuccess
}
func DictionaryClear(id IDKey) int {
	state := dictionaryRead(id)
	if state == nil {
		return BpaFailure
	}
	BpaaClear(state.values)
	clear(state.indexes)
	state.keys = nil
	state.iterator = 0
	return BpaSuccess
}
func DictionaryAddFrom(id, key IDKey, value []byte) int {
	if DictionaryContains(id, key) {
		return BpaFailure
	}
	return DictionarySetFrom(id, key, value)
}
func DictionaryUpdateFrom(id, key IDKey, value []byte) int {
	if !DictionaryContains(id, key) {
		return BpaFailure
	}
	return DictionarySetFrom(id, key, value)
}
func DictionaryKeysTo(id IDKey, output []IDKey) int {
	state := dictionaryRead(id)
	if state == nil || len(output) < len(state.keys) {
		return BpaFailure
	}
	copy(output, state.keys)
	return BpaGetSuccess
}
