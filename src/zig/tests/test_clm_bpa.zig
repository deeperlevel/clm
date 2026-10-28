//! Assertions for the BPA block and BPAA array APIs.

const std = @import("std");
const base = @import("clm_base");
const bpa = @import("clm_bpa");
const test_report = @import("test.zig");

fn check(
    runner: *test_report.TestRunner,
    result: *test_report.clm_test_result,
    condition: bool,
    description: []const u8,
) void {
    test_report.clm_test_assert(runner, result, condition, description);
}

fn readInt(bytes: []const u8) i32 {
    var value: i32 = 0;
    @memcpy(std.mem.asBytes(&value), bytes[0..@sizeOf(i32)]);
    return value;
}

fn compareInts(left: []const u8, right: []const u8) i32 {
    const a = readInt(left);
    const b = readInt(right);
    return @as(i32, @intFromBool(a > b)) - @as(i32, @intFromBool(a < b));
}

fn outOfOrder(left: []const u8, right: []const u8) bool {
    return readInt(left) > readInt(right);
}

/// Runs the BPA/BPAA conformance suite using the shared test reporter.
pub fn clm_bpa_tests(runner: *test_report.TestRunner) void {
    var result: test_report.clm_test_result = .{};
    test_report.clm_test_begin(runner, "CLM - Block Pool Allocation (BPA)");

    check(runner, &result, bpa.clm_bpa_pool_class_for(8) < bpa.CLM_BPA_POOL_CLASSES and bpa.clm_bpa_pool_class_for(0) == bpa.CLM_BPA_POOL_CLASSES, "select a bounded pool size class");
    check(runner, &result, bpa.clm_bpa_hash_id(0x100000001) != bpa.clm_bpa_hash_id(1), "mix wide block identifiers");
    check(runner, &result, bpa.clm_bpa_prepare_blocks() == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_prepare_arrays() == bpa.CLM_BPA_SUCCESS, "prepare dynamic registries");
    check(runner, &result, bpa.clm_bpa_rehash_blocks(16) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_rehash_arrays(16) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_rehash_arrays(0) == bpa.CLM_BPA_FAILURE, "validate legacy registry rehash requests");
    // Registry stress: indexed lookups, removals, and rehash after population.
    const registry_start: base.clm_id = 0xE00000;
    var registry_ok = true;
    for (0..96) |i| {
        registry_ok = registry_ok and bpa.clm_bpa_create(registry_start + @as(u64, @intCast(i)), bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_SUCCESS;
    }
    for (0..96) |i| {
        const id = registry_start + @as(u64, @intCast(i));
        registry_ok = registry_ok and bpa.clm_bpa_find_block(id) != null;
    }
    check(runner, &result, registry_ok, "indexed block lookup across multiple capacity growths");
    check(runner, &result, bpa.clm_bpa_rehash_blocks(256) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_find_block(registry_start + 95) != null, "rehash retains existing indexed block nodes");
    for (0..96) |i| {
        registry_ok = registry_ok and bpa.clm_bpa_delete(registry_start + @as(u64, @intCast(i))) == bpa.CLM_BPA_SUCCESS;
    }
    check(runner, &result, registry_ok and bpa.clm_bpa_find_block(registry_start + 95) == null, "indexed deletion removes all block entries");

    // A slab class must reuse checked-in slots without increasing capacity.
    var before_active: usize = 0;
    var before_capacity: usize = 0;
    _ = bpa.clm_bpa_pool_stats(32, &before_active, &before_capacity);
    const pooled = bpa.clm_bpa_pool_alloc(32);
    var after_capacity: usize = 0;
    var after_active: usize = 0;
    _ = bpa.clm_bpa_pool_stats(32, &after_active, &after_capacity);
    check(runner, &result, pooled != null and after_active == before_active + 1, "indexed pool allocation increments active slot count");
    if (pooled) |mem| bpa.clm_bpa_pool_free(mem, mem.len);
    var released_active: usize = 0;
    var released_capacity: usize = 0;
    _ = bpa.clm_bpa_pool_stats(32, &released_active, &released_capacity);
    check(runner, &result, released_active == before_active and released_capacity == after_capacity, "indexed pool release retains reusable capacity");
    const pooled_again = bpa.clm_bpa_pool_alloc(32);
    check(runner, &result, pooled != null and pooled_again != null and pooled.?.ptr == pooled_again.?.ptr, "pool reuses released slot by address");
    if (pooled_again) |mem| bpa.clm_bpa_pool_free(mem, mem.len);

    // Explicit caller-held lock protects multi-call sequences (non-recursive).
    bpa.clm_bpa_lock();
    const lock_checked = !bpa.clm_bpa_is_existing(0xF00000);
    bpa.clm_bpa_unlock();
    check(runner, &result, lock_checked, "external BPA lock supports caller-managed synchronization");

    check(runner, &result, !bpa.clm_bpa_is_existing(0xB001), "new block id is unused");
    check(runner, &result, bpa.clm_bpa_create(0xB001, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_SUCCESS, "create a typed integer block");
    check(runner, &result, bpa.clm_bpa_is_existing(0xB001), "created block is registered");
    check(runner, &result, bpa.clm_bpa_create(0xB001, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_FAILURE, "duplicate block IDs are rejected");

    var value: i32 = 9;
    check(runner, &result, bpa.clm_bpa_set(0xB001, std.mem.asBytes(&value)) == bpa.CLM_BPA_SET_SUCCESS, "replace fixed-size block contents");
    var read_value: i32 = 0;
    check(runner, &result, bpa.clm_bpa_get(0xB001, std.mem.asBytes(&read_value)) == bpa.CLM_BPA_GET_SUCCESS and read_value == 9, "read fixed-size block contents");
    value = 12;
    check(runner, &result, bpa.clm_bpa_replace(0xB001, std.mem.asBytes(&value)) == bpa.CLM_BPA_SET_SUCCESS and bpa.clm_bpa_get(0xB001, std.mem.asBytes(&read_value)) == bpa.CLM_BPA_GET_SUCCESS and read_value == 12, "replace a block through its type-aware API");
    value = 9;
    check(runner, &result, bpa.clm_bpa_set_from(0xB001, std.mem.asBytes(&value)) == bpa.CLM_BPA_SET_SUCCESS and bpa.clm_bpa_get_to(0xB001, std.mem.asBytes(&read_value)) == bpa.CLM_BPA_GET_SUCCESS and read_value == 9, "use fixed-width transfer aliases");
    check(runner, &result, bpa.clm_bpa_borrow(0xB001) != null, "borrow a block payload");
    check(runner, &result, bpa.clm_bpa_get(0xB001, std.mem.asBytes(&read_value)[0..2]) == bpa.CLM_BPA_FAILURE, "reject a short output buffer");
    check(runner, &result, bpa.clm_bpa_ref(0xB001) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_unref(0xB001) == bpa.CLM_BPA_SUCCESS, "retain and release a block");
    check(runner, &result, bpa.clm_bpa_unref(0xB001) == bpa.CLM_BPA_SUCCESS and !bpa.clm_bpa_is_existing(0xB001), "final unref deletes the block");
    check(runner, &result, bpa.clm_bpa_create(0xB002, bpa.CLM_BPA_TYPE_STRUCT) == bpa.CLM_BPA_FAILURE, "reject an unsized custom block");

    const struct_id: base.clm_id = 0xB003;
    check(runner, &result, bpa.clm_bpa_create_sized(struct_id, bpa.CLM_BPA_TYPE_STRUCT, 8) == bpa.CLM_BPA_SUCCESS, "create a sized custom block");
    const pair = [_]i32{ 123, 456 };
    check(runner, &result, bpa.clm_bpa_set(struct_id, std.mem.sliceAsBytes(&pair)) == bpa.CLM_BPA_SET_SUCCESS, "write custom struct bytes");
    var pair_out = [_]i32{ 0, 0 };
    check(runner, &result, bpa.clm_bpa_get(struct_id, std.mem.sliceAsBytes(&pair_out)) == bpa.CLM_BPA_GET_SUCCESS and pair_out[0] == 123 and pair_out[1] == 456, "read custom struct bytes");
    check(runner, &result, bpa.clm_bpa_delete(struct_id) == bpa.CLM_BPA_SUCCESS, "delete a custom block");
    const struct_array_id: base.clm_id = 0xBA04;
    check(runner, &result, bpa.clm_bpaa_create_sized(struct_array_id, bpa.CLM_BPA_TYPE_STRUCT, 2, 8, 0) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_append_raw(struct_array_id, std.mem.sliceAsBytes(&pair)) == bpa.CLM_BPA_SUCCESS, "create a sized struct array");
    check(runner, &result, bpa.clm_bpaa_delete(struct_array_id) == bpa.CLM_BPA_SUCCESS, "delete a sized struct array");

    check(runner, &result, bpa.clm_bpa_create(0xB004, bpa.CLM_BPA_TYPE_STRING) == bpa.CLM_BPA_SUCCESS, "create a string block");
    check(runner, &result, bpa.clm_bpa_set_string(0xB004, "hello") == 5, "copy string block contents");
    var text_output: [24]u8 = undefined;
    check(runner, &result, bpa.clm_bpa_get_string(0xB004, &text_output) == 5 and std.mem.eql(u8, text_output[0..6], "hello\x00") and bpa.clm_bpa_get_string_to(0xB004, &text_output) == 5, "retrieve a NUL-terminated string");
    const string_copy = bpa.clm_bpa_copy_string("copy");
    check(runner, &result, string_copy != null and string_copy.?.len == 5 and std.mem.eql(u8, string_copy.?, "copy\x00"), "copy bounded string bytes into pooled storage");
    if (string_copy) |copy| bpa.clm_bpa_pool_free(copy, copy.len);
    check(runner, &result, bpa.clm_bpa_set_string_raw(0xB004, "world") == 5 and std.mem.eql(u8, bpa.clm_bpa_string(0xB004).?, "world"), "replace string through the raw slice API");
    check(runner, &result, bpa.clm_bpa_set_string_from(0xB004, "again") == 5 and bpa.clm_bpa_string(0xB004).?.len == 5, "use the string transfer alias");
    check(runner, &result, bpa.clm_bpa_create(0xB006, bpa.CLM_BPA_TYPE_STRING) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_copy_block(0xB004, 0xB006) == bpa.CLM_BPA_SUCCESS and std.mem.eql(u8, bpa.clm_bpa_string(0xB006).?, "again"), "copy a string block independently");
    check(runner, &result, bpa.clm_bpa_delete(0xB006) == bpa.CLM_BPA_SUCCESS, "delete copied string block");
    check(runner, &result, bpa.clm_bpa_delete(0xB004) == bpa.CLM_BPA_SUCCESS, "delete a string block");

    const array_id: base.clm_id = 0xBA01;
    check(runner, &result, !bpa.clm_bpaa_is_existing(array_id), "new array id is unused");
    check(runner, &result, bpa.clm_bpaa_create(array_id, bpa.CLM_BPA_TYPE_INT, 1, 0) == bpa.CLM_BPA_SUCCESS, "create an integer array with reserved capacity");
    check(runner, &result, bpa.clm_bpaa_ref(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_unref(array_id) == bpa.CLM_BPA_SUCCESS, "retain and release an array");
    const values = [_]i32{ 4, 1, 3, 2 };
    for (values) |item| {
        check(runner, &result, bpa.clm_bpaa_append_raw(array_id, std.mem.asBytes(&item)) == bpa.CLM_BPA_SUCCESS, "append an integer array item");
    }
    var count: u32 = 0;
    var capacity: u32 = 0;
    check(runner, &result, bpa.clm_bpaa_get_count_to(array_id, &count) == bpa.CLM_BPA_GET_SUCCESS and count == 4, "read array count");
    check(runner, &result, bpa.clm_bpaa_get_capacity_to(array_id, &capacity) == bpa.CLM_BPA_GET_SUCCESS and capacity >= 4, "read grown array capacity");
    check(runner, &result, bpa.clm_bpaa_get_length_to(array_id, &count) == bpa.CLM_BPA_GET_SUCCESS and count == 4, "read initialized array length");
    var raw: i32 = 0;
    check(runner, &result, bpa.clm_bpaa_get_first_raw(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and raw == 4, "read first raw array value");
    check(runner, &result, bpa.clm_bpaa_get_last_raw(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and raw == 2, "read last raw array value");
    check(runner, &result, bpa.clm_bpaa_get_to(array_id, 0, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and raw == 4 and bpa.clm_bpaa_get_first_to(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and bpa.clm_bpaa_get_last_to(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS, "read elements through output-direction aliases");
    check(runner, &result, bpa.clm_bpaa_get_raw(array_id, -1, std.mem.asBytes(&raw)) == bpa.CLM_BPA_FAILURE, "reject negative array indices");

    check(runner, &result, bpa.clm_bpaa_move_to(array_id, 2) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_get_current_raw(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and raw == 3, "move and read array cursor");
    check(runner, &result, bpa.clm_bpaa_get_current_to(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and raw == 3, "read current item through output-direction alias");
    var cursor: i32 = -1;
    check(runner, &result, bpa.clm_bpaa_get_current_index_to(array_id, &cursor) == bpa.CLM_BPA_GET_SUCCESS and cursor == 2, "read array cursor position");
    check(runner, &result, bpa.clm_bpaa_move_first(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_move_next(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_move_previous(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_move_last(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_move_next(array_id) == bpa.CLM_BPA_FAILURE and bpa.clm_bpaa_reset(array_id) == bpa.CLM_BPA_SUCCESS, "move, reset, and bound the array cursor");

    var inserted: i32 = 8;
    check(runner, &result, bpa.clm_bpaa_insert_raw(array_id, 1, std.mem.asBytes(&inserted)) == bpa.CLM_BPA_SUCCESS, "insert an item at an index");
    check(runner, &result, bpa.clm_bpaa_set_raw(array_id, 1, std.mem.asBytes(&values[0])) == bpa.CLM_BPA_SET_SUCCESS, "set an item using raw array access");
    check(runner, &result, bpa.clm_bpaa_set_from(array_id, 1, std.mem.asBytes(&values[0])) == bpa.CLM_BPA_SET_SUCCESS, "replace an existing raw item");
    check(runner, &result, bpa.clm_bpaa_remove_first(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_remove_last(array_id) == bpa.CLM_BPA_SUCCESS, "remove array edge items");
    check(runner, &result, bpa.clm_bpaa_extend(array_id, 3) == bpa.CLM_BPA_SUCCESS, "extend array capacity");
    check(runner, &result, bpa.clm_bpaa_shrink(array_id, count) == bpa.CLM_BPA_SUCCESS, "shrink unused array capacity");
    check(runner, &result, bpa.clm_bpaa_seal(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_extend(array_id, 1) == bpa.CLM_BPA_FAILURE, "seal capacity changes");
    var sealed: u8 = 0;
    check(runner, &result, bpa.clm_bpaa_is_sealed_to(array_id, &sealed) == bpa.CLM_BPA_GET_SUCCESS and sealed == 1, "read sealed state");
    check(runner, &result, bpa.clm_bpaa_unseal(array_id) == bpa.CLM_BPA_SUCCESS, "unseal an array");

    const slice_id: base.clm_id = 0xBA02;
    const borrowed = bpa.clm_bpaa_slice_borrow(array_id, 0, 2);
    check(runner, &result, borrowed != null and borrowed.?.len == 8, "borrow contiguous fixed-width slice");
    var slice_output: [8]u8 = undefined;
    check(runner, &result, bpa.clm_bpaa_slice_to(array_id, 0, 2, &slice_output) == bpa.CLM_BPA_SUCCESS, "copy fixed-width slice to caller storage");
    check(runner, &result, bpa.clm_bpaa_slice(array_id, 0, 2, slice_id) == bpa.CLM_BPA_SUCCESS, "copy a slice into a new array");
    check(runner, &result, bpa.clm_bpaa_get_count_to(slice_id, &count) == bpa.CLM_BPA_GET_SUCCESS and count == 2, "validate copied slice count");
    check(runner, &result, bpa.clm_bpaa_delete(slice_id) == bpa.CLM_BPA_SUCCESS, "delete sliced array");

    check(runner, &result, bpa.clm_bpaa_sort_with(array_id, compareInts) == bpa.CLM_BPA_SUCCESS, "sort with a comparator callback");
    check(runner, &result, bpa.clm_bpaa_get_first_raw(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and raw == 1, "verify sorted array order");
    check(runner, &result, bpa.clm_bpaa_reverse(array_id) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_sort(array_id, outOfOrder) == bpa.CLM_BPA_SUCCESS, "reverse and predicate-sort fixed-width items");
    check(runner, &result, bpa.clm_bpaa_shift_left(array_id, 0, 1) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_shift_right(array_id, 0, 1) == bpa.CLM_BPA_SUCCESS, "rotate array suffixes");
    check(runner, &result, bpa.clm_bpaa_swap(array_id, 0, 1) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_copy(array_id, 0, 1) == bpa.CLM_BPA_SUCCESS, "swap and copy array items");
    check(runner, &result, bpa.clm_bpaa_shuffle(array_id) == bpa.CLM_BPA_SUCCESS, "shuffle fixed-width array");

    const block_id: base.clm_id = 0xB005;
    check(runner, &result, bpa.clm_bpa_create(block_id, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_SUCCESS, "create block for array transfers");
    check(runner, &result, bpa.clm_bpaa_get(array_id, 0, block_id, 4) == bpa.CLM_BPA_GET_SUCCESS, "copy array item to block");
    check(runner, &result, bpa.clm_bpaa_set(array_id, 0, block_id, 4) == bpa.CLM_BPA_SET_SUCCESS, "copy block into array item");
    check(runner, &result, bpa.clm_bpaa_append(array_id, block_id, 4) == bpa.CLM_BPA_SUCCESS, "append block to array");
    check(runner, &result, bpa.clm_bpaa_draw(array_id, block_id, 4) == bpa.CLM_BPA_GET_SUCCESS, "draw a random item into a block");
    check(runner, &result, bpa.clm_bpaa_draw_at(array_id, 0, block_id, 4) == bpa.CLM_BPA_GET_SUCCESS, "draw an indexed item into a block");
    check(runner, &result, bpa.clm_bpaa_draw_raw(array_id, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS and bpa.clm_bpaa_draw_at_raw(array_id, 0, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS, "draw fixed-width items to raw buffers");
    check(runner, &result, bpa.clm_bpaa_insert(array_id, 0, block_id, 4) == bpa.CLM_BPA_SUCCESS, "insert a BPA block into an array");
    const count_block: base.clm_id = 0xB007;
    check(runner, &result, bpa.clm_bpa_create(count_block, bpa.CLM_BPA_TYPE_UNSIGNED_INT) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_get_count(array_id, count_block) == bpa.CLM_BPA_GET_SUCCESS, "write array count into a metadata block");
    var count_read: u32 = 0;
    check(runner, &result, bpa.clm_bpa_get(count_block, std.mem.asBytes(&count_read)) == bpa.CLM_BPA_GET_SUCCESS and count_read == bpa.clm_bpaa_find_array(array_id).?.count, "read metadata block value");
    check(runner, &result, bpa.clm_bpaa_get_capacity(array_id, count_block) == bpa.CLM_BPA_GET_SUCCESS and bpa.clm_bpaa_get_length(array_id, count_block) == bpa.CLM_BPA_GET_SUCCESS, "write array capacity and length metadata");
    check(runner, &result, bpa.clm_bpa_delete(count_block) == bpa.CLM_BPA_SUCCESS, "delete metadata block");

    var borrowed_array: ?*bpa.clm_bpaa_array_node = null;
    check(runner, &result, bpa.clm_bpaa_advanced_borrow(array_id, &borrowed_array) == bpa.CLM_BPA_SUCCESS and borrowed_array != null, "borrow array metadata");
    if (borrowed_array) |array| {
        check(runner, &result, bpa.clm_bpaa_advanced_data(array) != null, "access borrowed fixed-width backing storage");
        check(runner, &result, bpa.clm_bpaa_advanced_get_raw(array, 0, std.mem.asBytes(&raw)) == bpa.CLM_BPA_GET_SUCCESS, "read array item through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_set_raw(array, 0, std.mem.asBytes(&raw)) == bpa.CLM_BPA_SET_SUCCESS, "write array item through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_append_raw(array, std.mem.asBytes(&inserted)) == bpa.CLM_BPA_SUCCESS, "append through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_insert_raw(array, 0, std.mem.asBytes(&inserted)) == bpa.CLM_BPA_SUCCESS, "insert through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_remove(array, 0) == bpa.CLM_BPA_SUCCESS, "remove inserted borrowed item");
        check(runner, &result, bpa.clm_bpaa_advanced_remove(array, @intCast(array.count - 1)) == bpa.CLM_BPA_SUCCESS, "remove through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_extend(array, 2) == bpa.CLM_BPA_SUCCESS, "extend through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_shrink(array, array.count) == bpa.CLM_BPA_SUCCESS, "shrink through borrowed node");
        check(runner, &result, bpa.clm_bpaa_advanced_clear(array) == bpa.CLM_BPA_SUCCESS, "clear through borrowed node");
    }
    check(runner, &result, bpa.clm_bpaa_advanced_release(&borrowed_array) == bpa.CLM_BPA_SUCCESS and borrowed_array == null, "release array metadata borrow");
    check(runner, &result, bpa.clm_bpaa_delete(array_id) == bpa.CLM_BPA_SUCCESS, "delete fixed-width array");
    check(runner, &result, bpa.clm_bpa_delete(block_id) == bpa.CLM_BPA_SUCCESS, "delete transfer block");

    const strings_id: base.clm_id = 0xBA03;
    check(runner, &result, bpa.clm_bpaa_create(strings_id, bpa.CLM_BPA_TYPE_STRING, 1, 7) == bpa.CLM_BPA_SUCCESS, "create a string array with encoding metadata");
    check(runner, &result, bpa.clm_bpaa_append_raw(strings_id, "one") == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_append_raw(strings_id, "two") == bpa.CLM_BPA_SUCCESS, "append copied strings");
    check(runner, &result, bpa.clm_bpaa_set_string_raw(strings_id, 0, "uno") == bpa.CLM_BPA_SET_SUCCESS and std.mem.eql(u8, bpa.clm_bpaa_string(strings_id, 0).?, "uno"), "replace a string array item");
    check(runner, &result, bpa.clm_bpaa_get_string_raw(strings_id, 1, &text_output) == 3 and std.mem.eql(u8, text_output[0..4], "two\x00"), "copy string array item to caller storage");
    check(runner, &result, bpa.clm_bpaa_get_string_to(strings_id, 0, &text_output) == 3 and std.mem.eql(u8, text_output[0..4], "uno\x00"), "read string through output-direction alias");
    check(runner, &result, bpa.clm_bpa_create(0xB008, bpa.CLM_BPA_TYPE_STRING) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpa_set_string(0xB008, "block") == 5, "create a source string block");
    check(runner, &result, bpa.clm_bpaa_append_string(strings_id, 0xB008) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_get_string(strings_id, 2, 0xB008) == bpa.CLM_BPA_GET_SUCCESS and std.mem.eql(u8, bpa.clm_bpa_string(0xB008).?, "block"), "transfer strings between array and BPA block");
    check(runner, &result, bpa.clm_bpaa_insert_string(strings_id, 1, 0xB008) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_set_string(strings_id, 1, 0xB008) == bpa.CLM_BPA_SET_SUCCESS, "insert and replace using a string block");
    check(runner, &result, bpa.clm_bpa_delete(0xB008) == bpa.CLM_BPA_SUCCESS, "delete source string block");
    check(runner, &result, bpa.clm_bpaa_insert_raw(strings_id, 1, "middle") == bpa.CLM_BPA_SUCCESS, "insert a string into an array");
    check(runner, &result, bpa.clm_bpaa_remove(strings_id, 1) == bpa.CLM_BPA_SUCCESS and bpa.clm_bpaa_clear(strings_id) == bpa.CLM_BPA_SUCCESS, "remove and clear string array items");
    check(runner, &result, bpa.clm_bpaa_delete(strings_id) == bpa.CLM_BPA_SUCCESS, "delete string array");

    var pool_active: usize = 0;
    var pool_capacity: usize = 0;
    check(runner, &result, bpa.clm_bpa_pool_stats(4, &pool_active, &pool_capacity) == bpa.CLM_BPA_SUCCESS and pool_active == 0 and pool_capacity >= bpa.CLM_BPA_POOL_SLAB_SLOTS, "report pooled allocation utilization");

    check(runner, &result, bpa.clm_bpa_pool_cleanup() == bpa.CLM_BPA_SUCCESS, "release idle BPA pool slabs");
    test_report.clm_test_summary(runner, result);
}

test "BPA reporter suite" {
    var output: [64 * 1024]u8 = undefined;
    var writer = std.Io.Writer.fixed(&output);
    var runner = test_report.TestRunner.init(&writer, null);
    clm_bpa_tests(&runner);
    if (runner.total_failed != 0) std.debug.print("{s}", .{writer.buffered()});
    try std.testing.expectEqual(@as(usize, 0), runner.total_failed);
    try std.testing.expect(runner.total_passed > 0);
}
