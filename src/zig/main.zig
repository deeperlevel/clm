const std = @import("std");
const bpa = @import("clm_bpa");
const test_report = @import("tests/test.zig");
const base_tests = @import("tests/test_clm_base.zig");
const bpa_tests = @import("tests/test_clm_bpa.zig");
const console_tests = @import("tests/test_clm_console.zig");
const dictionary_tests = @import("tests/test_clm_dictionary.zig");
const id_tests = @import("tests/test_clm_id.zig");
const string_tests = @import("tests/test_clm_string.zig");
const storage_tests = @import("tests/test_clm_storage.zig");

pub fn main(init: std.process.Init) void {
    var stdout_buffer: [4096]u8 = undefined;
    var stdout_writer = std.Io.File.stdout().writer(init.io, &stdout_buffer);

    var log = test_report.clm_test_log_open(init.arena.allocator(), init.io) catch {
        std.debug.print("Unable to open main.log for test output.\n", .{});
        std.process.exit(1);
    };

    var runner = test_report.TestRunner.init(&stdout_writer.interface, &log);
    base_tests.clm_base_tests(&runner);
    bpa_tests.clm_bpa_tests(&runner);
    console_tests.clm_console_tests(&runner, init.io);
    dictionary_tests.clm_dictionary_tests(&runner);
    id_tests.clm_id_tests(&runner);
    string_tests.clm_string_tests(&runner);
    storage_tests.clm_storage_tests(&runner);
    test_report.clm_overall_test_summary(&runner, "CLM Tests Summary");

    const log_closed = test_report.clm_test_log_close(&log);
    const bpa_closed = bpa.clm_bpa_shutdown() == bpa.CLM_BPA_SUCCESS;
    if (!log_closed) std.debug.print("Failed while writing or closing main.log.\n", .{});
    if (!bpa_closed) std.debug.print("Failed while shutting down BPA.\n", .{});
    if (runner.total_failed != 0 or !log_closed or !bpa_closed) std.process.exit(1);
}
