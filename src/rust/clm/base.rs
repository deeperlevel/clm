/*
 * =============================================================================
 * CLM - Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain - CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

//! Shared scalar aliases and geometry types used by CLM modules.

/// Unsigned 64-bit identifier type.
pub type ClmId = u64;

/// Signed 8-bit integer.
pub type Sbyte = i8;
/// Unsigned 8-bit integer.
pub type Byte = u8;
/// Signed 16-bit integer.
pub type Short = i16;
/// Unsigned 16-bit integer.
pub type Ushort = u16;
/// Signed 32-bit integer.
pub type Int = i32;
/// Unsigned 32-bit integer.
pub type Uint = u32;
/// Signed 64-bit integer.
pub type Long = i64;
/// Unsigned 64-bit integer.
pub type Ulong = u64;
/// 32-bit floating-point number.
pub type Float = f32;
/// 64-bit floating-point number.
pub type Double = f64;

/// Two-dimensional position.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Position2d<T> {
    pub x: T,
    pub y: T,
}

/// Three-dimensional position.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Position3dValue<T> {
    pub x: T,
    pub y: T,
    pub z: T,
}

/// Two-dimensional size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Size2d<T> {
    pub width: T,
    pub height: T,
}

/// Three-dimensional size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Size3dValue<T> {
    pub width: T,
    pub height: T,
    pub depth: T,
}

/// Two-dimensional rectangle described by its origin and size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Block2d<T> {
    pub x: T,
    pub y: T,
    pub width: T,
    pub height: T,
}

/// Three-dimensional block described by its origin and size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Block3dValue<T> {
    pub x: T,
    pub y: T,
    pub z: T,
    pub width: T,
    pub height: T,
    pub depth: T,
}

macro_rules! geometry_aliases {
    (
        $scalar:ty,
        $position:ident, $position_alias:ident,
        $position_3d:ident, $position_3d_alias:ident,
        $size:ident, $size_alias:ident,
        $size_3d:ident, $size_3d_alias:ident,
        $block:ident, $block_alias:ident,
        $block_3d:ident, $block_3d_alias:ident
    ) => {
        pub type $position = Position2d<$scalar>;
        pub type $position_alias = Position2d<$scalar>;
        pub type $position_3d = Position3dValue<$scalar>;
        pub type $position_3d_alias = Position3dValue<$scalar>;
        pub type $size = Size2d<$scalar>;
        pub type $size_alias = Size2d<$scalar>;
        pub type $size_3d = Size3dValue<$scalar>;
        pub type $size_3d_alias = Size3dValue<$scalar>;
        pub type $block = Block2d<$scalar>;
        pub type $block_alias = Block2d<$scalar>;
        pub type $block_3d = Block3dValue<$scalar>;
        pub type $block_3d_alias = Block3dValue<$scalar>;
    };
}

// =============================================================================
// Position, size, and block aliases for each supported scalar width.
// =============================================================================
geometry_aliases!(
    i8, PositionI8, PositionSbyte, Position3dI8, Position3dSbyte,
    SizeI8, SizeSbyte, Size3dI8, Size3dSbyte,
    BlockI8, BlockSbyte, Block3dI8, Block3dSbyte
);
geometry_aliases!(
    u8, PositionU8, PositionByte, Position3dU8, Position3dByte,
    SizeU8, SizeByte, Size3dU8, Size3dByte,
    BlockU8, BlockByte, Block3dU8, Block3dByte
);
geometry_aliases!(
    i16, PositionI16, PositionShort, Position3dI16, Position3dShort,
    SizeI16, SizeShort, Size3dI16, Size3dShort,
    BlockI16, BlockShort, Block3dI16, Block3dShort
);
geometry_aliases!(
    u16, PositionU16, PositionUshort, Position3dU16, Position3dUshort,
    SizeU16, SizeUshort, Size3dU16, Size3dUshort,
    BlockU16, BlockUshort, Block3dU16, Block3dUshort
);
geometry_aliases!(
    i32, PositionI32, PositionInt, Position3dI32, Position3dInt,
    SizeI32, SizeInt, Size3dI32, Size3dInt,
    BlockI32, BlockInt, Block3dI32, Block3dInt
);
geometry_aliases!(
    u32, PositionU32, PositionUint, Position3dU32, Position3dUint,
    SizeU32, SizeUint, Size3dU32, Size3dUint,
    BlockU32, BlockUint, Block3dU32, Block3dUint
);
geometry_aliases!(
    i64, PositionI64, PositionLong, Position3dI64, Position3dLong,
    SizeI64, SizeLong, Size3dI64, Size3dLong,
    BlockI64, BlockLong, Block3dI64, Block3dLong
);
geometry_aliases!(
    u64, PositionU64, PositionUlong, Position3dU64, Position3dUlong,
    SizeU64, SizeUlong, Size3dU64, Size3dUlong,
    BlockU64, BlockUlong, Block3dU64, Block3dUlong
);
geometry_aliases!(
    f32, PositionF32, PositionFloat, Position3dF32, Position3dFloat,
    SizeF32, SizeFloat, Size3dF32, Size3dFloat,
    BlockF32, BlockFloat, Block3dF32, Block3dFloat
);
geometry_aliases!(
    f64, PositionF64, PositionDouble, Position3dF64, Position3dDouble,
    SizeF64, SizeDouble, Size3dF64, Size3dDouble,
    BlockF64, BlockDouble, Block3dF64, Block3dDouble
);

pub type Position = PositionI32;
pub type Position3d = Position3dI32;
pub type Size = SizeI32;
pub type Size3d = Size3dI32;
pub type Block = BlockI32;
pub type Block3d = Block3dI32;