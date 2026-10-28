// =============================================================================
// CLM — Cross Language Modules
//
// An initiative by deeperlevel, developed with community contributions.
// https://github.com/deeperlevel/clm
// Contact: deeparchive@outlook.com
//
// Public Domain — CC0 1.0 Universal
// https://creativecommons.org/publicdomain/zero/1.0/
//
// Free to use, modify, redistribute, and bundle for any purpose.
// Attribution and retention of this notice are appreciated, not required.
//
// Provided AS IS, without warranty or liability.
//
// Don't start from scratch, bro.
// =============================================================================

//! Shared test reporting with colored console output and a plain-text log.

const std = @import("std");
const console = @import("clm_console");
const Writer = std.Io.Writer;

pub const DIVIDER_SINGLE = "-------------------------------------------------------------";
pub const DIVIDER_DOUBLE = "=============================================================";
pub const LOG_BUFFER_CAPACITY = 4096;

/// Per-suite assertion totals.
pub const clm_test_result = struct {
    passed: usize = 0,
    failed: usize = 0,
};

/// Owns the `main.log` file and its streaming writer.
pub const TestLog = struct {
    allocator: std.mem.Allocator,
    io: std.Io,
    file: std.Io.File,
    buffer: []u8,
    file_writer: std.Io.File.Writer,
    failed: bool = false,
    closed: bool = false,

    /// Creates or truncates a plain-text log file.
    pub fn open(
        allocator: std.mem.Allocator,
        io: std.Io,
        path: []const u8,
    ) !TestLog {
        const buffer = try allocator.alloc(u8, LOG_BUFFER_CAPACITY);
        errdefer allocator.free(buffer);
        const file = try std.Io.Dir.cwd().createFile(io, path, .{ .truncate = true });
        errdefer file.close(io);
        return .{
            .allocator = allocator,
            .io = io,
            .file = file,
            .buffer = buffer,
            .file_writer = std.Io.File.Writer.initStreaming(file, io, buffer),
        };
    }

    /// Flushes and closes the log, returning false if a write or flush failed.
    pub fn close(log: *TestLog) bool {
        if (log.closed) return !log.failed;
        log.file_writer.interface.flush() catch {
            log.failed = true;
        };
        log.file.close(log.io);
        log.allocator.free(log.buffer);
        log.closed = true;
        return !log.failed;
    }

    fn writer(log: *TestLog) *Writer {
        return &log.file_writer.interface;
    }
};

/// Holds output destinations and aggregate assertion counts for one test run.
pub const TestRunner = struct {
    console_writer: *Writer,
    log: ?*TestLog = null,
    total_passed: usize = 0,
    total_failed: usize = 0,

    /// Creates a reporter using the supplied console writer and optional log.
    pub fn init(console_writer: *Writer, log: ?*TestLog) TestRunner {
        return .{ .console_writer = console_writer, .log = log };
    }
};

/// Opens `main.log`, truncating any previous run's output.
pub fn clm_test_log_open(allocator: std.mem.Allocator, io: std.Io) !TestLog {
    return TestLog.open(allocator, io, "main.log");
}

/// Closes a log and reports whether all output was written successfully.
pub fn clm_test_log_close(log: *TestLog) bool {
    return log.close();
}

fn logWrite(runner: *TestRunner, text: []const u8) void {
    if (runner.log) |log| {
        if (log.closed or console.clm_console_write_to(log.writer(), text) != .success) {
            log.failed = true;
        }
    }
}

fn logFlush(runner: *TestRunner) void {
    if (runner.log) |log| {
        if (log.closed or console.clm_console_flush(log.writer()) != .success) {
            log.failed = true;
        }
    }
}

/// Writes uncolored text to the console and plain text to the optional log.
pub fn clm_test_write(runner: *TestRunner, text: []const u8) void {
    _ = console.clm_console_write(runner.console_writer, text);
    logWrite(runner, text);
}

/// Writes a line to the console and log, flushing both destinations.
pub fn clm_test_write_line(runner: *TestRunner, text: []const u8) void {
    _ = console.clm_console_write_line(runner.console_writer, text);
    if (runner.log) |log| {
        if (log.closed or console.clm_console_write_line_to(log.writer(), text) != .success) {
            log.failed = true;
        } else {
            logFlush(runner);
        }
    }
}

/// Writes colored text to the console and the same text without ANSI codes to the log.
pub fn clm_test_write_color(
    runner: *TestRunner,
    text: []const u8,
    color: []const u8,
) void {
    _ = console.clm_console_write_color(runner.console_writer, text, color);
    logWrite(runner, text);
}

/// Formats output for both destinations using Zig's compile-time format syntax.
pub fn clm_test_write_format(
    runner: *TestRunner,
    comptime format: []const u8,
    arguments: anytype,
) void {
    _ = console.clm_console_write_format(runner.console_writer, format, arguments);
    if (runner.log) |log| {
        if (log.closed or console.clm_console_write_format(log.writer(), format, arguments) != .success) {
            log.failed = true;
        }
    }
}

/// Formats a line for both destinations and flushes the log after writing it.
pub fn clm_test_write_line_format(
    runner: *TestRunner,
    comptime format: []const u8,
    arguments: anytype,
) void {
    _ = console.clm_console_write_line_format(runner.console_writer, format, arguments);
    if (runner.log) |log| {
        if (log.closed or console.clm_console_write_line_format_to(log.writer(), format, arguments) != .success) {
            log.failed = true;
        } else {
            logFlush(runner);
        }
    }
}

/// Prints a divider and colored suite title.
pub fn clm_test_begin(runner: *TestRunner, title: []const u8) void {
    clm_test_write_line(runner, "");
    clm_test_write_line(runner, DIVIDER_DOUBLE);
    clm_test_write_color(runner, title, console.CONSOLE_COLOR_YELLOW);
    clm_test_write_line(runner, "");
    clm_test_write_line(runner, DIVIDER_DOUBLE);
}

/// Records an assertion and prints its description as a colored result line.
pub fn clm_test_assert(
    runner: *TestRunner,
    result: *clm_test_result,
    condition: bool,
    description: []const u8,
) void {
    clm_test_write(runner, "    ");
    if (condition) {
        result.passed += 1;
        clm_test_write_color(runner, "PASSED:", console.CONSOLE_COLOR_GREEN);
    } else {
        result.failed += 1;
        clm_test_write_color(runner, "FAILED:", console.CONSOLE_COLOR_RED);
    }
    clm_test_write(runner, " ");
    clm_test_write_line(runner, description);
}

/// Adds suite totals and prints the colored passed/failed/total summary.
pub fn clm_test_summary(runner: *TestRunner, result: clm_test_result) void {
    runner.total_passed += result.passed;
    runner.total_failed += result.failed;

    clm_test_write_line(runner, DIVIDER_SINGLE);
    clm_test_write(runner, "    ");
    clm_test_write_color(runner, "PASSED:", console.CONSOLE_COLOR_GREEN);
    clm_test_write_format(runner, " {d}  |  ", .{result.passed});
    clm_test_write_color(runner, "FAILED:", console.CONSOLE_COLOR_RED);
    clm_test_write_format(runner, " {d}  |  ", .{result.failed});
    clm_test_write_color(runner, "TOTAL:", console.CONSOLE_COLOR_CYAN);
    clm_test_write_line_format(runner, " {d}", .{result.passed + result.failed});
}

/// Prints the final aggregate summary in the same format as a suite summary.
pub fn clm_overall_test_summary(runner: *TestRunner, title: []const u8) void {
    clm_test_write_line(runner, "");
    clm_test_write_line(runner, DIVIDER_DOUBLE);
    clm_test_write_color(runner, title, console.CONSOLE_COLOR_YELLOW);
    clm_test_write_line(runner, "");
    clm_test_write_line(runner, DIVIDER_DOUBLE);
    clm_test_write(runner, "  ");
    clm_test_write_color(runner, "PASSED:", console.CONSOLE_COLOR_GREEN);
    clm_test_write_format(runner, " {d}  |  ", .{runner.total_passed});
    clm_test_write_color(runner, "FAILED:", console.CONSOLE_COLOR_RED);
    clm_test_write_format(runner, " {d}  |  ", .{runner.total_failed});
    clm_test_write_color(runner, "TOTAL:", console.CONSOLE_COLOR_CYAN);
    clm_test_write_line_format(runner, " {d}", .{runner.total_passed + runner.total_failed});
    clm_test_write_line(runner, DIVIDER_DOUBLE);
    clm_test_write_line(runner, "");
}

test "assertion reports use colors on console and plain text in the log" {
    const path = "zig-test-main.log";
    var log = try TestLog.open(std.testing.allocator, std.testing.io, path);
    defer {
        _ = log.close();
        std.Io.Dir.cwd().deleteFile(std.testing.io, path) catch {};
    }

    var console_buffer: [256]u8 = undefined;
    var writer = Writer.fixed(&console_buffer);
    var runner = TestRunner.init(&writer, &log);
    clm_test_write_color(&runner, "PASSED:", console.CONSOLE_COLOR_GREEN);
    clm_test_write_line(&runner, " sample assertion");
    try std.testing.expectEqualStrings("\x1b[32mPASSED:\x1b[0m sample assertion\n", writer.buffered());
    try std.testing.expect(log.close());

    const log_text = try std.Io.Dir.cwd().readFileAlloc(
        std.testing.io,
        path,
        std.testing.allocator,
        .limited(256),
    );
    defer std.testing.allocator.free(log_text);
    try std.testing.expectEqualStrings("PASSED: sample assertion\n", log_text);
}

test "suite assertions update local and aggregate totals" {
    var output: [1024]u8 = undefined;
    var writer = Writer.fixed(&output);
    var runner = TestRunner.init(&writer, null);
    var result: clm_test_result = .{};

    clm_test_begin(&runner, "Example suite");
    clm_test_assert(&runner, &result, true, "truth holds");
    clm_test_assert(&runner, &result, false, "false is reported");
    clm_test_summary(&runner, result);

    try std.testing.expectEqual(@as(usize, 1), result.passed);
    try std.testing.expectEqual(@as(usize, 1), result.failed);
    try std.testing.expectEqual(@as(usize, 1), runner.total_passed);
    try std.testing.expectEqual(@as(usize, 1), runner.total_failed);
}
