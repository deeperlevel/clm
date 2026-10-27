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

from __future__ import annotations

import contextlib
import os
import re
import subprocess

import select
import shutil
import sys
import threading
import time
from dataclasses import dataclass
from typing import Iterator

@dataclass
class ClaConsoleState:
    lock = threading.RLock()
    initialized = False
    original_input = None
    cursor_visible: bool | None = None
    cursor_changed = False

try:
    from clm_position import Position
    from clm_size import Size
except ImportError:
    @dataclass(frozen=True)
    class Position:
        x: int
        y: int

    @dataclass(frozen=True)
    class Size:
        width: int
        height: int

class ConsoleError(RuntimeError):
    """Console operation failed."""

class UnsupportedConsoleCapability(ConsoleError):
    """Current terminal cannot reliably provide the requested operation."""

def _cla_console_tty(stream) -> bool:
    try:
        return bool(stream.isatty())
    except (OSError, ValueError, AttributeError):
        return False

def _cla_console_supports_ansi() -> bool:
    if not _cla_console_tty(sys.stdout):
        return False
    if os.name == 'nt':
        # Modern Windows terminals generally support VT. Without direct API
        # probing, this is a capability assumption, not a guarantee.
        return True
    return os.environ.get('TERM', '').lower() not in ('', 'dumb')

def _cla_console_ansi(sequence: str) -> None:
    if not _cla_console_supports_ansi():
        raise UnsupportedConsoleCapability('Output terminal does not support ANSI control')
    sys.stdout.write(sequence)
    sys.stdout.flush()

def cla_console_init() -> None:
    """Capture input state for restoration; can be called more than once."""

    with ClaConsoleState.lock:
        if ClaConsoleState.initialized:
            return
        if os.name != 'nt' and _cla_console_tty(sys.stdin):
            import termios
            try:
                ClaConsoleState.original_input = termios.tcgetattr(sys.stdin.fileno())
            except (OSError, ValueError, termios.error):
                ClaConsoleState.original_input = None
        ClaConsoleState.initialized = True

# noinspection bad-argument-type
def cla_console_restore() -> None:
    """Restore captured input mode and best-effort cursor visibility."""

    with ClaConsoleState.lock:
        if not ClaConsoleState.initialized:
            return
        try:
            if ClaConsoleState.original_input is not None and os.name != 'nt':
                import termios
                termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, ClaConsoleState.original_input)
            if ClaConsoleState.cursor_changed and _cla_console_supports_ansi():
                # A portable query for the original cursor visibility does not exist.
                _cla_console_ansi('\x1b[?25h')
        finally:
            ClaConsoleState.initialized = False
            ClaConsoleState.original_input = None
            ClaConsoleState.cursor_changed = False
            ClaConsoleState.cursor_visible = None

@contextlib.contextmanager
def cla_console_session() -> Iterator[None]:
    """Ensure restoration when leaving the context (including exceptions)."""
    cla_console_init()
    try:
        yield
    finally:
        cla_console_restore()

def cla_console_clear() -> None:
    """Clear the terminal using ANSI where available, otherwise a shell fallback.

    IDE output panels and redirected streams may not support clearing; in
    those environments the fallback may have no visible effect.
    """
    if _cla_console_supports_ansi():
        _cla_console_ansi('\x1b[2J\x1b[H')
        return

    # Shell commands are a best-effort fallback, not a promise that every
    # IDE-integrated output panel can be cleared.
    if os.name == 'nt':
        subprocess.run('cls', shell=True)
    elif _cla_console_tty(sys.stdout) and os.environ.get('TERM', '').lower() not in ('', 'dumb'):
        subprocess.run('clear')
    # When output is redirected, do not write control codes into the file.

def cla_console_flush() -> None:
    sys.stdout.flush()
    sys.stderr.flush()

def cla_console_set_title(title: str) -> None:
    if not isinstance(title, str) or any(ord(c) < 32 or ord(c) == 127 for c in title):
        raise ValueError('Title must be text without control characters')
    _cla_console_ansi(f'\x1b]2;{title}\x07')

def cla_console_get_window_size() -> Size:
    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    return Size(width=cols, height=rows)

def cla_console_set_window_size(size: Size) -> None:
    """Request resize through terminal escape sequence; may be ignored."""
    if not (1 <= size.width <= 32767 and 1 <= size.height <= 32767):
        raise ValueError('Invalid terminal dimensions')
    _cla_console_ansi(f'\x1b[8;{size.height};{size.width}t')

def cla_console_get_cursor_position(timeout: float | int = 0.25) -> Position | None:
    """Return cursor position or None; query works only on supported POSIX TTYs.

    A DSR reply consumes terminal input. Do not use while other readers run.
    Windows is deliberately not queried using platform-specific native APIs.
    """
    if timeout < 0:
        raise ValueError('timeout must be nonnegative')
    if os.name == 'nt' or not _cla_console_tty(sys.stdin) or not _cla_console_supports_ansi():
        return None
    import termios
    import tty
    with ClaConsoleState.lock:
        try:
            fd = sys.stdin.fileno()
            old = termios.tcgetattr(fd)
        except (OSError, ValueError, termios.error):
            return None
        reply = bytearray()
        try:
            tty.setraw(fd)
            _cla_console_ansi('\x1b[6n')
            deadline = time.monotonic() + timeout
            while len(reply) < 64:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                available, _, _ = select.select([fd], [], [], remaining)
                if not available:
                    break
                part = os.read(fd, 1)
                if not part:
                    break
                reply.extend(part)
                if part == b'R':
                    match = re.fullmatch(rb'\x1b\[(\d+);(\d+)R', reply)
                    if match:
                        row, col = map(int, match.groups())
                        if row > 0 and col > 0:
                            return Position(x=col - 1, y=row - 1)
                    break
        except (OSError, ValueError, ConsoleError):
            pass
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)
        return None

def cla_console_set_cursor_position(position: Position) -> None:
    if position.x < 0 or position.y < 0:
        raise ValueError('Cursor coordinates must be nonnegative')
    _cla_console_ansi(f'\x1b[{position.y + 1};{position.x + 1}H')

def cla_console_show_cursor() -> None:
    _cla_console_ansi('\x1b[?25h')
    ClaConsoleState.cursor_visible = True
    ClaConsoleState.cursor_changed = True

def cla_console_hide_cursor() -> None:
    _cla_console_ansi('\x1b[?25l')
    _cursor_visible = False
    _cursor_changed = True

def cla_console_is_cursor_visible() -> bool | None:
    """Last visibility requested by CLA; None if not known."""
    return ClaConsoleState.cursor_visible

def cla_console_is_attached() -> bool:
    """Whether standard output is connected to an interactive terminal."""
    return _cla_console_tty(sys.stdout)

def _cla_console_set_input_mode(flag: str, enabled: bool) -> None:
    if not _cla_console_tty(sys.stdin):
        raise UnsupportedConsoleCapability('stdin is not a terminal')
    if os.name == 'nt':
        raise UnsupportedConsoleCapability('Input mode control requires a Windows console backend')
    import termios
    fd = sys.stdin.fileno()
    attrs = termios.tcgetattr(fd)
    mask = termios.ECHO if flag == 'echo' else termios.ICANON
    if enabled:
        attrs[3] |= mask
    else:
        attrs[3] &= ~mask
    termios.tcsetattr(fd, termios.TCSADRAIN, attrs)

def cla_console_set_echo_on() -> None:
    _cla_console_set_input_mode('echo', True)

def cla_console_set_echo_off() -> None:
    _cla_console_set_input_mode('echo', False)

def cla_console_set_line_on() -> None:
    _cla_console_set_input_mode('line', True)

def cla_console_set_line_off() -> None:
    _cla_console_set_input_mode('line', False)

def cla_console_write(text: str) -> None:
    sys.stdout.write(str(text))

def cla_console_write_line(text: str = '') -> None:
    sys.stdout.write(str(text) + '\n')

def cla_console_read(prompt: str = '') -> str:
    return input(prompt)

def cla_console_read_line() -> str:
    return sys.stdin.readline()

def cla_console_read_key() -> str:
    """Return a Unicode key, or platform's raw special-key sequence."""
    if not _cla_console_tty(sys.stdin):
        raise UnsupportedConsoleCapability('read_key requires an interactive terminal')
    with ClaConsoleState.lock:
        if os.name == 'nt':
            import msvcrt
            ch = msvcrt.getwch()
            return ch + msvcrt.getwch() if ch in ('\x00', '\xe0') else ch
        import termios
        import tty
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            return sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)

def cla_console_read_character() -> str:
    return cla_console_read_key()

def cla_console_write_character(char: str) -> None:
    if not isinstance(char, str) or len(char) != 1:
        raise ValueError('Expected one Unicode code point')
    cla_console_write(char)

def cla_console_confirm(prompt: str = 'Are you sure? (y/n): ') -> bool:
    while True:
        response = cla_console_read(prompt).strip().casefold()
        if response in ('y', 'yes'):
            return True
        if response in ('n', 'no'):
            return False

def cla_console_write_color(m: str, color: str) -> None:
    colors = {'black': 30, 'red': 31, 'green': 32, 'yellow': 33,
              'blue': 34, 'magenta': 35, 'cyan': 36, 'white': 37, 'reset': 0}
    code = colors.get(color.casefold())
    if code is None:
        raise ValueError(f'Unknown console color: {color}')
    if _cla_console_supports_ansi():
        cla_console_write(f'\x1b[{code}m{m}\x1b[0m')
    else:
        cla_console_write(m)

def cla_console_write_effect(m: str, delay: float = 0.03) -> None:
    if delay < 0:
        raise ValueError('delay must be nonnegative')
    for char in m:
        sys.stdout.write(char)
        sys.stdout.flush()
        time.sleep(delay)
    cla_console_write_line()

class Console:
    """Portable, native-first terminal operations."""

    init = staticmethod(cla_console_init)
    restore = staticmethod(cla_console_restore)
    clear = staticmethod(cla_console_clear)
    flush = staticmethod(cla_console_flush)
    set_title = staticmethod(cla_console_set_title)
    get_window_size = staticmethod(cla_console_get_window_size)
    set_window_size = staticmethod(cla_console_set_window_size)
    get_cursor_position = staticmethod(cla_console_get_cursor_position)
    set_cursor_position = staticmethod(cla_console_set_cursor_position)
    show_cursor = staticmethod(cla_console_show_cursor)
    hide_cursor = staticmethod(cla_console_hide_cursor)
    is_cursor_visible = staticmethod(cla_console_is_cursor_visible)
    is_attached = staticmethod(cla_console_is_attached)
    set_echo_on = staticmethod(_cla_console_set_input_mode)
    set_echo_off = staticmethod(_cla_console_set_input_mode)
    set_line_on = staticmethod(_cla_console_set_input_mode)
    set_line_off = staticmethod(_cla_console_set_input_mode)
    write = staticmethod(cla_console_write)
    write_line = staticmethod(cla_console_write_line)
    cla_console_read = staticmethod(cla_console_read)
    readline = staticmethod(cla_console_read_line)
    read_key = staticmethod(cla_console_read_key)
    read_character = staticmethod(cla_console_read_key)
    write_character = staticmethod(cla_console_write_character)
    confirm = staticmethod(cla_console_confirm)
    write_color = staticmethod(cla_console_write_color)
    write_effect = staticmethod(cla_console_write_effect)
