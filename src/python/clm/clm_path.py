# =============================================================================
# CLM — Cross Language Modules
# An initiative by Deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm | deeparchive@outlook.com
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
# Don't start from scratch, bro.
# =============================================================================
"""ID-mapped filesystem path composition and queries with CLM-owned string storage."""
from __future__ import annotations

import ntpath
import os
import posixpath
import secrets
from threading import RLock

from clm_container import (
    CONTAINER_SUCCESS, CONTAINER_TYPE_STRING, CONTAINER_ERROR_DUPLICATE_ID,
    clm_container_create, clm_container_destroy, clm_container_add_string,
    clm_container_edit_string, clm_container_delete_string, clm_container_get_string,
)
from clm_storage import STORAGE_SUCCESS, STORAGE_TYPE_STRING, clm_storage_get_info, clm_storage_get

__all__ = [
    'PathError', 'Path', 'clm_path_initialize', 'clm_path_destroy',
    'clm_path_map', 'clm_path_map_from_storage', 'clm_path_unmap',
    'clm_path_is_mapped', 'clm_path_get', 'clm_path_join', 'clm_path_sanitize',
    'clm_path_change_extension', 'clm_path_trim_ending_separator',
    'clm_path_get_full_path', 'clm_path_get_relative_path',
    'clm_path_get_directory_name', 'clm_path_get_filename',
    'clm_path_get_file_name_without_extension', 'clm_path_get_file_extension',
    'clm_path_get_path_root', 'clm_path_get_random_filename',
    'clm_path_get_temporary_directory', 'clm_path_has_extension',
    'clm_path_is_full_path', 'clm_path_ends_in_directory_separator',
    'clm_path_exists', 'clm_path_get_invalid_path_characters',
    'clm_path_get_directory_separator',
]

class PathError(ValueError):
    """Missing ID, unsupported input or invalid path operation."""

_lock = RLock()
_registry_id: int | None = None


def _valid_id(path_id: int) -> bool:
    return type(path_id) is int and path_id >= 0


def clm_path_initialize(container_id: int | None = None) -> None:
    """Initialize a private CLM STRING container; repeated calls are harmless.

    If the supplied ID is occupied, initialization fails instead of destroying
    someone else's container. The backing ID is never exposed after allocation.
    """
    global _registry_id
    with _lock:
        if _registry_id is not None:
            return
        if container_id is not None and not _valid_id(container_id):
            raise PathError('Invalid internal container ID')
        if container_id is not None:
            if clm_container_create(container_id, CONTAINER_TYPE_STRING) != CONTAINER_SUCCESS:
                raise PathError('Internal container ID already in use')
            _registry_id = container_id
            return
        # No dependency on a magic reserved number. Locate an available high ID.
        for cid in range(0x7FF00000, 0x7FFFFFFF):
            if clm_container_create(cid, CONTAINER_TYPE_STRING) == CONTAINER_SUCCESS:
                _registry_id = cid
                return
        raise PathError('Unable to allocate path registry')


def clm_path_destroy() -> None:
    """Destroy the path registry and all mappings (idempotent)."""
    global _registry_id
    with _lock:
        if _registry_id is not None:
            clm_container_destroy(_registry_id)
            _registry_id = None


def _cid() -> int:
    if _registry_id is None:
        clm_path_initialize()
    assert _registry_id is not None
    return _registry_id


def _read(path_id: int) -> str:
    if not _valid_id(path_id):
        raise PathError('Path ID must be a nonnegative integer')
    status, value = clm_container_get_string(_cid(), path_id)
    if status != CONTAINER_SUCCESS:
        raise PathError(f'Unmapped path ID: {path_id}')
    return value


def _write(path_id: int, value: str) -> None:
    if not _valid_id(path_id) or not isinstance(value, str) or '\x00' in value:
        raise PathError('Invalid path ID or path string')
    cid = _cid()
    status = clm_container_edit_string(cid, path_id, value)
    if status != CONTAINER_SUCCESS:
        status = clm_container_add_string(cid, path_id, value)
    if status != CONTAINER_SUCCESS:
        raise PathError(f'Unable to map path ID {path_id}: {status}')


def clm_path_map(path_id: int, path: str) -> None:
    """Insert or replace a mapping; accepts fragments as well as full paths."""
    with _lock:
        _write(path_id, path)


def clm_path_map_from_storage(path_id: int, storage_id: int, index: int) -> None:
    """Copy a STRING Storage element into a path mapping; preserve its cursor."""
    with _lock:
        status, info = clm_storage_get_info(storage_id)
        if status != STORAGE_SUCCESS or info.type != STORAGE_TYPE_STRING or info.kind != 'linear':
            raise PathError('Source must be existing linear STRING storage')
        status, value = clm_storage_get(storage_id, index)
        if status != STORAGE_SUCCESS:
            raise PathError(f'Invalid storage element index: {index}')
        _write(path_id, value)


def clm_path_unmap(path_id: int) -> None:
    with _lock:
        _read(path_id)
        clm_container_delete_string(_cid(), path_id)


def clm_path_is_mapped(path_id: int) -> bool:
    with _lock:
        if not _valid_id(path_id):
            return False
        return clm_container_get_string(_cid(), path_id)[0] == CONTAINER_SUCCESS


def clm_path_get(path_id: int) -> str:
    """Read a mapped path; a returned Python string is an immutable snapshot."""
    with _lock:
        return _read(path_id)


def _flavor(path: str):
    """Windows grammar for drive/UNC paths, POSIX grammar otherwise."""
    return ntpath if ('\\' in path or (len(path) >= 2 and path[1] == ':')) else posixpath


def _slash(path: str) -> str:
    return path.replace('\\', '/')


def _transform(destination: int | None, default: int, func, *source_ids: int) -> None:
    with _lock:
        # Snapshot before updating so aliasing the destination is always safe.
        sources = [_read(i) for i in source_ids]
        result = func(*sources)
        _write(default if destination is None else destination, result)


def clm_path_join(first_id: int, second_id: int, destination: int | None = None) -> None:
    """Join registered paths. Default destination is first_id."""
    def join(a: str, b: str) -> str:
        # Interpret the second component using the same grammar as the base.
        module = ntpath if (_flavor(a) is ntpath or _flavor(b) is ntpath) else posixpath
        return _slash(module.join(a, b))
    _transform(destination, first_id, join, first_id, second_id)


def clm_path_sanitize(path_id: int, destination: int | None = None) -> None:
    """Lexically normalize dot segments; NOT a sandbox or symlink check."""
    _transform(destination, path_id, lambda p: _slash(_flavor(p).normpath(p)), path_id)


def clm_path_change_extension(path_id: int, extension_id: int, destination: int | None = None) -> None:
    def change(path: str, ext: str) -> str:
        if '/' in ext or '\\' in ext or '\x00' in ext:
            raise PathError('Extension must be a filename suffix')
        base, _ = _flavor(path).splitext(path)
        return base + (('.' + ext) if ext and not ext.startswith('.') else ext)
    _transform(destination, path_id, change, path_id, extension_id)


def clm_path_trim_ending_separator(path_id: int, destination: int | None = None) -> None:
    def trim(p: str) -> str:
        if not p or not p.endswith(('/', '\\')):
            return p
        root = _root(p)
        trimmed = p.rstrip('/\\')
        return root if root and len(trimmed) < len(root) else trimmed
    _transform(destination, path_id, trim, path_id)


def _root(p: str) -> str:
    module = _flavor(p)
    drive, tail = module.splitdrive(p)
    if tail.startswith(('/', '\\')):
        return _slash(drive + ('//' if module is posixpath and tail.startswith('//') else '/'))
    return _slash(drive)


def clm_path_get_full_path(path_id: int, destination: int | None = None) -> None:
    """Resolve against host working directory; foreign drive grammar is rejected."""
    def full(p: str) -> str:
        if _flavor(p) is ntpath and os.name != 'nt':
            raise PathError('Cannot resolve Windows drive path on this host')
        return _slash(os.path.abspath(p))
    _transform(destination, path_id, full, path_id)


def clm_path_get_relative_path(base_id: int, path_id: int, destination: int | None = None) -> None:
    def relative(base: str, p: str) -> str:
        if _flavor(base) is not _flavor(p):
            raise PathError('Incompatible path grammars')
        try:
            return _slash(_flavor(base).relpath(p, start=base))
        except ValueError as exc:
            raise PathError(str(exc)) from exc
    _transform(destination, path_id, relative, base_id, path_id)


def clm_path_get_directory_name(path_id: int, destination: int | None = None) -> None:
    _transform(destination, path_id, lambda p: _slash(_flavor(p).dirname(p)), path_id)


def clm_path_get_filename(path_id: int, destination: int | None = None) -> None:
    _transform(destination, path_id, lambda p: _flavor(p).basename(p), path_id)


def clm_path_get_file_name_without_extension(path_id: int, destination: int | None = None) -> None:
    _transform(destination, path_id, lambda p: _flavor(p).splitext(_flavor(p).basename(p))[0], path_id)


def clm_path_get_file_extension(path_id: int, destination: int | None = None) -> None:
    _transform(destination, path_id, lambda p: _flavor(p).splitext(p)[1], path_id)


def clm_path_get_path_root(path_id: int, destination: int | None = None) -> None:
    _transform(destination, path_id, _root, path_id)


def clm_path_get_random_filename(destination_id: int, extension_id: int | None = None) -> None:
    with _lock:
        ext = _read(extension_id) if extension_id is not None else ''
        if '/' in ext or '\\' in ext:
            raise PathError('Invalid filename extension')
        _write(destination_id, secrets.token_hex(6) + (('.' + ext) if ext and not ext.startswith('.') else ext))


def clm_path_get_temporary_directory(path_id: int, destination: int | None = None) -> None:
    """Create a temporary directory beneath mapped base, storing its new path."""
    import tempfile
    with _lock:
        base = _read(path_id)
        os.makedirs(base, exist_ok=True)
        result = tempfile.mkdtemp(dir=base)
        try:
            _write(path_id if destination is None else destination, _slash(result))
        except Exception:
            os.rmdir(result)
            raise


def clm_path_has_extension(path_id: int) -> bool:
    with _lock:
        p = _read(path_id)
        return bool(_flavor(p).splitext(p)[1])


def clm_path_ends_in_directory_separator(path_id: int) -> bool:
    with _lock:
        return _read(path_id).endswith(('/', '\\'))


def clm_path_is_full_path(path_id: int) -> bool:
    with _lock:
        p = _read(path_id)
        return _flavor(p).isabs(p)


def clm_path_exists(path_id: int) -> bool:
    with _lock:
        return os.path.exists(_read(path_id))


def clm_path_get_invalid_path_characters() -> list[str]:
    return list('<>:"|?*') + [chr(i) for i in range(32)] if os.name == 'nt' else ['\x00']


def clm_path_get_directory_separator() -> str:
    return os.sep


class Path:
    """Optional facade; procedural clm_path_* functions are canonical."""
    initialize = staticmethod(clm_path_initialize)
    destroy = staticmethod(clm_path_destroy)
    map = staticmethod(clm_path_map)
    map_from_storage = staticmethod(clm_path_map_from_storage)
    unmap = staticmethod(clm_path_unmap)
    is_mapped = staticmethod(clm_path_is_mapped)
    get = staticmethod(clm_path_get)
    join = staticmethod(clm_path_join)
    sanitize = staticmethod(clm_path_sanitize)
    change_extension = staticmethod(clm_path_change_extension)
    trim_ending_separator = staticmethod(clm_path_trim_ending_separator)
    get_full_path = staticmethod(clm_path_get_full_path)
    get_relative_path = staticmethod(clm_path_get_relative_path)
    get_directory_name = staticmethod(clm_path_get_directory_name)
    get_filename = staticmethod(clm_path_get_filename)
    get_file_name_without_extension = staticmethod(clm_path_get_file_name_without_extension)
    get_file_extension = staticmethod(clm_path_get_file_extension)
    get_path_root = staticmethod(clm_path_get_path_root)
    get_random_filename = staticmethod(clm_path_get_random_filename)
    get_temporary_directory = staticmethod(clm_path_get_temporary_directory)
    has_extension = staticmethod(clm_path_has_extension)
    ends_in_directory_separator = staticmethod(clm_path_ends_in_directory_separator)
    is_full_path = staticmethod(clm_path_is_full_path)
    exists = staticmethod(clm_path_exists)
    get_invalid_path_characters = staticmethod(clm_path_get_invalid_path_characters)
    get_directory_separator = staticmethod(clm_path_get_directory_separator)
