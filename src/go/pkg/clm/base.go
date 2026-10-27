package clm

type IDKey = uint64
type ID = IDKey

const IdKeyMax IDKey = 0xffffffffffffffff
const IDKeyMax = IdKeyMax

type (
	Sbyte  = int8
	Byte   = uint8
	Short  = int16
	Ushort = uint16
	Int    = int32
	Uint   = uint32
	Long   = int64
	Ulong  = uint64
	Float  = float32
	Double = float64
)

type PositionT[T any] struct {
	X T
	Y T
}

type Position3DT[T any] struct {
	X T
	Y T
	Z T
}

type SizeT[T any] struct {
	Width  T
	Height T
}

type Size3DT[T any] struct {
	Width  T
	Height T
	Depth  T
}

type BlockT[T any] struct {
	X      T
	Y      T
	Width  T
	Height T
}

type Block3DT[T any] struct {
	X      T
	Y      T
	Z      T
	Width  T
	Height T
	Depth  T
}

type (
	PositionInt8      = PositionT[int8]
	Position3DInt8    = Position3DT[int8]
	SizeInt8          = SizeT[int8]
	Size3DInt8        = Size3DT[int8]
	BlockInt8         = BlockT[int8]
	Block3DInt8       = Block3DT[int8]
	PositionUint8     = PositionT[uint8]
	Position3DUint8   = Position3DT[uint8]
	SizeUint8         = SizeT[uint8]
	Size3DUint8       = Size3DT[uint8]
	BlockUint8        = BlockT[uint8]
	Block3DUint8      = Block3DT[uint8]
	PositionInt16     = PositionT[int16]
	Position3DInt16   = Position3DT[int16]
	SizeInt16         = SizeT[int16]
	Size3DInt16       = Size3DT[int16]
	BlockInt16        = BlockT[int16]
	Block3DInt16      = Block3DT[int16]
	PositionUint16    = PositionT[uint16]
	Position3DUint16  = Position3DT[uint16]
	SizeUint16        = SizeT[uint16]
	Size3DUint16      = Size3DT[uint16]
	BlockUint16       = BlockT[uint16]
	Block3DUint16     = Block3DT[uint16]
	PositionInt32     = PositionT[int32]
	Position3DInt32   = Position3DT[int32]
	SizeInt32         = SizeT[int32]
	Size3DInt32       = Size3DT[int32]
	BlockInt32        = BlockT[int32]
	Block3DInt32      = Block3DT[int32]
	PositionUint32    = PositionT[uint32]
	Position3DUint32  = Position3DT[uint32]
	SizeUint32        = SizeT[uint32]
	Size3DUint32      = Size3DT[uint32]
	BlockUint32       = BlockT[uint32]
	Block3DUint32     = Block3DT[uint32]
	PositionInt64     = PositionT[int64]
	Position3DInt64   = Position3DT[int64]
	SizeInt64         = SizeT[int64]
	Size3DInt64       = Size3DT[int64]
	BlockInt64        = BlockT[int64]
	Block3DInt64      = Block3DT[int64]
	PositionUint64    = PositionT[uint64]
	Position3DUint64  = Position3DT[uint64]
	SizeUint64        = SizeT[uint64]
	Size3DUint64      = Size3DT[uint64]
	BlockUint64       = BlockT[uint64]
	Block3DUint64     = Block3DT[uint64]
	PositionFloat32   = PositionT[float32]
	Position3DFloat32 = Position3DT[float32]
	SizeFloat32       = SizeT[float32]
	Size3DFloat32     = Size3DT[float32]
	BlockFloat32      = BlockT[float32]
	Block3DFloat32    = Block3DT[float32]
	PositionFloat64   = PositionT[float64]
	Position3DFloat64 = Position3DT[float64]
	SizeFloat64       = SizeT[float64]
	Size3DFloat64     = Size3DT[float64]
	BlockFloat64      = BlockT[float64]
	Block3DFloat64    = Block3DT[float64]

	Position   = PositionInt32
	Position3D = Position3DInt32
	Size       = SizeInt32
	Size3D     = Size3DInt32
	Block      = BlockInt32
	Block3D    = Block3DInt32
)

type (
	PositionI8       = PositionInt8
	PositionU8       = PositionUint8
	PositionI16      = PositionInt16
	PositionU16      = PositionUint16
	PositionI32      = PositionInt32
	PositionU32      = PositionUint32
	PositionI64      = PositionInt64
	PositionU64      = PositionUint64
	PositionF32      = PositionFloat32
	PositionF64      = PositionFloat64
	PositionSbyte    = PositionInt8
	PositionByte     = PositionUint8
	PositionShort    = PositionInt16
	PositionUshort   = PositionUint16
	PositionInt      = PositionInt32
	PositionUint     = PositionUint32
	PositionLong     = PositionInt64
	PositionUlong    = PositionUint64
	PositionFloat    = PositionFloat32
	PositionDouble   = PositionFloat64
	Position3DI8     = Position3DInt8
	Position3DU8     = Position3DUint8
	Position3DI16    = Position3DInt16
	Position3DU16    = Position3DUint16
	Position3DI32    = Position3DInt32
	Position3DU32    = Position3DUint32
	Position3DI64    = Position3DInt64
	Position3DU64    = Position3DUint64
	Position3DF32    = Position3DFloat32
	Position3DF64    = Position3DFloat64
	Position3DSbyte  = Position3DInt8
	Position3DByte   = Position3DUint8
	Position3DShort  = Position3DInt16
	Position3DUshort = Position3DUint16
	Position3DInt    = Position3DInt32
	Position3DUint   = Position3DUint32
	Position3DLong   = Position3DInt64
	Position3DUlong  = Position3DUint64
	Position3DFloat  = Position3DFloat32
	Position3DDouble = Position3DFloat64
	SizeI8           = SizeInt8
	SizeU8           = SizeUint8
	SizeI16          = SizeInt16
	SizeU16          = SizeUint16
	SizeI32          = SizeInt32
	SizeU32          = SizeUint32
	SizeI64          = SizeInt64
	SizeU64          = SizeUint64
	SizeF32          = SizeFloat32
	SizeF64          = SizeFloat64
	SizeSbyte        = SizeInt8
	SizeByte         = SizeUint8
	SizeShort        = SizeInt16
	SizeUshort       = SizeUint16
	SizeInt          = SizeInt32
	SizeUint         = SizeUint32
	SizeLong         = SizeInt64
	SizeUlong        = SizeUint64
	SizeFloat        = SizeFloat32
	SizeDouble       = SizeFloat64
	Size3DI8         = Size3DInt8
	Size3DU8         = Size3DUint8
	Size3DI16        = Size3DInt16
	Size3DU16        = Size3DUint16
	Size3DI32        = Size3DInt32
	Size3DU32        = Size3DUint32
	Size3DI64        = Size3DInt64
	Size3DU64        = Size3DUint64
	Size3DF32        = Size3DFloat32
	Size3DF64        = Size3DFloat64
	Size3DSbyte      = Size3DInt8
	Size3DByte       = Size3DUint8
	Size3DShort      = Size3DInt16
	Size3DUshort     = Size3DUint16
	Size3DInt        = Size3DInt32
	Size3DUint       = Size3DUint32
	Size3DLong       = Size3DInt64
	Size3DUlong      = Size3DUint64
	Size3DFloat      = Size3DFloat32
	Size3DDouble     = Size3DFloat64
	BlockI8          = BlockInt8
	BlockU8          = BlockUint8
	BlockI16         = BlockInt16
	BlockU16         = BlockUint16
	BlockI32         = BlockInt32
	BlockU32         = BlockUint32
	BlockI64         = BlockInt64
	BlockU64         = BlockUint64
	BlockF32         = BlockFloat32
	BlockF64         = BlockFloat64
	BlockSbyte       = BlockInt8
	BlockByte        = BlockUint8
	BlockShort       = BlockInt16
	BlockUshort      = BlockUint16
	BlockInt         = BlockInt32
	BlockUint        = BlockUint32
	BlockLong        = BlockInt64
	BlockUlong       = BlockUint64
	BlockFloat       = BlockFloat32
	BlockDouble      = BlockFloat64
	Block3DI8        = Block3DInt8
	Block3DU8        = Block3DUint8
	Block3DI16       = Block3DInt16
	Block3DU16       = Block3DUint16
	Block3DI32       = Block3DInt32
	Block3DU32       = Block3DUint32
	Block3DI64       = Block3DInt64
	Block3DU64       = Block3DUint64
	Block3DF32       = Block3DFloat32
	Block3DF64       = Block3DFloat64
	Block3DSbyte     = Block3DInt8
	Block3DByte      = Block3DUint8
	Block3DShort     = Block3DInt16
	Block3DUshort    = Block3DUint16
	Block3DInt       = Block3DInt32
	Block3DUint      = Block3DUint32
	Block3DLong      = Block3DInt64
	Block3DUlong     = Block3DUint64
	Block3DFloat     = Block3DFloat32
	Block3DDouble    = Block3DFloat64
)
