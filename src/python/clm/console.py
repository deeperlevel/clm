"""Console operations corresponding to the portable C ``console.h`` API."""

from __future__ import annotations

import math
import sys
import time
from typing import TextIO

try:
    from .base import Position
except ImportError:
    from clm.base import Position

CLM_CONSOLE_SUCCESS = 0
CLM_CONSOLE_FAILURE = -1
CLM_CONSOLE_INVALID_ARGUMENT = -2
CLM_CONSOLE_INPUT_CAPACITY = 4096
_INT_MAX = 2**31 - 1

CONSOLE_COLOR_RESET = "\x1b[0m"

CONSOLE_COLOR_BLACK = "\x1b[30m"
CONSOLE_COLOR_RED = "\x1b[31m"
CONSOLE_COLOR_GREEN = "\x1b[32m"
CONSOLE_COLOR_YELLOW = "\x1b[33m"
CONSOLE_COLOR_BLUE = "\x1b[34m"
CONSOLE_COLOR_MAGENTA = "\x1b[35m"
CONSOLE_COLOR_CYAN = "\x1b[36m"
CONSOLE_COLOR_WHITE = "\x1b[37m"

CONSOLE_COLOR_BLACK_BOLD = "\x1b[1;30m"
CONSOLE_COLOR_RED_BOLD = "\x1b[1;31m"
CONSOLE_COLOR_GREEN_BOLD = "\x1b[1;32m"
CONSOLE_COLOR_YELLOW_BOLD = "\x1b[1;33m"
CONSOLE_COLOR_BLUE_BOLD = "\x1b[1;34m"
CONSOLE_COLOR_MAGENTA_BOLD = "\x1b[1;35m"
CONSOLE_COLOR_CYAN_BOLD = "\x1b[1;36m"
CONSOLE_COLOR_WHITE_BOLD = "\x1b[1;37m"

CONSOLE_COLOR_BLACK_BG = "\x1b[40m"
CONSOLE_COLOR_RED_BG = "\x1b[41m"
CONSOLE_COLOR_GREEN_BG = "\x1b[42m"
CONSOLE_COLOR_YELLOW_BG = "\x1b[43m"
CONSOLE_COLOR_BLUE_BG = "\x1b[44m"
CONSOLE_COLOR_MAGENTA_BG = "\x1b[45m"
CONSOLE_COLOR_CYAN_BG = "\x1b[46m"
CONSOLE_COLOR_WHITE_BG = "\x1b[47m"


def _write_to(stream: TextIO | None, text: str | None) -> int:
    """Write text and return a console status code."""
    if stream is None or not isinstance(text, str):
        return CLM_CONSOLE_INVALID_ARGUMENT
    try:
        return (
            CLM_CONSOLE_SUCCESS
            if stream.write(text) == len(text)
            else CLM_CONSOLE_FAILURE
        )
    except (OSError, UnicodeError, ValueError, AttributeError):
        return CLM_CONSOLE_FAILURE


def _flush_to(stream: TextIO | None) -> int:
    """Flush a text stream and return a console status code."""
    if stream is None:
        return CLM_CONSOLE_INVALID_ARGUMENT
    try:
        stream.flush()
        return CLM_CONSOLE_SUCCESS
    except (OSError, ValueError, AttributeError):
        return CLM_CONSOLE_FAILURE


def _write_stdout(text: str | None, flush: bool = False) -> int:
    """Write text to stdout, optionally flushing after a successful write."""
    status = _write_to(sys.stdout, text)
    if status == CLM_CONSOLE_SUCCESS and flush:
        return _flush_to(sys.stdout)
    return status


def _format_text(format_string: str | None, arguments: tuple[object, ...]) -> tuple[int, str]:
    """Apply Python percent formatting and return its status and result."""
    if not isinstance(format_string, str):
        return CLM_CONSOLE_INVALID_ARGUMENT, ""
    try:
        return CLM_CONSOLE_SUCCESS, format_string % arguments if arguments else format_string
    except (TypeError, ValueError, OverflowError):
        return CLM_CONSOLE_FAILURE, ""


def _nonnegative_seconds(value: object) -> float | None:
    """Convert a finite nonnegative number to seconds, or return ``None``."""
    if not isinstance(value, (int, float)):
        return None
    try:
        seconds = float(value)
    except (OverflowError, TypeError, ValueError):
        return None
    return seconds if math.isfinite(seconds) and seconds >= 0 else None


def clm_console_write_to(stream: TextIO | None, text: str | None) -> int:
    """Write text to a stream without adding a newline."""
    return _write_to(stream, text)


def clm_console_init() -> int:
    """Initialize the console interface; currently performs no setup."""
    return CLM_CONSOLE_SUCCESS


def clm_console_restore() -> int:
    """Restore console state; currently performs no platform-specific work."""
    return CLM_CONSOLE_SUCCESS


def clm_console_clear_to(stream: TextIO | None) -> int:
    """Write ANSI clear-screen and home-cursor controls to a stream."""
    return _write_to(stream, "\x1b[2J\x1b[H")


def clm_console_clear() -> int:
    """Clear stdout using ANSI controls and flush it."""
    status = clm_console_clear_to(sys.stdout)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_flush() -> int:
    """Flush stdout and return a console status code."""
    return _flush_to(sys.stdout)


def clm_console_set_cursor_position_to(
    stream: TextIO | None,
    position: Position | None,
) -> int:
    """Write an ANSI cursor position to a stream using zero-based coordinates."""
    if stream is None or position is None:
        return CLM_CONSOLE_INVALID_ARGUMENT
    try:
        x, y = position.x, position.y
    except AttributeError:
        return CLM_CONSOLE_INVALID_ARGUMENT
    if not isinstance(x, int) or not isinstance(y, int) or x < 0 or y < 0:
        return CLM_CONSOLE_INVALID_ARGUMENT
    return _write_to(stream, f"\x1b[{y + 1};{x + 1}H")


def clm_console_set_cursor_position(position: Position | None) -> int:
    """Move the stdout cursor to a zero-based position and flush it."""
    status = clm_console_set_cursor_position_to(sys.stdout, position)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_set_cursor_visible_to(
    stream: TextIO | None,
    visible: bool,
) -> int:
    """Write an ANSI cursor visibility control to a stream."""
    if stream is None or not isinstance(visible, bool):
        return CLM_CONSOLE_INVALID_ARGUMENT
    return _write_to(stream, "\x1b[?25h" if visible else "\x1b[?25l")


def clm_console_show_cursor() -> int:
    """Show the stdout cursor and flush the control sequence."""
    status = clm_console_set_cursor_visible_to(sys.stdout, True)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_hide_cursor() -> int:
    """Hide the stdout cursor and flush the control sequence."""
    status = clm_console_set_cursor_visible_to(sys.stdout, False)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_write(text: str | None) -> int:
    """Write text to stdout without a newline."""
    return _write_stdout(text)


def clm_console_vwrite_format_to(
    stream: TextIO | None,
    format_string: str | None,
    arguments: tuple[object, ...],
) -> int:
    """Format arguments and write the result to a stream without a newline."""
    status, text = _format_text(format_string, arguments)
    return _write_to(stream, text) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_vwrite_line_format_to(
    stream: TextIO | None,
    format_string: str | None,
    arguments: tuple[object, ...],
) -> int:
    """Format arguments and write the result plus a newline to a stream."""
    status = clm_console_vwrite_format_to(stream, format_string, arguments)
    return clm_console_write_to(stream, "\n") if status == CLM_CONSOLE_SUCCESS else status


def clm_console_write_format_to(
    stream: TextIO | None,
    format_string: str | None,
    *arguments: object,
) -> int:
    """Format arguments and write the result to a stream without a newline."""
    return clm_console_vwrite_format_to(stream, format_string, arguments)


def clm_console_write_line_format_to(
    stream: TextIO | None,
    format_string: str | None,
    *arguments: object,
) -> int:
    """Format arguments and write the result plus a newline to a stream."""
    return clm_console_vwrite_line_format_to(stream, format_string, arguments)


def clm_console_write_format(format_string: str | None, *arguments: object) -> int:
    """Format arguments and write the result to stdout without a newline."""
    status, text = _format_text(format_string, arguments)
    return _write_stdout(text) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_write_line_format(format_string: str | None, *arguments: object) -> int:
    """Format arguments and write the result plus a newline to stdout."""
    status, text = _format_text(format_string, arguments)
    if status != CLM_CONSOLE_SUCCESS:
        return status
    return _write_stdout(text + "\n", flush=True)


def clm_console_write_line_to(stream: TextIO | None, text: str | None) -> int:
    """Write text followed by a newline to a stream without flushing it."""
    status = _write_to(stream, text)
    return _write_to(stream, "\n") if status == CLM_CONSOLE_SUCCESS else status


def clm_console_write_line(text: str | None) -> int:
    """Write text followed by a newline to stdout and flush it."""
    status = clm_console_write_line_to(sys.stdout, text)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_read_line_from(
    stream: TextIO | None,
    capacity: int = CLM_CONSOLE_INPUT_CAPACITY,
) -> tuple[int, str]:
    """Read a line as ``(status, text)``; capacity counts characters including the terminator."""
    if (
        stream is None
        or not isinstance(capacity, int)
        or capacity < 2
        or capacity > _INT_MAX
    ):
        return CLM_CONSOLE_INVALID_ARGUMENT, ""
    try:
        text = stream.readline(capacity)
        if text == "":
            return CLM_CONSOLE_FAILURE, ""
        if text.endswith("\n"):
            text = text[:-1]
            if text.endswith("\r"):
                text = text[:-1]
            return CLM_CONSOLE_SUCCESS, text
        if len(text) == capacity:
            stream.readline()
            return CLM_CONSOLE_FAILURE, ""
        if text.endswith("\r"):
            text = text[:-1]
        return CLM_CONSOLE_SUCCESS, text
    except (OSError, UnicodeError, ValueError, AttributeError):
        return CLM_CONSOLE_FAILURE, ""


def clm_console_read_line() -> tuple[int, str]:
    """Read a line from stdin as ``(status, text)``."""
    return clm_console_read_line_from(sys.stdin)


def clm_console_read(prompt: str | None = None) -> tuple[int, str]:
    """Write an optional prompt, then read stdin as ``(status, text)``."""
    if prompt is not None and not isinstance(prompt, str):
        return CLM_CONSOLE_INVALID_ARGUMENT, ""
    if prompt is not None:
        status = _write_to(sys.stdout, prompt)
        if status != CLM_CONSOLE_SUCCESS:
            return status, ""
    status = _flush_to(sys.stdout)
    return clm_console_read_line() if status == CLM_CONSOLE_SUCCESS else (status, "")


def clm_console_read_character_from(stream: TextIO | None) -> tuple[int, str]:
    """Read one Unicode character from a stream as ``(status, character)``."""
    if stream is None:
        return CLM_CONSOLE_INVALID_ARGUMENT, ""
    try:
        character = stream.read(1)
        return (CLM_CONSOLE_SUCCESS, character) if character else (CLM_CONSOLE_FAILURE, "")
    except (OSError, UnicodeError, ValueError, AttributeError):
        return CLM_CONSOLE_FAILURE, ""


def clm_console_read_character() -> tuple[int, str]:
    """Read one Unicode character from stdin as ``(status, character)``."""
    return clm_console_read_character_from(sys.stdin)


def clm_console_read_key() -> tuple[int, str]:
    """Read one character from stdin; this does not enable raw-key input."""
    return clm_console_read_character()


def clm_console_write_character_to(
    stream: TextIO | None,
    character: str | None,
) -> int:
    """Write exactly one Unicode scalar value to a stream."""
    if stream is None or not isinstance(character, str) or len(character) != 1:
        return CLM_CONSOLE_INVALID_ARGUMENT
    if 0xD800 <= ord(character) <= 0xDFFF:
        return CLM_CONSOLE_INVALID_ARGUMENT
    return _write_to(stream, character)


def clm_console_write_character(character: str | None) -> int:
    """Write one Unicode scalar value to stdout and flush it."""
    status = clm_console_write_character_to(sys.stdout, character)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_confirm_from(
    input_stream: TextIO | None,
    output_stream: TextIO | None,
    prompt: str | None = None,
) -> tuple[int, bool | None]:
    """Prompt on supplied streams and parse yes/no as ``(status, answer)``."""
    if input_stream is None or output_stream is None:
        return CLM_CONSOLE_INVALID_ARGUMENT, None
    if prompt is not None and not isinstance(prompt, str):
        return CLM_CONSOLE_INVALID_ARGUMENT, None
    if prompt is not None:
        status = _write_to(output_stream, prompt)
        if status != CLM_CONSOLE_SUCCESS:
            return status, None
    status = _write_to(output_stream, " [y/n]: ")
    if status != CLM_CONSOLE_SUCCESS:
        return status, None
    status = _flush_to(output_stream)
    if status != CLM_CONSOLE_SUCCESS:
        return status, None
    status, response = clm_console_read_line_from(
        input_stream,
        CLM_CONSOLE_INPUT_CAPACITY,
    )
    if status != CLM_CONSOLE_SUCCESS:
        return status, None
    answer = response.strip().casefold()
    if answer in ("y", "yes"):
        return CLM_CONSOLE_SUCCESS, True
    if answer in ("n", "no"):
        return CLM_CONSOLE_SUCCESS, False
    return CLM_CONSOLE_INVALID_ARGUMENT, None


def clm_console_confirm(prompt: str | None = None) -> tuple[int, bool | None]:
    """Prompt on stdin/stdout and return the result as ``(status, answer)``."""
    return clm_console_confirm_from(sys.stdin, sys.stdout, prompt)


def clm_console_write_color_to(
    stream: TextIO | None,
    text: str | None,
    color: str | None,
) -> int:
    """Write an ANSI color prefix, text, and reset sequence to a stream."""
    if stream is None or not isinstance(text, str) or not isinstance(color, str):
        return CLM_CONSOLE_INVALID_ARGUMENT
    for part in (color, text, CONSOLE_COLOR_RESET):
        status = _write_to(stream, part)
        if status != CLM_CONSOLE_SUCCESS:
            return status
    return CLM_CONSOLE_SUCCESS


def clm_console_write_color(text: str | None, color: str | None) -> int:
    """Write ANSI-colored text to stdout and flush it."""
    status = clm_console_write_color_to(sys.stdout, text, color)
    return _flush_to(sys.stdout) if status == CLM_CONSOLE_SUCCESS else status


def clm_console_wait(seconds: float) -> int:
    """Wait for a finite nonnegative duration without busy-waiting."""
    duration = _nonnegative_seconds(seconds)
    if duration is None:
        return CLM_CONSOLE_INVALID_ARGUMENT
    try:
        time.sleep(duration)
        return CLM_CONSOLE_SUCCESS
    except (OverflowError, OSError, ValueError):
        return CLM_CONSOLE_FAILURE


def clm_console_write_effect_to(
    stream: TextIO | None,
    text: str | None,
    delay_seconds: float,
) -> int:
    """Write text one Unicode character at a time with a delay between characters."""
    if stream is None or not isinstance(text, str):
        return CLM_CONSOLE_INVALID_ARGUMENT
    delay = _nonnegative_seconds(delay_seconds)
    if delay is None:
        return CLM_CONSOLE_INVALID_ARGUMENT
    for index, character in enumerate(text):
        status = _write_to(stream, character)
        if status != CLM_CONSOLE_SUCCESS:
            return status
        status = _flush_to(stream)
        if status != CLM_CONSOLE_SUCCESS:
            return status
        if index + 1 < len(text):
            status = clm_console_wait(delay)
            if status != CLM_CONSOLE_SUCCESS:
                return status
    return CLM_CONSOLE_SUCCESS


def clm_console_write_effect(text: str | None, delay_seconds: float) -> int:
    """Write text to stdout one character at a time with the requested delay."""
    return clm_console_write_effect_to(sys.stdout, text, delay_seconds)


__all__ = [
    name
    for name in globals()
    if name.startswith("clm_console_")
    or name.startswith("CLM_CONSOLE_")
    or name.startswith("CONSOLE_COLOR_")
]