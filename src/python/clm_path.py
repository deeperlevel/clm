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

import os
import secrets
import tempfile

__all__ = [
    "cla_path_change_extension",
    "cla_path_ends_in_directory_separator",
    "cla_path_exists",
    "cla_path_get_directory_name",
    "cla_path_get_full_path",
    "cla_path_get_file_extension",
    "cla_path_get_filename",
    "cla_path_get_file_name_without_extension",
    "cla_path_get_invalid_path_characters",
    "cla_path_get_path_root",
    "cla_path_get_random_filename",
    "cla_path_get_relative_path",
    "cla_path_get_temporary_directory",
    "cla_path_has_extension",
    "cla_path_is_full_path", "cla_path_join",
    "cla_path_trim_ending_separator",
    "cla_path_get_directory_separator", "Path"
]

def cla_path_change_extension(path: str, extension: str) -> str:
    """Replace the final file extension, or remove it if *extension* is empty.

    An extension may be passed with or without its leading dot. Uses the host
    OS's ``splitext`` rules (for example, ``.gitignore`` has no extension).
    """
    root, _ = os.path.splitext(path)
    if extension and not extension.startswith('.'):
        extension = '.' + extension
    return root + extension


def cla_path_ends_in_directory_separator(path: str) -> bool:
    """Return whether *path* ends in a native or alternate directory separator."""
    return path.endswith(os.sep) or bool(os.altsep and path.endswith(os.altsep))


def cla_path_exists(path: str) -> bool:
    """Return whether the filesystem entry exists (including directories).

    Broken symbolic links return ``False``; OS permission errors may also be
    reported as ``False``, following ``os.path.exists`` semantics.
    """
    return os.path.exists(path)


def cla_path_get_directory_name(path: str) -> str:
    """Return the directory portion of *path*, or an empty string if absent."""
    return os.path.dirname(path)


def cla_path_get_full_path(path: str) -> str:
    """Return a normalized absolute path, without requiring it to exist.

    This does not resolve symbolic links; use ``realpath`` for that behavior.
    """
    return os.path.abspath(path)


def cla_path_get_file_extension(path: str) -> str:
    """Return the last filename extension, including '.', or an empty string."""
    return os.path.splitext(path)[1]


def cla_path_get_filename(path: str) -> str:
    """Return the final path component; trailing separators produce an empty string."""
    return os.path.basename(path)


def cla_path_get_file_name_without_extension(path: str) -> str:
    """Return the final path component with its last extension removed."""
    return os.path.splitext(os.path.basename(path))[0]


def cla_path_get_invalid_path_characters() -> list[str]:
    """Return common characters invalid within paths on the host OS.

    This is not a complete path validation rule: Windows also has reserved
    device names, disallowed trailing dots/spaces, and contextual separators.
    On POSIX, the NUL character cannot occur in paths (and '/' separates parts).
    """
    if os.name == 'nt':
        return list('<>:"|?*') + [chr(i) for i in range(32)]
    return ['\x00']


def cla_path_get_path_root(path: str) -> str:
    """Return the native path root (drive/share plus root separator if present).

    On Windows, distinguishes drive-absolute ``C:\\`` from drive-relative ``C:``.
    On POSIX, preserves the special leading ``//`` root where recognized.
    Relative paths without a drive return ``''``.
    """
    if hasattr(os.path, 'splitroot'):
        drive, root, _ = os.path.splitroot(path)
        return drive + root
    drive, tail = os.path.splitdrive(path)
    if tail.startswith(os.sep):
        count = len(tail) - len(tail.lstrip(os.sep))
        if os.sep == '/' and count == 2:
            return drive + '//'
        return drive + os.sep
    if os.altsep and tail.startswith(os.altsep):
        return drive + os.altsep
    return drive


def cla_path_get_random_filename(extension: str = '') -> str:
    """Generate a random 12-hex-character filename, optionally with extension.

    The name is not created or reserved; callers creating files must still
    handle collisions atomically. The random bytes come from ``secrets``.
    """
    name = secrets.token_hex(6)
    if extension and not extension.startswith('.'):
        extension = '.' + extension
    return name + extension


def cla_path_get_relative_path(relative_to: str, path: str) -> str:
    """Return *path* relative to directory *relative_to*.

    Both arguments use the host's path semantics; paths on different Windows
    drives cannot be made relative and may raise ``ValueError``.
    """
    return os.path.relpath(path, start=relative_to)


def cla_path_get_temporary_directory(path: str) -> str:
    """Create and return a unique temporary directory beneath *path*.

    Creates *path* when absent. The caller is responsible for removing the
    generated directory. This is a filesystem-modifying operation kept for
    compatibility; it may eventually move to ``cla_directory``.
    """
    base = os.path.abspath(path)
    os.makedirs(base, exist_ok=True)
    return tempfile.mkdtemp(dir=base)


def cla_path_has_extension(path: str) -> bool:
    """Return whether the final filename has an extension per ``splitext``."""
    return bool(os.path.splitext(path)[1])


def cla_path_is_full_path(path: str) -> bool:
    """Return whether *path* is absolute according to the host OS."""
    return os.path.isabs(path)


def cla_path_join(path1: str, path2: str) -> str:
    """Join two native path components.

    An absolute *path2* may replace *path1*, following ``os.path.join``.
    """
    return os.path.join(path1, path2)


def cla_path_trim_ending_separator(path: str) -> str:
    """Remove trailing directory separators without removing a path root."""
    if not path or not cla_path_ends_in_directory_separator(path):
        return path
    root = cla_path_get_path_root(path)
    trimmed = path.rstrip(os.sep + (os.altsep or ''))
    if root and len(trimmed) < len(root):
        return root
    return trimmed


def cla_path_get_directory_separator() -> str:
    """Return the native directory separator (``os.sep``)."""
    return os.sep


class Path:
    """Native-filesystem path operations using Python's standard library."""

    change_extension = staticmethod(cla_path_change_extension)
    ends_in_directory_separator = staticmethod(cla_path_ends_in_directory_separator)
    exists = staticmethod(cla_path_exists)
    get_directory_name = staticmethod(cla_path_get_directory_name)
    get_full_path = staticmethod(cla_path_get_full_path)
    get_file_extension = staticmethod(cla_path_get_file_extension)
    get_filename = staticmethod(cla_path_get_filename)
    get_file_name_without_extension = staticmethod(cla_path_get_file_name_without_extension)
    get_invalid_path_characters = staticmethod(cla_path_get_invalid_path_characters)
    get_path_root = staticmethod(cla_path_get_path_root)
    get_random_filename = staticmethod(cla_path_get_random_filename)
    get_relative_path = staticmethod(cla_path_get_relative_path)
    get_temporary_directory = staticmethod(cla_path_get_temporary_directory)
    has_extension = staticmethod(cla_path_has_extension)
    is_full_path = staticmethod(cla_path_is_full_path)
    join = staticmethod(cla_path_join)
    trim_ending_separator = staticmethod(cla_path_trim_ending_separator)
    get_directory_separator = staticmethod(cla_path_get_directory_separator)
