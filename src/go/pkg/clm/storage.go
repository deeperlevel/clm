package clm

import (
	"bytes"
	"math"
)

type StorageInfo struct {
	Columns, Rows, Layers   int
	FrameColumns, FrameRows int
	ElementSize             int
	Type                    byte
	Atlas                   bool
	DataID                  IDKey
}
type storageState struct {
	block *BpaBlockNode
	info  StorageInfo
}

var storages = map[IDKey]*storageState{}

func storageRead(id IDKey) *StorageInfo {
	state := storages[id]
	if state == nil || state.block != bpaBlocks[id] {
		return nil
	}
	array := bpaaArrays[state.info.DataID]
	if array == nil || array.Type != state.info.Type || array.BlockSize != state.info.ElementSize {
		return nil
	}
	return &state.info
}
func storageNew(id IDKey, kind byte, size, columns, rows, layers, frameColumns, frameRows int, atlas bool) int {
	width := bpaWidth(kind, size)
	if columns <= 0 || rows <= 0 || layers <= 0 || columns > math.MaxInt32 || rows > math.MaxInt32 || layers > math.MaxInt32 || kind == BpaTypeString || width <= 0 || BpaIsExisting(id) || BpaaIsExisting(id) {
		return BpaFailure
	}
	if atlas && (frameColumns <= 0 || frameRows <= 0 || columns%frameColumns != 0 || rows%frameRows != 0) {
		return BpaFailure
	}
	if uint64(columns)*uint64(rows) > uint64(math.MaxInt32)/uint64(layers) {
		return BpaFailure
	}
	total := columns * rows * layers
	if total > math.MaxInt/width {
		return BpaFailure
	}
	dataID := allocateArrayID(id)
	if BpaaCreateSized(dataID, kind, total, width, 0) < 0 {
		return BpaFailure
	}
	array := bpaaArrays[dataID]
	array.Count = total
	array.DeckSize = total
	if BpaCreateSized(id, BpaTypeStruct, 8) < 0 {
		BpaaDelete(dataID)
		return BpaFailure
	}
	storages[id] = &storageState{block: bpaBlocks[id], info: StorageInfo{Columns: columns, Rows: rows, Layers: layers, FrameColumns: frameColumns, FrameRows: frameRows, ElementSize: width, Type: kind, Atlas: atlas, DataID: dataID}}
	return BpaSuccess
}
func StorageNew(id IDKey, kind byte, size, columns, rows, layers int) int {
	return storageNew(id, kind, size, columns, rows, layers, 0, 0, false)
}
func StorageNew2D(id IDKey, kind byte, size, columns, rows int) int {
	return StorageNew(id, kind, size, columns, rows, 1)
}
func StorageAtlasNew(id IDKey, kind byte, size, columns, rows, frameColumns, frameRows int) int {
	return storageNew(id, kind, size, columns, rows, 1, frameColumns, frameRows, true)
}
func StorageAtlasNew3D(id IDKey, kind byte, size, columns, rows, layers, frameColumns, frameRows int) int {
	return storageNew(id, kind, size, columns, rows, layers, frameColumns, frameRows, true)
}
func StorageGetInfo(id IDKey) (StorageInfo, int) {
	info := storageRead(id)
	if info == nil {
		return StorageInfo{}, BpaFailure
	}
	return *info, BpaGetSuccess
}
func StorageDelete(id IDKey) int {
	info := storageRead(id)
	if info == nil {
		return BpaFailure
	}
	BpaaDelete(info.DataID)
	BpaDelete(id)
	delete(storages, id)
	return BpaSuccess
}
func storageIndex(info *StorageInfo, layer, row, column int) int {
	if info == nil || layer < 0 || row < 0 || column < 0 || layer >= info.Layers || row >= info.Rows || column >= info.Columns {
		return -1
	}
	return (layer*info.Rows+row)*info.Columns + column
}
func StorageGetTo(id IDKey, layer, row, column int, output []byte) int {
	info := storageRead(id)
	index := storageIndex(info, layer, row, column)
	if index < 0 {
		return BpaFailure
	}
	return BpaaGetRaw(info.DataID, index, output)
}
func StorageSetFrom(id IDKey, layer, row, column int, value []byte) int {
	info := storageRead(id)
	index := storageIndex(info, layer, row, column)
	if index < 0 {
		return BpaFailure
	}
	return BpaaSetRaw(info.DataID, index, value)
}
func StorageGet(id IDKey, layer, row, column int, destination IDKey) int {
	info := storageRead(id)
	index := storageIndex(info, layer, row, column)
	if index < 0 {
		return BpaFailure
	}
	return BpaaGet(info.DataID, index, destination, info.ElementSize)
}
func StorageSet(id IDKey, layer, row, column int, source IDKey) int {
	info := storageRead(id)
	index := storageIndex(info, layer, row, column)
	if index < 0 {
		return BpaFailure
	}
	return BpaaSet(info.DataID, index, source, info.ElementSize)
}
func StorageFillFrom(id IDKey, value []byte) int {
	info := storageRead(id)
	if info == nil || len(value) != info.ElementSize {
		return BpaFailure
	}
	snapshot := bytes.Clone(value)
	array := bpaaArrays[info.DataID]
	for index := 0; index < array.Count; index++ {
		copy(array.Data[index*info.ElementSize:], snapshot)
	}
	array.DeckSize = array.Count
	return BpaSuccess
}
func StorageSlice(id IDKey, layer, row, column, layers, rows, columns int, destination IDKey) int {
	info := storageRead(id)
	if storageIndex(info, layer, row, column) < 0 || layers <= 0 || rows <= 0 || columns <= 0 || layers > info.Layers-layer || rows > info.Rows-row || columns > info.Columns-column || StorageNew(destination, info.Type, info.ElementSize, columns, rows, layers) < 0 {
		return BpaFailure
	}
	source := bpaaArrays[info.DataID]
	target := bpaaArrays[storageRead(destination).DataID]
	width := info.ElementSize
	for depth := 0; depth < layers; depth++ {
		for vertical := 0; vertical < rows; vertical++ {
			from := ((layer+depth)*info.Rows+row+vertical)*info.Columns + column
			to := (depth*rows + vertical) * columns
			copy(target.Data[to*width:(to+columns)*width], source.Data[from*width:(from+columns)*width])
		}
	}
	return BpaSuccess
}
func storageFrame(id IDKey, layer, frameRow, frameColumn int) *StorageInfo {
	info := storageRead(id)
	if info == nil || !info.Atlas || layer < 0 || frameRow < 0 || frameColumn < 0 || layer >= info.Layers || frameRow >= info.Rows/info.FrameRows || frameColumn >= info.Columns/info.FrameColumns {
		return nil
	}
	return info
}
func StorageAtlasGetFrameTo(id IDKey, layer, frameRow, frameColumn int, output []byte) int {
	info := storageFrame(id, layer, frameRow, frameColumn)
	if info == nil {
		return BpaFailure
	}
	stride := info.FrameColumns * info.ElementSize
	needed := stride * info.FrameRows
	if len(output) < needed {
		return BpaFailure
	}
	snapshot := make([]byte, needed)
	data := bpaaArrays[info.DataID].Data
	for row := 0; row < info.FrameRows; row++ {
		offset := ((layer*info.Rows+frameRow*info.FrameRows+row)*info.Columns + frameColumn*info.FrameColumns) * info.ElementSize
		copy(snapshot[row*stride:], data[offset:offset+stride])
	}
	copy(output, snapshot)
	return BpaGetSuccess
}
func StorageAtlasSetFrameFrom(id IDKey, layer, frameRow, frameColumn int, value []byte) int {
	info := storageFrame(id, layer, frameRow, frameColumn)
	if info == nil {
		return BpaFailure
	}
	stride := info.FrameColumns * info.ElementSize
	if len(value) != stride*info.FrameRows {
		return BpaFailure
	}
	snapshot := bytes.Clone(value)
	array := bpaaArrays[info.DataID]
	for row := 0; row < info.FrameRows; row++ {
		offset := ((layer*info.Rows+frameRow*info.FrameRows+row)*info.Columns + frameColumn*info.FrameColumns) * info.ElementSize
		copy(array.Data[offset:offset+stride], snapshot[row*stride:(row+1)*stride])
	}
	array.DeckSize = array.Count
	return BpaSetSuccess
}
func StorageAtlasSliceFrame(id IDKey, layer, frameRow, frameColumn int, destination IDKey) int {
	info := storageFrame(id, layer, frameRow, frameColumn)
	if info == nil {
		return BpaFailure
	}
	return StorageSlice(id, layer, frameRow*info.FrameRows, frameColumn*info.FrameColumns, 1, info.FrameRows, info.FrameColumns, destination)
}
