//! Tests for the CLM base geometry types.

const base = @import("clm_base");
const test_report = @import("test.zig");

fn assert(
    runner: *test_report.TestRunner,
    result: *test_report.clm_test_result,
    condition: bool,
    description: []const u8,
) void {
    test_report.clm_test_assert(runner, result, condition, description);
}

pub fn clm_base_tests(runner: *test_report.TestRunner) void {
    var result: test_report.clm_test_result = .{};
    test_report.clm_test_begin(runner, "CLM - Base");

    var position: base.clm_position = .{ .x = 10, .y = 20 };
    assert(runner, &result, position.x == 10 and position.y == 20, "position.x == 10 && position.y == 20");
    position.x = 30;
    assert(runner, &result, position.x == 30, "position.x == 30");

    const position_3d: base.clm_position_3d = .{ .x = 10, .y = 20, .z = 30 };
    assert(runner, &result, position_3d.x == 10 and position_3d.y == 20 and position_3d.z == 30, "position_3d fields");

    var size: base.clm_size = .{ .width = 100, .height = 200 };
    assert(runner, &result, size.width == 100 and size.height == 200, "size.width == 100 && size.height == 200");
    size.width = 300;
    assert(runner, &result, size.width == 300, "size.width == 300");

    const size_3d: base.clm_size_3d = .{ .width = 100, .height = 200, .depth = 300 };
    assert(runner, &result, size_3d.width == 100 and size_3d.height == 200 and size_3d.depth == 300, "size_3d fields");

    const block: base.clm_block = .{ .x = 10, .y = 20, .width = 100, .height = 200 };
    assert(runner, &result, block.x == 10 and block.y == 20 and block.width == 100 and block.height == 200, "block fields");

    const block_3d: base.clm_block_3d = .{ .x = 10, .y = 20, .z = 30, .width = 100, .height = 200, .depth = 300 };
    assert(runner, &result, block_3d.x == 10 and block_3d.y == 20 and block_3d.z == 30, "block_3d position fields");
    assert(runner, &result, block_3d.width == 100 and block_3d.height == 200 and block_3d.depth == 300, "block_3d extent fields");

    const f_position: base.clm_position_f32 = .{ .x = 10.5, .y = 20.25 };
    assert(runner, &result, f_position.x == 10.5 and f_position.y == 20.25, "clm_position_f32 fields");
    const f_position_3d: base.clm_position_3d_f32 = .{ .x = 10.5, .y = 20.25, .z = 30.75 };
    assert(runner, &result, f_position_3d.x == 10.5 and f_position_3d.y == 20.25 and f_position_3d.z == 30.75, "clm_position_3d_f32 fields");
    const f_size: base.clm_size_f32 = .{ .width = 100.5, .height = 200.25 };
    assert(runner, &result, f_size.width == 100.5 and f_size.height == 200.25, "clm_size_f32 fields");
    const f_size_3d: base.clm_size_3d_f32 = .{ .width = 100.5, .height = 200.25, .depth = 300.75 };
    assert(runner, &result, f_size_3d.width == 100.5 and f_size_3d.height == 200.25 and f_size_3d.depth == 300.75, "clm_size_3d_f32 fields");
    const f_block: base.clm_block_f32 = .{ .x = 10.5, .y = 20.25, .width = 100.5, .height = 200.25 };
    assert(runner, &result, f_block.x == 10.5 and f_block.y == 20.25 and f_block.width == 100.5 and f_block.height == 200.25, "clm_block_f32 fields");

    const f_block_3d: base.clm_block_3d_f32 = .{
        .x = 10.5,
        .y = 20.25,
        .z = 30.75,
        .width = 100.5,
        .height = 200.25,
        .depth = 300.75,
    };
    assert(runner, &result, f_block_3d.x == 10.5, "clm_block_3d_f32.x == 10.5");
    assert(runner, &result, f_block_3d.y == 20.25, "clm_block_3d_f32.y == 20.25");
    assert(runner, &result, f_block_3d.z == 30.75, "clm_block_3d_f32.z == 30.75");
    assert(runner, &result, f_block_3d.width == 100.5, "clm_block_3d_f32.width == 100.5");
    assert(runner, &result, f_block_3d.height == 200.25, "clm_block_3d_f32.height == 200.25");
    assert(runner, &result, f_block_3d.depth == 300.75, "clm_block_3d_f32.depth == 300.75");

    assert(runner, &result, @sizeOf(base.clm_position_i8) == 2, "@sizeOf(clm_position_i8) == 2");
    assert(runner, &result, @sizeOf(base.clm_position_3d_i8) == 3, "@sizeOf(clm_position_3d_i8) == 3");
    assert(runner, &result, @sizeOf(base.clm_size_i8) == 2, "@sizeOf(clm_size_i8) == 2");
    assert(runner, &result, @sizeOf(base.clm_size_3d_i8) == 3, "@sizeOf(clm_size_3d_i8) == 3");
    assert(runner, &result, @sizeOf(base.clm_block_i8) == 4, "@sizeOf(clm_block_i8) == 4");
    assert(runner, &result, @sizeOf(base.clm_block_3d_i8) == 6, "@sizeOf(clm_block_3d_i8) == 6");

    assert(runner, &result, @sizeOf(base.clm_position_u8) == 2, "@sizeOf(clm_position_u8) == 2");
    assert(runner, &result, @sizeOf(base.clm_position_3d_u8) == 3, "@sizeOf(clm_position_3d_u8) == 3");
    assert(runner, &result, @sizeOf(base.clm_size_u8) == 2, "@sizeOf(clm_size_u8) == 2");
    assert(runner, &result, @sizeOf(base.clm_size_3d_u8) == 3, "@sizeOf(clm_size_3d_u8) == 3");
    assert(runner, &result, @sizeOf(base.clm_block_u8) == 4, "@sizeOf(clm_block_u8) == 4");
    assert(runner, &result, @sizeOf(base.clm_block_3d_u8) == 6, "@sizeOf(clm_block_3d_u8) == 6");

    assert(runner, &result, @sizeOf(base.clm_position_i16) == 4, "@sizeOf(clm_position_i16) == 4");
    assert(runner, &result, @sizeOf(base.clm_position_3d_i16) == 6, "@sizeOf(clm_position_3d_i16) == 6");
    assert(runner, &result, @sizeOf(base.clm_size_i16) == 4, "@sizeOf(clm_size_i16) == 4");
    assert(runner, &result, @sizeOf(base.clm_size_3d_i16) == 6, "@sizeOf(clm_size_3d_i16) == 6");
    assert(runner, &result, @sizeOf(base.clm_block_i16) == 8, "@sizeOf(clm_block_i16) == 8");
    assert(runner, &result, @sizeOf(base.clm_block_3d_i16) == 12, "@sizeOf(clm_block_3d_i16) == 12");

    assert(runner, &result, @sizeOf(base.clm_position_u16) == 4, "@sizeOf(clm_position_u16) == 4");
    assert(runner, &result, @sizeOf(base.clm_position_3d_u16) == 6, "@sizeOf(clm_position_3d_u16) == 6");
    assert(runner, &result, @sizeOf(base.clm_size_u16) == 4, "@sizeOf(clm_size_u16) == 4");
    assert(runner, &result, @sizeOf(base.clm_size_3d_u16) == 6, "@sizeOf(clm_size_3d_u16) == 6");
    assert(runner, &result, @sizeOf(base.clm_block_u16) == 8, "@sizeOf(clm_block_u16) == 8");
    assert(runner, &result, @sizeOf(base.clm_block_3d_u16) == 12, "@sizeOf(clm_block_3d_u16) == 12");

    assert(runner, &result, @sizeOf(base.clm_position_i32) == 8, "@sizeOf(clm_position_i32) == 8");
    assert(runner, &result, @sizeOf(base.clm_position_3d_i32) == 12, "@sizeOf(clm_position_3d_i32) == 12");
    assert(runner, &result, @sizeOf(base.clm_size_i32) == 8, "@sizeOf(clm_size_i32) == 8");
    assert(runner, &result, @sizeOf(base.clm_size_3d_i32) == 12, "@sizeOf(clm_size_3d_i32) == 12");
    assert(runner, &result, @sizeOf(base.clm_block_i32) == 16, "@sizeOf(clm_block_i32) == 16");
    assert(runner, &result, @sizeOf(base.clm_block_3d_i32) == 24, "@sizeOf(clm_block_3d_i32) == 24");

    assert(runner, &result, @sizeOf(base.clm_position_u32) == 8, "@sizeOf(clm_position_u32) == 8");
    assert(runner, &result, @sizeOf(base.clm_position_3d_u32) == 12, "@sizeOf(clm_position_3d_u32) == 12");
    assert(runner, &result, @sizeOf(base.clm_size_u32) == 8, "@sizeOf(clm_size_u32) == 8");
    assert(runner, &result, @sizeOf(base.clm_size_3d_u32) == 12, "@sizeOf(clm_size_3d_u32) == 12");
    assert(runner, &result, @sizeOf(base.clm_block_u32) == 16, "@sizeOf(clm_block_u32) == 16");
    assert(runner, &result, @sizeOf(base.clm_block_3d_u32) == 24, "@sizeOf(clm_block_3d_u32) == 24");

    assert(runner, &result, @sizeOf(base.clm_position_i64) == 16, "@sizeOf(clm_position_i64) == 16");
    assert(runner, &result, @sizeOf(base.clm_position_3d_i64) == 24, "@sizeOf(clm_position_3d_i64) == 24");
    assert(runner, &result, @sizeOf(base.clm_size_i64) == 16, "@sizeOf(clm_size_i64) == 16");
    assert(runner, &result, @sizeOf(base.clm_size_3d_i64) == 24, "@sizeOf(clm_size_3d_i64) == 24");
    assert(runner, &result, @sizeOf(base.clm_block_i64) == 32, "@sizeOf(clm_block_i64) == 32");
    assert(runner, &result, @sizeOf(base.clm_block_3d_i64) == 48, "@sizeOf(clm_block_3d_i64) == 48");

    assert(runner, &result, @sizeOf(base.clm_position_u64) == 16, "@sizeOf(clm_position_u64) == 16");
    assert(runner, &result, @sizeOf(base.clm_position_3d_u64) == 24, "@sizeOf(clm_position_3d_u64) == 24");
    assert(runner, &result, @sizeOf(base.clm_size_u64) == 16, "@sizeOf(clm_size_u64) == 16");
    assert(runner, &result, @sizeOf(base.clm_size_3d_u64) == 24, "@sizeOf(clm_size_3d_u64) == 24");
    assert(runner, &result, @sizeOf(base.clm_block_u64) == 32, "@sizeOf(clm_block_u64) == 32");
    assert(runner, &result, @sizeOf(base.clm_block_3d_u64) == 48, "@sizeOf(clm_block_3d_u64) == 48");

    assert(runner, &result, @sizeOf(base.clm_position_f32) == 8, "@sizeOf(clm_position_f32) == 8");
    assert(runner, &result, @sizeOf(base.clm_position_3d_f32) == 12, "@sizeOf(clm_position_3d_f32) == 12");
    assert(runner, &result, @sizeOf(base.clm_size_f32) == 8, "@sizeOf(clm_size_f32) == 8");
    assert(runner, &result, @sizeOf(base.clm_size_3d_f32) == 12, "@sizeOf(clm_size_3d_f32) == 12");
    assert(runner, &result, @sizeOf(base.clm_block_f32) == 16, "@sizeOf(clm_block_f32) == 16");
    assert(runner, &result, @sizeOf(base.clm_block_3d_f32) == 24, "@sizeOf(clm_block_3d_f32) == 24");

    assert(runner, &result, @sizeOf(base.clm_position_f64) == 16, "@sizeOf(clm_position_f64) == 16");
    assert(runner, &result, @sizeOf(base.clm_position_3d_f64) == 24, "@sizeOf(clm_position_3d_f64) == 24");
    assert(runner, &result, @sizeOf(base.clm_size_f64) == 16, "@sizeOf(clm_size_f64) == 16");
    assert(runner, &result, @sizeOf(base.clm_size_3d_f64) == 24, "@sizeOf(clm_size_3d_f64) == 24");
    assert(runner, &result, @sizeOf(base.clm_block_f64) == 32, "@sizeOf(clm_block_f64) == 32");
    assert(runner, &result, @sizeOf(base.clm_block_3d_f64) == 48, "@sizeOf(clm_block_3d_f64) == 48");

    test_report.clm_test_summary(runner, result);
}
