//! CLM base scalar aliases and geometry types.

pub const clm_id = u64;

pub const clm_sbyte = i8;
pub const clm_byte = u8;
pub const clm_short = i16;
pub const clm_ushort = u16;
pub const clm_int = i32;
pub const clm_uint = u32;
pub const clm_long = i64;
pub const clm_ulong = u64;
pub const clm_float = f32;
pub const clm_double = f64;

fn Position2(comptime T: type) type {
    return struct { x: T, y: T };
}

fn Position3(comptime T: type) type {
    return struct { x: T, y: T, z: T };
}

fn Size2(comptime T: type) type {
    return struct { width: T, height: T };
}

fn Size3(comptime T: type) type {
    return struct { width: T, height: T, depth: T };
}

fn Block2(comptime T: type) type {
    return struct { x: T, y: T, width: T, height: T };
}

fn Block3(comptime T: type) type {
    return struct { x: T, y: T, z: T, width: T, height: T, depth: T };
}

/// Coordinates in a two-dimensional space.
pub const clm_position_i8 = Position2(clm_sbyte);
pub const clm_position_sbyte = clm_position_i8;
pub const clm_position_u8 = Position2(clm_byte);
pub const clm_position_byte = clm_position_u8;
pub const clm_position_i16 = Position2(clm_short);
pub const clm_position_short = clm_position_i16;
pub const clm_position_u16 = Position2(clm_ushort);
pub const clm_position_ushort = clm_position_u16;
pub const clm_position_i32 = Position2(clm_int);
pub const clm_position = clm_position_i32;
pub const clm_position_int = clm_position_i32;
pub const clm_position_u32 = Position2(clm_uint);
pub const clm_position_uint = clm_position_u32;
pub const clm_position_i64 = Position2(clm_long);
pub const clm_position_long = clm_position_i64;
pub const clm_position_u64 = Position2(clm_ulong);
pub const clm_position_ulong = clm_position_u64;
pub const clm_position_f32 = Position2(clm_float);
pub const clm_position_float = clm_position_f32;
pub const clm_position_f64 = Position2(clm_double);
pub const clm_position_double = clm_position_f64;

/// Coordinates in a three-dimensional space.
pub const clm_position_3d_i8 = Position3(clm_sbyte);
pub const clm_position_3d_sbyte = clm_position_3d_i8;
pub const clm_position_3d_u8 = Position3(clm_byte);
pub const clm_position_3d_byte = clm_position_3d_u8;
pub const clm_position_3d_i16 = Position3(clm_short);
pub const clm_position_3d_short = clm_position_3d_i16;
pub const clm_position_3d_u16 = Position3(clm_ushort);
pub const clm_position_3d_ushort = clm_position_3d_u16;
pub const clm_position_3d_i32 = Position3(clm_int);
pub const clm_position_3d = clm_position_3d_i32;
pub const clm_position_3d_int = clm_position_3d_i32;
pub const clm_position_3d_u32 = Position3(clm_uint);
pub const clm_position_3d_uint = clm_position_3d_u32;
pub const clm_position_3d_i64 = Position3(clm_long);
pub const clm_position_3d_long = clm_position_3d_i64;
pub const clm_position_3d_u64 = Position3(clm_ulong);
pub const clm_position_3d_ulong = clm_position_3d_u64;
pub const clm_position_3d_f32 = Position3(clm_float);
pub const clm_position_3d_float = clm_position_3d_f32;
pub const clm_position_3d_f64 = Position3(clm_double);
pub const clm_position_3d_double = clm_position_3d_f64;

/// Width and height in a two-dimensional space.
pub const clm_size_i8 = Size2(clm_sbyte);
pub const clm_size_sbyte = clm_size_i8;
pub const clm_size_u8 = Size2(clm_byte);
pub const clm_size_byte = clm_size_u8;
pub const clm_size_i16 = Size2(clm_short);
pub const clm_size_short = clm_size_i16;
pub const clm_size_u16 = Size2(clm_ushort);
pub const clm_size_ushort = clm_size_u16;
pub const clm_size_i32 = Size2(clm_int);
pub const clm_size = clm_size_i32;
pub const clm_size_int = clm_size_i32;
pub const clm_size_u32 = Size2(clm_uint);
pub const clm_size_uint = clm_size_u32;
pub const clm_size_i64 = Size2(clm_long);
pub const clm_size_long = clm_size_i64;
pub const clm_size_u64 = Size2(clm_ulong);
pub const clm_size_ulong = clm_size_u64;
pub const clm_size_f32 = Size2(clm_float);
pub const clm_size_float = clm_size_f32;
pub const clm_size_f64 = Size2(clm_double);
pub const clm_size_double = clm_size_f64;

/// Width, height, and depth in a three-dimensional space.
pub const clm_size_3d_i8 = Size3(clm_sbyte);
pub const clm_size_3d_sbyte = clm_size_3d_i8;
pub const clm_size_3d_u8 = Size3(clm_byte);
pub const clm_size_3d_byte = clm_size_3d_u8;
pub const clm_size_3d_i16 = Size3(clm_short);
pub const clm_size_3d_short = clm_size_3d_i16;
pub const clm_size_3d_u16 = Size3(clm_ushort);
pub const clm_size_3d_ushort = clm_size_3d_u16;
pub const clm_size_3d_i32 = Size3(clm_int);
pub const clm_size_3d = clm_size_3d_i32;
pub const clm_size_3d_int = clm_size_3d_i32;
pub const clm_size_3d_u32 = Size3(clm_uint);
pub const clm_size_3d_uint = clm_size_3d_u32;
pub const clm_size_3d_i64 = Size3(clm_long);
pub const clm_size_3d_long = clm_size_3d_i64;
pub const clm_size_3d_u64 = Size3(clm_ulong);
pub const clm_size_3d_ulong = clm_size_3d_u64;
pub const clm_size_3d_f32 = Size3(clm_float);
pub const clm_size_3d_float = clm_size_3d_f32;
pub const clm_size_3d_f64 = Size3(clm_double);
pub const clm_size_3d_double = clm_size_3d_f64;

/// Position and extent in a two-dimensional space.
pub const clm_block_i8 = Block2(clm_sbyte);
pub const clm_block_sbyte = clm_block_i8;
pub const clm_block_u8 = Block2(clm_byte);
pub const clm_block_byte = clm_block_u8;
pub const clm_block_i16 = Block2(clm_short);
pub const clm_block_short = clm_block_i16;
pub const clm_block_u16 = Block2(clm_ushort);
pub const clm_block_ushort = clm_block_u16;
pub const clm_block_i32 = Block2(clm_int);
pub const clm_block = clm_block_i32;
pub const clm_block_int = clm_block_i32;
pub const clm_block_u32 = Block2(clm_uint);
pub const clm_block_uint = clm_block_u32;
pub const clm_block_i64 = Block2(clm_long);
pub const clm_block_long = clm_block_i64;
pub const clm_block_u64 = Block2(clm_ulong);
pub const clm_block_ulong = clm_block_u64;
pub const clm_block_f32 = Block2(clm_float);
pub const clm_block_float = clm_block_f32;
pub const clm_block_f64 = Block2(clm_double);
pub const clm_block_double = clm_block_f64;

/// Position and extent in a three-dimensional space.
pub const clm_block_3d_i8 = Block3(clm_sbyte);
pub const clm_block_3d_sbyte = clm_block_3d_i8;
pub const clm_block_3d_u8 = Block3(clm_byte);
pub const clm_block_3d_byte = clm_block_3d_u8;
pub const clm_block_3d_i16 = Block3(clm_short);
pub const clm_block_3d_short = clm_block_3d_i16;
pub const clm_block_3d_u16 = Block3(clm_ushort);
pub const clm_block_3d_ushort = clm_block_3d_u16;
pub const clm_block_3d_i32 = Block3(clm_int);
pub const clm_block_3d = clm_block_3d_i32;
pub const clm_block_3d_int = clm_block_3d_i32;
pub const clm_block_3d_u32 = Block3(clm_uint);
pub const clm_block_3d_uint = clm_block_3d_u32;
pub const clm_block_3d_i64 = Block3(clm_long);
pub const clm_block_3d_long = clm_block_3d_i64;
pub const clm_block_3d_u64 = Block3(clm_ulong);
pub const clm_block_3d_ulong = clm_block_3d_u64;
pub const clm_block_3d_f32 = Block3(clm_float);
pub const clm_block_3d_float = clm_block_3d_f32;
pub const clm_block_3d_f64 = Block3(clm_double);
pub const clm_block_3d_double = clm_block_3d_f64;
