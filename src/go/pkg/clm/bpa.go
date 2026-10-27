package clm

import (
	"bytes"
	"encoding/binary"
	"math"
	"math/rand/v2"
	"slices"
)

const (
	BpaFailure    = -1
	BpaSuccess    = 0
	BpaGetSuccess = 1
	BpaSetSuccess = 2
)

const (
	BpaTypeStruct byte = iota
	BpaTypeSignedByte
	BpaTypeByte
	BpaTypeShort
	BpaTypeUnsignedShort
	BpaTypeInt
	BpaTypeUnsignedInt
	BpaTypeLong
	BpaTypeUnsignedLong
	BpaTypeFloat
	BpaTypeDouble
	BpaTypeString
	BpaTypeUUID
	BpaTypeDatetime
)

type BpaBlockNode struct {
	ID        IDKey
	Type      byte
	Refcount  uint16
	BlockSize int
	Encoding  byte
	Length    int
	Data      []byte
}

type BpaaArrayNode struct {
	ID                         IDKey
	Type                       byte
	Refcount                   uint16
	Count, Capacity, BlockSize int
	Encoding                   byte
	Index                      int
	Sealed                     bool
	DeckSize                   int
	Data                       []byte
	Strings                    []string
}

type BpaaCompareFn func(left, right []byte) int
type BpaaPredicateFn func(left, right []byte) bool

var bpaBlocks = map[IDKey]*BpaBlockNode{}
var bpaaArrays = map[IDKey]*BpaaArrayNode{}

func BpaTypeSize(kind byte) int {
	switch kind {
	case BpaTypeSignedByte, BpaTypeByte:
		return 1
	case BpaTypeShort, BpaTypeUnsignedShort:
		return 2
	case BpaTypeInt, BpaTypeUnsignedInt, BpaTypeFloat:
		return 4
	case BpaTypeLong, BpaTypeUnsignedLong, BpaTypeDouble, BpaTypeDatetime:
		return 8
	case BpaTypeUUID:
		return 16
	default:
		panic("unhandled default case")
	}
	return 0
}

func bpaWidth(kind byte, size int) int {
	if kind == BpaTypeStruct {
		if size > 0 && uint64(size) <= math.MaxUint32 {
			return size
		}
		return -1
	}
	if kind == BpaTypeString {
		if size == 0 {
			return 0
		}
		return -1
	}
	width := BpaTypeSize(kind)
	if width == 0 || (size != 0 && size != width) {
		return -1
	}
	return width
}

func BpaHashID(id IDKey) uint64 {
	id ^= id >> 30
	id *= 0xbf58476d1ce4e5b9
	id ^= id >> 27
	id *= 0x94d049bb133111eb
	return id ^ (id >> 31)
}

func BpaFindBlock(id IDKey) *BpaBlockNode { return bpaBlocks[id] }
func BpaIsExisting(id IDKey) bool         { return bpaBlocks[id] != nil }
func BpaCreate(id IDKey, kind byte) int   { return BpaCreateSized(id, kind, 0) }
func BpaCreateSized(id IDKey, kind byte, size int) int {
	width := bpaWidth(kind, size)
	if BpaIsExisting(id) || width < 0 {
		return BpaFailure
	}
	data := BpaPoolAlloc(width)
	if kind == BpaTypeString {
		data = []byte{0}
	}
	bpaBlocks[id] = &BpaBlockNode{ID: id, Type: kind, Refcount: 1, BlockSize: width, Data: data}
	return BpaSuccess
}
func BpaDelete(id IDKey) int {
	if !BpaIsExisting(id) {
		return BpaFailure
	}
	block := bpaBlocks[id]
	BpaPoolFree(block.Data, len(block.Data))
	delete(bpaBlocks, id)
	delete(dictionaries, id)
	delete(storages, id)
	return BpaSuccess
}
func BpaRef(id IDKey) int {
	block := bpaBlocks[id]
	if block == nil || block.Refcount == math.MaxUint16 {
		return BpaFailure
	}
	block.Refcount++
	return BpaSuccess
}
func BpaUnref(id IDKey) int {
	block := bpaBlocks[id]
	if block == nil {
		return BpaFailure
	}
	block.Refcount--
	if block.Refcount == 0 {
		return BpaDelete(id)
	}
	return BpaSuccess
}
func BpaSet(id IDKey, value []byte) int {
	block := bpaBlocks[id]
	if block == nil || block.Type == BpaTypeString || len(value) != block.BlockSize {
		return BpaFailure
	}
	copy(block.Data, value)
	return BpaSetSuccess
}
func BpaGet(id IDKey, output []byte) int {
	block := bpaBlocks[id]
	if block == nil || block.Type == BpaTypeString || len(output) < block.BlockSize {
		return BpaFailure
	}
	copy(output, block.Data)
	return BpaGetSuccess
}
func BpaReplace(id IDKey, value []byte) int {
	if block := bpaBlocks[id]; block != nil && block.Type == BpaTypeString {
		return BpaSetString(id, value)
	}
	return BpaSet(id, value)
}
func bpaCString(value []byte) ([]byte, bool) {
	end := bytes.IndexByte(value, 0)
	if end < 0 {
		return nil, false
	}
	return value[:end], true
}
func BpaSetString(id IDKey, value []byte) int {
	block := bpaBlocks[id]
	text, valid := bpaCString(value)
	if block == nil || block.Type != BpaTypeString || !valid {
		return BpaFailure
	}
	data := BpaPoolAlloc(len(text) + 1)
	copy(data, text)
	BpaPoolFree(block.Data, len(block.Data))
	block.Data = data
	block.Length = len(text)
	return len(text)
}
func BpaGetString(id IDKey, output []byte) int {
	block := bpaBlocks[id]
	if block == nil || block.Type != BpaTypeString || len(output) <= block.Length {
		return BpaFailure
	}
	copy(output, block.Data)
	return block.Length
}

func BpaaFindArray(id IDKey) *BpaaArrayNode { return bpaaArrays[id] }
func BpaaIsExisting(id IDKey) bool          { return bpaaArrays[id] != nil }
func BpaaCreate(id IDKey, kind byte, capacity int, encoding byte) int {
	return BpaaCreateSized(id, kind, capacity, 0, encoding)
}
func BpaaCreateSized(id IDKey, kind byte, capacity, size int, encoding byte) int {
	width := bpaWidth(kind, size)
	if BpaaIsExisting(id) || capacity < 0 || capacity > math.MaxInt32 || width < 0 || (width > 0 && capacity > math.MaxInt/width) {
		return BpaFailure
	}
	array := &BpaaArrayNode{ID: id, Type: kind, Refcount: 1, Capacity: capacity, BlockSize: width, Encoding: encoding}
	if kind == BpaTypeString {
		array.Strings = make([]string, capacity)
	} else {
		array.Data = make([]byte, capacity*width)
	}
	bpaaArrays[id] = array
	return BpaSuccess
}
func BpaaDelete(id IDKey) int {
	if !BpaaIsExisting(id) {
		return BpaFailure
	}
	delete(bpaaArrays, id)
	return BpaSuccess
}
func BpaaRef(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil || array.Refcount == math.MaxUint16 {
		return BpaFailure
	}
	array.Refcount++
	return BpaSuccess
}
func BpaaUnref(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	array.Refcount--
	if array.Refcount == 0 {
		return BpaaDelete(id)
	}
	return BpaSuccess
}
func bpaaResize(array *BpaaArrayNode, capacity int) int {
	if array == nil || array.Sealed || capacity < array.Count || capacity > math.MaxInt32 || capacity < 0 || (array.BlockSize > 0 && capacity > math.MaxInt/array.BlockSize) {
		return BpaFailure
	}
	if array.Type == BpaTypeString {
		data := make([]string, capacity)
		copy(data, array.Strings)
		array.Strings = data
	} else {
		data := make([]byte, capacity*array.BlockSize)
		copy(data, array.Data)
		array.Data = data
	}
	array.Capacity = capacity
	return BpaSuccess
}
func BpaaExtend(id IDKey, amount int) int {
	array := bpaaArrays[id]
	if array == nil || amount < 0 || amount > math.MaxInt32-array.Capacity {
		return BpaFailure
	}
	return bpaaResize(array, array.Capacity+amount)
}
func BpaaShrink(id IDKey, capacity int) int {
	array := bpaaArrays[id]
	if array == nil || capacity > array.Capacity {
		return BpaFailure
	}
	return bpaaResize(array, capacity)
}
func bpaaReserve(array *BpaaArrayNode) bool {
	if array.Count < array.Capacity {
		return true
	}
	capacity := max(1, array.Capacity*2)
	return bpaaResize(array, capacity) == BpaSuccess
}
func bpaaValid(array *BpaaArrayNode, index int) bool {
	return array != nil && index >= 0 && index < array.Count
}
func BpaaInsertRaw(id IDKey, index int, value []byte) int {
	array := bpaaArrays[id]
	if array == nil || index < 0 || index > array.Count {
		return BpaFailure
	}
	var text []byte
	if array.Type == BpaTypeString {
		var valid bool
		text, valid = bpaCString(value)
		if !valid {
			return BpaFailure
		}
	} else if len(value) != array.BlockSize {
		return BpaFailure
	}
	snapshot := bytes.Clone(value)
	if !bpaaReserve(array) {
		return BpaFailure
	}
	if array.Type == BpaTypeString {
		copy(array.Strings[index+1:], array.Strings[index:array.Count])
		array.Strings[index] = string(text)
	} else {
		width := array.BlockSize
		copy(array.Data[(index+1)*width:], array.Data[index*width:array.Count*width])
		copy(array.Data[index*width:], snapshot)
	}
	array.Count++
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaAppendRaw(id IDKey, value []byte) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaInsertRaw(id, array.Count, value)
}
func BpaaSetRaw(id IDKey, index int, value []byte) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, index) || array.Type == BpaTypeString || len(value) != array.BlockSize {
		return BpaFailure
	}
	copy(array.Data[index*array.BlockSize:], value)
	array.DeckSize = array.Count
	return BpaSetSuccess
}
func BpaaGetRaw(id IDKey, index int, output []byte) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, index) || array.Type == BpaTypeString || len(output) < array.BlockSize {
		return BpaFailure
	}
	copy(output, array.Data[index*array.BlockSize:(index+1)*array.BlockSize])
	return BpaGetSuccess
}
func BpaaSetStringRaw(id IDKey, index int, value []byte) int {
	array := bpaaArrays[id]
	text, valid := bpaCString(value)
	if !bpaaValid(array, index) || array.Type != BpaTypeString || !valid {
		return BpaFailure
	}
	array.Strings[index] = string(text)
	array.DeckSize = array.Count
	return len(text)
}
func BpaaGetStringRaw(id IDKey, index int, output []byte) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, index) || array.Type != BpaTypeString || len(output) <= len(array.Strings[index]) {
		return BpaFailure
	}
	text := array.Strings[index]
	copy(output, text)
	output[len(text)] = 0
	return len(text)
}
func bpaaCompatible(array *BpaaArrayNode, block *BpaBlockNode, size int) bool {
	return array != nil && block != nil && (array.Type == block.Type || (array.Type == BpaTypeStruct && block.Type != BpaTypeString)) && (array.Type == BpaTypeString || (array.BlockSize == block.BlockSize && size == array.BlockSize))
}
func BpaaAppend(id, source IDKey, size int) int {
	array, block := bpaaArrays[id], bpaBlocks[source]
	if !bpaaCompatible(array, block, size) {
		return BpaFailure
	}
	return BpaaAppendRaw(id, block.Data)
}
func BpaaInsert(id IDKey, index int, source IDKey, size int) int {
	if !bpaaCompatible(bpaaArrays[id], bpaBlocks[source], size) {
		return BpaFailure
	}
	return BpaaInsertRaw(id, index, bpaBlocks[source].Data)
}
func BpaaSet(id IDKey, index int, source IDKey, size int) int {
	if !bpaaCompatible(bpaaArrays[id], bpaBlocks[source], size) {
		return BpaFailure
	}
	if bpaaArrays[id].Type == BpaTypeString {
		if BpaaSetStringRaw(id, index, bpaBlocks[source].Data) < 0 {
			return BpaFailure
		}
		return BpaSetSuccess
	}
	return BpaaSetRaw(id, index, bpaBlocks[source].Data)
}
func BpaaGet(id IDKey, index int, destination IDKey, size int) int {
	array, block := bpaaArrays[id], bpaBlocks[destination]
	if !bpaaCompatible(array, block, size) || !bpaaValid(array, index) {
		return BpaFailure
	}
	if array.Type == BpaTypeString {
		if BpaSetString(destination, append([]byte(array.Strings[index]), 0)) < 0 {
			return BpaFailure
		}
		return BpaGetSuccess
	}
	return BpaaGetRaw(id, index, block.Data)
}
func BpaaAppendString(id, source IDKey) int {
	if array := bpaaArrays[id]; array == nil || array.Type != BpaTypeString {
		return BpaFailure
	}
	return BpaaAppend(id, source, 0)
}
func BpaaSetString(id IDKey, index int, source IDKey) int {
	if array := bpaaArrays[id]; array == nil || array.Type != BpaTypeString {
		return BpaFailure
	}
	return BpaaSet(id, index, source, 0)
}
func BpaaGetString(id IDKey, index int, destination IDKey) int {
	if array := bpaaArrays[id]; array == nil || array.Type != BpaTypeString {
		return BpaFailure
	}
	return BpaaGet(id, index, destination, 0)
}
func BpaaRemove(id IDKey, index int) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, index) {
		return BpaFailure
	}
	if array.Type == BpaTypeString {
		copy(array.Strings[index:], array.Strings[index+1:array.Count])
		array.Strings[array.Count-1] = ""
	} else {
		width := array.BlockSize
		copy(array.Data[index*width:], array.Data[(index+1)*width:array.Count*width])
		clear(array.Data[(array.Count-1)*width : array.Count*width])
	}
	array.Count--
	array.Index = min(array.Index, max(0, array.Count-1))
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaRemoveFirst(id IDKey) int { return BpaaRemove(id, 0) }
func BpaaRemoveLast(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaRemove(id, array.Count-1)
}
func BpaaClear(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	clear(array.Data)
	clear(array.Strings)
	array.Count = 0
	array.Index = 0
	array.DeckSize = 0
	return BpaSuccess
}
func BpaaSeal(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	array.Sealed = true
	return BpaSuccess
}
func BpaaUnseal(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	array.Sealed = false
	return BpaSuccess
}
func BpaaMoveTo(id IDKey, index int) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, index) {
		return BpaFailure
	}
	array.Index = index
	return BpaSuccess
}
func BpaaMoveFirst(id IDKey) int { return BpaaMoveTo(id, 0) }
func BpaaMoveLast(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaMoveTo(id, array.Count-1)
}
func BpaaMoveNext(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaMoveTo(id, array.Index+1)
}
func BpaaMovePrevious(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaMoveTo(id, array.Index-1)
}
func BpaaReset(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	array.Index = 0
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaGetFirstRaw(id IDKey, output []byte) int { return BpaaGetRaw(id, 0, output) }
func BpaaGetLastRaw(id IDKey, output []byte) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaGetRaw(id, array.Count-1, output)
}
func BpaaGetCurrentRaw(id IDKey, output []byte) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaGetRaw(id, array.Index, output)
}
func BpaaSwap(id IDKey, left, right int) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, left) || !bpaaValid(array, right) {
		return BpaFailure
	}
	if array.Type == BpaTypeString {
		array.Strings[left], array.Strings[right] = array.Strings[right], array.Strings[left]
	} else {
		width := array.BlockSize
		snapshot := bytes.Clone(array.Data[left*width : (left+1)*width])
		copy(array.Data[left*width:], array.Data[right*width:(right+1)*width])
		copy(array.Data[right*width:], snapshot)
	}
	return BpaSuccess
}
func BpaaCopy(id IDKey, source, destination int) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, source) || !bpaaValid(array, destination) {
		return BpaFailure
	}
	if array.Type == BpaTypeString {
		array.Strings[destination] = array.Strings[source]
	} else {
		width := array.BlockSize
		copy(array.Data[destination*width:], array.Data[source*width:(source+1)*width])
	}
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaReverse(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil || array.Type == BpaTypeString {
		return BpaFailure
	}
	for index := 0; index < array.Count/2; index++ {
		BpaaSwap(id, index, array.Count-1-index)
	}
	array.Index = 0
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaShuffle(id IDKey) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	rand.Shuffle(array.Count, func(left, right int) { BpaaSwap(id, left, right) })
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaSortWith(id IDKey, compare BpaaCompareFn) int {
	array := bpaaArrays[id]
	if array == nil || compare == nil || array.Type == BpaTypeString {
		return BpaFailure
	}
	items := make([][]byte, array.Count)
	for index := range items {
		items[index] = bytes.Clone(array.Data[index*array.BlockSize : (index+1)*array.BlockSize])
	}
	slices.SortFunc(items, compare)
	for index, item := range items {
		copy(array.Data[index*array.BlockSize:], item)
	}
	array.Index = 0
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaSort(id IDKey, predicate BpaaPredicateFn) int {
	if predicate == nil {
		return BpaFailure
	}
	return BpaaSortWith(id, func(left, right []byte) int {
		if predicate(left, right) {
			return 1
		}
		if predicate(right, left) {
			return -1
		}
		return 0
	})
}
func BpaaDrawAtRaw(id IDKey, index int, output []byte) int {
	array := bpaaArrays[id]
	if array == nil || array.Count == 0 {
		return BpaFailure
	}
	if array.DeckSize == 0 {
		BpaaShuffle(id)
	}
	if index < 0 || index >= array.DeckSize {
		return BpaFailure
	}
	status := BpaaGetRaw(id, index, output)
	if status < 0 {
		return status
	}
	BpaaSwap(id, index, array.DeckSize-1)
	array.DeckSize--
	return status
}
func BpaaDrawRaw(id IDKey, output []byte) int { return BpaaDrawAtRaw(id, 0, output) }
func BpaaDrawAt(id IDKey, index int, destination IDKey, size int) int {
	array := bpaaArrays[id]
	if array == nil || array.Count == 0 || !bpaaCompatible(array, bpaBlocks[destination], size) {
		return BpaFailure
	}
	if array.DeckSize == 0 {
		BpaaShuffle(id)
	}
	if index < 0 || index >= array.DeckSize {
		return BpaFailure
	}
	status := BpaaGet(id, index, destination, size)
	if status < 0 {
		return status
	}
	BpaaSwap(id, index, array.DeckSize-1)
	array.DeckSize--
	return status
}
func BpaaDraw(id, destination IDKey, size int) int { return BpaaDrawAt(id, 0, destination, size) }
func BpaaSliceBorrow(id IDKey, start, count int) ([]byte, int) {
	array := bpaaArrays[id]
	if array == nil || array.Type == BpaTypeString || start < 0 || count < 0 || start > array.Count || count > array.Count-start {
		return nil, BpaFailure
	}
	if count == 0 {
		return nil, BpaSuccess
	}
	return array.Data[start*array.BlockSize : (start+count)*array.BlockSize], BpaSuccess
}
func BpaaSliceTo(id IDKey, start, count int, output []byte) int {
	data, status := BpaaSliceBorrow(id, start, count)
	if status < 0 || len(output) < len(data) {
		return BpaFailure
	}
	copy(output, data)
	return BpaSuccess
}
func BpaaSlice(id IDKey, start, count int, destination IDKey) int {
	array := bpaaArrays[id]
	if array == nil || start < 0 || count < 0 || start > array.Count || count > array.Count-start || BpaaCreateSized(destination, array.Type, count, array.BlockSize, array.Encoding) < 0 {
		return BpaFailure
	}
	for index := start; index < start+count; index++ {
		if array.Type == BpaTypeString {
			BpaaAppendRaw(destination, append([]byte(array.Strings[index]), 0))
		} else {
			BpaaAppendRaw(destination, array.Data[index*array.BlockSize:(index+1)*array.BlockSize])
		}
	}
	return BpaSuccess
}
func BpaaAdvancedBorrow(id IDKey) (*BpaaArrayNode, int) {
	if BpaaRef(id) < 0 {
		return nil, BpaFailure
	}
	return bpaaArrays[id], BpaSuccess
}
func BpaaAdvancedRelease(array **BpaaArrayNode) int {
	if array == nil || *array == nil || bpaaArrays[(*array).ID] != *array {
		return BpaFailure
	}
	status := BpaaUnref((*array).ID)
	if status == BpaSuccess {
		*array = nil
	}
	return status
}
func BpaaAdvancedData(array *BpaaArrayNode) []byte {
	if array == nil || array.Type == BpaTypeString {
		return nil
	}
	return array.Data
}
func BpaShutdown() int {
	clear(bpaaArrays)
	for id := range bpaBlocks {
		BpaDelete(id)
	}
	clear(dictionaries)
	clear(storages)
	clear(idNames)
	clear(idSpellings)
	return BpaPoolCleanup()
}

const (
	BpaInitialCapacity = 16
	BpaPoolSlabSlots   = 32
	BpaPoolMaxSlot     = 4096
	BpaPoolClasses     = 10
)

type bpaPoolClass struct {
	free             [][]byte
	active, capacity int
}

var bpaPools [BpaPoolClasses]bpaPoolClass
var bpaPoolOwners = map[*byte]int{}

func BpaPoolClassFor(size int) int {
	if size <= 0 || size > BpaPoolMaxSlot {
		return BpaPoolClasses
	}
	width := 8
	for index := 0; index < BpaPoolClasses; index++ {
		if size <= width {
			return index
		}
		width *= 2
	}
	return BpaPoolClasses
}
func BpaPoolAlloc(size int) []byte {
	if size <= 0 {
		return nil
	}
	class := BpaPoolClassFor(size)
	if class == BpaPoolClasses {
		return make([]byte, size)
	}
	pool := &bpaPools[class]
	width := 8 << class
	if len(pool.free) == 0 {
		slab := make([]byte, width*BpaPoolSlabSlots)
		for index := 0; index < BpaPoolSlabSlots; index++ {
			pool.free = append(pool.free, slab[index*width:(index+1)*width:(index+1)*width])
		}
		pool.capacity += BpaPoolSlabSlots
	}
	last := len(pool.free) - 1
	data := pool.free[last]
	pool.free[last] = nil
	pool.free = pool.free[:last]
	clear(data)
	pool.active++
	bpaPoolOwners[&data[0]] = class
	return data[:size]
}
func BpaPoolFree(data []byte, size int) {
	if len(data) == 0 {
		return
	}
	class, found := bpaPoolOwners[&data[0]]
	if !found || class != BpaPoolClassFor(size) {
		return
	}
	delete(bpaPoolOwners, &data[0])
	pool := &bpaPools[class]
	pool.active--
	pool.free = append(pool.free, data[:8<<class])
}
func BpaPoolStats(size int) (active, capacity, status int) {
	class := BpaPoolClassFor(size)
	if class == BpaPoolClasses {
		return 0, 0, BpaFailure
	}
	pool := &bpaPools[class]
	return pool.active, pool.capacity, BpaSuccess
}
func BpaPoolCleanup() int {
	for _, pool := range bpaPools {
		if pool.active != 0 {
			return BpaFailure
		}
	}
	clear(bpaPools[:])
	clear(bpaPoolOwners)
	return BpaSuccess
}

func BpaBorrow(id IDKey) ([]byte, int) {
	block := bpaBlocks[id]
	if block == nil {
		return nil, BpaFailure
	}
	return block.Data, BpaGetSuccess
}
func BpaCopyBlock(source, destination IDKey) int {
	left, right := bpaBlocks[source], bpaBlocks[destination]
	if left == nil || right == nil || left.Type != right.Type || left.BlockSize != right.BlockSize {
		return BpaFailure
	}
	if left.Type == BpaTypeString {
		if BpaSetString(destination, left.Data) < 0 {
			return BpaFailure
		}
	} else {
		copy(right.Data, left.Data)
	}
	return BpaSuccess
}
func BpaGetTo(id IDKey, output []byte) int             { return BpaGet(id, output) }
func BpaSetFrom(id IDKey, value []byte) int            { return BpaSet(id, value) }
func BpaGetStringTo(id IDKey, output []byte) int       { return BpaGetString(id, output) }
func BpaSetStringFrom(id IDKey, value []byte) int      { return BpaSetString(id, value) }
func BpaaGetTo(id IDKey, index int, output []byte) int { return BpaaGetRaw(id, index, output) }
func BpaaGetFirstTo(id IDKey, output []byte) int       { return BpaaGetFirstRaw(id, output) }
func BpaaGetLastTo(id IDKey, output []byte) int        { return BpaaGetLastRaw(id, output) }
func BpaaGetCurrentTo(id IDKey, output []byte) int     { return BpaaGetCurrentRaw(id, output) }
func BpaaGetStringTo(id IDKey, index int, output []byte) int {
	return BpaaGetStringRaw(id, index, output)
}
func BpaaSetFrom(id IDKey, index int, value []byte) int { return BpaaSetRaw(id, index, value) }
func BpaaSetStringFrom(id IDKey, index int, value []byte) int {
	return BpaaSetStringRaw(id, index, value)
}
func BpaaInsertFrom(id IDKey, index int, value []byte) int { return BpaaInsertRaw(id, index, value) }
func BpaaAppendFrom(id IDKey, value []byte) int            { return BpaaAppendRaw(id, value) }
func BpaaDrawTo(id IDKey, output []byte) int               { return BpaaDrawRaw(id, output) }
func BpaaDrawAtTo(id IDKey, index int, output []byte) int  { return BpaaDrawAtRaw(id, index, output) }
func BpaaInsertString(id IDKey, index int, source IDKey) int {
	array := bpaaArrays[id]
	if array == nil || array.Type != BpaTypeString {
		return BpaFailure
	}
	return BpaaInsert(id, index, source, 0)
}
func BpaaGetCountTo(id IDKey) (int, int) {
	array := bpaaArrays[id]
	if array == nil {
		return 0, BpaFailure
	}
	return array.Count, BpaGetSuccess
}
func BpaaGetCapacityTo(id IDKey) (int, int) {
	array := bpaaArrays[id]
	if array == nil {
		return 0, BpaFailure
	}
	return array.Capacity, BpaGetSuccess
}
func BpaaGetLengthTo(id IDKey) (int, int) {
	array := bpaaArrays[id]
	if array == nil || array.Type != BpaTypeString || !bpaaValid(array, array.Index) {
		return 0, BpaFailure
	}
	return len(array.Strings[array.Index]), BpaGetSuccess
}
func BpaaGetCurrentIndexTo(id IDKey) (int, int) {
	array := bpaaArrays[id]
	if array == nil {
		return 0, BpaFailure
	}
	return array.Index, BpaGetSuccess
}
func BpaaIsSealedTo(id IDKey) (bool, int) {
	array := bpaaArrays[id]
	if array == nil {
		return false, BpaFailure
	}
	return array.Sealed, BpaGetSuccess
}
func BpaaWriteU32(destination IDKey, value uint32) int {
	block := bpaBlocks[destination]
	if block == nil || block.Type != BpaTypeUnsignedInt {
		return BpaFailure
	}
	binary.LittleEndian.PutUint32(block.Data, value)
	return BpaGetSuccess
}
func BpaaWriteI32(destination IDKey, value int32) int {
	block := bpaBlocks[destination]
	if block == nil || block.Type != BpaTypeInt {
		return BpaFailure
	}
	binary.LittleEndian.PutUint32(block.Data, uint32(value))
	return BpaGetSuccess
}
func BpaaWriteByte(destination IDKey, value byte) int {
	block := bpaBlocks[destination]
	if block == nil || block.Type != BpaTypeByte {
		return BpaFailure
	}
	block.Data[0] = value
	return BpaGetSuccess
}
func BpaaGetCount(id, destination IDKey) int {
	value, status := BpaaGetCountTo(id)
	if status < 0 {
		return status
	}
	return BpaaWriteU32(destination, uint32(value))
}
func BpaaGetCapacity(id, destination IDKey) int {
	value, status := BpaaGetCapacityTo(id)
	if status < 0 {
		return status
	}
	return BpaaWriteU32(destination, uint32(value))
}
func BpaaGetLength(id, destination IDKey) int {
	value, status := BpaaGetLengthTo(id)
	if status < 0 {
		return status
	}
	return BpaaWriteU32(destination, uint32(value))
}
func BpaaGetCurrentIndex(id, destination IDKey) int {
	value, status := BpaaGetCurrentIndexTo(id)
	if status < 0 {
		return status
	}
	return BpaaWriteI32(destination, int32(value))
}
func BpaaIsSealed(id, destination IDKey) int {
	value, status := BpaaIsSealedTo(id)
	if status < 0 {
		return status
	}
	flag := byte(0)
	if value {
		flag = 1
	}
	return BpaaWriteByte(destination, flag)
}
func BpaaGetFirst(id, destination IDKey, size int) int { return BpaaGet(id, 0, destination, size) }
func BpaaGetLast(id, destination IDKey, size int) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaGet(id, array.Count-1, destination, size)
}
func BpaaGetCurrent(id, destination IDKey, size int) int {
	array := bpaaArrays[id]
	if array == nil {
		return BpaFailure
	}
	return BpaaGet(id, array.Index, destination, size)
}
func bpaaRotate(id IDKey, index, positions int, right bool) int {
	array := bpaaArrays[id]
	if !bpaaValid(array, index) || array.Type == BpaTypeString || positions < 0 {
		return BpaFailure
	}
	length := array.Count - index
	shift := positions % length
	if shift == 0 {
		return BpaSuccess
	}
	if right {
		shift = length - shift
	}
	data := array.Data[index*array.BlockSize : array.Count*array.BlockSize]
	snapshot := bytes.Clone(data[:shift*array.BlockSize])
	copy(data, data[shift*array.BlockSize:])
	copy(data[(length-shift)*array.BlockSize:], snapshot)
	array.Index = 0
	array.DeckSize = array.Count
	return BpaSuccess
}
func BpaaShiftLeft(id IDKey, index, positions int) int {
	return bpaaRotate(id, index, positions, false)
}
func BpaaShiftRight(id IDKey, index, positions int) int {
	return bpaaRotate(id, index, positions, true)
}
func bpaaAdvancedID(array *BpaaArrayNode) (IDKey, bool) {
	if array == nil {
		return 0, false
	}
	return array.ID, bpaaArrays[array.ID] == array && array.Type != BpaTypeString
}
func BpaaAdvancedGetRaw(array *BpaaArrayNode, index int, output []byte) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaGetRaw(id, index, output)
}
func BpaaAdvancedSetRaw(array *BpaaArrayNode, index int, value []byte) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaSetRaw(id, index, value)
}
func BpaaAdvancedInsertRaw(array *BpaaArrayNode, index int, value []byte) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaInsertRaw(id, index, value)
}
func BpaaAdvancedAppendRaw(array *BpaaArrayNode, value []byte) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaAppendRaw(id, value)
}
func BpaaAdvancedRemove(array *BpaaArrayNode, index int) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaRemove(id, index)
}
func BpaaAdvancedClear(array *BpaaArrayNode) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaClear(id)
}
func BpaaAdvancedExtend(array *BpaaArrayNode, amount int) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaExtend(id, amount)
}
func BpaaAdvancedShrink(array *BpaaArrayNode, capacity int) int {
	id, valid := bpaaAdvancedID(array)
	if !valid {
		return BpaFailure
	}
	return BpaaShrink(id, capacity)
}
