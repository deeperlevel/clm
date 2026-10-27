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
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#pragma once

#include "test.hpp"
#include "../clm/base.hpp"
#include <iostream>

static int clm_base_tests_passed = 0;
static int clm_base_tests_failed = 0;

#include <type_traits>

using namespace clm::base;

static int test_clm_base()
{
    clm_test_result result = {0};

    constexpr position pos{.x = 10, .y = 20};
    CLM_ASSERT(pos.x == 10 && pos.y == 20);

    constexpr position_3d pos3d{.x = 10, .y = 20, .z = 30};
    CLM_ASSERT(pos3d.x == 10 && pos3d.y == 20 && pos3d.z == 30);

    constexpr size sz{.width = 100, .height = 200};
    CLM_ASSERT(sz.width == 100 && sz.height == 200);

    constexpr size_3d sz3d{.width = 100, .height = 200, .depth = 300};
    CLM_ASSERT(sz3d.width == 100 && sz3d.height == 200 && sz3d.depth == 300);

    constexpr block rect{.x = 10, .y = 20, .width = 100, .height = 200};
    CLM_ASSERT(rect.x == 10 && rect.y == 20 && rect.width == 100 && rect.height == 200);

    constexpr block_3d box{.x = 10, .y = 20, .z = 30, .width = 100, .height = 200, .depth = 300};
    CLM_ASSERT(box.x == 10 && box.y == 20 && box.z == 30);
    CLM_ASSERT(box.width == 100 && box.height == 200 && box.depth == 300);

    // 32-bit Float Geometries Testing (Mapped to f32 namespace)
    constexpr f32::position fpos{.x = 10.5f, .y = 20.25f};
    CLM_ASSERT(fpos.x == 10.5f && fpos.y == 20.25f);

    constexpr f32::position_3d fpos3d{.x = 10.5f, .y = 20.25f, .z = 30.75f};
    CLM_ASSERT(fpos3d.x == 10.5f && fpos3d.y == 20.25f && fpos3d.z == 30.75f);

    constexpr f32::size fsz{.width = 100.5f, .height = 200.25f};
    CLM_ASSERT(fsz.width == 100.5f && fsz.height == 200.25f);

    constexpr f32::size_3d fsz3d{.width = 100.5f, .height = 200.25f, .depth = 300.75f};
    CLM_ASSERT(fsz3d.width == 100.5f && fsz3d.height == 200.25f && fsz3d.depth == 300.75f);

    constexpr f32::block frect{.x = 10.5f, .y = 20.25f, .width = 100.5f, .height = 200.25f};
    CLM_ASSERT(frect.x == 10.5f && frect.y == 20.25f && frect.width == 100.5f && frect.height == 200.25f);

    constexpr f32::block_3d fbox{.x = 10.5f, .y = 20.25f, .z = 30.75f, .width = 100.5f, .height = 200.25f, .depth = 300.75f};
    CLM_ASSERT(fbox.x == 10.5f && fbox.y == 20.25f && fbox.z == 30.75f);
    CLM_ASSERT(fbox.width == 100.5f && fbox.height == 200.25f && fbox.depth == 300.75f);

    // Exact-Width Hardware Byte-Size Boundary Asserts
    CLM_ASSERT(sizeof(i32::position) == 8 && sizeof(i32::position_3d) == 12);
    CLM_ASSERT(sizeof(i32::size) == 8 && sizeof(i32::size_3d) == 12);
    CLM_ASSERT(sizeof(i32::block) == 16 && sizeof(i32::block_3d) == 24);

    CLM_ASSERT(sizeof(f32::position) == 8 && sizeof(f32::position_3d) == 12);
    CLM_ASSERT(sizeof(f32::size) == 8 && sizeof(f32::size_3d) == 12);
    CLM_ASSERT(sizeof(f32::block) == 16 && sizeof(f32::block_3d) == 24);

    // POD Component Safety Pipeline Verification for clm_ecs & Cache Locality
    #define CLM_CHECK_POD(T) CLM_ASSERT(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>)
    CLM_CHECK_POD(i8::position);
    CLM_CHECK_POD(i8::position_3d);
    CLM_CHECK_POD(i8::size);
    CLM_CHECK_POD(i8::size_3d);
    CLM_CHECK_POD(i8::block);
    CLM_CHECK_POD(i8::block_3d);

    CLM_CHECK_POD(u8::position);
    CLM_CHECK_POD(u8::position_3d);
    CLM_CHECK_POD(u8::size);
    CLM_CHECK_POD(u8::size_3d);
    CLM_CHECK_POD(u8::block);
    CLM_CHECK_POD(u8::block_3d);

    CLM_CHECK_POD(i16::position);
    CLM_CHECK_POD(i16::position_3d);
    CLM_CHECK_POD(i16::size);
    CLM_CHECK_POD(i16::size_3d);
    CLM_CHECK_POD(i16::block);
    CLM_CHECK_POD(i16::block_3d);

    CLM_CHECK_POD(u16::position);
    CLM_CHECK_POD(u16::position_3d);
    CLM_CHECK_POD(u16::size);
    CLM_CHECK_POD(u16::size_3d);
    CLM_CHECK_POD(u16::block);
    CLM_CHECK_POD(u16::block_3d);

    CLM_CHECK_POD(i32::position);
    CLM_CHECK_POD(i32::position_3d);
    CLM_CHECK_POD(i32::size);
    CLM_CHECK_POD(i32::size_3d);
    CLM_CHECK_POD(i32::block);
    CLM_CHECK_POD(i32::block_3d);

    CLM_CHECK_POD(u32::position);
    CLM_CHECK_POD(u32::position_3d);
    CLM_CHECK_POD(u32::size);
    CLM_CHECK_POD(u32::size_3d);
    CLM_CHECK_POD(u32::block);
    CLM_CHECK_POD(u32::block_3d);

    CLM_CHECK_POD(i64::position);
    CLM_CHECK_POD(i64::position_3d);
    CLM_CHECK_POD(i64::size);
    CLM_CHECK_POD(i64::size_3d);
    CLM_CHECK_POD(i64::block);
    CLM_CHECK_POD(i64::block_3d);

    CLM_CHECK_POD(u64::position);
    CLM_CHECK_POD(u64::position_3d);
    CLM_CHECK_POD(u64::size);
    CLM_CHECK_POD(u64::size_3d);
    CLM_CHECK_POD(u64::block);
    CLM_CHECK_POD(u64::block_3d);

    CLM_CHECK_POD(f32::position);
    CLM_CHECK_POD(f32::position_3d);
    CLM_CHECK_POD(f32::size);
    CLM_CHECK_POD(f32::size_3d);
    CLM_CHECK_POD(f32::block);
    CLM_CHECK_POD(f32::block_3d);

    CLM_CHECK_POD(f64::position);
    CLM_CHECK_POD(f64::position_3d);
    CLM_CHECK_POD(f64::size);
    CLM_CHECK_POD(f64::size_3d);
    CLM_CHECK_POD(f64::block);
    CLM_CHECK_POD(f64::block_3d);

    #undef CLM_CHECK_POD

    clm_test_summary(result.passed, result.failed);

    return result.failed;
}
