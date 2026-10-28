"""Small assertion and reporting harness modeled after C's ``tests/test.h``."""

from __future__ import annotations

from dataclasses import dataclass
from os import PathLike
from typing import TextIO

from clm.console import (
    CLM_CONSOLE_SUCCESS,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_YELLOW,
    clm_console_write,
    clm_console_write_color,
    clm_console_write_line,
    clm_console_write_to,
)

DIVIDER_SINGLE = "-------------------------------------------------------------"
DIVIDER_DOUBLE = "============================================================="


@dataclass(slots=True)
class TestResult:
    passed: int = 0
    failed: int = 0


_total_passed = 0
_total_failed = 0
_log_file: TextIO | None = None
_log_failed = False


def clm_test_log_open(path: str | PathLike[str] = "__main__.log") -> bool:
    """Open a UTF-8 log file in overwrite mode; reject a second open log."""
    global _log_file, _log_failed
    if path is None or _log_file is not None:
        return False
    try:
        _log_file = open(path, "w", encoding="utf-8", newline="\n")
    except (OSError, TypeError, ValueError):
        return False
    _log_failed = False
    return True


def clm_test_log_close() -> bool:
    """Flush and close the log, returning false if any log operation failed."""
    global _log_file, _log_failed
    stream = _log_file
    if stream is None:
        return False
    success = not _log_failed
    _log_file = None
    try:
        stream.flush()
    except OSError:
        success = False
    try:
        stream.close()
    except OSError:
        success = False
    success = success and not _log_failed
    _log_failed = False
    return success


def _log_write(text: str) -> None:
    """Write plain text to the open log and remember any output failure."""
    global _log_failed
    if _log_file is not None and clm_console_write_to(_log_file, text) != CLM_CONSOLE_SUCCESS:
        _log_failed = True


def clm_test_write(text: str) -> None:
    """Write uncolored text to the console and plain text to the log."""
    clm_console_write(text)
    _log_write(text)


def clm_test_write_line(text: str) -> None:
    """Write a line to the console and log, flushing the log afterward."""
    global _log_failed
    clm_console_write_line(text)
    if _log_file is not None:
        _log_write(text + "\n")
        try:
            _log_file.flush()
        except OSError:
            _log_failed = True


def clm_test_write_color(text: str, color: str) -> None:
    """Write ANSI-colored text to the console and uncolored text to the log."""
    clm_console_write_color(text, color)
    _log_write(text)


def _format_text(format_string: str, arguments: tuple[object, ...]) -> str:
    """Format a message using Python's percent-format syntax."""
    return format_string % arguments if arguments else format_string


def clm_test_write_format(format_string: str, *arguments: object) -> None:
    """Write formatted text without a newline to the console and log."""
    text = _format_text(format_string, arguments)
    clm_console_write(text)
    _log_write(text)


def clm_test_write_line_format(format_string: str, *arguments: object) -> None:
    """Write a formatted line to the console and log, flushing the log."""
    clm_test_write_line(_format_text(format_string, arguments))


def clm_test_begin(title: str) -> None:
    """Print a titled test section with the standard double divider."""
    clm_test_write_line("")
    clm_test_write_line(DIVIDER_DOUBLE)
    clm_test_write_color(title, CONSOLE_COLOR_YELLOW)
    clm_test_write_line("")
    clm_test_write_line(DIVIDER_DOUBLE)


def clm_test_assert(result: TestResult, condition: bool, message: str) -> None:
    """Record and print one assertion result using C harness-style output."""
    clm_test_write("    ")
    if condition:
        result.passed += 1
        clm_test_write_color("PASSED:", CONSOLE_COLOR_GREEN)
    else:
        result.failed += 1
        clm_test_write_color("FAILED:", CONSOLE_COLOR_RED)
    clm_test_write_line(" " + message)


def clm_test_summary(result: TestResult) -> int:
    """Print a section summary, add it to overall totals, and return failures."""
    global _total_passed, _total_failed
    _total_passed += result.passed
    _total_failed += result.failed
    clm_test_write_line(DIVIDER_SINGLE)
    clm_test_write("    ")
    clm_test_write_color("PASSED:", CONSOLE_COLOR_GREEN)
    clm_test_write_format(" %d  |  ", result.passed)
    clm_test_write_color("FAILED:", CONSOLE_COLOR_RED)
    clm_test_write_format(" %d  |  ", result.failed)
    clm_test_write_color("TOTAL:", CONSOLE_COLOR_CYAN)
    clm_test_write_line_format(" %d", result.passed + result.failed)
    return result.failed


def clm_test_get_totals() -> tuple[int, int]:
    """Return the cumulative ``(passed, failed)`` assertion counts."""
    return _total_passed, _total_failed


def clm_overall_test_summary(title: str) -> None:
    """Print the titled overall summary and cumulative assertion totals."""
    passed, failed = clm_test_get_totals()
    clm_test_write_line("")
    clm_test_write_line(DIVIDER_DOUBLE)
    clm_test_write_color(title, CONSOLE_COLOR_YELLOW)
    clm_test_write_line("")
    clm_test_write_line(DIVIDER_DOUBLE)
    clm_test_write("  ")
    clm_test_write_color("PASSED:", CONSOLE_COLOR_GREEN)
    clm_test_write_format(" %d  |  ", passed)
    clm_test_write_color("FAILED:", CONSOLE_COLOR_RED)
    clm_test_write_format(" %d  |  ", failed)
    clm_test_write_color("TOTAL:", CONSOLE_COLOR_CYAN)
    clm_test_write_line_format(" %d", passed + failed)
    clm_test_write_line(DIVIDER_DOUBLE)
    clm_test_write_line("")


__all__ = [
    "DIVIDER_SINGLE",
    "DIVIDER_DOUBLE",
    "TestResult",
    "clm_test_log_open",
    "clm_test_log_close",
    "clm_test_write",
    "clm_test_write_line",
    "clm_test_write_color",
    "clm_test_write_format",
    "clm_test_write_line_format",
    "clm_test_begin",
    "clm_test_assert",
    "clm_test_summary",
    "clm_test_get_totals",
    "clm_overall_test_summary",
]
