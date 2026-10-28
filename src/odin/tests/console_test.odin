package tests

import "../clm"
import "core:math"
import "core:os"
import "core:strings"

console_test_close :: proc(file: ^os.File) {
	if file == nil {
		return
	}
	path := strings.clone(os.name(file))
	defer delete(path)
	assert(os.close(file) == nil)
	assert(os.remove(path) == nil)
}

console_test_input :: proc(file: ^os.File, text: string) {
	assert(os.truncate(file, 0) == nil)
	_, err := os.seek(file, 0, .Start)
	assert(err == nil)
	assert(clm.clm_console_write_to(file, text) == clm.CLM_CONSOLE_SUCCESS)
	_, err = os.seek(file, 0, .Start)
	assert(err == nil)
}

console_test_equals :: proc(file: ^os.File, expected: string) -> bool {
	_, err := os.seek(file, 0, .Start)
	if err != nil {
		return false
	}
	actual: [512]u8
	n, read_err := os.read(file, actual[:])
	return (read_err == nil || read_err == .EOF) && string(actual[:n]) == expected
}

console_test_line :: proc(buffer: []u8) -> string {
	for byte, i in buffer {
		if byte == 0 {
			return string(buffer[:i])
		}
	}
	return string(buffer)
}

console_tests :: proc() -> i32 {
	result: test_result
	test_begin("CLM Console")
	output, output_err := os.create_temp_file("", "clm-console-output-*")
	input, input_err := os.create_temp_file("", "clm-console-input-*")
	defer console_test_close(output)
	defer console_test_close(input)
	assert_test(&result, output_err == nil && input_err == nil)
	if output == nil || input == nil {
		test_summary_counts(result.passed, result.failed)
		return result.failed
	}

	assert_test(&result, clm.clm_console_init() == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_restore() == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_flush() == clm.CLM_CONSOLE_SUCCESS)
	// Odin strings have no NULL text sentinel; invalid streams exercise argument validation.
	assert_test(&result, clm.clm_console_write_to(nil, "text") == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_write_line_to(nil, "line") == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_write_character_to(nil, "X") == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_write_effect_to(nil, "text", 0) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_wait(-1) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_wait(math.nan_f64()) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_wait(math.inf_f64(1)) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_wait(0) == clm.CLM_CONSOLE_SUCCESS)

	assert_test(&result, clm.clm_console_clear_to(output) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_set_cursor_position_to(output, {2, 4}) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_set_cursor_position_to(output, {-1, 0}) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_set_cursor_visible_to(output, false) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_set_cursor_visible_to(output, true) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_to(output, "text") == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_line_to(output, "line") == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_format_to(output, " value=%d %s", 42, "ok") == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_line_format_to(output, "line:%02d", 7) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_line_format_to(nil, "%d", 1) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_write_character_to(output, "X") == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_character_to(output, "") == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_write_character_to(output, "XY") == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_write_character_to(output, "\xc3\xa9") == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_color_to(output, "red", clm.CONSOLE_COLOR_RED) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, clm.clm_console_write_effect_to(output, "go", 0) == clm.CLM_CONSOLE_SUCCESS)
	assert_test(&result, console_test_equals(output,
		"\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25h" +
		"textline\n value=42 okline:07\nX\xc3\xa9\x1b[31mred\x1b[0mgo"))
	for invalid in ([]string{"\x80", "\xc0\x80", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf0\x90", "\x00"}) {
		assert_test(&result, clm.clm_console_write_character_to(output, invalid) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	}
	assert_test(&result, clm.clm_console_write_character_to(output, "\xf0\x9f\x98\x80") == clm.CLM_CONSOLE_SUCCESS)

	line: [16]u8
	console_test_input(input, "hello\r\nlast")
	assert_test(&result, clm.clm_console_read_line_from(input, line[:]) == clm.CLM_CONSOLE_SUCCESS && console_test_line(line[:]) == "hello")
	assert_test(&result, clm.clm_console_read_line_from(input, line[:]) == clm.CLM_CONSOLE_SUCCESS && console_test_line(line[:]) == "last")
	assert_test(&result, clm.clm_console_read_line_from(input, line[:]) == clm.CLM_CONSOLE_FAILURE)
	console_test_input(input, "123456789\nok\n")
	short_line: [8]u8
	assert_test(&result, clm.clm_console_read_line_from(input, short_line[:]) == clm.CLM_CONSOLE_FAILURE)
	assert_test(&result, clm.clm_console_read_line_from(input, short_line[:]) == clm.CLM_CONSOLE_SUCCESS && console_test_line(short_line[:]) == "ok")
	console_test_input(input, "1234567\n\r\nlast\r")
	assert_test(&result, clm.clm_console_read_line_from(input, short_line[:]) == clm.CLM_CONSOLE_SUCCESS && console_test_line(short_line[:]) == "1234567")
	assert_test(&result, clm.clm_console_read_line_from(input, short_line[:]) == clm.CLM_CONSOLE_SUCCESS && console_test_line(short_line[:]) == "")
	assert_test(&result, clm.clm_console_read_line_from(input, short_line[:]) == clm.CLM_CONSOLE_SUCCESS && console_test_line(short_line[:]) == "last")
	console_test_input(input, "Q")
	character: u8
	assert_test(&result, clm.clm_console_read_character_from(input, &character) == clm.CLM_CONSOLE_SUCCESS && character == 'Q')
	assert_test(&result, clm.clm_console_read_character_from(input, &character) == clm.CLM_CONSOLE_FAILURE)
	assert_test(&result, clm.clm_console_read_character_from(input, nil) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_read_line_from(nil, line[:]) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_read_line_from(input, line[:1]) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_read_line(nil) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_read("", nil) == clm.CLM_CONSOLE_INVALID_ARGUMENT)
	assert_test(&result, clm.clm_console_read_key(nil) == clm.CLM_CONSOLE_INVALID_ARGUMENT)

	write_only, write_only_error := os.open(os.name(output), {.Write})
	assert_test(&result, write_only_error == nil)
	if write_only != nil {
		assert_test(&result, clm.clm_console_read_line_from(write_only, line[:]) == clm.CLM_CONSOLE_FAILURE)
		assert_test(&result, os.close(write_only) == nil)
	}
	read_only, read_only_error := os.open(os.name(input), {.Read})
	assert_test(&result, read_only_error == nil)
	if read_only != nil {
		assert_test(&result, clm.clm_console_write_to(read_only, "text") == clm.CLM_CONSOLE_FAILURE)
		assert_test(&result, os.close(read_only) == nil)
	}

	console_test_input(input, " yes \n")
	console_test_input(output, "")
	confirmed := false
	assert_test(&result, clm.clm_console_confirm_from(input, output, "Continue?", &confirmed) == clm.CLM_CONSOLE_SUCCESS && confirmed)
	assert_test(&result, console_test_equals(output, "Continue? [y/n]: "))
	console_test_input(input, "no\n")
	assert_test(&result, clm.clm_console_confirm_from(input, output, "", &confirmed) == clm.CLM_CONSOLE_SUCCESS && !confirmed)
	console_test_input(input, "yesterday\n")
	confirmed = true
	assert_test(&result, clm.clm_console_confirm_from(input, output, "", &confirmed) == clm.CLM_CONSOLE_INVALID_ARGUMENT && confirmed)
	for answer in ([]string{"Y\n", "YES\n", "\tYeS \r\n", "n\n", "NO\n"}) {
		console_test_input(input, answer)
		assert_test(&result, clm.clm_console_confirm_from(input, output, "", &confirmed) == clm.CLM_CONSOLE_SUCCESS)
	}
	test_summary_counts(result.passed, result.failed)
	return result.failed
}
