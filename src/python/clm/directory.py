"""CLM directory port (Python), native path operations; experimental subset."""
import os
import shutil
import tempfile
from pathlib import Path

SUCCESS, FAILURE, UNSUPPORTED, STOPPED = 0, -1, -2, 1
VISIT_CONTINUE, VISIT_STOP, VISIT_ERROR = 0, 1, -1
ENTRY_FILE, ENTRY_DIRECTORY, ENTRY_LINK, ENTRY_OTHER = 1, 2, 4, 8
ENUM_RECURSIVE, ENUM_INCLUDE_HIDDEN, ENUM_FOLLOW_SYMLINKS, ENUM_STAY_ON_FILESYSTEM = 1, 2, 4, 8

def exists_as(path: str) -> int:
    try: return int(Path(path).is_dir())
    except (OSError, ValueError, TypeError): return FAILURE

def create_as(path: str) -> int:
    try: Path(path).mkdir(mode=0o700); return SUCCESS
    except OSError: return FAILURE

def create_recursive_as(path: str) -> int:
    try: Path(path).mkdir(mode=0o700, parents=True, exist_ok=True); return SUCCESS
    except OSError: return FAILURE

def delete_as(path: str) -> int:
    try: Path(path).rmdir(); return SUCCESS
    except OSError: return FAILURE

def _guard(path):
    p = Path(path).absolute()
    return str(p) not in ('/', str(Path(p.anchor))) and not p.is_symlink()

def delete_recursive_as(path: str) -> int:
    if not _guard(path): return FAILURE
    try: shutil.rmtree(path); return SUCCESS
    except OSError: return FAILURE

def clear_as(path: str) -> int:
    if not _guard(path): return FAILURE
    try:
        for p in Path(path).iterdir():
            if p.is_dir() and not p.is_symlink(): shutil.rmtree(p)
            else: p.unlink()
        return SUCCESS
    except OSError: return FAILURE

def copy_as(source: str, destination: str, overwrite: bool = False) -> int:
    try:
        a, b = Path(source).resolve(), Path(destination).resolve()
        if a == b or a in b.parents: return FAILURE
        if b.exists() and not overwrite: return FAILURE
        if any(p.is_symlink() for p in a.rglob('*')): return UNSUPPORTED
        shutil.copytree(a, b, dirs_exist_ok=overwrite); return SUCCESS
    except OSError: return FAILURE

def move_as(source: str, destination: str) -> int:
    try:
        if os.path.lexists(destination): return FAILURE
        os.rename(source, destination); return SUCCESS
    except OSError: return FAILURE

def is_empty_as(path: str):
    try:
        with os.scandir(path) as iterator: return SUCCESS, next(iterator, None) is None
    except OSError: return FAILURE, False

def _find(path, types):
    result = []
    with os.scandir(path) as it:
        for e in it:
            if e.is_symlink(): kind = ENTRY_LINK
            elif e.is_dir(follow_symlinks=False): kind = ENTRY_DIRECTORY
            elif e.is_file(follow_symlinks=False): kind = ENTRY_FILE
            else: kind = ENTRY_OTHER
            if kind & types: result.append(e.path)
    return result

def find_entries_as(path: str):
    try: return SUCCESS, _find(path, 15)
    except OSError: return FAILURE, []

def find_files_as(path: str):
    try: return SUCCESS, _find(path, ENTRY_FILE)
    except OSError: return FAILURE, []

def find_directories_as(path: str):
    try: return SUCCESS, _find(path, ENTRY_DIRECTORY)
    except OSError: return FAILURE, []

def get_entries_count_as(path: str):
    status, items = find_entries_as(path); return status, len(items)

def get_current_to():
    try: return SUCCESS, os.getcwd()
    except OSError: return FAILURE, ''

def set_current_as(path: str) -> int:
    try: os.chdir(path); return SUCCESS
    except OSError: return FAILURE

def get_temporary_to(): return SUCCESS, tempfile.gettempdir()

def create_temporary_to(path: str):
    try: return SUCCESS, tempfile.mkdtemp(prefix='clm-', dir=path)
    except OSError: return FAILURE, ''

def get_size_as(path: str):
    total = 0
    try:
        for root, dirs, files in os.walk(path, followlinks=False):
            dirs[:] = [d for d in dirs if not Path(root, d).is_symlink()]
            for name in files:
                p = Path(root, name)
                if not p.is_symlink(): total += p.stat().st_size
        return SUCCESS, total
    except OSError: return FAILURE, 0
