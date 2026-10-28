package clm

import "core:fmt"
import "core:math"
import "core:os"
import "core:time"

// Uses Odin strings and fmt verbs, ^os.File streams, and pointer outputs.
// Empty strings are valid text (there is no C NULL-string sentinel).
// Input strings borrow shared buffers; clone them before the next read.
// Waits sleep instead of consuming CPU in the C binding's busy loop.
/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
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

Console_Status :: i32
/*
Result codes returned by console operations.
*/
clm_console_status :: Console_Status
// Operation completed successfully.
CLM_CONSOLE_SUCCESS         :: Console_Status(0)
// I/O, formatting, allocation, or clock operation failed.
CLM_CONSOLE_FAILURE         :: Console_Status(-1)
// A required argument or value is invalid.
CLM_CONSOLE_INVALID_ARGUMENT :: Console_Status(-2)

/*
Capacity of the shared line/key input buffers, including the NUL terminator.
*/
CLM_CONSOLE_INPUT_CAPACITY :: 4096

CONSOLE_COLOR_RESET        :: "\x1b[0m"
CONSOLE_COLOR_BLACK        :: "\x1b[30m"
CONSOLE_COLOR_RED          :: "\x1b[31m"
CONSOLE_COLOR_GREEN        :: "\x1b[32m"
CONSOLE_COLOR_YELLOW       :: "\x1b[33m"
CONSOLE_COLOR_BLUE         :: "\x1b[34m"
CONSOLE_COLOR_MAGENTA      :: "\x1b[35m"
CONSOLE_COLOR_CYAN         :: "\x1b[36m"
CONSOLE_COLOR_WHITE        :: "\x1b[37m"
CONSOLE_COLOR_BLACK_BOLD   :: "\x1b[1;30m"
CONSOLE_COLOR_RED_BOLD     :: "\x1b[1;31m"
CONSOLE_COLOR_GREEN_BOLD   :: "\x1b[1;32m"
CONSOLE_COLOR_YELLOW_BOLD  :: "\x1b[1;33m"
CONSOLE_COLOR_BLUE_BOLD    :: "\x1b[1;34m"
CONSOLE_COLOR_MAGENTA_BOLD :: "\x1b[1;35m"
CONSOLE_COLOR_CYAN_BOLD    :: "\x1b[1;36m"
CONSOLE_COLOR_WHITE_BOLD   :: "\x1b[1;37m"
CONSOLE_COLOR_BLACK_BG     :: "\x1b[40m"
CONSOLE_COLOR_RED_BG       :: "\x1b[41m"
CONSOLE_COLOR_GREEN_BG     :: "\x1b[42m"
CONSOLE_COLOR_YELLOW_BG    :: "\x1b[43m"
CONSOLE_COLOR_BLUE_BG      :: "\x1b[44m"
CONSOLE_COLOR_MAGENTA_BG   :: "\x1b[45m"
CONSOLE_COLOR_CYAN_BG      :: "\x1b[46m"
CONSOLE_COLOR_WHITE_BG     :: "\x1b[47m"

console_input_buffer: [CLM_CONSOLE_INPUT_CAPACITY]u8
console_character_buffer: [2]u8

/*
Writes an Odin string to a stream without adding a newline.
stream: Destination stream.
text: Text to write.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream, or FAILURE on I/O error.
*/
clm_console_write_to :: proc(stream: ^os.File, text: string) -> Console_Status {
	if stream == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	n, err := os.write(stream, transmute([]u8)text)
	if err != nil || n != len(text) {
		return CLM_CONSOLE_FAILURE
	}
	return CLM_CONSOLE_SUCCESS
}

/*
Initializes the console interface; currently performs no setup.
Returns: CLM_CONSOLE_SUCCESS.
*/
clm_console_init :: proc() -> Console_Status {
	return CLM_CONSOLE_SUCCESS
}

/*
Restores console state; currently performs no platform-specific operation.
Returns: CLM_CONSOLE_SUCCESS.
*/
clm_console_restore :: proc() -> Console_Status {
	return CLM_CONSOLE_SUCCESS
}

/*
Emits ANSI clear-screen and home-cursor controls to a stream.
The stream must be connected to an ANSI-compatible terminal for control
effects; redirected output may contain the escape sequence literally.
stream: Destination stream.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream, or FAILURE on I/O error.
*/
clm_console_clear_to :: proc(stream: ^os.File) -> Console_Status {
	return clm_console_write_to(stream, "\x1b[2J\x1b[H")
}

/*
Clears stdout and flushes it.
Returns: CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
*/
clm_console_clear :: proc() -> Console_Status {
	status := clm_console_clear_to(os.stdout)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Flushes stdout.
Returns: CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
*/
clm_console_flush :: proc() -> Console_Status {
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Emits an ANSI cursor-position sequence to a stream.
Position coordinates are zero-based in the API and converted to ANSI's
one-based row/column coordinates.
stream: Destination stream.
position: Zero-based x/y cursor position.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream or negative coordinate,
        or FAILURE on formatting/I/O error.
*/
clm_console_set_cursor_position_to :: proc(stream: ^os.File, position: position_i32) -> Console_Status {
	if stream == nil || position.x < 0 || position.y < 0 {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	sequence := fmt.aprintf("\x1b[%d;%dH", i64(position.y)+1, i64(position.x)+1)
	defer delete(sequence)
	return clm_console_write_to(stream, sequence)
}

/*
Moves the cursor on stdout and flushes the stream.
position: Zero-based x/y cursor position.
Returns: SUCCESS, INVALID_ARGUMENT for negative coordinates, or FAILURE.
*/
clm_console_set_cursor_position :: proc(position: position_i32) -> Console_Status {
	status := clm_console_set_cursor_position_to(os.stdout, position)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Emits an ANSI sequence to show or hide the cursor on a stream.
stream: Destination stream.
visible: true to show the cursor; false to hide it.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream, or FAILURE on I/O error.
*/
clm_console_set_cursor_visible_to :: proc(stream: ^os.File, visible: bool) -> Console_Status {
	return clm_console_write_to(stream, "\x1b[?25h" if visible else "\x1b[?25l")
}

/*
Shows the cursor on stdout and flushes the stream.
Returns: CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
*/
clm_console_show_cursor :: proc() -> Console_Status {
	status := clm_console_set_cursor_visible_to(os.stdout, true)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Hides the cursor on stdout and flushes the stream.
Returns: CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
*/
clm_console_hide_cursor :: proc() -> Console_Status {
	status := clm_console_set_cursor_visible_to(os.stdout, false)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Writes text to stdout without a newline.
text: Odin string.
Returns: SUCCESS, or FAILURE on I/O error.
*/
clm_console_write :: proc(text: string) -> Console_Status {
	return clm_console_write_to(os.stdout, text)
}

/*
Writes Odin fmt-style formatted output to a stream without a newline.
stream: Destination stream.
format: Odin fmt-style format string.
args: Values matching the format specifiers.
Returns: SUCCESS, INVALID_ARGUMENT for nil stream, or FAILURE.
*/
clm_console_write_format_to :: proc(stream: ^os.File, format: string, args: ..any) -> Console_Status {
	if stream == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	output := fmt.aprintf(format, ..args)
	defer delete(output)
	return clm_console_write_to(stream, output)
}

/*
Formats variadic args and writes the result to a stream.
Accepts native variadic args; forward a slice with ..args.
stream: Destination stream.
format: Odin fmt-style format string.
args: Values matching the format specifiers.
Returns: SUCCESS, INVALID_ARGUMENT for nil stream, or FAILURE.
*/
clm_console_vwrite_format_to :: proc(stream: ^os.File, format: string, args: ..any) -> Console_Status {
	return clm_console_write_format_to(stream, format, ..args)
}

/*
Writes Odin fmt-style formatted output to a stream followed by LF.
stream: Destination stream.
format: Odin fmt-style format string.
args: Values matching the format specifiers.
Returns: SUCCESS, INVALID_ARGUMENT for nil stream, or FAILURE.
*/
clm_console_write_line_format_to :: proc(stream: ^os.File, format: string, args: ..any) -> Console_Status {
	status := clm_console_write_format_to(stream, format, ..args)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return clm_console_write_to(stream, "\n")
}

/*
Formats and writes a line to a stream, appending one LF character.
stream: Destination stream.
format: Odin fmt-style format string.
args: Values matching the format specifiers.
Returns: SUCCESS, INVALID_ARGUMENT for nil stream, or FAILURE.
*/
clm_console_vwrite_line_format_to :: proc(stream: ^os.File, format: string, args: ..any) -> Console_Status {
	return clm_console_write_line_format_to(stream, format, ..args)
}

/*
Writes Odin fmt-style formatted output to stdout without a newline.
format: Odin fmt-style format string.
args: Values matching the format specifiers.
Returns: SUCCESS, or FAILURE.
*/
clm_console_write_format :: proc(format: string, args: ..any) -> Console_Status {
	return clm_console_write_format_to(os.stdout, format, ..args)
}

/*
Writes Odin fmt-style formatted output to stdout followed by LF, then flushes.
format: Odin fmt-style format string.
args: Values matching the format specifiers.
Returns: SUCCESS, or FAILURE.
*/
clm_console_write_line_format :: proc(format: string, args: ..any) -> Console_Status {
	status := clm_console_write_line_format_to(os.stdout, format, ..args)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Writes text and a newline to a stream without flushing it.
stream: Destination stream.
text: Odin string.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream, or FAILURE on I/O error.
*/
clm_console_write_line_to :: proc(stream: ^os.File, text: string) -> Console_Status {
	status := clm_console_write_to(stream, text)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return clm_console_write_to(stream, "\n")
}

/*
Writes text and a newline to stdout, then flushes it.
text: Odin string.
Returns: SUCCESS, or FAILURE on I/O error.
*/
clm_console_write_line :: proc(text: string) -> Console_Status {
	status := clm_console_write_line_to(os.stdout, text)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

console_read_byte :: proc(stream: ^os.File, byte: ^u8) -> (Console_Status, bool) {
	buffer: [1]u8
	n, err := os.read(stream, buffer[:])
	if n == 1 && (err == nil || err == .EOF) {
		byte^ = buffer[0]
		return CLM_CONSOLE_SUCCESS, false
	}
	if err == .EOF || (err == nil && n == 0) {
		return CLM_CONSOLE_FAILURE, true
	}
	return CLM_CONSOLE_FAILURE, false
}

/*
Reads one line from a stream, stripping LF and an optional preceding CR.
Lines that exceed capacity are drained and reported as failures.
stream: Input stream.
buffer: Receives the NUL-terminated line; len(buffer) must be at least 2.

Returns: SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on EOF,
        I/O error, or an overlong line.
*/
clm_console_read_line_from :: proc(stream: ^os.File, buffer: []u8) -> Console_Status {
	if stream == nil || len(buffer) < 2 {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	length := 0
	for length < len(buffer)-1 {
		byte: u8
		status, eof := console_read_byte(stream, &byte)
		if status != CLM_CONSOLE_SUCCESS {
			if eof && length > 0 {
				if buffer[length-1] == '\r' {
					length -= 1
				}
				buffer[length] = 0
				return CLM_CONSOLE_SUCCESS
			}
			buffer[0] = 0
			return CLM_CONSOLE_FAILURE
		}
		if byte == '\n' {
			if length > 0 && buffer[length-1] == '\r' {
				length -= 1
			}
			buffer[length] = 0
			return CLM_CONSOLE_SUCCESS
		}
		buffer[length] = byte
		length += 1
	}

	next: u8
	next_status, eof := console_read_byte(stream, &next)
	if next_status == CLM_CONSOLE_SUCCESS && next != '\n' {
		for next_status == CLM_CONSOLE_SUCCESS && next != '\n' {
			next_status, eof = console_read_byte(stream, &next)
		}
		buffer[0] = 0
		return CLM_CONSOLE_FAILURE
	}
	if next_status == CLM_CONSOLE_FAILURE && (!eof || length == 0) {
		buffer[0] = 0
		return CLM_CONSOLE_FAILURE
	}
	if length > 0 && buffer[length-1] == '\r' {
		length -= 1
	}
	buffer[length] = 0
	return CLM_CONSOLE_SUCCESS
}

/*
Reads a line from stdin into shared internal storage.
The returned string borrows shared storage overwritten by the next line read.
Input capacity is CLM_CONSOLE_INPUT_CAPACITY bytes including the terminator.
out_value: Receives the borrowed line string; must not be nil.
Returns: SUCCESS, INVALID_ARGUMENT for nil, or FAILURE on input error/EOF.
*/
clm_console_read_line :: proc(out_value: ^string) -> Console_Status {
	if out_value == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	status := clm_console_read_line_from(os.stdin, console_input_buffer[:])
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	length := 0
	for length < len(console_input_buffer) && console_input_buffer[length] != 0 {
		length += 1
	}
	out_value^ = string(console_input_buffer[:length])
	return CLM_CONSOLE_SUCCESS
}

/*
Writes an optional prompt to stdout, flushes it, then reads a line.
prompt: Optional prompt; an empty string suppresses prompt output.
out_value: Receives a string borrowing shared input storage.
Returns: SUCCESS, INVALID_ARGUMENT for nil output, or FAILURE on I/O/input.
*/
clm_console_read :: proc(prompt: string, out_value: ^string) -> Console_Status {
	if out_value == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	if prompt != "" && clm_console_write_to(os.stdout, prompt) != CLM_CONSOLE_SUCCESS {
		return CLM_CONSOLE_FAILURE
	}
	if os.flush(os.stdout) != nil {
		return CLM_CONSOLE_FAILURE
	}
	return clm_console_read_line(out_value)
}

/*
Reads one byte from a stream.
This reads a byte, not a decoded Unicode character.
stream: Input stream.
out_character: Receives the byte as u8; must not be nil.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream/output, or FAILURE on EOF/I/O.
*/
clm_console_read_character_from :: proc(stream: ^os.File, out_character: ^u8) -> Console_Status {
	if stream == nil || out_character == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	status, _ := console_read_byte(stream, out_character)
	return status
}

/*
Reads one byte from stdin into shared, NUL-terminated storage.
Terminal input may be line-buffered; this is not raw key input.
The string borrows shared storage overwritten by the next character read.
out_value: Receives the shared one-byte string; must not be nil.
Returns: SUCCESS, INVALID_ARGUMENT for nil, or FAILURE on EOF/input error.
*/
clm_console_read_character :: proc(out_value: ^string) -> Console_Status {
	if out_value == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	status := clm_console_read_character_from(os.stdin, &console_character_buffer[0])
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	console_character_buffer[1] = 0
	out_value^ = string(console_character_buffer[:1])
	return CLM_CONSOLE_SUCCESS
}

/*
Reads one byte from stdin; equivalent to clm_console_read_character.
out_value: Receives a string borrowing shared one-byte storage.
Returns: SUCCESS, INVALID_ARGUMENT for nil, or FAILURE on EOF/input error.
*/
clm_console_read_key :: proc(out_value: ^string) -> Console_Status {
	return clm_console_read_character(out_value)
}

/*
Writes one complete, valid UTF-8 encoded Unicode scalar value to a stream.
stream: Destination stream.
character: UTF-8 string containing exactly one scalar.
Returns: SUCCESS, INVALID_ARGUMENT for empty/malformed/multiple sequences or a nil stream,
        or FAILURE on I/O error.
*/
clm_console_write_character_to :: proc(stream: ^os.File, character: string) -> Console_Status {
	if stream == nil || len(character) == 0 || character[0] == 0 {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	first := character[0]
	length := 0
	if first <= 0x7f {
		length = 1
	} else if first >= 0xc2 && first <= 0xdf {
		length = 2
	} else if first >= 0xe0 && first <= 0xef {
		length = 3
	} else if first >= 0xf0 && first <= 0xf4 {
		length = 4
	} else {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	if len(character) != length {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	for i in 1..<length {
		if character[i] < 0x80 || character[i] > 0xbf {
			return CLM_CONSOLE_INVALID_ARGUMENT
		}
	}
	if (length == 3 && first == 0xe0 && character[1] < 0xa0) ||
	   (length == 3 && first == 0xed && character[1] >= 0xa0) ||
	   (length == 4 && first == 0xf0 && character[1] < 0x90) ||
	   (length == 4 && first == 0xf4 && character[1] >= 0x90) {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	return clm_console_write_to(stream, character)
}

/*
Writes one UTF-8 scalar value to stdout and flushes it.
character: UTF-8 string containing exactly one scalar.
Returns: SUCCESS, INVALID_ARGUMENT for malformed input, or FAILURE.
*/
clm_console_write_character :: proc(character: string) -> Console_Status {
	status := clm_console_write_character_to(os.stdout, character)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

console_ascii_space :: proc(byte: u8) -> bool {
	return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\v' || byte == '\f'
}

console_ascii_lower :: proc(byte: u8) -> u8 {
	return byte + ('a' - 'A') if byte >= 'A' && byte <= 'Z' else byte
}

/*
Prompts for and parses a yes/no response from supplied streams.
Leading/trailing whitespace is ignored; accepted answers are y/yes/n/no,
case-insensitively.
input: Stream from which the response line is read.
output: Stream to which prompt text is written and flushed.
prompt: Optional prompt text; an empty string suppresses only the custom text.
out_value: Receives true for yes and false for no.
Returns: SUCCESS, INVALID_ARGUMENT for invalid arguments/response, or FAILURE
        on I/O error or an overlong input line.
*/
clm_console_confirm_from :: proc(input: ^os.File, output: ^os.File, prompt: string, out_value: ^bool) -> Console_Status {
	if input == nil || output == nil || out_value == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	if prompt != "" && clm_console_write_to(output, prompt) != CLM_CONSOLE_SUCCESS {
		return CLM_CONSOLE_FAILURE
	}
	if clm_console_write_to(output, " [y/n]: ") != CLM_CONSOLE_SUCCESS || os.flush(output) != nil {
		return CLM_CONSOLE_FAILURE
	}
	response: [CLM_CONSOLE_INPUT_CAPACITY]u8
	status := clm_console_read_line_from(input, response[:])
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	start, end := 0, 0
	for end < len(response) && response[end] != 0 {
		end += 1
	}
	for start < end && console_ascii_space(response[start]) {
		start += 1
	}
	for end > start && console_ascii_space(response[end-1]) {
		end -= 1
	}
	length := end - start
	if length == 1 {
		answer := console_ascii_lower(response[start])
		if answer == 'y' || answer == 'n' {
			out_value^ = answer == 'y'
			return CLM_CONSOLE_SUCCESS
		}
	}
	if length == 3 &&
	   console_ascii_lower(response[start]) == 'y' &&
	   console_ascii_lower(response[start+1]) == 'e' &&
	   console_ascii_lower(response[start+2]) == 's' {
		out_value^ = true
		return CLM_CONSOLE_SUCCESS
	}
	if length == 2 &&
	   console_ascii_lower(response[start]) == 'n' &&
	   console_ascii_lower(response[start+1]) == 'o' {
		out_value^ = false
		return CLM_CONSOLE_SUCCESS
	}
	return CLM_CONSOLE_INVALID_ARGUMENT
}

/*
Prompts on stdin/stdout for a yes/no response.
prompt: Optional prompt text.
out_value: Receives the parsed answer; must not be nil.
Returns: SUCCESS, INVALID_ARGUMENT for invalid response/input, or FAILURE.
*/
clm_console_confirm :: proc(prompt: string, out_value: ^bool) -> Console_Status {
	return clm_console_confirm_from(os.stdin, os.stdout, prompt, out_value)
}

/*
Writes ANSI color prefix, text, and reset sequence to a stream.
ANSI interpretation depends on the destination terminal.
stream: Destination stream.
text: Text to write.
color: ANSI color/style sequence.
Returns: SUCCESS, INVALID_ARGUMENT for a nil stream, or FAILURE on I/O.
*/
clm_console_write_color_to :: proc(stream: ^os.File, text: string, color: string) -> Console_Status {
	if stream == nil {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	if clm_console_write_to(stream, color) != CLM_CONSOLE_SUCCESS ||
	   clm_console_write_to(stream, text) != CLM_CONSOLE_SUCCESS ||
	   clm_console_write_to(stream, CONSOLE_COLOR_RESET) != CLM_CONSOLE_SUCCESS {
		return CLM_CONSOLE_FAILURE
	}
	return CLM_CONSOLE_SUCCESS
}

/*
Writes ANSI-colored text to stdout and flushes it.
text: Text to write.
color: ANSI color/style sequence.
Returns: SUCCESS, or FAILURE on I/O.
*/
clm_console_write_color :: proc(text: string, color: string) -> Console_Status {
	status := clm_console_write_color_to(os.stdout, text, color)
	if status != CLM_CONSOLE_SUCCESS {
		return status
	}
	return CLM_CONSOLE_FAILURE if os.flush(os.stdout) != nil else CLM_CONSOLE_SUCCESS
}

/*
Sleeps for a duration without busy-waiting.
Unlike the C implementation, this uses core:time.sleep.
seconds: Finite nonnegative seconds representable as an i64 nanosecond duration.
Returns: SUCCESS, or INVALID_ARGUMENT for negative/non-finite/unrepresentable input.
*/
clm_console_wait :: proc(seconds: f64) -> Console_Status {
	if math.is_nan(seconds) || math.is_inf(seconds) || seconds < 0 {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	if seconds == 0 {
		return CLM_CONSOLE_SUCCESS
	}
	max_seconds := f64(max(i64))
	if seconds > max_seconds / 1e9 {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	time.sleep(time.Duration(i64(seconds * 1e9)))
	return CLM_CONSOLE_SUCCESS
}

/*
Writes text one byte at a time, waiting between bytes.
UTF-8 multibyte sequences are emitted byte-by-byte. Waiting sleeps between bytes.
stream: Destination stream.
text: Odin string.
delay_seconds: Finite nonnegative delay between bytes.
Returns: SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on I/O
        or invalid wait duration.
*/
clm_console_write_effect_to :: proc(stream: ^os.File, text: string, delay_seconds: f64) -> Console_Status {
	if stream == nil || math.is_nan(delay_seconds) || math.is_inf(delay_seconds) || delay_seconds < 0 {
		return CLM_CONSOLE_INVALID_ARGUMENT
	}
	for i in 0..<len(text) {
		n, err := os.write(stream, []u8{text[i]})
		if err != nil || n != 1 || os.flush(stream) != nil {
			return CLM_CONSOLE_FAILURE
		}
		if i+1 < len(text) && delay_seconds > 0 {
			if clm_console_wait(delay_seconds) != CLM_CONSOLE_SUCCESS {
				return CLM_CONSOLE_FAILURE
			}
		}
	}
	return CLM_CONSOLE_SUCCESS
}

/*
Writes text to stdout one byte at a time with a sleeping delay.
text: Odin string.
delay_seconds: Finite nonnegative delay between bytes.
Returns: SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on I/O
        or invalid wait duration.
*/
clm_console_write_effect :: proc(text: string, delay_seconds: f64) -> Console_Status {
	return clm_console_write_effect_to(os.stdout, text, delay_seconds)
}
