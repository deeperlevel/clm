package clm

import "core:fmt"
import "core:os"

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

idkey :: u64
clm_id :: idkey

clm_byte :: u8
clm_sbyte :: i8
clm_short :: i16
clm_ushort :: u16
clm_int :: i32
clm_uint :: u32
clm_long :: i64
clm_ulong :: u64
clm_float :: f32
clm_double :: f64

ID_KEY_MIN :: idkey(0)
ID_KEY_MAX :: ~u64(0)

make_id :: proc(value: u64) -> idkey {
	if value < u64(ID_KEY_MIN) || value > u64(ID_KEY_MAX) {
		fmt.eprintln("CLM: ID outside configured range")
		os.exit(1)
	}
	return idkey(value)
}

position :: struct($T: typeid) {
    x, y: T
}

position_i8 :: distinct position(clm_sbyte)
position_u8 :: distinct position(clm_byte)
position_i16 :: distinct position(clm_short)
position_u16 :: distinct position(clm_ushort)
position_i32 :: distinct position(clm_int)
position_u32 :: distinct position(clm_uint)
position_i64 :: distinct position(clm_long)
position_u64 :: distinct position(clm_ulong)
position_f32 :: distinct position(clm_float)
position_f64 :: distinct position(clm_double)

position_sbyte :: distinct position(clm_sbyte)
position_byte :: distinct position(clm_byte)
position_short :: distinct position(clm_short)
position_ushort :: distinct position(clm_ushort)
position_int :: distinct position(clm_int)
position_uint :: distinct position(clm_uint)
position_long :: distinct position(clm_long)
position_ulong :: distinct position(clm_ulong)
position_float :: distinct position(clm_float)
position_double :: distinct position(clm_double)

position_3d :: struct($T: typeid) {
	x, y, z: T
}

position_3d_i8 :: distinct position_3d(clm_sbyte)
position_3d_u8 :: distinct position_3d(clm_byte)
position_3d_i16 :: distinct position_3d(clm_short)
position_3d_u16 :: distinct position_3d(clm_ushort)
position_3d_i32 :: distinct position_3d(clm_int)
position_3d_u32 :: distinct position_3d(clm_uint)
position_3d_i64 :: distinct position_3d(clm_long)
position_3d_u64 :: distinct position_3d(clm_ulong)
position_3d_f32 :: distinct position_3d(clm_float)
position_3d_f64 :: distinct position_3d(clm_double)

position_3d_sbyte :: distinct position_3d(clm_sbyte)
position_3d_byte :: distinct position_3d(clm_byte)
position_3d_short :: distinct position_3d(clm_short)
position_3d_ushort :: distinct position_3d(clm_ushort)
position_3d_int :: distinct position_3d(clm_int)
position_3d_uint :: distinct position_3d(clm_uint)
position_3d_long :: distinct position_3d(clm_long)
position_3d_ulong :: distinct position_3d(clm_ulong)
position_3d_float :: distinct position_3d(clm_float)
position_3d_double :: distinct position_3d(clm_double)

size :: struct($T: typeid) {
	width, height: T
}

size_i8 :: distinct size(clm_sbyte)
size_u8 :: distinct size(clm_byte)
size_i16 :: distinct size(clm_short)
size_u16 :: distinct size(clm_ushort)
size_i32 :: distinct size(clm_int)
size_u32 :: distinct size(clm_uint)
size_i64 :: distinct size(clm_long)
size_u64 :: distinct size(clm_ulong)
size_f32 :: distinct size(clm_float)
size_f64 :: distinct size(clm_double)

size_sbyte :: distinct size(clm_sbyte)
size_byte :: distinct size(clm_byte)
size_short :: distinct size(clm_short)
size_ushort :: distinct size(clm_ushort)
size_int :: distinct size(clm_int)
size_uint :: distinct size(clm_uint)
size_long :: distinct size(clm_long)
size_ulong :: distinct size(clm_ulong)
size_float :: distinct size(clm_float)
size_double :: distinct size(clm_double)

size_3d :: struct($T: typeid) {
	width, height, depth: T
}

size_3d_i8 :: distinct size_3d(clm_sbyte)
size_3d_u8 :: distinct size_3d(clm_byte)
size_3d_i16 :: distinct size_3d(clm_short)
size_3d_u16 :: distinct size_3d(clm_ushort)
size_3d_i32 :: distinct size_3d(clm_int)
size_3d_u32 :: distinct size_3d(clm_uint)
size_3d_i64 :: distinct size_3d(clm_long)
size_3d_u64 :: distinct size_3d(clm_ulong)
size_3d_f32 :: distinct size_3d(clm_float)
size_3d_f64 :: distinct size_3d(clm_double)

size_3d_sbyte :: distinct size_3d(clm_sbyte)
size_3d_byte :: distinct size_3d(clm_byte)
size_3d_short :: distinct size_3d(clm_short)
size_3d_ushort :: distinct size_3d(clm_ushort)
size_3d_int :: distinct size_3d(clm_int)
size_3d_uint :: distinct size_3d(clm_uint)
size_3d_long :: distinct size_3d(clm_long)
size_3d_ulong :: distinct size_3d(clm_ulong)
size_3d_float :: distinct size_3d(clm_float)
size_3d_double :: distinct size_3d(clm_double)

block :: struct($T: typeid) {
	x, y, width, height: T
}

block_i8 :: distinct block(clm_sbyte)
block_u8 :: distinct block(clm_byte)
block_i16 :: distinct block(clm_short)
block_u16 :: distinct block(clm_ushort)
block_i32 :: distinct block(clm_int)
block_u32 :: distinct block(clm_uint)
block_i64 :: distinct block(clm_long)
block_u64 :: distinct block(clm_ulong)
block_f32 :: distinct block(clm_float)
block_f64 :: distinct block(clm_double)

block_sbyte :: distinct block(clm_sbyte)
block_byte :: distinct block(clm_byte)
block_short :: distinct block(clm_short)
block_ushort :: distinct block(clm_ushort)
block_int :: distinct block(clm_int)
block_uint :: distinct block(clm_uint)
block_long :: distinct block(clm_long)
block_ulong :: distinct block(clm_ulong)
block_float :: distinct block(clm_float)
block_double :: distinct block(clm_double)

block_3d :: struct($T: typeid) {
	x, y, z: T,
	width, height, depth: T
}

block_3d_i8 :: distinct block_3d(clm_sbyte)
block_3d_u8 :: distinct block_3d(clm_byte)
block_3d_i16 :: distinct block_3d(clm_short)
block_3d_u16 :: distinct block_3d(clm_ushort)
block_3d_i32 :: distinct block_3d(clm_int)
block_3d_u32 :: distinct block_3d(clm_uint)
block_3d_i64 :: distinct block_3d(clm_long)
block_3d_u64 :: distinct block_3d(clm_ulong)
block_3d_f32 :: distinct block_3d(clm_float)
block_3d_f64 :: distinct block_3d(clm_double)

block_3d_sbyte :: distinct block_3d(clm_sbyte)
block_3d_byte :: distinct block_3d(clm_byte)
block_3d_short :: distinct block_3d(clm_short)
block_3d_ushort :: distinct block_3d(clm_ushort)
block_3d_int :: distinct block_3d(clm_int)
block_3d_uint :: distinct block_3d(clm_uint)
block_3d_long :: distinct block_3d(clm_long)
block_3d_ulong :: distinct block_3d(clm_ulong)
block_3d_float :: distinct block_3d(clm_float)
block_3d_double :: distinct block_3d(clm_double)