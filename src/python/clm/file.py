"""CLM file port (Python). ID-based open resources; UTF-8 filesystem paths.
Experimental subset of the C contract. Call shutdown() to close all files.
"""
import os
from pathlib import Path

SUCCESS, FAILURE, UNSUPPORTED = 0, -1, -2
READ, WRITE, APPEND, CREATE, TRUNCATE, EXCLUSIVE = 1, 2, 4, 8, 16, 32
SEEK_BEGIN, SEEK_CURRENT, SEEK_END = 0, 1, 2
_registry = {}
_retired = set()

def open_as(id: int, path: str, mode: int) -> int:
    if id in _retired or id in _registry or not path or not mode & 7 or mode & ~63:
        return FAILURE
    if (mode & EXCLUSIVE and not mode & CREATE) or (mode & TRUNCATE and (not mode & WRITE or mode & APPEND)):
        return FAILURE
    flags = os.O_RDWR if mode & READ and mode & (WRITE | APPEND) else os.O_WRONLY if mode & (WRITE | APPEND) else os.O_RDONLY
    flags |= os.O_CREAT if mode & CREATE else 0
    flags |= os.O_EXCL if mode & EXCLUSIVE else 0
    flags |= os.O_APPEND if mode & APPEND else 0
    flags |= getattr(os, 'O_NOFOLLOW', 0)
    try:
        fd = os.open(path, flags, 0o600)
        if not Path(path).is_file():
            os.close(fd)
            return FAILURE
        if mode & TRUNCATE:
            os.ftruncate(fd, 0)
        _registry[id] = (fd, mode)
        return SUCCESS
    except OSError:
        return FAILURE

def open(id: int, path: str, mode: int) -> int:
    return open_as(id, path, mode)

def is_open(id: int) -> int:
    return int(id in _registry)

def close(id: int) -> int:
    entry = _registry.pop(id, None)
    if entry is None:
        return FAILURE
    _retired.add(id)
    try:
        os.close(entry[0]); return SUCCESS
    except OSError:
        return FAILURE

def shutdown() -> int:
    status = SUCCESS
    for id in list(_registry):
        if close(id): status = FAILURE
    _retired.clear()
    return status

def read_to(id: int, size: int):
    entry = _registry.get(id)
    if entry is None or not entry[1] & READ or size < 0: return FAILURE, b''
    try:
        chunks, remaining = [], size
        while remaining:
            chunk = os.read(entry[0], min(remaining, 1 << 20))
            if not chunk: break
            chunks.append(chunk); remaining -= len(chunk)
        return SUCCESS, b''.join(chunks)
    except OSError: return FAILURE, b''

def write_from(id: int, data: bytes):
    entry = _registry.get(id)
    if entry is None or not entry[1] & (WRITE | APPEND): return FAILURE, 0
    written = 0
    try:
        while written < len(data):
            n = os.write(entry[0], data[written:written + (1 << 20)])
            if n == 0: return FAILURE, written
            written += n
        return SUCCESS, written
    except OSError: return FAILURE, written

def seek(id: int, offset: int, origin: int = SEEK_BEGIN) -> int:
    entry = _registry.get(id)
    if entry is None or origin not in (0, 1, 2): return FAILURE
    try: os.lseek(entry[0], offset, origin); return SUCCESS
    except OSError: return FAILURE

def tell(id: int):
    entry = _registry.get(id)
    if entry is None: return FAILURE, 0
    try: return SUCCESS, os.lseek(entry[0], 0, os.SEEK_CUR)
    except OSError: return FAILURE, 0

def get_open_size(id: int):
    entry = _registry.get(id)
    if entry is None: return FAILURE, 0
    try: return SUCCESS, os.fstat(entry[0]).st_size
    except OSError: return FAILURE, 0

def sync(id: int) -> int:
    entry = _registry.get(id)
    if entry is None: return FAILURE
    try: os.fsync(entry[0]); return SUCCESS
    except OSError: return FAILURE

def exists_as(path: str) -> int:
    try: return int(Path(path).is_file())
    except (OSError, ValueError, TypeError): return FAILURE

def create_as(path: str) -> int:
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        os.close(fd); return SUCCESS
    except OSError: return FAILURE

def delete_as(path: str) -> int:
    try: os.unlink(path); return SUCCESS
    except OSError: return FAILURE

def get_size_as(path: str):
    try: return SUCCESS, Path(path).stat().st_size if Path(path).is_file() else 0
    except OSError: return FAILURE, 0

def read_all_to(path: str):
    try:
        with builtins_open(path, 'rb') as f: return SUCCESS, f.read()
    except OSError: return FAILURE, b''

def write_all_from(path: str, content: bytes) -> int:
    try:
        with builtins_open(path, 'wb') as f: f.write(content)
        return SUCCESS
    except OSError: return FAILURE

def append_from(path: str, content: bytes) -> int:
    try:
        with builtins_open(path, 'ab') as f: f.write(content)
        return SUCCESS
    except OSError: return FAILURE

def read_all_text_to(path: str, encoding='utf-8'):
    try:
        with builtins_open(path, 'r', encoding=encoding, newline='') as f: return SUCCESS, f.read()
    except (OSError, UnicodeError, ValueError): return FAILURE, ''

def write_all_text_from(path: str, text: str, encoding='utf-8') -> int:
    try:
        with builtins_open(path, 'w', encoding=encoding, newline='') as f: f.write(text)
        return SUCCESS
    except (OSError, UnicodeError, ValueError): return FAILURE

from builtins import open as builtins_open
