module clm.base;

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

alias clm_sbyte = byte;
alias clm_byte = ubyte;
alias clm_short = short;
alias clm_ushort = ushort;
alias clm_int = int;
alias clm_uint = uint;
alias clm_long = long;
alias clm_ulong = ulong;
alias clm_float = float;
alias clm_double = double;
alias clm_id = ulong;

alias IdKey = ulong;
enum IdKey ID_KEY_MIN = 0;
enum IdKey ID_KEY_MAX = IdKey.max;

IdKey clm_base_make_id(ulong value) {
    if (value > cast(ulong)ID_KEY_MAX)
        throw new Exception("CLM: ID outside configured range");
    return cast(IdKey)value;
}

IdKey clm_base_make_id(long value) {
    if (value < 0)
        throw new Exception("CLM: negative ID");
    return clm_base_make_id(cast(ulong)value);
}

struct Position(T) {
    T x;
    T y;
}
alias Position_i8  = Position!clm_sbyte;
alias Position_u8  = Position!clm_byte;
alias Position_i16 = Position!clm_short;
alias Position_u16 = Position!clm_ushort;
alias Position_i32 = Position!clm_int;
alias Position_u32 = Position!clm_uint;
alias Position_i64 = Position!clm_long;
alias Position_u64 = Position!clm_ulong;
alias Position_f32 = Position!clm_float;
alias Position_f64 = Position!clm_double;

struct Position3D(T) {
    T x;
    T y;
    T z;
}
alias Position3D_i8  = Position3D!clm_sbyte;
alias Position3D_u8  = Position3D!clm_byte;
alias Position3D_i16 = Position3D!clm_short;
alias Position3D_u16 = Position3D!clm_ushort;
alias Position3D_i32 = Position3D!clm_int;
alias Position3D_u32 = Position3D!clm_uint;
alias Position3D_i64 = Position3D!clm_long;
alias Position3D_u64 = Position3D!clm_ulong;
alias Position3D_f32 = Position3D!clm_float;
alias Position3D_f64 = Position3D!clm_double;

struct Size(T) {
    T width;
    T height;
}
alias Size_i8  = Size!clm_sbyte;
alias Size_u8  = Size!clm_byte;
alias Size_i16 = Size!clm_short;
alias Size_u16 = Size!clm_ushort;
alias Size_i32 = Size!clm_int;
alias Size_u32 = Size!clm_uint;
alias Size_i64 = Size!clm_long;
alias Size_u64 = Size!clm_ulong;
alias Size_f32 = Size!clm_float;
alias Size_f64 = Size!clm_double;

struct Size3D(T) {
    T width;
    T height;
    T depth;
}
alias Size3D_i8  = Size3D!clm_sbyte;
alias Size3D_u8  = Size3D!clm_byte;
alias Size3D_i16 = Size3D!clm_short;
alias Size3D_u16 = Size3D!clm_ushort;
alias Size3D_i32 = Size3D!clm_int;
alias Size3D_u32 = Size3D!clm_uint;
alias Size3D_i64 = Size3D!clm_long;
alias Size3D_u64 = Size3D!clm_ulong;
alias Size3D_f32 = Size3D!clm_float;
alias Size3D_f64 = Size3D!clm_double;

struct Block(T) {
    T x;
    T y;
    T width;
    T height;
}
alias Block_i8  = Block!clm_sbyte;
alias Block_u8  = Block!clm_byte;
alias Block_i16 = Block!clm_short;
alias Block_u16 = Block!clm_ushort;
alias Block_i32 = Block!clm_int;
alias Block_u32 = Block!clm_uint;
alias Block_i64 = Block!clm_long;
alias Block_u64 = Block!clm_ulong;
alias Block_f32 = Block!clm_float;
alias Block_f64 = Block!clm_double;

struct Block3D(T) {
    T x;
    T y;
    T z;
    T width;
    T height;
    T depth;
}
alias Block3D_i8  = Block3D!clm_sbyte;
alias Block3D_u8  = Block3D!clm_byte;
alias Block3D_i16 = Block3D!clm_short;
alias Block3D_u16 = Block3D!clm_ushort;
alias Block3D_i32 = Block3D!clm_int;
alias Block3D_u32 = Block3D!clm_uint;
alias Block3D_i64 = Block3D!clm_long;
alias Block3D_u64 = Block3D!clm_ulong;
alias Block3D_f32 = Block3D!clm_float;
alias Block3D_f64 = Block3D!clm_double;

alias clm_position_i8 = Position_i8;
alias clm_position_sbyte = Position_i8;
alias clm_position_u8 = Position_u8;
alias clm_position_byte = Position_u8;
alias clm_position_i16 = Position_i16;
alias clm_position_short = Position_i16;
alias clm_position_u16 = Position_u16;
alias clm_position_ushort = Position_u16;
alias clm_position_i32 = Position_i32;
alias clm_position_int = Position_i32;
alias clm_position = Position_i32;
alias clm_position_u32 = Position_u32;
alias clm_position_uint = Position_u32;
alias clm_position_i64 = Position_i64;
alias clm_position_long = Position_i64;
alias clm_position_u64 = Position_u64;
alias clm_position_ulong = Position_u64;
alias clm_position_f32 = Position_f32;
alias clm_position_float = Position_f32;
alias clm_position_f64 = Position_f64;
alias clm_position_double = Position_f64;

alias clm_position_3d_i8 = Position3D_i8;
alias clm_position_3d_sbyte = Position3D_i8;
alias clm_position_3d_u8 = Position3D_u8;
alias clm_position_3d_byte = Position3D_u8;
alias clm_position_3d_i16 = Position3D_i16;
alias clm_position_3d_short = Position3D_i16;
alias clm_position_3d_u16 = Position3D_u16;
alias clm_position_3d_ushort = Position3D_u16;
alias clm_position_3d_i32 = Position3D_i32;
alias clm_position_3d_int = Position3D_i32;
alias clm_position_3d = Position3D_i32;
alias clm_position_3d_u32 = Position3D_u32;
alias clm_position_3d_uint = Position3D_u32;
alias clm_position_3d_i64 = Position3D_i64;
alias clm_position_3d_long = Position3D_i64;
alias clm_position_3d_u64 = Position3D_u64;
alias clm_position_3d_ulong = Position3D_u64;
alias clm_position_3d_f32 = Position3D_f32;
alias clm_position_3d_float = Position3D_f32;
alias clm_position_3d_f64 = Position3D_f64;
alias clm_position_3d_double = Position3D_f64;

alias clm_size_i8 = Size_i8;
alias clm_size_sbyte = Size_i8;
alias clm_size_u8 = Size_u8;
alias clm_size_byte = Size_u8;
alias clm_size_i16 = Size_i16;
alias clm_size_short = Size_i16;
alias clm_size_u16 = Size_u16;
alias clm_size_ushort = Size_u16;
alias clm_size_i32 = Size_i32;
alias clm_size_int = Size_i32;
alias clm_size = Size_i32;
alias clm_size_u32 = Size_u32;
alias clm_size_uint = Size_u32;
alias clm_size_i64 = Size_i64;
alias clm_size_long = Size_i64;
alias clm_size_u64 = Size_u64;
alias clm_size_ulong = Size_u64;
alias clm_size_f32 = Size_f32;
alias clm_size_float = Size_f32;
alias clm_size_f64 = Size_f64;
alias clm_size_double = Size_f64;

alias clm_size_3d_i8 = Size3D_i8;
alias clm_size_3d_sbyte = Size3D_i8;
alias clm_size_3d_u8 = Size3D_u8;
alias clm_size_3d_byte = Size3D_u8;
alias clm_size_3d_i16 = Size3D_i16;
alias clm_size_3d_short = Size3D_i16;
alias clm_size_3d_u16 = Size3D_u16;
alias clm_size_3d_ushort = Size3D_u16;
alias clm_size_3d_i32 = Size3D_i32;
alias clm_size_3d_int = Size3D_i32;
alias clm_size_3d = Size3D_i32;
alias clm_size_3d_u32 = Size3D_u32;
alias clm_size_3d_uint = Size3D_u32;
alias clm_size_3d_i64 = Size3D_i64;
alias clm_size_3d_long = Size3D_i64;
alias clm_size_3d_u64 = Size3D_u64;
alias clm_size_3d_ulong = Size3D_u64;
alias clm_size_3d_f32 = Size3D_f32;
alias clm_size_3d_float = Size3D_f32;
alias clm_size_3d_f64 = Size3D_f64;
alias clm_size_3d_double = Size3D_f64;

alias clm_block_i8 = Block_i8;
alias clm_block_sbyte = Block_i8;
alias clm_block_u8 = Block_u8;
alias clm_block_byte = Block_u8;
alias clm_block_i16 = Block_i16;
alias clm_block_short = Block_i16;
alias clm_block_u16 = Block_u16;
alias clm_block_ushort = Block_u16;
alias clm_block_i32 = Block_i32;
alias clm_block_int = Block_i32;
alias clm_block = Block_i32;
alias clm_block_u32 = Block_u32;
alias clm_block_uint = Block_u32;
alias clm_block_i64 = Block_i64;
alias clm_block_long = Block_i64;
alias clm_block_u64 = Block_u64;
alias clm_block_ulong = Block_u64;
alias clm_block_f32 = Block_f32;
alias clm_block_float = Block_f32;
alias clm_block_f64 = Block_f64;
alias clm_block_double = Block_f64;

alias clm_block_3d_i8 = Block3D_i8;
alias clm_block_3d_sbyte = Block3D_i8;
alias clm_block_3d_u8 = Block3D_u8;
alias clm_block_3d_byte = Block3D_u8;
alias clm_block_3d_i16 = Block3D_i16;
alias clm_block_3d_short = Block3D_i16;
alias clm_block_3d_u16 = Block3D_u16;
alias clm_block_3d_ushort = Block3D_u16;
alias clm_block_3d_i32 = Block3D_i32;
alias clm_block_3d_int = Block3D_i32;
alias clm_block_3d = Block3D_i32;
alias clm_block_3d_u32 = Block3D_u32;
alias clm_block_3d_uint = Block3D_u32;
alias clm_block_3d_i64 = Block3D_i64;
alias clm_block_3d_long = Block3D_i64;
alias clm_block_3d_u64 = Block3D_u64;
alias clm_block_3d_ulong = Block3D_u64;
alias clm_block_3d_f32 = Block3D_f32;
alias clm_block_3d_float = Block3D_f32;
alias clm_block_3d_f64 = Block3D_f64;
alias clm_block_3d_double = Block3D_f64;
