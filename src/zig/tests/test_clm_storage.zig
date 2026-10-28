//! Tests for CLM grid and atlas storage.

const std = @import("std");
const storage = @import("clm_storage");
const bpa = @import("clm_bpa");
const test_report = @import("test.zig");

fn assert(
    runner: *test_report.TestRunner,
    result: *test_report.clm_test_result,
    condition: bool,
    description: []const u8,
) void {
    test_report.clm_test_assert(runner, result, condition, description);
}

pub fn clm_storage_tests(runner: *test_report.TestRunner) void {
    var result: test_report.clm_test_result = .{};
    test_report.clm_test_begin(runner, "CLM - Storage");

    var value: i32 = 0;
    var replacement: i32 = 72;
    assert(runner, &result, storage.clm_storage_new_2d(100, bpa.CLM_BPA_TYPE_INT, 0, 4, 3) == bpa.CLM_BPA_SUCCESS, "create a 2D integer grid");
    assert(runner, &result, storage.clm_storage_new_2d(100, bpa.CLM_BPA_TYPE_INT, 0, 4, 3) == bpa.CLM_BPA_FAILURE, "reject duplicate storage ID");
    assert(runner, &result, storage.clm_storage_get_to(100, 0, 2, 3, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and value == 0, "new grid values are zero-initialized");
    assert(runner, &result, storage.clm_storage_set_from(100, 0, 1, 2, std.mem.asBytes(&replacement)) == bpa.CLM_BPA_SET_SUCCESS, "write a grid value");
    assert(runner, &result, storage.clm_storage_get_to(100, 0, 1, 2, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and value == 72, "read a grid value");
    assert(runner, &result, storage.clm_storage_get_to(100, 1, 1, 2, std.mem.asBytes(&value)) == bpa.CLM_BPA_FAILURE, "reject an invalid layer");
    assert(runner, &result, storage.clm_storage_get_to(100, 0, 3, 2, std.mem.asBytes(&value)) == bpa.CLM_BPA_FAILURE, "reject an invalid row");
    assert(runner, &result, storage.clm_storage_set_from(100, 0, 1, 2, std.mem.asBytes(&replacement)[0..2]) == bpa.CLM_BPA_FAILURE, "reject incorrectly sized elements");

    var info: storage.clm_storage_info = .{};
    assert(runner, &result, storage.clm_storage_get_info(100, &info) == bpa.CLM_BPA_GET_SUCCESS and
        info.columns == 4 and info.rows == 3 and info.layers == 1, "retrieve grid metadata");
    assert(runner, &result, storage.clm_storage_slice(100, 0, 1, 1, 1, 2, 2, 101) == bpa.CLM_BPA_SUCCESS, "slice a rectangular region");
    assert(runner, &result, storage.clm_storage_get_to(101, 0, 0, 1, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and value == 72, "read copied slice data");
    replacement = 21;
    assert(runner, &result, storage.clm_storage_set_from(101, 0, 0, 1, std.mem.asBytes(&replacement)) == bpa.CLM_BPA_SET_SUCCESS and
        storage.clm_storage_get_to(100, 0, 1, 2, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and value == 72, "slice storage is independent");
    assert(runner, &result, storage.clm_storage_slice(100, 0, 2, 2, 1, 2, 2, 102) == bpa.CLM_BPA_FAILURE, "reject an out-of-bounds slice");
    assert(runner, &result, storage.clm_storage_delete(101) == bpa.CLM_BPA_SUCCESS and
        storage.clm_storage_delete(100) == bpa.CLM_BPA_SUCCESS and
        storage.clm_storage_get_info(100, &info) == bpa.CLM_BPA_FAILURE, "delete storage resources");

    assert(runner, &result, storage.clm_storage_new(110, bpa.CLM_BPA_TYPE_BYTE, 0, 2, 3, 2) == bpa.CLM_BPA_SUCCESS, "create layered 3D storage");
    var byte_value: u8 = 9;
    var byte_result: u8 = 0;
    assert(runner, &result, storage.clm_storage_set_from(110, 1, 2, 1, std.mem.asBytes(&byte_value)) == bpa.CLM_BPA_SET_SUCCESS and
        storage.clm_storage_get_to(110, 0, 2, 1, std.mem.asBytes(&byte_result)) == bpa.CLM_BPA_GET_SUCCESS and byte_result == 0, "layers store independent element values");
    assert(runner, &result, storage.clm_storage_slice(110, 1, 1, 0, 1, 2, 2, 111) == bpa.CLM_BPA_SUCCESS, "slice a 3D grid");
    assert(runner, &result, storage.clm_storage_get_to(111, 0, 1, 1, std.mem.asBytes(&byte_result)) == bpa.CLM_BPA_GET_SUCCESS and byte_result == 9, "slice preserves layered data");
    assert(runner, &result, storage.clm_storage_delete(111) == bpa.CLM_BPA_SUCCESS and storage.clm_storage_delete(110) == bpa.CLM_BPA_SUCCESS, "delete layered grids");

    assert(runner, &result, storage.clm_storage_atlas_new(120, bpa.CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2) == bpa.CLM_BPA_SUCCESS, "create a 2D atlas");
    const frame = [_]u8{ 1, 2, 3, 4 };
    var frame_result = [_]u8{ 0, 0, 0, 0 };
    assert(runner, &result, storage.clm_storage_atlas_set_frame_from(120, 0, 1, 1, &frame) == bpa.CLM_BPA_SET_SUCCESS and
        storage.clm_storage_atlas_get_frame_to(120, 0, 1, 1, &frame_result) == bpa.CLM_BPA_GET_SUCCESS and
        std.mem.eql(u8, &frame, &frame_result), "write and read a packed atlas frame");
    assert(runner, &result, storage.clm_storage_atlas_get_frame_to(120, 0, 0, 0, &frame_result) == bpa.CLM_BPA_GET_SUCCESS and
        frame_result[0] == 0 and frame_result[3] == 0, "unwritten atlas frames remain zero");
    assert(runner, &result, storage.clm_storage_atlas_get_frame_to(120, 0, 2, 0, &frame_result) == bpa.CLM_BPA_FAILURE, "reject an invalid frame row");
    assert(runner, &result, storage.clm_storage_atlas_get_frame_to(120, 0, 1, 1, frame_result[0..3]) == bpa.CLM_BPA_FAILURE, "reject an undersized frame buffer");
    assert(runner, &result, storage.clm_storage_atlas_slice_frame(120, 0, 1, 1, 121) == bpa.CLM_BPA_SUCCESS and
        storage.clm_storage_get_to(121, 0, 1, 1, std.mem.asBytes(&byte_result)) == bpa.CLM_BPA_GET_SUCCESS and
        byte_result == 4, "copy an atlas frame into regular storage");
    assert(runner, &result, storage.clm_storage_delete(121) == bpa.CLM_BPA_SUCCESS and storage.clm_storage_delete(120) == bpa.CLM_BPA_SUCCESS, "delete atlas resources");

    assert(runner, &result, storage.clm_storage_atlas_new(122, bpa.CLM_BPA_TYPE_BYTE, 0, 5, 4, 2, 2) == bpa.CLM_BPA_FAILURE, "reject atlas dimensions not divisible by frame size");
    assert(runner, &result, storage.clm_storage_new_2d(123, bpa.CLM_BPA_TYPE_STRING, 0, 2, 2) == bpa.CLM_BPA_FAILURE, "reject string payloads");
    assert(runner, &result, storage.clm_storage_new_2d(123, bpa.CLM_BPA_TYPE_INT, 0, 0, 2) == bpa.CLM_BPA_FAILURE, "reject zero dimensions");
    assert(runner, &result, storage.clm_storage_new(123, bpa.CLM_BPA_TYPE_INT, 0, 4294967295, 4294967295, 2) == bpa.CLM_BPA_FAILURE, "reject dimension overflow");

    assert(runner, &result, storage.clm_storage_atlas_new_3d(130, bpa.CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2, 2) == bpa.CLM_BPA_SUCCESS, "create a layered atlas");
    assert(runner, &result, storage.clm_storage_atlas_set_frame_from(130, 1, 0, 1, &frame) == bpa.CLM_BPA_SET_SUCCESS and
        storage.clm_storage_atlas_get_frame_to(130, 1, 0, 1, &frame_result) == bpa.CLM_BPA_GET_SUCCESS and
        std.mem.eql(u8, &frame, &frame_result), "write and read a frame on a higher layer");
    assert(runner, &result, storage.clm_storage_get_to(130, 0, 0, 2, std.mem.asBytes(&byte_result)) == bpa.CLM_BPA_GET_SUCCESS and byte_result == 0, "atlas layers have independent data");
    assert(runner, &result, storage.clm_storage_delete(130) == bpa.CLM_BPA_SUCCESS, "delete layered atlas");

    assert(runner, &result, storage.clm_storage_new_2d(140, bpa.CLM_BPA_TYPE_INT, 0, 2, 2) == bpa.CLM_BPA_SUCCESS, "create storage for fill and block-copy tests");
    value = 5;
    assert(runner, &result, storage.clm_storage_fill_from(140, std.mem.asBytes(&value)) == bpa.CLM_BPA_SUCCESS, "fill all grid elements");
    assert(runner, &result, storage.clm_storage_get_to(140, 0, 1, 1, std.mem.asBytes(&replacement)) == bpa.CLM_BPA_GET_SUCCESS and replacement == 5, "read filled element");
    assert(runner, &result, bpa.clm_bpa_create(141, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_SUCCESS, "create BPA source for block copy");
    value = 83;
    assert(runner, &result, bpa.clm_bpa_set(141, std.mem.asBytes(&value)) == bpa.CLM_BPA_SET_SUCCESS, "write BPA block");
    assert(runner, &result, storage.clm_storage_set(140, 0, 0, 0, 141) == bpa.CLM_BPA_SET_SUCCESS, "copy BPA block into grid");
    assert(runner, &result, storage.clm_storage_get(140, 0, 0, 0, 141) == bpa.CLM_BPA_GET_SUCCESS and
        bpa.clm_bpa_get(141, std.mem.asBytes(&replacement)) == bpa.CLM_BPA_GET_SUCCESS and replacement == 83, "copy grid element into BPA block");
    assert(runner, &result, bpa.clm_bpa_delete(141) == bpa.CLM_BPA_SUCCESS, "delete BPA block");
    assert(runner, &result, storage.clm_storage_delete(140) == bpa.CLM_BPA_SUCCESS, "delete filled grid");
    _ = bpa.clm_bpa_shutdown();

    test_report.clm_test_summary(runner, result);
}
