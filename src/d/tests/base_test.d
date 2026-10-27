module tests.base_test;

import std.stdio;
import clm;
import tests.test;

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

__gshared int clmBaseTestsPassed = 0;
__gshared int clmBaseTestsFailed = 0;

int clmBaseTests()
{
    ClmTestResult result;
    clmTestBegin("CLM - Base");

    clmAssert(result, clm_base_make_id(cast(ulong)0) == cast(IdKey)0, "creates the minimum unsigned ID");
    clmAssert(result, clm_base_make_id(cast(ulong)1001) == cast(IdKey)1001, "creates an unsigned ID");
    clmAssert(result, clm_base_make_id(cast(long)1001) == cast(IdKey)1001, "creates a signed ID");
    clmAssert(result, clm_base_make_id(cast(ulong)ID_KEY_MAX) == ID_KEY_MAX, "creates the maximum ID");

    // Position position = { x: 10, y: 20 };
    // clmAssert(result, position.x == 10 && position.y == 20, 
    //     "position.x == 10 && position.y == 20");

    // position.x = 30;
    // clmAssert(result, position.x == 30, "position.x == 30");

    // const Position3D position3D = { x: 10, y: 20, z: 30 };
    // clmAssert(result, position3D.x == 10 && position3D.y == 20 && position3D.z == 30, 
    //     "position3D.x == 10 && position3D.y == 20 && position3D.z == 30");

    // Size size = { width: 100, height: 200 };
    // clmAssert(result, size.width == 100 && size.height == 200, 
    //     "size.width == 100 && size.height == 200");

    // size.width = 300;
    // clmAssert(result, size.width == 300, "size.width == 300");

    // const Size3D size3D = { width: 100, height: 200, depth: 300 };
    // clmAssert(result, size3D.width == 100 && size3D.height == 200 && size3D.depth == 300, 
    //     "size3D.width == 100 && size3D.height == 200 && size3D.depth == 300");

    // const Block block = { x: 10, y: 20, width: 100, height: 200 };
    // clmAssert(result, block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200, 
    //     "block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200");

    // const Block3D block3D = { x: 10, y: 20, z: 30, width: 100, height: 200, depth: 300 };
    // clmAssert(result, block3D.x == 10 && block3D.y == 20 && block3D.z == 30, 
    //     "block3D.x == 10 && block3D.y == 20 && block3D.z == 30");
    // clmAssert(result, block3D.width == 100 && block3D.height == 200 && block3D.depth == 300, 
    //     "block3D.width == 100 && block3D.height == 200 && block3D.depth == 300");

    // const FPosition fPosition = { x: 10.5f, y: 20.25f };
    // clmAssert(result, fPosition.x == 10.5f && fPosition.y == 20.25f, 
    //     "fPosition.x == 10.5f && fPosition.y == 20.25f");

    // const FPosition3D fPosition3D = { x: 10.5f, y: 20.25f, z: 30.75f };
    // clmAssert(result, fPosition3D.x == 10.5f && fPosition3D.y == 20.25f && fPosition3D.z == 30.75f, 
    //     "fPosition3D.x == 10.5f && fPosition3D.y == 20.25f && fPosition3D.z == 30.75f");

    // const FSize fSize = { width: 100.5f, height: 200.25f };
    // clmAssert(result, fSize.width == 100.5f && fSize.height == 200.25f, 
    //     "fSize.width == 100.5f && fSize.height == 200.25f");

    // const FSize3D fSize3D = { width: 100.5f, height: 200.25f, depth: 300.75f };
    // clmAssert(result, fSize3D.width == 100.5f && fSize3D.height == 200.25f && fSize3D.depth == 300.75f, 
    //     "fSize3D.width == 100.5f && fSize3D.height == 200.25f && fSize3D.depth == 300.75f");

    // const FBlock fBlock = { x: 10.5f, y: 20.25f, width: 100.5f, height: 200.25f };
    // clmAssert(result, fBlock.x == 10.5f && fBlock.y == 20.25f && fBlock.width == 100.5f && fBlock.height == 200.25f, 
    //     "fBlock.x == 10.5f && fBlock.y == 20.25f && fBlock.width == 100.5f && fBlock.height == 200.25f");

    // const FBlock3D fBlock3D = {
    //     x: 10.5f, y: 20.25f, z: 30.75f,
    //     width: 100.5f, height: 200.25f, depth: 300.75f
    // };

    // clmAssert(result, fBlock3D.x == 10.5f, "fBlock3D.x == 10.5f");
    // clmAssert(result, fBlock3D.y == 20.25f, "fBlock3D.y == 20.25f");
    // clmAssert(result, fBlock3D.z == 30.75f, "fBlock3D.z == 30.75f");
    // clmAssert(result, fBlock3D.width == 100.5f, "fBlock3D.width == 100.5f");
    // clmAssert(result, fBlock3D.height == 200.25f, "fBlock3D.height == 200.25f");
    // clmAssert(result, fBlock3D.depth == 300.75f, "fBlock3D.depth == 300.75f");

    // clmAssert(result, Position.sizeof == 8, "Position.sizeof == 8");
    // clmAssert(result, Position3D.sizeof == 12, "Position3D.sizeof == 12");

    // clmAssert(result, Size.sizeof == 8, "Size.sizeof == 8");
    // clmAssert(result, Size3D.sizeof == 12, "Size3D.sizeof == 12");

    auto position = clm_position(10, 20);
    auto position3D = clm_position_3d(10, 20, 30);
    auto size = clm_size(100, 200);
    auto size3D = clm_size_3d(100, 200, 300);
    auto block = clm_block(10, 20, 100, 200);
    auto block3D = clm_block_3d(10, 20, 30, 100, 200, 300);
    clmAssert(result, position.x == 10 && position.y == 20, "constructs 2D position");
    clmAssert(result, position3D.x == 10 && position3D.y == 20 && position3D.z == 30, "constructs 3D position");
    clmAssert(result, size.width == 100 && size.height == 200, "constructs 2D size");
    clmAssert(result, size3D.width == 100 && size3D.height == 200 && size3D.depth == 300, "constructs 3D size");
    clmAssert(result, block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200, "constructs 2D block");
    clmAssert(result, block3D.z == 30 && block3D.depth == 300, "constructs 3D block");
    clmAssert(result, clm_position_i8.sizeof == 2 && clm_position_3d_i8.sizeof == 3, "preserves packed position widths");
    clmAssert(result, clm_size_i16.sizeof == 4 && clm_size_3d_i16.sizeof == 6, "preserves short size widths");
    clmAssert(result, clm_block_i32.sizeof == 16 && clm_block_3d_i32.sizeof == 24, "preserves integer block widths");
    clmAssert(result, clm_position_f32.sizeof == 8 && clm_position_3d_f32.sizeof == 12, "preserves float position widths");

    clmTestSummary(result.passed, result.failed);

    return result.failed;
}
