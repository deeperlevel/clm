# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by Deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm
# Contact: deeparchive@outlook.com
#
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
#
# Free to use, modify, redistribute, and bundle for any purpose.
# Attribution and retention of this notice are appreciated, not required.
#
# Provided AS IS, without warranty or liability.
#
# Don't start from scratch, bro.
# =============================================================================

"""Tests for the standalone, procedural CLA Console API.

Run this against the refactored cla_console.py that exports cla_console_* functions.
"""
import contextlib
import io
import unittest
from unittest.mock import patch

import clm_console
from clm_console import (
    Position,
    cla_console_clear,
    cla_console_get_cursor_position,
    cla_console_get_window_size,
    cla_console_session,
    cla_console_set_cursor_position,
    cla_console_set_title,
    cla_console_write,
    cla_console_write_character,
    cla_console_write_color,
    cla_console_write_effect,
    cla_console_write_line, UnsupportedConsoleCapability,
)


class ConsoleTests(unittest.TestCase):
    def test_output(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            cla_console_write('A')
            cla_console_write_line('B')
            cla_console_write('C')
            cla_console_write_line('D')
            cla_console_write_character('é')
        self.assertEqual(stream.getvalue(), 'AB\nCD\né')

    def test_invalid_character(self):
        with self.assertRaises(ValueError):
            cla_console_write_character('abc')

    def test_redirected_color_is_plain_text(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            cla_console_write_color('hello', 'green')
        self.assertEqual(stream.getvalue(), 'hello')

    def test_unsupported_color(self):
        with self.assertRaises(ValueError):
            cla_console_write_color('x', 'ultraviolet')

    def test_reject_control_title(self):
        with self.assertRaises(ValueError):
            cla_console_set_title('bad\x1btitle')

    def test_clear_with_redirected_output(self):
        # Clear has a Windows fallback now; redirected output should not crash
        # or be polluted with ANSI sequences. Don't actually invoke cls/clear.
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream), patch.object(clm_console.os, 'system', return_value=0):
            try:
                cla_console_clear()
            except UnsupportedConsoleCapability:
                # Acceptable when there is no usable console backend.
                pass
        self.assertNotIn('\x1b', stream.getvalue())

    def test_size(self):
        size = cla_console_get_window_size()
        self.assertGreater(size.width, 0)
        self.assertGreater(size.height, 0)

    def test_negative_cursor(self):
        with self.assertRaises(ValueError):
            cla_console_set_cursor_position(Position(-1, 0))

    def test_cursor_unavailable_in_redirected_environment(self):
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertIsNone(cla_console_get_cursor_position())

    def test_session_restores(self):
        with self.assertRaises(ZeroDivisionError):
            with cla_console_session():
                # noinspection statement-effect
                1 / 0
        self.assertFalse(clm_console.ClaConsoleState.initialized)

    def test_typewriter_effect(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            cla_console_write_effect('hey', 0)
        self.assertEqual(stream.getvalue(), 'hey\n')

    def test_windows_dll_not_used(self):
        self.assertNotIn('ctypes', vars(clm_console))
        self.assertNotIn('kernel32', vars(clm_console))
        self.assertNotIn('user32', vars(clm_console))


if __name__ == '__main__':
    unittest.main()
