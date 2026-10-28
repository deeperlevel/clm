//! Standard-stream and ANSI terminal helpers.

use super::base::Position;
use std::fmt;
use std::io::{self, BufRead, Read, Write};
use std::thread;
use std::time::Duration;

/// Result codes returned by console operations.
#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ConsoleStatus {
	/// Operation completed successfully.
	Success = 0,
	/// I/O, formatting, allocation, or clock operation failed.
	Failure = -1,
	/// A required argument or value is invalid.
	InvalidArgument = -2,
}

/// Capacity of the standard line input buffer, including the NUL terminator
/// used by the C interface.
pub const INPUT_CAPACITY: usize = 4096;

pub const COLOR_RESET: &str = "\x1b[0m";
pub const COLOR_BLACK: &str = "\x1b[30m";
pub const COLOR_RED: &str = "\x1b[31m";
pub const COLOR_GREEN: &str = "\x1b[32m";
pub const COLOR_YELLOW: &str = "\x1b[33m";
pub const COLOR_BLUE: &str = "\x1b[34m";
pub const COLOR_MAGENTA: &str = "\x1b[35m";
pub const COLOR_CYAN: &str = "\x1b[36m";
pub const COLOR_WHITE: &str = "\x1b[37m";

pub const COLOR_BLACK_BOLD: &str = "\x1b[1;30m";
pub const COLOR_RED_BOLD: &str = "\x1b[1;31m";
pub const COLOR_GREEN_BOLD: &str = "\x1b[1;32m";
pub const COLOR_YELLOW_BOLD: &str = "\x1b[1;33m";
pub const COLOR_BLUE_BOLD: &str = "\x1b[1;34m";
pub const COLOR_MAGENTA_BOLD: &str = "\x1b[1;35m";
pub const COLOR_CYAN_BOLD: &str = "\x1b[1;36m";
pub const COLOR_WHITE_BOLD: &str = "\x1b[1;37m";

pub const COLOR_BLACK_BG: &str = "\x1b[40m";
pub const COLOR_RED_BG: &str = "\x1b[41m";
pub const COLOR_GREEN_BG: &str = "\x1b[42m";
pub const COLOR_YELLOW_BG: &str = "\x1b[43m";
pub const COLOR_BLUE_BG: &str = "\x1b[44m";
pub const COLOR_MAGENTA_BG: &str = "\x1b[45m";
pub const COLOR_CYAN_BG: &str = "\x1b[46m";
pub const COLOR_WHITE_BG: &str = "\x1b[47m";

fn status_from_io(result: io::Result<()>) -> ConsoleStatus {
	match result {
		Ok(()) => ConsoleStatus::Success,
		Err(_) => ConsoleStatus::Failure,
	}
}

/// Writes text to a stream without adding a newline.
///
/// The stream is supplied by mutable reference, so a null stream cannot be
/// passed. I/O errors return [`ConsoleStatus::Failure`].
pub fn write_to<W: Write>(stream: &mut W, text: &str) -> ConsoleStatus {
	status_from_io(stream.write_all(text.as_bytes()))
}

/// Initializes the standard console interface; currently performs no setup.
pub fn init() -> ConsoleStatus {
	ConsoleStatus::Success
}

/// Restores console state; currently performs no platform-specific operation.
pub fn restore() -> ConsoleStatus {
	ConsoleStatus::Success
}

/// Emits ANSI clear-screen and home-cursor controls to a stream.
///
/// The stream must be connected to an ANSI-compatible terminal for control
/// effects; redirected output may contain the escape sequence literally.
pub fn clear_to<W: Write>(stream: &mut W) -> ConsoleStatus {
	write_to(stream, "\x1b[2J\x1b[H")
}

/// Clears stdout and flushes it.
pub fn clear() -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	if clear_to(&mut stream) != ConsoleStatus::Success {
		return ConsoleStatus::Failure;
	}
	status_from_io(stream.flush())
}

/// Flushes stdout.
pub fn flush() -> ConsoleStatus {
	let stdout = io::stdout();
	status_from_io(stdout.lock().flush())
}

/// Emits an ANSI cursor-position sequence to a stream.
///
/// Position coordinates are zero-based in the API and converted to ANSI's
/// one-based row/column coordinates. Negative coordinates are invalid.
pub fn set_cursor_position_to<W: Write>(
	stream: &mut W,
	position: Position,
) -> ConsoleStatus {
	if position.x < 0 || position.y < 0 {
		return ConsoleStatus::InvalidArgument;
	}
	let row = position.y as u32 + 1;
	let column = position.x as u32 + 1;
	status_from_io(write!(stream, "\x1b[{row};{column}H"))
}

/// Moves the cursor on stdout and flushes the stream.
pub fn set_cursor_position(position: Position) -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	let status = set_cursor_position_to(&mut stream, position);
	if status == ConsoleStatus::Success {
		status_from_io(stream.flush())
	} else {
		status
	}
}

/// Emits an ANSI sequence to show or hide the cursor on a stream.
pub fn set_cursor_visible_to<W: Write>(stream: &mut W, visible: bool) -> ConsoleStatus {
	write_to(stream, if visible { "\x1b[?25h" } else { "\x1b[?25l" })
}

/// Shows the cursor on stdout and flushes the stream.
pub fn show_cursor() -> ConsoleStatus {
	set_cursor_visible(true)
}

/// Hides the cursor on stdout and flushes the stream.
pub fn hide_cursor() -> ConsoleStatus {
	set_cursor_visible(false)
}

fn set_cursor_visible(visible: bool) -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	let status = set_cursor_visible_to(&mut stream, visible);
	if status == ConsoleStatus::Success {
		status_from_io(stream.flush())
	} else {
		status
	}
}

/// Writes text to stdout without a newline.
pub fn write(text: &str) -> ConsoleStatus {
	let stdout = io::stdout();
	write_to(&mut stdout.lock(), text)
}

/// Formats and writes output to a stream without a newline.
///
/// Use Rust formatting syntax, for example `write_format_to(stream,
/// format_args!("{name}: {count}", name = "items", count = 3))`.
pub fn write_format_to<W: Write>(
	stream: &mut W,
	arguments: fmt::Arguments<'_>,
) -> ConsoleStatus {
	status_from_io(stream.write_fmt(arguments))
}

/// Formats and writes a line to a stream, appending one LF character.
pub fn write_line_format_to<W: Write>(
	stream: &mut W,
	arguments: fmt::Arguments<'_>,
) -> ConsoleStatus {
	let status = write_format_to(stream, arguments);
	if status != ConsoleStatus::Success {
		return status;
	}
	status_from_io(stream.write_all(b"\n"))
}

/// Formats and writes output to stdout without a newline.
pub fn write_format(arguments: fmt::Arguments<'_>) -> ConsoleStatus {
	let stdout = io::stdout();
	write_format_to(&mut stdout.lock(), arguments)
}

/// Formats and writes a line to stdout, then flushes it.
pub fn write_line_format(arguments: fmt::Arguments<'_>) -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	let status = write_line_format_to(&mut stream, arguments);
	if status == ConsoleStatus::Success {
		status_from_io(stream.flush())
	} else {
		status
	}
}

/// Writes text and a newline to a stream without flushing it.
pub fn write_line_to<W: Write>(stream: &mut W, text: &str) -> ConsoleStatus {
	let status = write_to(stream, text);
	if status != ConsoleStatus::Success {
		return status;
	}
	status_from_io(stream.write_all(b"\n"))
}

/// Writes text and a newline to stdout, then flushes it.
pub fn write_line(text: &str) -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	let status = write_line_to(&mut stream, text);
	if status == ConsoleStatus::Success {
		status_from_io(stream.flush())
	} else {
		status
	}
}

/// Reads one line from a stream, stripping LF and an optional preceding CR.
/// Lines that exceed capacity are drained and reported as failures.
///
/// Capacity is measured in bytes and must be 2..=i32::MAX, including the
/// terminator used by the C interface. Invalid UTF-8 input is reported as a
/// failure because Rust strings must contain valid UTF-8.
pub fn read_line_from<R: BufRead>(
	stream: &mut R,
	capacity: usize,
) -> Result<String, ConsoleStatus> {
	if !(2..=i32::MAX as usize).contains(&capacity) {
		return Err(ConsoleStatus::InvalidArgument);
	}

	let mut bytes = Vec::with_capacity(capacity.min(INPUT_CAPACITY));
	let mut overflow = false;
	let mut received_input = false;
	loop {
		let available = stream.fill_buf().map_err(|_| ConsoleStatus::Failure)?;
		if available.is_empty() {
			break;
		}

		received_input = true;
		let newline = available.iter().position(|byte| *byte == b'\n');
		let consumed = newline.map_or(available.len(), |index| index + 1);
		let content_length = newline.map_or(consumed, |index| index);
		let retained = (capacity - 1 - bytes.len()).min(content_length);
		bytes.extend_from_slice(&available[..retained]);
		overflow |= retained < content_length;
		stream.consume(consumed);

		if newline.is_some() {
			break;
		}
	}

	if !received_input || overflow {
		return Err(ConsoleStatus::Failure);
	}
	if bytes.last() == Some(&b'\r') {
		bytes.pop();
	}
	String::from_utf8(bytes).map_err(|_| ConsoleStatus::Failure)
}

/// Reads a line from stdin into owned storage.
pub fn read_line() -> Result<String, ConsoleStatus> {
	let stdin = io::stdin();
	read_line_from(&mut stdin.lock(), INPUT_CAPACITY)
}

/// Writes an optional prompt to stdout, flushes it, then reads a line.
pub fn read(prompt: Option<&str>) -> Result<String, ConsoleStatus> {
	let stdout = io::stdout();
	let mut output = stdout.lock();
	if let Some(prompt) = prompt {
		if write_to(&mut output, prompt) != ConsoleStatus::Success {
			return Err(ConsoleStatus::Failure);
		}
	}
	output.flush().map_err(|_| ConsoleStatus::Failure)?;
	drop(output);
	read_line()
}

/// Reads one byte from a stream; this is not a decoded Unicode character.
pub fn read_character_from<R: Read>(stream: &mut R) -> Result<u8, ConsoleStatus> {
	let mut byte = [0_u8; 1];
	match stream.read(&mut byte) {
		Ok(1) => Ok(byte[0]),
		Ok(0) | Err(_) => Err(ConsoleStatus::Failure),
		Ok(_) => unreachable!(),
	}
}

/// Reads one byte from stdin. Standard input may be line-buffered; this is not
/// raw key input.
pub fn read_character() -> Result<u8, ConsoleStatus> {
	let stdin = io::stdin();
	read_character_from(&mut stdin.lock())
}

/// Reads one byte from stdin; equivalent to [`read_character`].
pub fn read_key() -> Result<u8, ConsoleStatus> {
	read_character()
}

/// Writes one Unicode scalar value, encoded as UTF-8, to a stream.
pub fn write_character_to<W: Write>(stream: &mut W, character: char) -> ConsoleStatus {
	let mut bytes = [0_u8; 4];
	status_from_io(stream.write_all(character.encode_utf8(&mut bytes).as_bytes()))
}

/// Writes one UTF-8 scalar value to stdout and flushes it.
pub fn write_character(character: char) -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	let status = write_character_to(&mut stream, character);
	if status == ConsoleStatus::Success {
		status_from_io(stream.flush())
	} else {
		status
	}
}

/// Prompts for and parses a yes/no response from supplied streams.
/// Leading/trailing whitespace is ignored; accepted answers are y/yes/n/no,
/// case-insensitively.
pub fn confirm_from<R: BufRead, W: Write>(
	input: &mut R,
	output: &mut W,
	prompt: Option<&str>,
) -> Result<bool, ConsoleStatus> {
	if let Some(prompt) = prompt {
		if write_to(output, prompt) != ConsoleStatus::Success {
			return Err(ConsoleStatus::Failure);
		}
	}
	if write_to(output, " [y/n]: ") != ConsoleStatus::Success
		|| output.flush().is_err()
	{
		return Err(ConsoleStatus::Failure);
	}
	let response = read_line_from(input, INPUT_CAPACITY)?;
	match response.trim().to_ascii_lowercase().as_str() {
		"y" | "yes" => Ok(true),
		"n" | "no" => Ok(false),
		_ => Err(ConsoleStatus::InvalidArgument),
	}
}

/// Prompts on stdin/stdout for a yes/no response.
pub fn confirm(prompt: Option<&str>) -> Result<bool, ConsoleStatus> {
	let stdin = io::stdin();
	let stdout = io::stdout();
	confirm_from(&mut stdin.lock(), &mut stdout.lock(), prompt)
}

/// Writes ANSI color prefix, text, and reset sequence to a stream.
/// ANSI interpretation depends on the destination terminal.
pub fn write_color_to<W: Write>(
	stream: &mut W,
	text: &str,
	color: &str,
) -> ConsoleStatus {
	if write_to(stream, color) != ConsoleStatus::Success
		|| write_to(stream, text) != ConsoleStatus::Success
		|| write_to(stream, COLOR_RESET) != ConsoleStatus::Success
	{
		ConsoleStatus::Failure
	} else {
		ConsoleStatus::Success
	}
}

/// Writes ANSI-colored text to stdout and flushes it.
pub fn write_color(text: &str, color: &str) -> ConsoleStatus {
	let stdout = io::stdout();
	let mut stream = stdout.lock();
	let status = write_color_to(&mut stream, text, color);
	if status == ConsoleStatus::Success {
		status_from_io(stream.flush())
	} else {
		status
	}
}

/// Waits for a finite nonnegative duration.
///
/// Unlike the C implementation's busy loop, Rust's thread sleep does not
/// consume CPU while waiting. Unrepresentable durations return failure.
pub fn wait(seconds: f64) -> ConsoleStatus {
	if !seconds.is_finite() || seconds < 0.0 {
		return ConsoleStatus::InvalidArgument;
	}
	if seconds == 0.0 {
		return ConsoleStatus::Success;
	}
	match Duration::try_from_secs_f64(seconds) {
		Ok(duration) => {
			thread::sleep(duration);
			ConsoleStatus::Success
		}
		Err(_) => ConsoleStatus::Failure,
	}
}

/// Writes text one byte at a time, waiting between bytes.
/// UTF-8 multibyte sequences are emitted byte-by-byte.
pub fn write_effect_to<W: Write>(
	stream: &mut W,
	text: &str,
	delay_seconds: f64,
) -> ConsoleStatus {
	if !delay_seconds.is_finite() || delay_seconds < 0.0 {
		return ConsoleStatus::InvalidArgument;
	}
	for (index, byte) in text.as_bytes().iter().enumerate() {
		if stream.write_all(std::slice::from_ref(byte)).is_err() || stream.flush().is_err() {
			return ConsoleStatus::Failure;
		}
		if index + 1 < text.len() && wait(delay_seconds) != ConsoleStatus::Success {
			return ConsoleStatus::Failure;
		}
	}
	ConsoleStatus::Success
}

/// Writes text to stdout one byte at a time with a delay between bytes.
pub fn write_effect(text: &str, delay_seconds: f64) -> ConsoleStatus {
	let stdout = io::stdout();
	write_effect_to(&mut stdout.lock(), text, delay_seconds)
}

#[cfg(test)]
mod tests {
	use super::*;
	use std::io::Cursor;

	#[test]
	fn read_line_strips_lf_and_optional_cr() {
		let mut input = Cursor::new(b"hello\r\n".to_vec());
		assert_eq!(read_line_from(&mut input, 16), Ok("hello".to_owned()));
	}

	#[test]
	fn read_line_drains_overlong_input() {
		let mut input = Cursor::new(b"too long\nok\n".to_vec());
		assert_eq!(read_line_from(&mut input, 5), Err(ConsoleStatus::Failure));
		assert_eq!(read_line_from(&mut input, 5), Ok("ok".to_owned()));
	}

	#[test]
	fn confirm_accepts_trimmed_case_insensitive_answers() {
		let mut input = Cursor::new(b"  YeS  \n".to_vec());
		let mut output = Vec::new();
		assert_eq!(confirm_from(&mut input, &mut output, Some("Continue?")), Ok(true));
		assert_eq!(output, b"Continue? [y/n]: ");
	}

	#[test]
	fn ansi_helpers_emit_expected_sequences() {
		let mut output = Vec::new();
		assert_eq!(set_cursor_position_to(&mut output, Position { x: 2, y: 4 }), ConsoleStatus::Success);
		assert_eq!(write_color_to(&mut output, "ok", COLOR_RED), ConsoleStatus::Success);
		assert_eq!(output, b"\x1b[5;3H\x1b[31mok\x1b[0m");
	}
}