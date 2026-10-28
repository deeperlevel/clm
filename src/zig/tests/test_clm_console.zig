//! Tests for CLM console input and output.

const std = @import("std");
const console = @import("clm_console");
const test_report = @import("test.zig");
const Writer = std.Io.Writer;
const Reader = std.Io.Reader;

fn assert(
    runner: *test_report.TestRunner,
    result: *test_report.clm_test_result,
    condition: bool,
    description: []const u8,
) void {
    test_report.clm_test_assert(runner, result, condition, description);
}

pub fn clm_console_tests(runner: *test_report.TestRunner, io: std.Io) void {
    var result: test_report.clm_test_result = .{};
    test_report.clm_test_begin(runner, "CLM - Console");

    assert(runner, &result, console.clm_console_init() == .success, "clm_console_init() == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_restore() == .success, "clm_console_restore() == CLM_CONSOLE_SUCCESS");
    var basic_output: [1]u8 = undefined;
    var basic_writer = Writer.fixed(&basic_output);
    assert(runner, &result, console.clm_console_flush(&basic_writer) == .success, "clm_console_flush() == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_wait(io, -1.0) == .invalid_argument, "clm_console_wait(-1.0) == CLM_CONSOLE_INVALID_ARGUMENT");
    assert(runner, &result, console.clm_console_wait(io, std.math.nan(f64)) == .invalid_argument, "clm_console_wait(NAN) == CLM_CONSOLE_INVALID_ARGUMENT");
    var empty_reader = Reader.fixed("");
    var undersized_buffer: [1]u8 = undefined;
    assert(runner, &result, console.clm_console_read_line_from(&empty_reader, &undersized_buffer) == .invalid_argument, "clm_console_read_line_from() rejects undersized buffer");

    var output: [512]u8 = undefined;
    var writer = Writer.fixed(&output);
    assert(runner, &result, console.clm_console_clear_to(&writer) == .success, "clm_console_clear_to(output) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_set_cursor_position_to(&writer, .{ .x = 2, .y = 4 }) == .success, "clm_console_set_cursor_position_to(output, x=2, y=4) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_set_cursor_position_to(&writer, .{ .x = -1, .y = 0 }) == .invalid_argument, "clm_console_set_cursor_position_to(output, x=-1, y=0) == CLM_CONSOLE_INVALID_ARGUMENT");
    assert(runner, &result, console.clm_console_set_cursor_visible_to(&writer, false) == .success, "clm_console_set_cursor_visible_to(output, false) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_set_cursor_visible_to(&writer, true) == .success, "clm_console_set_cursor_visible_to(output, true) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_to(&writer, "text") == .success, "clm_console_write_to(output, text) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_line_to(&writer, "line") == .success, "clm_console_write_line_to(output, line) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_format_to(&writer, " value={d} {s}", .{ 42, "ok" }) == .success, "clm_console_write_format_to(output, format) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_line_format_to(&writer, "line:{d:0>2}", .{7}) == .success, "clm_console_write_line_format_to(output, format) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_character_to(&writer, "X") == .success, "clm_console_write_character_to(output, X) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_character_to(&writer, "") == .invalid_argument, "clm_console_write_character_to(output, empty) == CLM_CONSOLE_INVALID_ARGUMENT");
    assert(runner, &result, console.clm_console_write_character_to(&writer, "XY") == .invalid_argument, "clm_console_write_character_to(output, XY) == CLM_CONSOLE_INVALID_ARGUMENT");
    assert(runner, &result, console.clm_console_write_character_to(&writer, "\xC3\xA9") == .success, "clm_console_write_character_to(output, UTF-8 scalar) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_color_to(&writer, "red", console.CONSOLE_COLOR_RED) == .success, "clm_console_write_color_to(output, red) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, console.clm_console_write_effect_to(io, &writer, "go", 0.0) == .success, "clm_console_write_effect_to(output, go, 0.0) == CLM_CONSOLE_SUCCESS");
    assert(runner, &result, std.mem.eql(u8, writer.buffered(), "\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25htextline\n value=42 okline:07\nX\xC3\xA9\x1b[31mred\x1b[0mgo"), "console output matches expected ANSI and formatted text");

    var line_reader = Reader.fixed("hello\r\nlast");
    var line_buffer: [16]u8 = undefined;
    assert(runner, &result, console.clm_console_read_line_from(&line_reader, &line_buffer) == .success and std.mem.eql(u8, std.mem.sliceTo(&line_buffer, 0), "hello"), "read line strips CRLF");
    assert(runner, &result, console.clm_console_read_line_from(&line_reader, &line_buffer) == .success and std.mem.eql(u8, std.mem.sliceTo(&line_buffer, 0), "last"), "read line accepts final line without LF");
    assert(runner, &result, console.clm_console_read_line_from(&line_reader, &line_buffer) == .failure, "read line reports EOF");

    var long_reader = Reader.fixed("123456789\nok\n");
    var short_buffer: [8]u8 = undefined;
    assert(runner, &result, console.clm_console_read_line_from(&long_reader, &short_buffer) == .failure, "read line rejects overlong input");
    assert(runner, &result, console.clm_console_read_line_from(&long_reader, &short_buffer) == .success and std.mem.eql(u8, std.mem.sliceTo(&short_buffer, 0), "ok"), "read line drains overlong input before next line");

    var character_reader = Reader.fixed("Q");
    var character: u8 = 0;
    assert(runner, &result, console.clm_console_read_character_from(&character_reader, &character) == .success and character == 'Q', "read character returns input byte");

    var yes_reader = Reader.fixed(" yes \n");
    var yes_output: [64]u8 = undefined;
    var yes_writer = Writer.fixed(&yes_output);
    var confirmed = false;
    var confirm_buffer: [16]u8 = undefined;
    assert(runner, &result, console.clm_console_confirm_from(&yes_reader, &yes_writer, "Continue?", &confirmed, &confirm_buffer) == .success and confirmed, "confirm accepts trimmed yes");
    assert(runner, &result, std.mem.eql(u8, yes_writer.buffered(), "Continue? [y/n]: "), "confirm writes prompt");

    var no_reader = Reader.fixed("no\n");
    var no_output: [32]u8 = undefined;
    var no_writer = Writer.fixed(&no_output);
    confirmed = true;
    assert(runner, &result, console.clm_console_confirm_from(&no_reader, &no_writer, null, &confirmed, &confirm_buffer) == .success and !confirmed, "confirm accepts no without custom prompt");

    var invalid_reader = Reader.fixed("yesterday\n");
    var invalid_output: [32]u8 = undefined;
    var invalid_writer = Writer.fixed(&invalid_output);
    confirmed = true;
    assert(runner, &result, console.clm_console_confirm_from(&invalid_reader, &invalid_writer, null, &confirmed, &confirm_buffer) == .invalid_argument and confirmed, "confirm rejects invalid response without changing output value");

    test_report.clm_test_summary(runner, result);
}
