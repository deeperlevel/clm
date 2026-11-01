/*
* =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
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

#pragma once

#include "test.hpp"
#include "../clm/base.hpp"

using clm_position = clm::base::i32::position;
using clm_position_3d = clm::base::i32::position_3d;
using clm_size = clm::base::i32::size;
using clm_size_3d = clm::base::i32::size_3d;
using clm_block = clm::base::i32::block;
using clm_block_3d = clm::base::i32::block_3d;

#define CLM_BASE_TEST_TYPE_ALIASES(suffix) \
    using clm_position_##suffix = clm::base::suffix::position; \
    using clm_position_3d_##suffix = clm::base::suffix::position_3d; \
    using clm_size_##suffix = clm::base::suffix::size; \
    using clm_size_3d_##suffix = clm::base::suffix::size_3d; \
    using clm_block_##suffix = clm::base::suffix::block; \
    using clm_block_3d_##suffix = clm::base::suffix::block_3d

CLM_BASE_TEST_TYPE_ALIASES(i8);
CLM_BASE_TEST_TYPE_ALIASES(u8);
CLM_BASE_TEST_TYPE_ALIASES(i16);
CLM_BASE_TEST_TYPE_ALIASES(u16);
CLM_BASE_TEST_TYPE_ALIASES(i32);
CLM_BASE_TEST_TYPE_ALIASES(u32);
CLM_BASE_TEST_TYPE_ALIASES(i64);
CLM_BASE_TEST_TYPE_ALIASES(u64);
CLM_BASE_TEST_TYPE_ALIASES(f32);
CLM_BASE_TEST_TYPE_ALIASES(f64);

#undef CLM_BASE_TEST_TYPE_ALIASES

static int clm_base_tests_passed = 0;
static int clm_base_tests_failed = 0;

static int clm_base_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Base");

    clm_position position = {.x = 10, .y = 20};

    CLM_ASSERT(position.x == 10 && position.y == 20, "position.x == 10 && position.y == 20");

    position.x = 30;
    CLM_ASSERT(position.x == 30, "position.x == 30");

    const clm_position_3d position_3d = {.x = 10, .y = 20, .z = 30};

    CLM_ASSERT(position_3d.x == 10 && position_3d.y == 20 && position_3d.z == 30, "position_3d.x == 10 && position_3d.y == 20 && position_3d.z == 30");

    clm_size size = {.width = 100, .height = 200};
    CLM_ASSERT(size.width == 100 && size.height == 200, "size.width == 100 && size.height == 200");

    size.width = 300;
    CLM_ASSERT(size.width == 300, "size.width == 300");

    const clm_size_3d size_3d = {.width = 100, .height = 200, .depth = 300};
    CLM_ASSERT(size_3d.width == 100 && size_3d.height == 200 && size_3d.depth == 300, "size_3d.width == 100 && size_3d.height == 200 && size_3d.depth == 300");

    const clm_block block = {.x = 10, .y = 20, .width = 100, .height = 200};
    CLM_ASSERT(block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200, "block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200");

    const clm_block_3d block_3d = {.x = 10, .y = 20, .z = 30, .width = 100, .height = 200, .depth = 300};
    CLM_ASSERT(block_3d.x == 10 && block_3d.y == 20 && block_3d.z == 30, "block_3d.x == 10 && block_3d.y == 20 && block_3d.z == 30");
    CLM_ASSERT(block_3d.width == 100 && block_3d.height == 200 && block_3d.depth == 300, "block_3d.width == 100 && block_3d.height == 200 && block_3d.depth == 300");

    const clm_position_f32 f_position = {.x = 10.5f, .y = 20.25f};
    CLM_ASSERT(f_position.x == 10.5f && f_position.y == 20.25f, "f_position.x == 10.5f && f_position.y == 20.25f");

    const clm_position_3d_f32 f_position_3d = {.x = 10.5f, .y = 20.25f, .z = 30.75f};
    CLM_ASSERT(f_position_3d.x == 10.5f && f_position_3d.y == 20.25f && f_position_3d.z == 30.75f, "f_position_3d.x == 10.5f && f_position_3d.y == 20.25f && f_position_3d.z == 30.75f");

    const clm_size_f32 f_size = {.width = 100.5f, .height = 200.25f};
    CLM_ASSERT(f_size.width == 100.5f && f_size.height == 200.25f, "f_size.width == 100.5f && f_size.height == 200.25f");

    const clm_size_3d_f32 f_size_3d = {.width = 100.5f, .height = 200.25f, .depth = 300.75f};
    CLM_ASSERT(f_size_3d.width == 100.5f && f_size_3d.height == 200.25f && f_size_3d.depth == 300.75f, "f_size_3d.width == 100.5f && f_size_3d.height == 200.25f && f_size_3d.depth == 300.75f");

    const clm_block_f32 f_block = {.x = 10.5f, .y = 20.25f, .width = 100.5f, .height = 200.25f};
    CLM_ASSERT(f_block.x == 10.5f && f_block.y == 20.25f && f_block.width == 100.5f && f_block.height == 200.25f, "f_block.x == 10.5f && f_block.y == 20.25f && f_block.width == 100.5f && f_block.height == 200.25f");

    const clm_block_3d_f32 f_block_3d = {
        .x = 10.5f, .y = 20.25f, .z = 30.75f,
        .width = 100.5f, .height = 200.25f, .depth = 300.75f
    };

    CLM_ASSERT(f_block_3d.x == 10.5f, "f_block_3d.x == 10.5f");
    CLM_ASSERT(f_block_3d.y == 20.25f, "f_block_3d.y == 20.25f");
    CLM_ASSERT(f_block_3d.z == 30.75f, "f_block_3d.z == 30.75f");
    CLM_ASSERT(f_block_3d.width == 100.5f, "f_block_3d.width == 100.5f");
    CLM_ASSERT(f_block_3d.height == 200.25f, "f_block_3d.height == 200.25f");
    CLM_ASSERT(f_block_3d.depth == 300.75f, "f_block_3d.depth == 300.75f");

    CLM_ASSERT(sizeof(clm_position_i8) == 2, "sizeof(clm_position_i8) == 2");
    CLM_ASSERT(sizeof(clm_position_3d_i8) == 3, "sizeof(clm_position_3d_i8) == 3");
    CLM_ASSERT(sizeof(clm_size_i8) == 2, "sizeof(clm_size_i8) == 2");
    CLM_ASSERT(sizeof(clm_size_3d_i8) == 3, "sizeof(clm_size_3d_i8) == 3");
    CLM_ASSERT(sizeof(clm_block_i8) == 4, "sizeof(clm_block_i8) == 4");
    CLM_ASSERT(sizeof(clm_block_3d_i8) == 6, "sizeof(clm_block_3d_i8) == 6");

    CLM_ASSERT(sizeof(clm_position_u8) == 2, "sizeof(clm_position_u8) == 2");
    CLM_ASSERT(sizeof(clm_position_3d_u8) == 3, "sizeof(clm_position_3d_u8) == 3");
    CLM_ASSERT(sizeof(clm_size_u8) == 2, "sizeof(clm_size_u8) == 2");
    CLM_ASSERT(sizeof(clm_size_3d_u8) == 3, "sizeof(clm_size_3d_u8) == 3");
    CLM_ASSERT(sizeof(clm_block_u8) == 4, "sizeof(clm_block_u8) == 4");
    CLM_ASSERT(sizeof(clm_block_3d_u8) == 6, "sizeof(clm_block_3d_u8) == 6");

    CLM_ASSERT(sizeof(clm_position_i16) == 4, "sizeof(clm_position_i16) == 4");
    CLM_ASSERT(sizeof(clm_position_3d_i16) == 6, "sizeof(clm_position_3d_i16) == 6");
    CLM_ASSERT(sizeof(clm_size_i16) == 4, "sizeof(clm_size_i16) == 4");
    CLM_ASSERT(sizeof(clm_size_3d_i16) == 6, "sizeof(clm_size_3d_i16) == 6");
    CLM_ASSERT(sizeof(clm_block_i16) == 8, "sizeof(clm_block_i16) == 8");
    CLM_ASSERT(sizeof(clm_block_3d_i16) == 12, "sizeof(clm_block_3d_i16) == 12");

    CLM_ASSERT(sizeof(clm_position_u16) == 4, "sizeof(clm_position_u16) == 4");
    CLM_ASSERT(sizeof(clm_position_3d_u16) == 6, "sizeof(clm_position_3d_u16) == 6");
    CLM_ASSERT(sizeof(clm_size_u16) == 4, "sizeof(clm_size_u16) == 4");
    CLM_ASSERT(sizeof(clm_size_3d_u16) == 6, "sizeof(clm_size_3d_u16) == 6");
    CLM_ASSERT(sizeof(clm_block_u16) == 8, "sizeof(clm_block_u16) == 8");
    CLM_ASSERT(sizeof(clm_block_3d_u16) == 12, "sizeof(clm_block_3d_u16) == 12");

    CLM_ASSERT(sizeof(clm_position_i32) == 8, "sizeof(clm_position_i32) == 8");
    CLM_ASSERT(sizeof(clm_position_3d_i32) == 12, "sizeof(clm_position_3d_i32) == 12");
    CLM_ASSERT(sizeof(clm_size_i32) == 8, "sizeof(clm_size_i32) == 8");
    CLM_ASSERT(sizeof(clm_size_3d_i32) == 12, "sizeof(clm_size_3d_i32) == 12");
    CLM_ASSERT(sizeof(clm_block_i32) == 16, "sizeof(clm_block_i32) == 16");
    CLM_ASSERT(sizeof(clm_block_3d_i32) == 24, "sizeof(clm_block_3d_i32) == 24");

    CLM_ASSERT(sizeof(clm_position_u32) == 8, "sizeof(clm_position_u32) == 8");
    CLM_ASSERT(sizeof(clm_position_3d_u32) == 12, "sizeof(clm_position_3d_u32) == 12");
    CLM_ASSERT(sizeof(clm_size_u32) == 8, "sizeof(clm_size_u32) == 8");
    CLM_ASSERT(sizeof(clm_size_3d_u32) == 12, "sizeof(clm_size_3d_u32) == 12");
    CLM_ASSERT(sizeof(clm_block_u32) == 16, "sizeof(clm_block_u32) == 16");
    CLM_ASSERT(sizeof(clm_block_3d_u32) == 24, "sizeof(clm_block_3d_u32) == 24");

    CLM_ASSERT(sizeof(clm_position_i64) == 16, "sizeof(clm_position_i64) == 16");
    CLM_ASSERT(sizeof(clm_position_3d_i64) == 24, "sizeof(clm_position_3d_i64) == 24");
    CLM_ASSERT(sizeof(clm_size_i64) == 16, "sizeof(clm_size_i64) == 16");
    CLM_ASSERT(sizeof(clm_size_3d_i64) == 24, "sizeof(clm_size_3d_i64) == 24");
    CLM_ASSERT(sizeof(clm_block_i64) == 32, "sizeof(clm_block_i64) == 32");
    CLM_ASSERT(sizeof(clm_block_3d_i64) == 48, "sizeof(clm_block_3d_i64) == 48");

    CLM_ASSERT(sizeof(clm_position_u64) == 16, "sizeof(clm_position_u64) == 16");
    CLM_ASSERT(sizeof(clm_position_3d_u64) == 24, "sizeof(clm_position_3d_u64) == 24");
    CLM_ASSERT(sizeof(clm_size_u64) == 16, "sizeof(clm_size_u64) == 16");
    CLM_ASSERT(sizeof(clm_size_3d_u64) == 24, "sizeof(clm_size_3d_u64) == 24");
    CLM_ASSERT(sizeof(clm_block_u64) == 32, "sizeof(clm_block_u64) == 32");
    CLM_ASSERT(sizeof(clm_block_3d_u64) == 48, "sizeof(clm_block_3d_u64) == 48");

    CLM_ASSERT(sizeof(clm_position_f32) == 8, "sizeof(clm_position_f32) == 8");
    CLM_ASSERT(sizeof(clm_position_3d_f32) == 12, "sizeof(clm_position_3d_f32) == 12");
    CLM_ASSERT(sizeof(clm_size_f32) == 8, "sizeof(clm_size_f32) == 8");
    CLM_ASSERT(sizeof(clm_size_3d_f32) == 12, "sizeof(clm_size_3d_f32) == 12");
    CLM_ASSERT(sizeof(clm_block_f32) == 16, "sizeof(clm_block_f32) == 16");
    CLM_ASSERT(sizeof(clm_block_3d_f32) == 24, "sizeof(clm_block_3d_f32) == 24");

    CLM_ASSERT(sizeof(clm_position_f64) == 16, "sizeof(clm_position_f64) == 16");
    CLM_ASSERT(sizeof(clm_position_3d_f64) == 24, "sizeof(clm_position_3d_f64) == 24");
    CLM_ASSERT(sizeof(clm_size_f64) == 16, "sizeof(clm_size_f64) == 16");
    CLM_ASSERT(sizeof(clm_size_3d_f64) == 24, "sizeof(clm_size_3d_f64) == 24");
    CLM_ASSERT(sizeof(clm_block_f64) == 32, "sizeof(clm_block_f64) == 32");
    CLM_ASSERT(sizeof(clm_block_3d_f64) == 48, "sizeof(clm_block_3d_f64) == 48");

    clm_test_summary(result.passed, result.failed);

    return result.failed;
}


static int test_clm_base(void)
{
    return clm_base_tests();
}

