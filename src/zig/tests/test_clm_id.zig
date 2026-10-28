//! Tests for CLM ID registration and generation.

const std = @import("std");
const id = @import("clm_id");
const test_report = @import("test.zig");

fn assert(
    runner: *test_report.TestRunner,
    result: *test_report.clm_test_result,
    condition: bool,
    description: []const u8,
) void {
    test_report.clm_test_assert(runner, result, condition, description);
}

/// Exercises name validation, aliases, casing, generation, and storage lifecycle.
pub fn clm_id_tests(runner: *test_report.TestRunner) void {
    var result: test_report.clm_test_result = .{};
    var value: id.clm_id = id.CLM_ID_NOT_FOUND;
    test_report.clm_test_begin(runner, "CLM - ID");

    assert(runner, &result, id.clm_id_is_case_sensitive(), "name matching is case-sensitive by default");
    assert(runner, &result, id.clm_id_clear() == id.CLM_BPA_SUCCESS, "initialize and clear ID mappings");
    assert(runner, &result, id.clm_id_register("host", 1001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("host") == 1001, "register and resolve a name");
    assert(runner, &result, id.clm_id_map("mapped-name", 1002) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("mapped-name") == 1002, "map alias for registration");
    assert(runner, &result, id.clm_id_register("default_host", 1001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("default_host") == 1001, "allow multiple names to share one ID");
    assert(runner, &result, id.clm_id_register("host", 1002) == id.CLM_BPA_FAILURE, "reject duplicate name without changing its mapping");
    assert(runner, &result, id.clm_id_register("\xc0\xaf", 1003) == id.CLM_BPA_FAILURE, "reject malformed UTF-8 names");
    assert(runner, &result, id.clm_id_register("", 1003) == id.CLM_BPA_FAILURE, "reject empty names");
    assert(runner, &result, id.clm_id_get("HOST") == id.CLM_ID_NOT_FOUND, "distinguish names by case in default mode");
    assert(runner, &result, id.clm_id_try_get("host", &value) == id.CLM_BPA_GET_SUCCESS and value == 1001 and id.clm_id_try_get("missing", &value) == id.CLM_BPA_FAILURE, "provide status-based lookup");
    assert(runner, &result, id.clm_id_try_get("host", null) == id.CLM_BPA_FAILURE, "reject a missing output pointer");

    assert(runner, &result, id.clm_id_set("host", 2001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("host") == 2001 and id.clm_id_get("default_host") == 1001, "update one mapping while retaining aliases");
    assert(runner, &result, id.clm_id_set("new_name", 3001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("new_name") == 3001, "insert through set");
    assert(runner, &result, id.clm_id_remove("new_name") == id.CLM_BPA_SUCCESS and id.clm_id_get("new_name") == id.CLM_ID_NOT_FOUND, "remove one mapping by name");
    assert(runner, &result, id.clm_id_remove("missing") == id.CLM_BPA_FAILURE, "report removal of an unknown name");

    assert(runner, &result, id.clm_id_register("Host", 4001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_set_case_sensitive(false) == id.CLM_BPA_FAILURE and id.clm_id_is_case_sensitive() and id.clm_id_get("Host") == 4001 and id.clm_id_get("host") == 2001, "reject case-fold collisions without changing registry state");
    assert(runner, &result, id.clm_id_remove("Host") == id.CLM_BPA_SUCCESS and id.clm_id_set_case_sensitive(false) == id.CLM_BPA_SUCCESS and !id.clm_id_is_case_sensitive() and id.clm_id_get("HOST") == 2001, "enable ASCII case-insensitive lookup");
    assert(runner, &result, id.clm_id_set("HOST", 2002) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("host") == 2002, "update a name without changing its registered spelling");
    assert(runner, &result, id.clm_id_register("\xc3\x84", 6001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_register("\xc3\xa4", 6002) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("\xc3\x84") == 6001 and id.clm_id_get("\xc3\xa4") == 6002, "leave non-ASCII UTF-8 bytes case-sensitive");

    assert(runner, &result, id.clm_id_register("alias", 2002) == id.CLM_BPA_SET_SUCCESS and id.clm_id_unmap(2002) == id.CLM_BPA_SUCCESS and id.clm_id_get("host") == id.CLM_ID_NOT_FOUND and id.clm_id_get("alias") == id.CLM_ID_NOT_FOUND, "unmap every alias for a numeric ID");
    assert(runner, &result, id.clm_id_unmap_name("default_host") == id.CLM_BPA_SUCCESS and id.clm_id_get("default_host") == id.CLM_ID_NOT_FOUND, "unmap a single name");

    assert(runner, &result, id.clm_id_gen_next() == 0 and id.clm_id_gen_next() == 1 and id.clm_id_gen_next() == 2, "generate sequential IDs starting at zero");
    id.clm_id_gen_reset();
    assert(runner, &result, id.clm_id_gen_next() == 0, "reset sequential ID generation");
    id.clm_id_generator_value = id.CLM_ID_NOT_FOUND - 1;
    id.clm_id_generator_exhausted = false;
    assert(runner, &result, id.clm_id_gen_next() == id.CLM_ID_NOT_FOUND - 1 and id.clm_id_gen_next() == id.CLM_ID_NOT_FOUND, "report generator exhaustion without wrapping");
    id.clm_id_gen_reset();

    assert(runner, &result, id.clm_id_clear() == id.CLM_BPA_SUCCESS and id.clm_id_get("default_host") == id.CLM_ID_NOT_FOUND and !id.clm_id_is_case_sensitive(), "clear mappings while preserving matching mode");
    assert(runner, &result, id.clm_id_register("after-clear", 5001) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("AFTER-CLEAR") == 5001, "use the registry after clearing");
    assert(runner, &result, id.clm_id_shutdown() == id.CLM_BPA_SUCCESS and id.clm_id_get("after-clear") == id.CLM_ID_NOT_FOUND and id.clm_id_register("after-shutdown", 5002) == id.CLM_BPA_SET_SUCCESS and id.clm_id_get("after-shutdown") == 5002 and id.clm_id_shutdown() == id.CLM_BPA_SUCCESS, "release and lazily recreate private registry storage");

    assert(runner, &result, id.clm_id_set_case_sensitive(true) == id.CLM_BPA_SUCCESS, "restore default matching mode");
    test_report.clm_test_summary(runner, result);
}

test "CLM ID suite assertions pass" {
    var output: [8192]u8 = undefined;
    var writer = std.Io.Writer.fixed(&output);
    var runner = test_report.TestRunner.init(&writer, null);
    clm_id_tests(&runner);
    try std.testing.expectEqual(@as(usize, 0), runner.total_failed);
}
