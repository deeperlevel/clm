//! Tests for CLM dictionary storage.

const std = @import("std");
const dictionary = @import("clm_dictionary");
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

pub fn clm_dictionary_tests(runner: *test_report.TestRunner) void {
    var result: test_report.clm_test_result = .{};
    test_report.clm_test_begin(runner, "CLM - Dictionary");

    const dictionary_id: u64 = 71000;
    const source_id: u64 = 71001;
    const destination_id: u64 = 71002;
    const string_source_id: u64 = 71004;
    var value: i32 = 0;
    const source_value: i32 = 9876;
    var iterated_key: u64 = 0;
    var iterated_count: i32 = 0;

    assert(runner, &result, dictionary.clm_dictionary_new(dictionary_id, bpa.CLM_BPA_TYPE_INT, 0) == bpa.CLM_BPA_SUCCESS, "create integer dictionary");
    assert(runner, &result, dictionary.clm_dictionary_count(dictionary_id) == 0, "new dictionary starts empty");
    assert(runner, &result, dictionary.clm_dictionary_new(dictionary_id, bpa.CLM_BPA_TYPE_INT, 0) == bpa.CLM_BPA_FAILURE, "reject duplicate dictionary ID");
    assert(runner, &result, dictionary.clm_dictionary_new(71003, bpa.CLM_BPA_TYPE_STRING, 1) == bpa.CLM_BPA_FAILURE, "reject invalid string dictionary size");
    assert(runner, &result, bpa.clm_bpa_create_sized(71040, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(dictionary.clm_dictionary_metadata)) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_count(71040) == bpa.CLM_BPA_FAILURE and
        dictionary.clm_dictionary_delete(71040) == bpa.CLM_BPA_FAILURE and
        bpa.clm_bpa_delete(71040) == bpa.CLM_BPA_SUCCESS, "do not mistake an unrelated struct block for a dictionary");

    var inserted_all = true;
    for (0..40) |index| {
        const key: u64 = 1000 + index;
        const item: i32 = @intCast(index * 10);
        if (dictionary.clm_dictionary_set_from(dictionary_id, key, std.mem.asBytes(&item)) != bpa.CLM_BPA_SET_SUCCESS)
            inserted_all = false;
    }
    assert(runner, &result, inserted_all, "insert entries and grow hash table");
    assert(runner, &result, dictionary.clm_dictionary_count(dictionary_id) == 40, "count entries after insertion");
    assert(runner, &result, !dictionary.clm_dictionary_contains(dictionary_id, 0), "key zero is absent until inserted");
    assert(runner, &result, dictionary.clm_dictionary_set_from(dictionary_id, 1005, std.mem.asBytes(&source_value)) == bpa.CLM_BPA_SET_SUCCESS and
        dictionary.clm_dictionary_get_to(dictionary_id, 1005, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and
        value == source_value and dictionary.clm_dictionary_count(dictionary_id) == 40, "replace existing value without adding a key");

    assert(runner, &result, bpa.clm_bpa_create(source_id, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_SUCCESS, "create BPA source block");
    assert(runner, &result, bpa.clm_bpa_set(source_id, std.mem.asBytes(&source_value)) == bpa.CLM_BPA_SET_SUCCESS, "write BPA source block");
    assert(runner, &result, dictionary.clm_dictionary_set(dictionary_id, 0, source_id) == bpa.CLM_BPA_SET_SUCCESS, "set from BPA block");
    assert(runner, &result, dictionary.clm_dictionary_get_to(dictionary_id, 0, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and value == source_value, "retrieve value under key zero");
    assert(runner, &result, dictionary.clm_dictionary_count(dictionary_id) == 41, "count key zero as a distinct entry");
    assert(runner, &result, bpa.clm_bpa_create(destination_id, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_get(dictionary_id, 0, destination_id) == bpa.CLM_BPA_GET_SUCCESS and
        bpa.clm_bpa_get(destination_id, std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and value == source_value, "retrieve into compatible BPA block");

    assert(runner, &result, dictionary.clm_dictionary_remove(dictionary_id, 1005) == bpa.CLM_BPA_SUCCESS and
        !dictionary.clm_dictionary_contains(dictionary_id, 1005) and
        dictionary.clm_dictionary_count(dictionary_id) == 40, "remove key and compact value storage");
    assert(runner, &result, dictionary.clm_dictionary_get_to(dictionary_id, 1005, std.mem.asBytes(&value)) == bpa.CLM_BPA_FAILURE, "missing key retrieval fails");
    const new_value: i32 = 12345;
    assert(runner, &result, dictionary.clm_dictionary_set_from(dictionary_id, 99999, std.mem.asBytes(&new_value)) == bpa.CLM_BPA_SET_SUCCESS and
        dictionary.clm_dictionary_count(dictionary_id) == 41, "insert after removal into table with tombstones");

    var iteration_valid = dictionary.clm_dictionary_iterate_reset(dictionary_id) == bpa.CLM_BPA_SUCCESS;
    var seen_keys = std.mem.zeroes([42]u8);
    while (iteration_valid and dictionary.clm_dictionary_iterate_next(dictionary_id, &iterated_key) == bpa.CLM_BPA_GET_SUCCESS) {
        var expected_value: i32 = undefined;
        var seen_index: usize = undefined;
        if (iterated_key == 0) {
            expected_value = source_value;
            seen_index = 0;
        } else if (iterated_key == 99999) {
            expected_value = new_value;
            seen_index = 1;
        } else if (iterated_key >= 1000 and iterated_key < 1040 and iterated_key != 1005) {
            expected_value = @intCast((iterated_key - 1000) * 10);
            seen_index = @intCast(iterated_key - 1000 + 2);
        } else {
            iteration_valid = false;
            break;
        }
        if (seen_keys[seen_index] != 0 or
            !dictionary.clm_dictionary_contains(dictionary_id, iterated_key) or
            dictionary.clm_dictionary_get_to(dictionary_id, iterated_key, std.mem.asBytes(&value)) != bpa.CLM_BPA_GET_SUCCESS or
            value != expected_value)
            iteration_valid = false;
        seen_keys[seen_index] = 1;
        iterated_count += 1;
    }
    assert(runner, &result, iteration_valid and iterated_count == 41, "iterate all present keys and retrieve their values");
    assert(runner, &result, dictionary.clm_dictionary_iterate_reset(dictionary_id) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_iterate_next(dictionary_id, &iterated_key) == bpa.CLM_BPA_GET_SUCCESS, "reset iteration");

    assert(runner, &result, bpa.clm_bpa_create(string_source_id, bpa.CLM_BPA_TYPE_STRING) == bpa.CLM_BPA_SUCCESS and
        bpa.clm_bpa_set_string(string_source_id, "dictionary value") == 16 and
        dictionary.clm_dictionary_new(71010, bpa.CLM_BPA_TYPE_STRING, 0) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_set(71010, 5, string_source_id) == bpa.CLM_BPA_SET_SUCCESS, "create string dictionary and copy a BPA string");
    var text = std.mem.zeroes([32]u8);
    assert(runner, &result, dictionary.clm_dictionary_get_to(71010, 5, &text) == bpa.CLM_BPA_GET_SUCCESS and
        std.mem.eql(u8, std.mem.sliceTo(&text, 0), "dictionary value"), "retrieve string value to caller buffer");
    assert(runner, &result, dictionary.clm_dictionary_set_from(71010, 6, "\x00") == bpa.CLM_BPA_SET_SUCCESS and
        dictionary.clm_dictionary_get_to(71010, 6, &text) == bpa.CLM_BPA_GET_SUCCESS and text[0] == 0, "store and retrieve an empty string");

    const TestValue = extern struct { first: i32, second: i32 };
    const struct_value = TestValue{ .first = 11, .second = 22 };
    var struct_result = TestValue{ .first = 0, .second = 0 };
    assert(runner, &result, dictionary.clm_dictionary_new(71020, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(TestValue)) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_set_from(71020, 7, std.mem.asBytes(&struct_value)) == bpa.CLM_BPA_SET_SUCCESS and
        dictionary.clm_dictionary_get_to(71020, 7, std.mem.asBytes(&struct_result)) == bpa.CLM_BPA_GET_SUCCESS and
        std.mem.eql(u8, std.mem.asBytes(&struct_value), std.mem.asBytes(&struct_result)), "store and retrieve fixed-size struct values");

    var collision_keys = std.mem.zeroes([3]u64);
    var collision_bucket: usize = 0;
    var collision_key_count: usize = 0;
    for (0..100_000) |candidate| {
        if (collision_key_count >= 3) break;
        const bucket = bpa.clm_bpa_hash_id(candidate) & 15;
        if (collision_key_count == 0) collision_bucket = bucket;
        if (bucket == collision_bucket) {
            collision_keys[collision_key_count] = candidate;
            collision_key_count += 1;
        }
    }
    var collisions_work = collision_key_count == 3 and
        dictionary.clm_dictionary_new(71030, bpa.CLM_BPA_TYPE_INT, 0) == bpa.CLM_BPA_SUCCESS;
    for (0..3) |index| {
        if (!collisions_work) break;
        const item: i32 = @intCast(index + 1);
        if (dictionary.clm_dictionary_set_from(71030, collision_keys[index], std.mem.asBytes(&item)) != bpa.CLM_BPA_SET_SUCCESS)
            collisions_work = false;
    }
    const collision_value: i32 = 3;
    assert(runner, &result, collisions_work and
        dictionary.clm_dictionary_remove(71030, collision_keys[0]) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_get_to(71030, collision_keys[2], std.mem.asBytes(&value)) == bpa.CLM_BPA_GET_SUCCESS and
        value == collision_value, "continue probing through a removed colliding key");

    assert(runner, &result, dictionary.clm_dictionary_new(71050, bpa.CLM_BPA_TYPE_INT, 0) == bpa.CLM_BPA_SUCCESS, "create ordered dictionary");
    const ordered_keys = [_]u64{ 900, 2, 77, 0 };
    var ordered_valid = true;
    for (ordered_keys, 0..) |key, index| {
        const item: i32 = @intCast(index + 100);
        if (dictionary.clm_dictionary_add_from(71050, key, std.mem.asBytes(&item)) != bpa.CLM_BPA_SET_SUCCESS)
            ordered_valid = false;
    }
    assert(runner, &result, ordered_valid and dictionary.clm_dictionary_count(71050) == 4, "add distinct keys in insertion order");
    assert(runner, &result, dictionary.clm_dictionary_add_from(71050, 2, std.mem.asBytes(&source_value)) == bpa.CLM_BPA_FAILURE, "insert-only rejects an existing key");
    assert(runner, &result, dictionary.clm_dictionary_update_from(71050, 333, std.mem.asBytes(&source_value)) == bpa.CLM_BPA_FAILURE, "update-only rejects an absent key");
    assert(runner, &result, dictionary.clm_dictionary_update_from(71050, 2, std.mem.asBytes(&source_value)) == bpa.CLM_BPA_SET_SUCCESS, "updating existing key preserves position");
    var copied_keys = std.mem.zeroes([4]u64);
    assert(runner, &result, dictionary.clm_dictionary_keys_to(71050, &copied_keys) == bpa.CLM_BPA_GET_SUCCESS and
        std.mem.eql(u64, &copied_keys, &ordered_keys), "keys copy retains insertion order");
    var order_matches = dictionary.clm_dictionary_iterate_reset(71050) == bpa.CLM_BPA_SUCCESS;
    for (ordered_keys) |key| {
        if (!order_matches) break;
        if (dictionary.clm_dictionary_iterate_next(71050, &iterated_key) != bpa.CLM_BPA_GET_SUCCESS or iterated_key != key)
            order_matches = false;
    }
    assert(runner, &result, order_matches and dictionary.clm_dictionary_iterate_next(71050, &iterated_key) == bpa.CLM_BPA_FAILURE, "iterator follows insertion order");
    assert(runner, &result, dictionary.clm_dictionary_remove(71050, 2) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_add_from(71050, 2, std.mem.asBytes(&source_value)) == bpa.CLM_BPA_SET_SUCCESS, "deleting and readding key appends it to end");
    const expected_after_readd = [_]u64{ 900, 77, 0, 2 };
    assert(runner, &result, dictionary.clm_dictionary_keys_to(71050, &copied_keys) == bpa.CLM_BPA_GET_SUCCESS and
        std.mem.eql(u64, &copied_keys, &expected_after_readd), "reinserted key comes last");
    assert(runner, &result, dictionary.clm_dictionary_clear(71050) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_count(71050) == 0 and
        !dictionary.clm_dictionary_contains(71050, 900), "clear preserves empty dictionary resource");
    assert(runner, &result, dictionary.clm_dictionary_delete(71050) == bpa.CLM_BPA_SUCCESS, "delete ordered dictionary");

    assert(runner, &result, dictionary.clm_dictionary_delete(71030) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_delete(71020) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_delete(71010) == bpa.CLM_BPA_SUCCESS and
        dictionary.clm_dictionary_delete(dictionary_id) == bpa.CLM_BPA_SUCCESS, "delete dictionaries and their backing arrays");
    assert(runner, &result, dictionary.clm_dictionary_count(dictionary_id) == bpa.CLM_BPA_FAILURE, "deleted dictionary is no longer accessible");
    assert(runner, &result, bpa.clm_bpa_delete(source_id) == bpa.CLM_BPA_SUCCESS and
        bpa.clm_bpa_delete(destination_id) == bpa.CLM_BPA_SUCCESS and
        bpa.clm_bpa_delete(string_source_id) == bpa.CLM_BPA_SUCCESS, "delete BPA source and destination blocks");

    test_report.clm_test_summary(runner, result);
}
