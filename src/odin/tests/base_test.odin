package tests

import "../clm"
import "core:fmt"

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liabi    lity.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */
 
base_tests :: proc() -> i32 {
	result: test_result

	assert_test(&result, clm.make_id(0) == clm.idkey(0))
	assert_test(&result, clm.make_id(1001) == clm.idkey(1001))
	assert_test(&result, clm.make_id(clm.ID_KEY_MAX) == clm.ID_KEY_MAX)

	pos: clm.position_i32
	pos.x = 10
	pos.y = 20
	assert_test(&result, pos.x == 10 && pos.y == 20)

	pos.x = 30
	assert_test(&result, pos.x == 30)

	pos_3d := clm.position_3d_i32{x = 10, y = 20, z = 30}
	assert_test(&result, pos_3d.x == 10 && pos_3d.y == 20 && pos_3d.z == 30)

	sz := clm.size_i32{width = 100, height = 200}
	assert_test(&result, sz.width == 100 && sz.height == 200)

	sz.width = 300
	assert_test(&result, sz.width == 300)

	sz_3d := clm.size_3d_i32{width = 100, height = 200, depth = 300}
	assert_test(&result, sz_3d.width == 100 && sz_3d.height == 200 && sz_3d.depth == 300)

	blk := clm.block_i32{x = 10, y = 20, width = 100, height = 200}
	assert_test(&result, blk.x == 10 && blk.y == 20 && blk.width == 100 && blk.height == 200)

	blk_3d := clm.block_3d_i32{x = 10, y = 20, z = 30, width = 100, height = 200, depth = 300}
	assert_test(&result, blk_3d.x == 10 && blk_3d.y == 20 && blk_3d.z == 30)
	assert_test(&result, blk_3d.width == 100 && blk_3d.height == 200 && blk_3d.depth == 300)

	f_pos := clm.position_f32{x = 10.5, y = 20.25}
	assert_test(&result, f_pos.x == 10.5 && f_pos.y == 20.25)

	f_pos_3d := clm.position_3d_f32{x = 10.5, y = 20.25, z = 30.75}
	assert_test(&result, f_pos_3d.x == 10.5 && f_pos_3d.y == 20.25 && f_pos_3d.z == 30.75)

	f_sz := clm.size_f32{width = 100.5, height = 200.25}
	assert_test(&result, f_sz.width == 100.5 && f_sz.height == 200.25)

	f_sz_3d := clm.size_3d_f32{width = 100.5, height = 200.25, depth = 300.75}
	assert_test(&result, f_sz_3d.width == 100.5 && f_sz_3d.height == 200.25 && f_sz_3d.depth == 300.75)

	f_blk := clm.block_f32{x = 10.5, y = 20.25, width = 100.5, height = 200.25}
	assert_test(&result, f_blk.x == 10.5 && f_blk.y == 20.25 && f_blk.width == 100.5 && f_blk.height == 200.25)

	f_blk_3d := clm.block_3d_f32{
		x = 10.5, y = 20.25, z = 30.75,
		width = 100.5, height = 200.25, depth = 300.75,
	}
	assert_test(&result, f_blk_3d.x == 10.5)
	assert_test(&result, f_blk_3d.y == 20.25)
	assert_test(&result, f_blk_3d.z == 30.75)
	assert_test(&result, f_blk_3d.width == 100.5)
	assert_test(&result, f_blk_3d.height == 200.25)
	assert_test(&result, f_blk_3d.depth == 300.75)

	assert_test(&result, size_of(clm.position_i8) == 2)
	assert_test(&result, size_of(clm.position_u8) == 2)
	assert_test(&result, size_of(clm.position_i16) == 4)
	assert_test(&result, size_of(clm.position_u16) == 4)
	assert_test(&result, size_of(clm.position_i32) == 8)
	assert_test(&result, size_of(clm.position_u32) == 8)
	assert_test(&result, size_of(clm.position_i64) == 16)
	assert_test(&result, size_of(clm.position_u64) == 16)
	assert_test(&result, size_of(clm.position_f32) == 8)
	assert_test(&result, size_of(clm.position_f64) == 16)

	assert_test(&result, size_of(clm.position_3d_i8) == 3)
	assert_test(&result, size_of(clm.position_3d_u8) == 3)
	assert_test(&result, size_of(clm.position_3d_i16) == 6)
	assert_test(&result, size_of(clm.position_3d_u16) == 6)
	assert_test(&result, size_of(clm.position_3d_i32) == 12)
	assert_test(&result, size_of(clm.position_3d_u32) == 12)
	assert_test(&result, size_of(clm.position_3d_i64) == 24)
	assert_test(&result, size_of(clm.position_3d_u64) == 24)
	assert_test(&result, size_of(clm.position_3d_f32) == 12)
	assert_test(&result, size_of(clm.position_3d_f64) == 24)

	assert_test(&result, size_of(clm.size_i8) == 2)
	assert_test(&result, size_of(clm.size_u8) == 2)
	assert_test(&result, size_of(clm.size_i16) == 4)
	assert_test(&result, size_of(clm.size_u16) == 4)
	assert_test(&result, size_of(clm.size_i32) == 8)
	assert_test(&result, size_of(clm.size_u32) == 8)
	assert_test(&result, size_of(clm.size_i64) == 16)
	assert_test(&result, size_of(clm.size_u64) == 16)
	assert_test(&result, size_of(clm.size_f32) == 8)
	assert_test(&result, size_of(clm.size_f64) == 16)

	assert_test(&result, size_of(clm.size_3d_i8) == 3)
	assert_test(&result, size_of(clm.size_3d_u8) == 3)
	assert_test(&result, size_of(clm.size_3d_i16) == 6)
	assert_test(&result, size_of(clm.size_3d_u16) == 6)
	assert_test(&result, size_of(clm.size_3d_i32) == 12)
	assert_test(&result, size_of(clm.size_3d_u32) == 12)
	assert_test(&result, size_of(clm.size_3d_i64) == 24)
	assert_test(&result, size_of(clm.size_3d_u64) == 24)
	assert_test(&result, size_of(clm.size_3d_f32) == 12)
	assert_test(&result, size_of(clm.size_3d_f64) == 24)

	assert_test(&result, size_of(clm.block_i8) == 4)
	assert_test(&result, size_of(clm.block_u8) == 4)
	assert_test(&result, size_of(clm.block_i16) == 8)
	assert_test(&result, size_of(clm.block_u16) == 8)
	assert_test(&result, size_of(clm.block_i32) == 16)
	assert_test(&result, size_of(clm.block_u32) == 16)
	assert_test(&result, size_of(clm.block_i64) == 32)
	assert_test(&result, size_of(clm.block_u64) == 32)
	assert_test(&result, size_of(clm.block_f32) == 16)
	assert_test(&result, size_of(clm.block_f64) == 32)

	assert_test(&result, size_of(clm.block_3d_i8) == 6)
	assert_test(&result, size_of(clm.block_3d_u8) == 6)
	assert_test(&result, size_of(clm.block_3d_i16) == 12)
	assert_test(&result, size_of(clm.block_3d_u16) == 12)
	assert_test(&result, size_of(clm.block_3d_i32) == 24)
	assert_test(&result, size_of(clm.block_3d_u32) == 24)
	assert_test(&result, size_of(clm.block_3d_i64) == 48)
	assert_test(&result, size_of(clm.block_3d_u64) == 48)
	assert_test(&result, size_of(clm.block_3d_f32) == 24)
	assert_test(&result, size_of(clm.block_3d_f64) == 48)

	test_summary("CLM Base", result.passed, result.failed)

	return result.failed
}
