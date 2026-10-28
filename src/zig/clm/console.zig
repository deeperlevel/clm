//! Console input, output, ANSI controls, and simple timing effects.

const std = @import("std");
const base = @import("clm_base");

pub const clm_console_status = enum(i32) {
    success = 0,
    failure = -1,
    invalid_argument = -2,
};

pub const CLM_CONSOLE_SUCCESS = clm_console_status.success;
pub const CLM_CONSOLE_FAILURE = clm_console_status.failure;
pub const CLM_CONSOLE_INVALID_ARGUMENT = clm_console_status.invalid_argument;

/// Capacity of the shared line/key input buffers, including the NUL terminator.
pub const CLM_CONSOLE_INPUT_CAPACITY = 4096;

// ANSI SGR reset
pub const CONSOLE_COLOR_RESET = "\x1b[0m";

// Regular Text Colors
pub const CONSOLE_COLOR_BLACK = "\x1b[30m";
pub const CONSOLE_COLOR_RED = "\x1b[31m";
pub const CONSOLE_COLOR_GREEN = "\x1b[32m";
pub const CONSOLE_COLOR_YELLOW = "\x1b[33m";
pub const CONSOLE_COLOR_BLUE = "\x1b[34m";
pub const CONSOLE_COLOR_MAGENTA = "\x1b[35m";
pub const CONSOLE_COLOR_CYAN = "\x1b[36m";
pub const CONSOLE_COLOR_WHITE = "\x1b[37m";

// Bold Text Colors
pub const CONSOLE_COLOR_BLACK_BOLD = "\x1b[1;30m";
pub const CONSOLE_COLOR_RED_BOLD = "\x1b[1;31m";
pub const CONSOLE_COLOR_GREEN_BOLD = "\x1b[1;32m";
pub const CONSOLE_COLOR_YELLOW_BOLD = "\x1b[1;33m";
pub const CONSOLE_COLOR_BLUE_BOLD = "\x1b[1;34m";
pub const CONSOLE_COLOR_MAGENTA_BOLD = "\x1b[1;35m";
pub const CONSOLE_COLOR_CYAN_BOLD = "\x1b[1;36m";
pub const CONSOLE_COLOR_WHITE_BOLD = "\x1b[1;37m";

// Background Colors
pub const CONSOLE_COLOR_BLACK_BG = "\x1b[40m";
pub const CONSOLE_COLOR_RED_BG = "\x1b[41m";
pub const CONSOLE_COLOR_GREEN_BG = "\x1b[42m";
pub const CONSOLE_COLOR_YELLOW_BG = "\x1b[43m";
pub const CONSOLE_COLOR_BLUE_BG = "\x1b[44m";
pub const CONSOLE_COLOR_MAGENTA_BG = "\x1b[45m";
pub const CONSOLE_COLOR_CYAN_BG = "\x1b[46m";
pub const CONSOLE_COLOR_WHITE_BG = "\x1b[47m";

const Writer = std.Io.Writer;
const Reader = std.Io.Reader;

/// Writes a byte slice to a writer without adding a newline.
/// @param writer Destination writer.
/// @param text Text to write.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_to(writer: *Writer, text: []const u8) clm_console_status {
    writer.writeAll(text) catch return .failure;
    return .success;
}

/// Initializes the console interface; currently performs no setup.
/// @return CLM_CONSOLE_SUCCESS.
/// /
pub fn clm_console_init() clm_console_status {
    return .success;
}

/// Restores console state; currently performs no platform-specific operation.
/// @return CLM_CONSOLE_SUCCESS.
/// /
pub fn clm_console_restore() clm_console_status {
    return .success;
}

/// Emits ANSI clear-screen and home-cursor controls to a writer.
/// The destination must be connected to an ANSI-compatible terminal for control
/// effects; redirected output may contain the escape sequence literally.
/// @param writer Destination writer.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_clear_to(writer: *Writer) clm_console_status {
    return clm_console_write_to(writer, "\x1b[2J\x1b[H");
}

/// Clears the supplied writer and flushes it.
/// @param writer Destination writer.
/// @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
/// /
pub fn clm_console_clear(writer: *Writer) clm_console_status {
    const status = clm_console_clear_to(writer);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Flushes a writer.
/// @param writer Destination writer.
/// @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
/// /
pub fn clm_console_flush(writer: *Writer) clm_console_status {
    writer.flush() catch return .failure;
    return .success;
}

/// Emits an ANSI cursor-position sequence to a writer.
/// Position coordinates are zero-based in the API and converted to ANSI's
/// one-based row/column coordinates.
/// @param writer Destination writer.
/// @param position Zero-based x/y cursor position.
/// @return SUCCESS, INVALID_ARGUMENT for a negative coordinate, or FAILURE.
/// /
pub fn clm_console_set_cursor_position_to(
    writer: *Writer,
    position: base.clm_position,
) clm_console_status {
    if (position.x < 0 or position.y < 0) return .invalid_argument;
    writer.print("\x1b[{d};{d}H", .{
        @as(u32, @intCast(position.y)) + 1,
        @as(u32, @intCast(position.x)) + 1,
    }) catch return .failure;
    return .success;
}

/// Moves the cursor on the supplied writer and flushes it.
/// @param writer Destination writer.
/// @param position Zero-based x/y cursor position.
/// @return SUCCESS, INVALID_ARGUMENT for negative coordinates, or FAILURE.
/// /
pub fn clm_console_set_cursor_position(
    writer: *Writer,
    position: base.clm_position,
) clm_console_status {
    const status = clm_console_set_cursor_position_to(writer, position);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Emits an ANSI sequence to show or hide the cursor on a writer.
/// @param writer Destination writer.
/// @param visible true to show the cursor; false to hide it.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_set_cursor_visible_to(
    writer: *Writer,
    visible: bool,
) clm_console_status {
    return clm_console_write_to(writer, if (visible) "\x1b[?25h" else "\x1b[?25l");
}

/// Shows the cursor on the supplied writer and flushes it.
/// @param writer Destination writer.
/// @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
/// /
pub fn clm_console_show_cursor(writer: *Writer) clm_console_status {
    const status = clm_console_set_cursor_visible_to(writer, true);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Hides the cursor on the supplied writer and flushes it.
/// @param writer Destination writer.
/// @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
/// /
pub fn clm_console_hide_cursor(writer: *Writer) clm_console_status {
    const status = clm_console_set_cursor_visible_to(writer, false);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Writes text without a newline.
/// @param writer Destination writer.
/// @param text Text to write.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write(writer: *Writer, text: []const u8) clm_console_status {
    return clm_console_write_to(writer, text);
}

/// Formats arguments and writes the result to a writer without a newline.
/// Formatting is checked at compile time using Zig's format syntax.
/// @param writer Destination writer.
/// @param format Zig format string.
/// @param arguments Values matching the format specifiers.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_format_to(
    writer: *Writer,
    comptime format: []const u8,
    arguments: anytype,
) clm_console_status {
    writer.print(format, arguments) catch return .failure;
    return .success;
}

/// Formats arguments and writes the result to a writer followed by LF.
/// @param writer Destination writer.
/// @param format Zig format string.
/// @param arguments Values matching the format specifiers.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_line_format_to(
    writer: *Writer,
    comptime format: []const u8,
    arguments: anytype,
) clm_console_status {
    const status = clm_console_write_format_to(writer, format, arguments);
    if (status != .success) return status;
    writer.writeByte('\n') catch return .failure;
    return .success;
}

/// Writes formatted output using a Zig argument tuple instead of a C va_list.
pub fn clm_console_vwrite_format_to(
    writer: *Writer,
    comptime format: []const u8,
    arguments: anytype,
) clm_console_status {
    return clm_console_write_format_to(writer, format, arguments);
}

/// Writes formatted output and LF using a Zig argument tuple instead of a C va_list.
pub fn clm_console_vwrite_line_format_to(
    writer: *Writer,
    comptime format: []const u8,
    arguments: anytype,
) clm_console_status {
    return clm_console_write_line_format_to(writer, format, arguments);
}

/// Writes formatted output to a writer without a newline.
/// Formatting is checked at compile time using Zig's format syntax.
/// @param writer Destination writer.
/// @param format Zig format string.
/// @param arguments Values matching the format specifiers.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_format(
    writer: *Writer,
    comptime format: []const u8,
    arguments: anytype,
) clm_console_status {
    return clm_console_write_format_to(writer, format, arguments);
}

/// Writes formatted output to a writer followed by LF and flushes it.
/// @param writer Destination writer.
/// @param format Zig format string.
/// @param arguments Values matching the format specifiers.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_line_format(
    writer: *Writer,
    comptime format: []const u8,
    arguments: anytype,
) clm_console_status {
    const status = clm_console_write_line_format_to(writer, format, arguments);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Writes text and a newline to a writer without flushing it.
/// @param writer Destination writer.
/// @param text Text to write.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_line_to(
    writer: *Writer,
    text: []const u8,
) clm_console_status {
    const status = clm_console_write_to(writer, text);
    if (status != .success) return status;
    writer.writeByte('\n') catch return .failure;
    return .success;
}

/// Writes text and a newline to a writer, then flushes it.
/// @param writer Destination writer.
/// @param text Text to write.
/// @return SUCCESS, or FAILURE on I/O error.
/// /
pub fn clm_console_write_line(writer: *Writer, text: []const u8) clm_console_status {
    const status = clm_console_write_line_to(writer, text);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Reads one line from a reader, stripping LF and an optional preceding CR.
/// Lines that exceed the supplied buffer are drained and reported as failures.
/// @param reader Input reader.
/// @param buffer Receives the line and a trailing NUL byte; must have at least
///               two bytes.
/// @return SUCCESS, INVALID_ARGUMENT for an undersized buffer, or FAILURE on
///         EOF, I/O error, or an overlong line.
/// /
pub fn clm_console_read_line_from(
    reader: *Reader,
    buffer: []u8,
) clm_console_status {
    if (buffer.len < 2) return .invalid_argument;

    var length: usize = 0;
    var overlong = false;
    while (true) {
        const byte = reader.takeByte() catch |err| {
            if (err == error.EndOfStream and length != 0 and !overlong) break;
            buffer[0] = 0;
            return .failure;
        };
        if (byte == '\n') break;
        if (length < buffer.len - 1 and !overlong) {
            buffer[length] = byte;
            length += 1;
        } else {
            overlong = true;
        }
    }

    if (overlong) {
        buffer[0] = 0;
        return .failure;
    }
    if (length != 0 and buffer[length - 1] == '\r') length -= 1;
    buffer[length] = 0;
    return .success;
}

/// Reads a line into caller-owned storage.
/// The returned slice is valid until the caller reuses the buffer.
/// @param reader Input reader.
/// @param buffer Receives the line; must have at least two bytes.
/// @param out_value Receives the line slice.
/// @return SUCCESS, INVALID_ARGUMENT for an undersized buffer, or FAILURE on
///         input error/EOF.
/// /
pub fn clm_console_read_line(
    reader: *Reader,
    buffer: []u8,
    out_value: *[]const u8,
) clm_console_status {
    const status = clm_console_read_line_from(reader, buffer);
    if (status != .success) return status;
    out_value.* = std.mem.sliceTo(buffer, 0);
    return .success;
}

/// Writes an optional prompt to a writer, flushes it, then reads a line.
/// @param reader Input reader.
/// @param writer Output writer.
/// @param prompt Optional prompt; null suppresses prompt output.
/// @param buffer Receives the line; must have at least two bytes.
/// @param out_value Receives the line slice.
/// @return SUCCESS, INVALID_ARGUMENT for an undersized buffer, or FAILURE.
/// /
pub fn clm_console_read(
    reader: *Reader,
    writer: *Writer,
    prompt: ?[]const u8,
    buffer: []u8,
    out_value: *[]const u8,
) clm_console_status {
    if (buffer.len < 2) return .invalid_argument;
    if (prompt) |text| {
        const status = clm_console_write_to(writer, text);
        if (status != .success) return status;
    }
    writer.flush() catch return .failure;
    return clm_console_read_line(reader, buffer, out_value);
}

/// Reads one byte from a reader.
/// This reads a byte, not a decoded Unicode character.
/// @param reader Input reader.
/// @param out_character Receives the byte.
/// @return SUCCESS, or FAILURE on EOF/I/O error.
/// /
pub fn clm_console_read_character_from(
    reader: *Reader,
    out_character: *u8,
) clm_console_status {
    out_character.* = reader.takeByte() catch return .failure;
    return .success;
}

/// Reads one byte into caller-owned, NUL-terminated storage.
/// Standard input may be line-buffered; this is not raw key input.
/// @param reader Input reader.
/// @param buffer Receives the one-byte string.
/// @param out_value Receives the one-byte slice.
/// @return SUCCESS, or FAILURE on EOF/input error.
/// /
pub fn clm_console_read_character(
    reader: *Reader,
    buffer: *[2]u8,
    out_value: *[]const u8,
) clm_console_status {
    const status = clm_console_read_character_from(reader, &buffer[0]);
    if (status != .success) return status;
    buffer[1] = 0;
    out_value.* = buffer[0..1];
    return .success;
}

/// Reads one byte; equivalent to clm_console_read_character.
/// @param reader Input reader.
/// @param buffer Receives the one-byte string.
/// @param out_value Receives the one-byte slice.
/// @return SUCCESS, or FAILURE on EOF/input error.
/// /
pub fn clm_console_read_key(
    reader: *Reader,
    buffer: *[2]u8,
    out_value: *[]const u8,
) clm_console_status {
    return clm_console_read_character(reader, buffer, out_value);
}

fn utf8ScalarLength(bytes: []const u8) ?usize {
    if (bytes.len == 0) return null;
    const first = bytes[0];
    const length: usize = if (first <= 0x7f) 1 else if (first >= 0xc2 and first <= 0xdf) 2 else if (first >= 0xe0 and first <= 0xef) 3 else if (first >= 0xf0 and first <= 0xf4) 4 else return null;
    if (bytes.len != length) return null;
    for (bytes[1..]) |byte| {
        if ((byte & 0xc0) != 0x80) return null;
    }
    if ((length == 3 and first == 0xe0 and bytes[1] < 0xa0) or
        (length == 3 and first == 0xed and bytes[1] >= 0xa0) or
        (length == 4 and first == 0xf0 and bytes[1] < 0x90) or
        (length == 4 and first == 0xf4 and bytes[1] >= 0x90)) return null;
    return length;
}

/// Writes one complete, valid UTF-8 encoded Unicode scalar value to a writer.
/// @param writer Destination writer.
/// @param character UTF-8 bytes containing exactly one scalar.
/// @return SUCCESS, INVALID_ARGUMENT for malformed/multiple sequences, or
///         FAILURE on I/O error.
/// /
pub fn clm_console_write_character_to(
    writer: *Writer,
    character: []const u8,
) clm_console_status {
    if (utf8ScalarLength(character) == null) return .invalid_argument;
    return clm_console_write_to(writer, character);
}

/// Writes one UTF-8 scalar value to a writer and flushes it.
/// @param writer Destination writer.
/// @param character UTF-8 bytes containing exactly one scalar.
/// @return SUCCESS, INVALID_ARGUMENT for malformed input, or FAILURE.
/// /
pub fn clm_console_write_character(
    writer: *Writer,
    character: []const u8,
) clm_console_status {
    const status = clm_console_write_character_to(writer, character);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

fn isAsciiWhitespace(byte: u8) bool {
    return byte == ' ' or (byte >= '\t' and byte <= '\r');
}

fn eqlAsciiIgnoreCase(left: []const u8, right: []const u8) bool {
    if (left.len != right.len) return false;
    for (left, right) |left_byte, right_byte| {
        if (std.ascii.toLower(left_byte) != right_byte) return false;
    }
    return true;
}

/// Prompts for and parses a yes/no response from supplied streams.
/// Leading/trailing ASCII whitespace is ignored; accepted answers are y/yes/n/no,
/// case-insensitively.
/// @param reader Input reader.
/// @param writer Output writer to which prompt text is written and flushed.
/// @param prompt Optional prompt text; null suppresses only the custom text.
/// @param out_value Receives true for yes and false for no.
/// @param buffer Caller-owned line buffer; must have at least two bytes.
/// @return SUCCESS, INVALID_ARGUMENT for invalid arguments/response, or FAILURE
///         on I/O error or an overlong input line.
/// /
pub fn clm_console_confirm_from(
    reader: *Reader,
    writer: *Writer,
    prompt: ?[]const u8,
    out_value: *bool,
    buffer: []u8,
) clm_console_status {
    if (buffer.len < 2) return .invalid_argument;
    if (prompt) |text| {
        const status = clm_console_write_to(writer, text);
        if (status != .success) return status;
    }
    const prompt_status = clm_console_write_to(writer, " [y/n]: ");
    if (prompt_status != .success) return prompt_status;
    writer.flush() catch return .failure;

    const status = clm_console_read_line_from(reader, buffer);
    if (status != .success) return status;
    const response = std.mem.sliceTo(buffer, 0);
    var first: usize = 0;
    var last: usize = response.len;
    while (first < last and isAsciiWhitespace(response[first])) : (first += 1) {}
    while (last > first and isAsciiWhitespace(response[last - 1])) : (last -= 1) {}
    const answer = response[first..last];
    if (eqlAsciiIgnoreCase(answer, "y") or eqlAsciiIgnoreCase(answer, "yes")) {
        out_value.* = true;
        return .success;
    }
    if (eqlAsciiIgnoreCase(answer, "n") or eqlAsciiIgnoreCase(answer, "no")) {
        out_value.* = false;
        return .success;
    }
    return .invalid_argument;
}

/// Prompts on supplied readers/writers for a yes/no response.
/// @param reader Input reader.
/// @param writer Output writer.
/// @param prompt Optional prompt text.
/// @param out_value Receives the parsed answer.
/// @param buffer Caller-owned line buffer; must have at least two bytes.
/// @return SUCCESS, INVALID_ARGUMENT for invalid response/input, or FAILURE.
/// /
pub fn clm_console_confirm(
    reader: *Reader,
    writer: *Writer,
    prompt: ?[]const u8,
    out_value: *bool,
    buffer: []u8,
) clm_console_status {
    return clm_console_confirm_from(reader, writer, prompt, out_value, buffer);
}

/// Writes ANSI color prefix, text, and reset sequence to a writer.
/// ANSI interpretation depends on the destination terminal.
/// @param writer Destination writer.
/// @param text Text to write.
/// @param color ANSI color/style sequence.
/// @return SUCCESS, or FAILURE on write error.
/// /
pub fn clm_console_write_color_to(
    writer: *Writer,
    text: []const u8,
    color: []const u8,
) clm_console_status {
    if (clm_console_write_to(writer, color) != .success or
        clm_console_write_to(writer, text) != .success or
        clm_console_write_to(writer, CONSOLE_COLOR_RESET) != .success) return .failure;
    return .success;
}

/// Writes ANSI-colored text to a writer and flushes it.
/// @param writer Destination writer.
/// @param text Text to write.
/// @param color ANSI color/style sequence.
/// @return SUCCESS, or FAILURE on I/O error.
/// /
pub fn clm_console_write_color(
    writer: *Writer,
    text: []const u8,
    color: []const u8,
) clm_console_status {
    const status = clm_console_write_color_to(writer, text, color);
    if (status != .success) return status;
    writer.flush() catch return .failure;
    return .success;
}

/// Waits for a duration using the supplied Zig I/O clock.
/// This yields to the I/O runtime rather than busy-waiting.
/// @param io I/O runtime used to measure and wait for time.
/// @param seconds Finite nonnegative duration in seconds.
/// @return SUCCESS, INVALID_ARGUMENT for negative/non-finite/out-of-range input,
///         or FAILURE if the clock or wait operation fails.
/// /
pub fn clm_console_wait(io: std.Io, seconds: f64) clm_console_status {
    if (!std.math.isFinite(seconds) or seconds < 0.0) return .invalid_argument;
    if (seconds == 0.0) return .success;
    const nanoseconds = seconds * 1_000_000_000.0;
    const maximum = @as(f64, @floatFromInt(std.math.maxInt(i96)));
    if (!std.math.isFinite(nanoseconds) or nanoseconds >= maximum) return .invalid_argument;
    const duration: i96 = @intFromFloat(nanoseconds);
    std.Io.sleep(io, .{ .nanoseconds = duration }, .awake) catch return .failure;
    return .success;
}

/// Writes text one byte at a time, waiting between bytes.
/// UTF-8 multibyte sequences are emitted byte-by-byte.
/// @param io I/O runtime used for the delay.
/// @param writer Destination writer.
/// @param text Text to write.
/// @param delay_seconds Finite nonnegative delay between bytes.
/// @return SUCCESS, INVALID_ARGUMENT for invalid delay, or FAILURE on I/O/write.
/// /
pub fn clm_console_write_effect_to(
    io: std.Io,
    writer: *Writer,
    text: []const u8,
    delay_seconds: f64,
) clm_console_status {
    if (!std.math.isFinite(delay_seconds) or delay_seconds < 0.0) return .invalid_argument;
    for (text, 0..) |byte, index| {
        writer.writeByte(byte) catch return .failure;
        writer.flush() catch return .failure;
        if (index + 1 < text.len and clm_console_wait(io, delay_seconds) != .success) return .failure;
    }
    return .success;
}

/// Writes text one byte at a time with a delay using the supplied I/O runtime.
/// @param io I/O runtime used for the delay.
/// @param writer Destination writer.
/// @param text Text to write.
/// @param delay_seconds Finite nonnegative delay between bytes.
/// @return SUCCESS, INVALID_ARGUMENT for invalid delay, or FAILURE on I/O/write.
/// /
pub fn clm_console_write_effect(
    io: std.Io,
    writer: *Writer,
    text: []const u8,
    delay_seconds: f64,
) clm_console_status {
    return clm_console_write_effect_to(io, writer, text, delay_seconds);
}

test "formatted output and cursor position" {
    var storage: [64]u8 = undefined;
    var writer = Writer.fixed(&storage);
    try std.testing.expectEqual(.success, clm_console_write_format(&writer, "value={d}", .{42}));
    try std.testing.expectEqual(.success, clm_console_set_cursor_position_to(&writer, .{ .x = 2, .y = 1 }));
    try std.testing.expectEqualStrings("value=42\x1b[2;3H", writer.buffered());
}

test "line input strips CRLF and terminates the caller buffer" {
    var reader = Reader.fixed("hello\r\n");
    var buffer: [16]u8 = undefined;
    var line: []const u8 = undefined;
    try std.testing.expectEqual(.success, clm_console_read_line(&reader, &buffer, &line));
    try std.testing.expectEqualStrings("hello", line);
    try std.testing.expectEqual(@as(u8, 0), buffer[line.len]);
}

test "character output accepts exactly one UTF-8 scalar" {
    var storage: [8]u8 = undefined;
    var writer = Writer.fixed(&storage);
    try std.testing.expectEqual(.success, clm_console_write_character_to(&writer, "λ"));
    try std.testing.expectEqual(.invalid_argument, clm_console_write_character_to(&writer, "ab"));
    try std.testing.expectEqualStrings("λ", writer.buffered());
}

test "confirmation trims whitespace and accepts mixed case" {
    var reader = Reader.fixed(" YeS \r\n");
    var output: [32]u8 = undefined;
    var writer = Writer.fixed(&output);
    var buffer: [32]u8 = undefined;
    var answer = false;
    try std.testing.expectEqual(.success, clm_console_confirm_from(&reader, &writer, "Continue?", &answer, &buffer));
    try std.testing.expect(answer);
    try std.testing.expectEqualStrings("Continue? [y/n]: ", writer.buffered());
}

test "overlong input is drained before the next line" {
    var reader = Reader.fixed("long\nok\n");
    var buffer: [4]u8 = undefined;
    try std.testing.expectEqual(.failure, clm_console_read_line_from(&reader, &buffer));
    try std.testing.expectEqual(@as(u8, 0), buffer[0]);
    try std.testing.expectEqual(.success, clm_console_read_line_from(&reader, &buffer));
    try std.testing.expectEqualStrings("ok", std.mem.sliceTo(&buffer, 0));
}
