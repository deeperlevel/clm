"""Console API checks using the CLM test reporting style."""

import io

from clm.base import Position
from clm.console import (
    CLM_CONSOLE_FAILURE,
    CLM_CONSOLE_INVALID_ARGUMENT,
    CLM_CONSOLE_SUCCESS,
    clm_console_confirm_from,
    clm_console_read_line_from,
    clm_console_set_cursor_position_to,
    clm_console_wait,
    clm_console_write_character_to,
    clm_console_write_effect_to,
    clm_console_write_format_to,
)
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def run_tests() -> int:
    """Run console checks and return the number of failed assertions."""
    result = TestResult()
    clm_test_begin("CLM - Console")

    stream = io.StringIO()
    status = clm_console_write_format_to(stream, "%s:%d", "item", 4)
    clm_test_assert(
        result,
        status == CLM_CONSOLE_SUCCESS and stream.getvalue() == "item:4",
        "formatted stream output writes the expected text",
    )

    stream = io.StringIO()
    status = clm_console_set_cursor_position_to(stream, Position[int](2, 4))
    clm_test_assert(
        result,
        status == CLM_CONSOLE_SUCCESS and stream.getvalue() == "\x1b[5;3H",
        "cursor positioning converts zero-based coordinates to ANSI",
    )

    stream = io.StringIO()
    valid_status = clm_console_write_character_to(stream, "🙂")
    invalid_status = clm_console_write_character_to(stream, "ab")
    clm_test_assert(
        result,
        valid_status == CLM_CONSOLE_SUCCESS
        and invalid_status == CLM_CONSOLE_INVALID_ARGUMENT
        and stream.getvalue() == "🙂",
        "character output accepts one Unicode scalar value",
    )

    line_status = clm_console_read_line_from(io.StringIO("hello\r\n"), 16)
    long_stream = io.StringIO("abcd\nnext\n")
    overlong_status = clm_console_read_line_from(long_stream, 4)
    next_line_status = clm_console_read_line_from(long_stream, 8)
    clm_test_assert(
        result,
        line_status == (CLM_CONSOLE_SUCCESS, "hello")
        and overlong_status == (CLM_CONSOLE_FAILURE, "")
        and next_line_status == (CLM_CONSOLE_SUCCESS, "next"),
        "line input strips CRLF and drains overlong lines",
    )

    output = io.StringIO()
    confirmation = clm_console_confirm_from(io.StringIO(" YES \n"), output, "Proceed?")
    clm_test_assert(
        result,
        confirmation == (CLM_CONSOLE_SUCCESS, True)
        and output.getvalue() == "Proceed? [y/n]: ",
        "confirmation parses yes and writes its prompt",
    )

    output = io.StringIO()
    status = clm_console_write_effect_to(output, "abc", 0)
    clm_test_assert(
        result,
        status == CLM_CONSOLE_SUCCESS and output.getvalue() == "abc",
        "write effect emits text without adding a newline",
    )

    invalid_capacity = clm_console_read_line_from(io.StringIO(""), 2**31)
    invalid_delay = clm_console_wait(10**1000)
    clm_test_assert(
        result,
        invalid_capacity == (CLM_CONSOLE_INVALID_ARGUMENT, "")
        and invalid_delay == CLM_CONSOLE_INVALID_ARGUMENT,
        "out-of-range capacity and delay values are rejected",
    )

    return clm_test_summary(result)