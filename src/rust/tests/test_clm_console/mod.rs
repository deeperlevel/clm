//! Tests for standard-stream, input, and ANSI helpers in `clm::console`.

use super::{TestResult, TestRunner};
use super::clm::{base, console};
use std::io::Cursor;

/// Runs the console operations against in-memory streams and records results.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - Console");

	runner.assert(
		&mut result,
		console::init() == console::ConsoleStatus::Success,
		"console::init() succeeds",
	);
	runner.assert(
		&mut result,
		console::restore() == console::ConsoleStatus::Success,
		"console::restore() succeeds",
	);
	runner.assert(
		&mut result,
		console::flush() == console::ConsoleStatus::Success,
		"console::flush() succeeds",
	);
	runner.assert(
		&mut result,
		console::wait(-1.0) == console::ConsoleStatus::InvalidArgument,
		"console::wait(-1.0) rejects a negative duration",
	);
	runner.assert(
		&mut result,
		console::wait(f64::NAN) == console::ConsoleStatus::InvalidArgument,
		"console::wait(NaN) rejects a non-finite duration",
	);
	runner.assert(
		&mut result,
		console::read_line_from(&mut Cursor::new(b""), 1)
			== Err(console::ConsoleStatus::InvalidArgument),
		"console::read_line_from rejects a capacity smaller than two",
	);

	let mut output = Vec::new();
	runner.assert(
		&mut result,
		console::clear_to(&mut output) == console::ConsoleStatus::Success,
		"console::clear_to writes the ANSI clear sequence",
	);
	runner.assert(
		&mut result,
		console::set_cursor_position_to(
			&mut output,
			base::Position { x: 2, y: 4 },
		) == console::ConsoleStatus::Success,
		"console::set_cursor_position_to accepts nonnegative coordinates",
	);
	runner.assert(
		&mut result,
		console::set_cursor_position_to(
			&mut output,
			base::Position { x: -1, y: 0 },
		) == console::ConsoleStatus::InvalidArgument,
		"console::set_cursor_position_to rejects negative coordinates",
	);
	console::set_cursor_visible_to(&mut output, false);
	console::set_cursor_visible_to(&mut output, true);
	console::write_to(&mut output, "text");
	console::write_line_to(&mut output, "line");
	console::write_format_to(&mut output, format_args!(" value={} {}", 42, "ok"));
	console::write_line_format_to(&mut output, format_args!("line:{:02}", 7));
	console::write_character_to(&mut output, 'X');
	console::write_character_to(&mut output, 'é');
	console::write_color_to(&mut output, "red", console::COLOR_RED);
	console::write_effect_to(&mut output, "go", 0.0);
	let expected = b"\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25htextline\n value=42 okline:07\nX\xc3\xa9\x1b[31mred\x1b[0mgo";
	runner.assert(
		&mut result,
		output.as_slice() == expected,
		"console stream helpers emit the expected ANSI and text bytes",
	);
	runner.assert(
		&mut result,
		console::write_effect_to(&mut Vec::new(), "", f64::INFINITY)
			== console::ConsoleStatus::InvalidArgument,
		"console::write_effect_to rejects a non-finite delay",
	);

	let mut input = Cursor::new(b"hello\r\nlast".to_vec());
	runner.assert(
		&mut result,
		console::read_line_from(&mut input, 16) == Ok("hello".to_owned()),
		"console::read_line_from strips LF and a preceding CR",
	);
	runner.assert(
		&mut result,
		console::read_line_from(&mut input, 16) == Ok("last".to_owned()),
		"console::read_line_from accepts a final line without LF",
	);
	runner.assert(
		&mut result,
		console::read_line_from(&mut input, 16) == Err(console::ConsoleStatus::Failure),
		"console::read_line_from reports end of input",
	);

	let mut input = Cursor::new(b"123456789\nok\n".to_vec());
	runner.assert(
		&mut result,
		console::read_line_from(&mut input, 8) == Err(console::ConsoleStatus::Failure),
		"console::read_line_from rejects and drains overlong lines",
	);
	runner.assert(
		&mut result,
		console::read_line_from(&mut input, 8) == Ok("ok".to_owned()),
		"console::read_line_from can read after draining an overlong line",
	);

	let mut input = Cursor::new(b"Q".to_vec());
	runner.assert(
		&mut result,
		console::read_character_from(&mut input) == Ok(b'Q'),
		"console::read_character_from reads one byte",
	);

	let mut input = Cursor::new(b" yes \n".to_vec());
	let mut prompt_output = Vec::new();
	runner.assert(
		&mut result,
		console::confirm_from(&mut input, &mut prompt_output, Some("Continue?")) == Ok(true),
		"console::confirm_from accepts a trimmed yes response",
	);
	runner.assert(
		&mut result,
		prompt_output == b"Continue? [y/n]: ",
		"console::confirm_from writes the expected prompt",
	);

	let mut input = Cursor::new(b"no\n".to_vec());
	runner.assert(
		&mut result,
		console::confirm_from(&mut input, &mut Vec::new(), None) == Ok(false),
		"console::confirm_from accepts no without a custom prompt",
	);
	let mut input = Cursor::new(b"yesterday\n".to_vec());
	runner.assert(
		&mut result,
		console::confirm_from(&mut input, &mut Vec::new(), None)
			== Err(console::ConsoleStatus::InvalidArgument),
		"console::confirm_from rejects responses outside yes or no",
	);

	runner.summary(&result);
}