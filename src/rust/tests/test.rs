//! Test reporting helpers that mirror the C `tests/test.h` interface.
//!
//! Status labels are colorized on stdout through `clm::console`; the log file
//! receives the same report text without ANSI escape sequences.

#[cfg(test)]
#[allow(dead_code)]
#[path = "../clm/mod.rs"]
mod clm;
#[cfg(not(test))]
use crate::clm;
#[path = "test_clm_base/mod.rs"]
mod test_clm_base;
#[path = "test_clm_bpa/mod.rs"]
mod test_clm_bpa;
#[path = "test_clm_console/mod.rs"]
mod test_clm_console;
#[path = "test_clm_dictionary/mod.rs"]
mod test_clm_dictionary;
#[path = "test_clm_id/mod.rs"]
mod test_clm_id;
#[path = "test_clm_storage/mod.rs"]
mod test_clm_storage;
#[path = "test_clm_string/mod.rs"]
mod test_clm_string;

use clm::console;
use std::fmt;
use std::fs::File;
#[cfg(test)]
use std::fs;
use std::io::Write;
#[cfg(test)]
use std::io;
use std::path::Path;

const DIVIDER_SINGLE: &str = "-------------------------------------------------------------";
const DIVIDER_DOUBLE: &str = "=============================================================";

/// Counts the passing and failing assertions in one test suite.
#[derive(Default)]
struct TestResult {
	passed: usize,
	failed: usize,
}

/// Writes colored test reports to the console and plain text to `main.log`.
#[derive(Default)]
struct TestRunner {
	passed: usize,
	failed: usize,
	log_file: Option<File>,
	log_failed: bool,
}

impl TestRunner {
	/// Opens a report file, truncating any previous contents.
	///
	/// Returns `false` if a file is already open or the path cannot be opened.
	fn log_open(&mut self, path: impl AsRef<Path>) -> bool {
		if self.log_file.is_some() {
			return false;
		}
		match File::create(path) {
			Ok(file) => {
				self.log_file = Some(file);
				self.log_failed = false;
				true
			}
			Err(_) => false,
		}
	}

	/// Flushes and closes the report file, returning whether logging succeeded.
	fn log_close(&mut self) -> bool {
		let Some(mut file) = self.log_file.take() else {
			return false;
		};
		if file.flush().is_err() {
			self.log_failed = true;
		}
		!self.log_failed
	}

	/// Writes text without a newline to the console and report file.
	fn write(&mut self, text: &str) {
		console::write(text);
		self.log_write(text.as_bytes());
	}

	/// Writes a line to the console and report file, flushing the report file.
	fn write_line(&mut self, text: &str) {
		console::write_line(text);
		self.log_write_line(text);
	}

	/// Writes colored text to the console and uncolored text to the report file.
	fn write_color(&mut self, text: &str, color: &str) {
		console::write_color(text, color);
		self.log_write(text.as_bytes());
	}

	/// Formats text using Rust formatting syntax and writes it without a newline.
	fn write_format(&mut self, arguments: fmt::Arguments<'_>) {
		let text = arguments.to_string();
		console::write(&text);
		self.log_write(text.as_bytes());
	}

	/// Formats a line using Rust formatting syntax and flushes the report file.
	fn write_line_format(&mut self, arguments: fmt::Arguments<'_>) {
		let text = arguments.to_string();
		console::write_line(&text);
		self.log_write_line(&text);
	}

	/// Starts a named test section with the standard divider and title.
	fn begin(&mut self, title: &str) {
		self.write_line("");
		self.write_line(DIVIDER_DOUBLE);
		self.write_color(title, console::COLOR_YELLOW);
		self.write_line("");
		self.write_line(DIVIDER_DOUBLE);
	}

	/// Records an assertion and writes its colored status and description.
	fn assert(&mut self, result: &mut TestResult, condition: bool, description: &str) {
		self.write("    ");
		if condition {
			self.write_color("PASSED:", console::COLOR_GREEN);
			result.passed += 1;
		} else {
			self.write_color("FAILED:", console::COLOR_RED);
			result.failed += 1;
		}
		self.write(" ");
		self.write_line(description);
	}

	/// Adds one suite's result to the cumulative totals and writes its summary.
	fn summary(&mut self, result: &TestResult) {
		self.passed += result.passed;
		self.failed += result.failed;

		self.write_line(DIVIDER_SINGLE);
		self.write("    ");
		self.write_color("PASSED:", console::COLOR_GREEN);
		self.write_format(format_args!(" {}  |  ", result.passed));
		self.write_color("FAILED:", console::COLOR_RED);
		self.write_format(format_args!(" {}  |  ", result.failed));
		self.write_color("TOTAL:", console::COLOR_CYAN);
		self.write_line_format(format_args!(" {}", result.passed + result.failed));
	}

	/// Writes the overall test summary.
	fn overall_summary(&mut self, title: &str) {
		let passed = self.passed;
		let failed = self.failed;
		self.write_line("");
		self.write_line(DIVIDER_DOUBLE);
		self.write_color(title, console::COLOR_YELLOW);
		self.write_line("");
		self.write_line(DIVIDER_DOUBLE);
		self.write("  ");
		self.write_color("PASSED:", console::COLOR_GREEN);
		self.write_format(format_args!(" {}  |  ", passed));
		self.write_color("FAILED:", console::COLOR_RED);
		self.write_format(format_args!(" {}  |  ", failed));
		self.write_color("TOTAL:", console::COLOR_CYAN);
		self.write_line_format(format_args!(" {}", passed + failed));
		self.write_line(DIVIDER_DOUBLE);
		self.write_line("");
	}

	fn log_write(&mut self, text: &[u8]) {
		let failed = self
			.log_file
			.as_mut()
			.is_some_and(|file| file.write_all(text).is_err());
		self.log_failed |= failed;
	}

	fn log_write_line(&mut self, text: &str) {
		let failed = self.log_file.as_mut().is_some_and(|file| {
			file.write_all(text.as_bytes())
				.and_then(|()| file.write_all(b"\n"))
				.and_then(|()| file.flush())
				.is_err()
		});
		self.log_failed |= failed;
	}
}

/// Runs the CLM test suites and writes their report to `main.log`.
///
/// Returns `false` if a suite assertion fails or the report cannot be written.
pub fn run_all_tests() -> bool {
	let log_path = Path::new(env!("CARGO_MANIFEST_DIR")).join("main.log");
	let mut runner = TestRunner::default();
	if !runner.log_open(&log_path) {
		eprintln!("Unable to open main.log for test output.");
		return false;
	}

	test_clm_base::run(&mut runner);
	test_clm_bpa::run(&mut runner);
	test_clm_console::run(&mut runner);
	test_clm_dictionary::run(&mut runner);
	test_clm_id::run(&mut runner);
	test_clm_string::run(&mut runner);
	test_clm_storage::run(&mut runner);
	runner.overall_summary("CLM Tests Summary");

	let log_closed = runner.log_close();
	if !log_closed {
		eprintln!("Failed while writing or closing main.log.");
	}
	runner.failed == 0 && log_closed
}

#[test]
fn clm_test_suites_pass_and_write_plain_text_to_log() -> io::Result<()> {
	assert!(run_all_tests());

	let log_path = Path::new(env!("CARGO_MANIFEST_DIR")).join("main.log");
	let report = fs::read_to_string(&log_path)?;
	assert!(report.contains("CLM - Base"));
	assert!(report.contains("CLM - BPA"));
	assert!(report.contains("CLM - Console"));
	assert!(report.contains("CLM - Dictionary"));
	assert!(report.contains("CLM - ID"));
	assert!(report.contains("CLM - Storage"));
	assert!(report.contains("CLM - String"));
	assert!(report.contains("CLM Tests Summary"));
	assert!(!report.contains('\x1b'));
	Ok(())
}
