# CLM — Cross Language Modules | Deeperlevel | CC0 1.0 Universal
# https://github.com/deeperlevel/clm | Don't start from scratch, bro.
"""ID-based native directory operations. All path arguments are mapped CLM Path IDs."""
from __future__ import annotations
import os
import shutil
from _clm_fs_common import (CLMFilesystemError, UnsupportedFilesystemOperation,
    path, replace_storage, permissions_get, permissions_set, timestamp_get, timestamp_set)

DirectoryError = CLMFilesystemError


def clm_directory_create(directory_id: int, recursive: bool = True) -> None:
    p = path(directory_id)
    if recursive: os.makedirs(p, exist_ok=True)
    else: os.mkdir(p)


def clm_directory_move(source_id: int, destination_id: int) -> None:
    src, dst = path(source_id), path(destination_id)
    if os.path.lexists(dst): raise FileExistsError(dst)
    os.rename(src, dst)


def clm_directory_remove(directory_id: int, recursive: bool = False) -> None:
    p = path(directory_id)
    if os.path.islink(p):
        raise DirectoryError('Refusing to remove symbolic link as a directory')
    if recursive: shutil.rmtree(p)
    else: os.rmdir(p)


def clm_directory_is_directory_existing(directory_id: int) -> bool:
    return os.path.isdir(path(directory_id))


def clm_directory_is_directory_empty(directory_id: int) -> bool:
    with os.scandir(path(directory_id)) as it:
        return next(it, None) is None


def clm_directory_get_current_directory(destination_id: int) -> None:
    from clm_path import clm_path_map
    clm_path_map(destination_id, os.getcwd().replace('\\', '/'))


def clm_directory_set_current_directory(directory_id: int) -> None:
    os.chdir(path(directory_id))


def _search(directory_id: int, recursive: bool, destination: int, target: str) -> int:
    p = path(directory_id)
    if not os.path.isdir(p): raise NotADirectoryError(p)
    result = []
    if recursive:
        for root, directories, files in os.walk(p, followlinks=False):
            directories.sort(); files.sort()
            if target in ('directories', 'entries'):
                result.extend(os.path.join(root, n) for n in directories)
            if target in ('files', 'entries'):
                result.extend(os.path.join(root, n) for n in files)
            if target == 'files':
                # A symbolic link to a directory is not a file; do not follow it.
                pass
    else:
        with os.scandir(p) as it:
            for entry in it:
                if target == 'directories' and entry.is_dir(follow_symlinks=False): result.append(entry.path)
                elif target == 'files' and entry.is_file(follow_symlinks=False): result.append(entry.path)
                elif target == 'entries': result.append(entry.path)
    # Canonical absolute paths sorted by normalized '/' form, deterministic.
    values = sorted((os.path.abspath(q).replace('\\', '/') for q in result))
    replace_storage(destination, 'string', values, require_capacity=True)
    return len(values)


def clm_directory_find_directories(directory_id: int, recursive: bool = False, destination: int = None) -> int:
    return _search(directory_id, recursive, destination, 'directories')


def clm_directory_find_files(directory_id: int, recursive: bool = False, destination: int = None) -> int:
    return _search(directory_id, recursive, destination, 'files')


def clm_directory_find_entries(directory_id: int, recursive: bool = False, destination: int = None) -> int:
    return _search(directory_id, recursive, destination, 'entries')


def clm_directory_get_creation_time(directory_id: int) -> int: return timestamp_get(path(directory_id), 'creation')
def clm_directory_get_last_accessed_time(directory_id: int) -> int: return timestamp_get(path(directory_id), 'accessed')
def clm_directory_get_last_write_time(directory_id: int) -> int: return timestamp_get(path(directory_id), 'write')
def clm_directory_set_creation_time(directory_id: int, timestamp_ms: int) -> None: timestamp_set(path(directory_id), 'creation', timestamp_ms)
def clm_directory_set_last_accessed_time(directory_id: int, timestamp_ms: int) -> None: timestamp_set(path(directory_id), 'accessed', timestamp_ms)
def clm_directory_set_last_write_time(directory_id: int, timestamp_ms: int) -> None: timestamp_set(path(directory_id), 'write', timestamp_ms)

for _attribute in ('read', 'write', 'execute', 'hidden'):
    def _get(pid, attr=_attribute): return permissions_get(path(pid), attr)
    def _set(pid, enabled, attr=_attribute): return permissions_set(path(pid), attr, enabled)
    _get.__name__ = f'clm_directory_get_{_attribute}'
    _set.__name__ = f'clm_directory_set_{_attribute}'
    globals()[_get.__name__] = _get
    globals()[_set.__name__] = _set

del _attribute, _get, _set
__all__ = [k for k in globals() if k.startswith('clm_directory_')] + ['DirectoryError', 'UnsupportedFilesystemOperation']
