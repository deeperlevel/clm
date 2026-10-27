package tests

import (
	"unsafe"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

func baseWidths[T any](result *CLMTestResult, width uintptr) {
	result.Assert(unsafe.Sizeof(clm.PositionT[T]{}) == 2*width, "2D position width")
	result.Assert(unsafe.Sizeof(clm.Position3DT[T]{}) == 3*width, "3D position width")
	result.Assert(unsafe.Sizeof(clm.SizeT[T]{}) == 2*width, "2D size width")
	result.Assert(unsafe.Sizeof(clm.Size3DT[T]{}) == 3*width, "3D size width")
	result.Assert(unsafe.Sizeof(clm.BlockT[T]{}) == 4*width, "2D block width")
	result.Assert(unsafe.Sizeof(clm.Block3DT[T]{}) == 6*width, "3D block width")
}
func BaseTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - Base")
	position := clm.Position{X: 10, Y: 20}
	result.Assert(position.X == 10 && position.Y == 20, "default position")
	position.X = 30
	result.Assert(position.X == 30, "mutable position")
	position3D := clm.Position3D{X: 10, Y: 20, Z: 30}
	result.Assert(position3D.X == 10 && position3D.Y == 20 && position3D.Z == 30, "3D position")
	size := clm.Size{Width: 100, Height: 200}
	result.Assert(size.Width == 100 && size.Height == 200, "default size")
	size.Width = 300
	result.Assert(size.Width == 300, "mutable size")
	size3D := clm.Size3D{Width: 100, Height: 200, Depth: 300}
	result.Assert(size3D.Width == 100 && size3D.Height == 200 && size3D.Depth == 300, "3D size")
	block := clm.Block{X: 10, Y: 20, Width: 100, Height: 200}
	result.Assert(block.X == 10 && block.Y == 20 && block.Width == 100 && block.Height == 200, "default block")
	block3D := clm.Block3D{X: 10, Y: 20, Z: 30, Width: 100, Height: 200, Depth: 300}
	result.Assert(block3D.X == 10 && block3D.Y == 20 && block3D.Z == 30 && block3D.Width == 100 && block3D.Height == 200 && block3D.Depth == 300, "3D block")
	floatPosition := clm.PositionFloat32{X: 10.5, Y: 20.25}
	result.Assert(floatPosition.X == 10.5 && floatPosition.Y == 20.25, "floating position")
	floatPosition3D := clm.Position3DFloat32{X: 10.5, Y: 20.25, Z: 30.75}
	result.Assert(floatPosition3D.Z == 30.75, "floating 3D position")
	floatSize := clm.SizeFloat32{Width: 100.5, Height: 200.25}
	result.Assert(floatSize.Width == 100.5 && floatSize.Height == 200.25, "floating size")
	floatSize3D := clm.Size3DFloat32{Width: 100.5, Height: 200.25, Depth: 300.75}
	result.Assert(floatSize3D.Depth == 300.75, "floating 3D size")
	floatBlock := clm.BlockFloat32{X: 10.5, Y: 20.25, Width: 100.5, Height: 200.25}
	result.Assert(floatBlock.X == 10.5 && floatBlock.Height == 200.25, "floating block")
	floatBlock3D := clm.Block3DFloat32{X: 10.5, Y: 20.25, Z: 30.75, Width: 100.5, Height: 200.25, Depth: 300.75}
	result.Assert(floatBlock3D.X == 10.5 && floatBlock3D.Y == 20.25 && floatBlock3D.Z == 30.75 && floatBlock3D.Width == 100.5 && floatBlock3D.Height == 200.25 && floatBlock3D.Depth == 300.75, "floating 3D block")
	baseWidths[int8](&result, 1)
	baseWidths[uint8](&result, 1)
	baseWidths[int16](&result, 2)
	baseWidths[uint16](&result, 2)
	baseWidths[int32](&result, 4)
	baseWidths[uint32](&result, 4)
	baseWidths[int64](&result, 8)
	baseWidths[uint64](&result, 8)
	baseWidths[float32](&result, 4)
	baseWidths[float64](&result, 8)
	result.Assert(unsafe.Sizeof(clm.IDKey(0)) == 8 && clm.IDKey(0x100000001) == 0x100000001, "64-bit IDs")
	CLMTestSummary("Base Tests Summary", result)
	return result
}
