package tests

import "../clm"
import "core:fmt"
import "core:os"

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

test_result :: struct {
	passed: i32,
	failed: i32,
}

DIVIDER_SINGLE :: "-------------------------------------------------------------"
DIVIDER_DOUBLE :: "============================================================="

tests_passed: i32 = 0
tests_failed: i32 = 0
test_log_file: ^os.File
test_log_failed: bool

test_log_open :: proc(path: string) -> bool {
	if path == "" || test_log_file != nil {
		return false
	}
	file, err := os.open(path, {.Write, .Create, .Trunc})
	if err != nil {
		return false
	}
	test_log_file = file
	test_log_failed = false
	return true
}

test_log_close :: proc() -> bool {
	if test_log_file == nil {
		return false
	}
	success := !test_log_failed
	if os.flush(test_log_file) != nil {
		success = false
	}
	if os.close(test_log_file) != nil {
		success = false
	}
	test_log_file = nil
	return success && !test_log_failed
}

test_write :: proc(text: string) {
	_ = clm.clm_console_write(text)
	if test_log_file != nil && clm.clm_console_write_to(test_log_file, text) != clm.CLM_CONSOLE_SUCCESS {
		test_log_failed = true
	}
}

test_write_line :: proc(text: string) {
	_ = clm.clm_console_write_line(text)
	if test_log_file != nil {
		if clm.clm_console_write_line_to(test_log_file, text) != clm.CLM_CONSOLE_SUCCESS ||
		   os.flush(test_log_file) != nil {
			test_log_failed = true
		}
	}
}

test_write_color :: proc(text: string, color: string) {
	_ = clm.clm_console_write_color(text, color)
	if test_log_file != nil && clm.clm_console_write_to(test_log_file, text) != clm.CLM_CONSOLE_SUCCESS {
		test_log_failed = true
	}
}

test_write_format :: proc(format: string, args: ..any) {
	text := fmt.aprintf(format, ..args)
	defer delete(text)
	_ = clm.clm_console_write(text)
	if test_log_file != nil && clm.clm_console_write_to(test_log_file, text) != clm.CLM_CONSOLE_SUCCESS {
		test_log_failed = true
	}
}

test_write_line_format :: proc(format: string, args: ..any) {
	text := fmt.aprintf(format, ..args)
	defer delete(text)
	_ = clm.clm_console_write_line_to(os.stdout, text)
	_ = clm.clm_console_flush()
	if test_log_file != nil {
		if clm.clm_console_write_line_to(test_log_file, text) != clm.CLM_CONSOLE_SUCCESS ||
		   os.flush(test_log_file) != nil {
			test_log_failed = true
		}
	}
}

test_begin :: proc(title: string) {
	test_write_line("")
	test_write_line(DIVIDER_DOUBLE)
	test_write_color(title, clm.CONSOLE_COLOR_YELLOW)
	test_write_line("")
	test_write_line(DIVIDER_DOUBLE)
}

assert_test :: proc(result: ^test_result, condition: bool, message := #caller_expression(condition)) {
	assert(result != nil, "assert_test requires a test result")
	test_write("    ")
	if condition {
		result.passed += 1
		test_write_color("PASSED:", clm.CONSOLE_COLOR_GREEN)
	} else {
		result.failed += 1
		test_write_color("FAILED:", clm.CONSOLE_COLOR_RED)
	}
	test_write(" ")
	test_write_line(message)
}

test_summary_counts :: proc(passed, failed: i32) {
	tests_passed += passed
	tests_failed += failed

	test_write_line(DIVIDER_SINGLE)
	test_write("    ")
	test_write_color("PASSED:", clm.CONSOLE_COLOR_GREEN)
	test_write_format(" %d  |  ", passed)
	test_write_color("FAILED:", clm.CONSOLE_COLOR_RED)
	test_write_format(" %d  |  ", failed)
	test_write_color("TOTAL:", clm.CONSOLE_COLOR_CYAN)
	test_write_line_format(" %d", passed+failed)
}

test_summary :: proc(title: string, passed, failed: i32) {
	test_begin(title)
	test_summary_counts(passed, failed)
}

overall_test_summary :: proc(title: string) {
	test_write_line("")
	test_write_line(DIVIDER_DOUBLE)
	test_write_color(title, clm.CONSOLE_COLOR_YELLOW)
	test_write_line("")
	test_write_line(DIVIDER_DOUBLE)
	test_write("  ")
	test_write_color("PASSED:", clm.CONSOLE_COLOR_GREEN)
	test_write_format(" %d  |  ", tests_passed)
	test_write_color("FAILED:", clm.CONSOLE_COLOR_RED)
	test_write_format(" %d  |  ", tests_failed)
	test_write_color("TOTAL:", clm.CONSOLE_COLOR_CYAN)
	test_write_line_format(" %d", tests_passed+tests_failed)
	test_write_line(DIVIDER_DOUBLE)
	test_write_line("")
}
