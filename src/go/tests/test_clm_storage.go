package tests

import (
	"bytes"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

func StorageTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - Storage")
	defer clm.BpaShutdown()
	output := make([]byte, 4)
	result.Assert(clm.StorageNew2D(100, clm.BpaTypeInt, 0, 4, 3) == 0, "create 2D grid")
	result.Assert(clm.StorageNew2D(100, clm.BpaTypeInt, 0, 4, 3) == -1, "reject duplicate storage")
	result.Assert(clm.StorageGetTo(100, 0, 2, 3, output) == 1 && bytes.Equal(output, intBytes(0)), "zero-initialized cells")
	result.Assert(clm.StorageSetFrom(100, 0, 1, 2, intBytes(72)) == 2 && clm.StorageGetTo(100, 0, 1, 2, output) == 1 && bytes.Equal(output, intBytes(72)), "set and retrieve grid cell")
	result.Assert(clm.StorageGetTo(100, 1, 1, 2, output) == -1 && clm.StorageGetTo(100, 0, 3, 2, output) == -1 && clm.StorageSetFrom(100, 0, 1, 2, output[:2]) == -1, "reject invalid coordinates and widths")
	info, status := clm.StorageGetInfo(100)
	result.Assert(status == 1 && info.Columns == 4 && info.Rows == 3 && info.Layers == 1, "storage metadata")
	result.Assert(clm.StorageSlice(100, 0, 1, 1, 1, 2, 2, 101) == 0 && clm.StorageGetTo(101, 0, 0, 1, output) == 1 && bytes.Equal(output, intBytes(72)), "slice rectangular grid")
	result.Assert(clm.StorageSetFrom(101, 0, 0, 1, intBytes(21)) == 2 && clm.StorageGetTo(100, 0, 1, 2, output) == 1 && bytes.Equal(output, intBytes(72)), "slice owns independent data")
	result.Assert(clm.StorageSlice(100, 0, 2, 2, 1, 2, 2, 102) == -1, "reject out-of-bounds slice")
	result.Assert(clm.StorageDelete(101) == 0 && clm.StorageDelete(100) == 0, "delete grids")
	_, status = clm.StorageGetInfo(100)
	result.Assert(status == -1, "deleted metadata unavailable")
	result.Assert(clm.StorageNew(110, clm.BpaTypeByte, 0, 2, 3, 2) == 0 && clm.StorageSetFrom(110, 1, 2, 1, []byte{9}) == 2, "create and write 3D grid")
	result.Assert(clm.StorageGetTo(110, 0, 2, 1, output) == 1 && output[0] == 0, "layers independent")
	result.Assert(clm.StorageSlice(110, 1, 1, 0, 1, 2, 2, 111) == 0 && clm.StorageGetTo(111, 0, 1, 1, output) == 1 && output[0] == 9, "slice 3D region")
	result.Assert(clm.StorageDelete(111) == 0 && clm.StorageDelete(110) == 0, "delete 3D grids")
	frame := []byte{1, 2, 3, 4}
	result.Assert(clm.StorageAtlasNew(120, clm.BpaTypeByte, 0, 4, 4, 2, 2) == 0, "create atlas")
	result.Assert(clm.StorageAtlasSetFrameFrom(120, 0, 1, 1, frame) == 2 && clm.StorageAtlasGetFrameTo(120, 0, 1, 1, output) == 1 && bytes.Equal(output, frame), "round-trip atlas frame")
	result.Assert(clm.StorageAtlasGetFrameTo(120, 0, 0, 0, output) == 1 && bytes.Equal(output, []byte{0, 0, 0, 0}), "other frames unchanged")
	result.Assert(clm.StorageAtlasGetFrameTo(120, 0, 2, 0, output) == -1 && clm.StorageAtlasGetFrameTo(120, 0, 1, 1, output[:3]) == -1, "reject invalid frame and short output")
	result.Assert(clm.StorageAtlasSliceFrame(120, 0, 1, 1, 121) == 0 && clm.StorageGetTo(121, 0, 1, 1, output) == 1 && output[0] == 4, "extract atlas frame")
	result.Assert(clm.StorageDelete(121) == 0 && clm.StorageDelete(120) == 0, "delete atlas resources")
	result.Assert(clm.StorageAtlasNew(122, clm.BpaTypeByte, 0, 5, 4, 2, 2) == -1 && clm.StorageNew2D(123, clm.BpaTypeString, 0, 2, 2) == -1 && clm.StorageNew2D(123, clm.BpaTypeInt, 0, 0, 2) == -1 && clm.StorageNew(123, clm.BpaTypeInt, 0, 4294967295, 4294967295, 2) == -1, "reject invalid atlas, type, zero dimensions and overflow")
	result.Assert(clm.StorageAtlasNew3D(130, clm.BpaTypeByte, 0, 4, 4, 2, 2, 2) == 0 && clm.StorageAtlasSetFrameFrom(130, 1, 0, 1, frame) == 2 && clm.StorageAtlasGetFrameTo(130, 1, 0, 1, output) == 1 && bytes.Equal(output, frame), "layered atlas frame")
	result.Assert(clm.StorageGetTo(130, 0, 0, 2, output) == 1 && output[0] == 0 && clm.StorageDelete(130) == 0, "atlas layer isolation")
	result.Assert(clm.StorageNew2D(140, clm.BpaTypeInt, 0, 2, 2) == 0 && clm.StorageFillFrom(140, intBytes(5)) == 0 && clm.StorageGetTo(140, 0, 1, 1, output) == 1 && bytes.Equal(output, intBytes(5)), "fill every grid cell")
	result.Assert(clm.BpaCreate(141, clm.BpaTypeInt) == 0 && clm.BpaSet(141, intBytes(83)) == 2 && clm.StorageSet(140, 0, 0, 0, 141) == 2 && clm.StorageGet(140, 0, 0, 0, 141) == 1 && clm.BpaGet(141, output) == 1 && bytes.Equal(output, intBytes(83)), "transfer compatible BPA block")
	result.Assert(clm.BpaDelete(141) == 0 && clm.StorageDelete(140) == 0, "release storage transfers")
	CLMTestSummary("Storage Tests Summary", result)
	return result
}
