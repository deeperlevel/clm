# CLM — Cross Language Modules | Deeperlevel | CC0 1.0 Universal
# https://github.com/deeperlevel/clm | Don't start from scratch, bro.
"""ID-based file operations with CLM Storage buffers and explicit block streaming."""
from __future__ import annotations
from dataclasses import dataclass
import os
import threading
from _clm_fs_common import (CLMFilesystemError, UnsupportedFilesystemOperation,
    path, get_values, replace_storage, permissions_get, permissions_set,
    timestamp_get, timestamp_set)
from clm_storage import STORAGE_SUCCESS, clm_storage_get_info

FileError = CLMFilesystemError




def _writable_path(path_id):
    target = path(path_id)
    if os.path.exists(target) and not permissions_get(target, 'write'):
        raise PermissionError(f'File is marked read-only: {target}')
    return target

def _binary(sid):
    _, values = get_values(sid, 'byte')
    return bytes(n & 255 for n in values)


def _text(sid):
    _, values = get_values(sid, 'string')
    if len(values) != 1: raise FileError('Text Storage must have exactly one STRING element')
    return values[0]


def _lines(sid):
    _, values = get_values(sid, 'string')
    if any('\r' in line or '\n' in line for line in values):
        raise FileError('Each STRING line must not contain newline characters')
    return values


def _encode_bytes(data):
    # CLM BYTE is signed, but binary values are raw unsigned octets.
    return [b if b < 128 else b - 256 for b in data]


def clm_file_load(path_id: int, storage_id: int) -> int:
    with open(path(path_id), 'rb') as f: data = f.read()
    replace_storage(storage_id, 'byte', _encode_bytes(data))
    return len(data)


def clm_file_save(path_id: int, storage_id: int) -> int:
    data = _binary(storage_id)
    with open(_writable_path(path_id), 'wb') as f: return f.write(data)


def clm_file_load_text(path_id: int, storage_id: int, encoding: str = 'utf-8') -> int:
    with open(path(path_id), 'r', encoding=encoding, newline='') as f: content = f.read()
    replace_storage(storage_id, 'string', [content])
    return len(content)


def clm_file_save_text(path_id: int, storage_id: int, encoding: str = 'utf-8') -> int:
    data = _text(storage_id)
    with open(_writable_path(path_id), 'w', encoding=encoding, newline='') as f: return f.write(data)


def clm_file_load_lines(path_id: int, storage_id: int, encoding: str = 'utf-8') -> int:
    with open(path(path_id), 'r', encoding=encoding, newline=None) as f: content = f.read()
    # splitlines preserves blank lines, but a final line terminator is not an extra line.
    values = content.splitlines()
    replace_storage(storage_id, 'string', values)
    return len(values)


def clm_file_save_lines(path_id: int, storage_id: int, encoding: str = 'utf-8') -> int:
    values = _lines(storage_id)
    data = os.linesep.join(values)
    with open(_writable_path(path_id), 'w', encoding=encoding, newline='') as f: f.write(data)
    return len(values)


def clm_file_append(path_id: int, storage_id: int) -> int:
    data = _binary(storage_id)
    with open(_writable_path(path_id), 'ab') as f: return f.write(data)


def clm_file_prepend(path_id: int, storage_id: int) -> int:
    p = _writable_path(path_id); data = _binary(storage_id)
    with open(p, 'rb') as f: original = f.read()
    with open(p, 'wb') as f: f.write(data + original)
    return len(data)


def clm_file_insert(path_id: int, storage_id: int, offset: int) -> int:
    if type(offset) is not int or offset < 0: raise FileError('Invalid byte offset')
    p = _writable_path(path_id); data = _binary(storage_id)
    with open(p, 'rb') as f: original = f.read()
    if offset > len(original): raise FileError('Insert offset beyond end of file')
    with open(p, 'wb') as f: f.write(original[:offset] + data + original[offset:])
    return len(data)


def clm_file_clear(path_id: int) -> None:
    with open(_writable_path(path_id), 'wb'): pass


def _edit_lines(path_id, storage_id, operation, at=None, encoding='utf-8'):
    p = _writable_path(path_id); inserted = _lines(storage_id)
    with open(p, 'r', encoding=encoding, newline=None) as f: source = f.read()
    original = source.splitlines()
    if operation == 'insert':
        if type(at) is not int or not 0 <= at <= len(original): raise FileError('Invalid line index')
        result = original[:at] + inserted + original[at:]
    elif operation == 'prepend': result = inserted + original
    elif operation == 'append': result = original + inserted
    else: raise ValueError(operation)
    # Preserve an ending newline if present; use OS separator for new output.
    ending = source.endswith(('\n', '\r')) or operation == 'append' and not source
    data = os.linesep.join(result) + (os.linesep if result and ending else '')
    with open(p, 'w', encoding=encoding, newline='') as f: f.write(data)
    return len(inserted)


def clm_file_append_lines(path_id, storage_id, encoding='utf-8'): return _edit_lines(path_id, storage_id, 'append', encoding=encoding)
def clm_file_prepend_lines(path_id, storage_id, encoding='utf-8'): return _edit_lines(path_id, storage_id, 'prepend', encoding=encoding)
def clm_file_insert_lines(path_id, storage_id, index, encoding='utf-8'): return _edit_lines(path_id, storage_id, 'insert', at=index, encoding=encoding)

def _one_line(value):
    if not isinstance(value, str) or '\n' in value or '\r' in value: raise FileError('Line must be a STRING without newline')
    return value

def _edit_single_line(path_id, line, operation, index=None, encoding='utf-8'):
    line = _one_line(line)
    p = _writable_path(path_id)
    with open(p, 'r', encoding=encoding, newline=None) as f: source = f.read()
    original = source.splitlines()
    if operation == 'append': result = original + [line]
    elif operation == 'prepend': result = [line] + original
    else:
        if type(index) is not int or not 0 <= index <= len(original): raise FileError('Invalid line index')
        result = original[:index] + [line] + original[index:]
    ending = source.endswith(('\r','\n'))
    with open(p, 'w', encoding=encoding, newline='') as f:
        f.write(os.linesep.join(result) + (os.linesep if ending else ''))
    return 1

def clm_file_append_line(path_id, line_storage_id, encoding='utf-8'):
    return _edit_single_line(path_id, _text(line_storage_id), 'append', encoding=encoding)
def clm_file_prepend_line(path_id, line_storage_id, encoding='utf-8'):
    return _edit_single_line(path_id, _text(line_storage_id), 'prepend', encoding=encoding)
def clm_file_insert_line(path_id, line_storage_id, index, encoding='utf-8'):
    return _edit_single_line(path_id, _text(line_storage_id), 'insert', index, encoding=encoding)


def clm_file_is_file_existing(path_id: int) -> bool: return os.path.isfile(path(path_id))
def clm_file_get_size(path_id: int) -> int: return os.path.getsize(path(path_id))
def clm_file_get_creation_time(path_id: int) -> int: return timestamp_get(path(path_id), 'creation')
def clm_file_get_last_accessed_time(path_id: int) -> int: return timestamp_get(path(path_id), 'accessed')
def clm_file_get_last_write_time(path_id: int) -> int: return timestamp_get(path(path_id), 'write')
def clm_file_set_creation_time(path_id: int, timestamp_ms: int) -> None: timestamp_set(path(path_id), 'creation', timestamp_ms)
def clm_file_set_last_accessed_time(path_id: int, timestamp_ms: int) -> None: timestamp_set(path(path_id), 'accessed', timestamp_ms)
def clm_file_set_last_write_time(path_id: int, timestamp_ms: int) -> None: timestamp_set(path(path_id), 'write', timestamp_ms)

for _attr in ('read', 'write', 'execute', 'hidden'):
    def _g(pid, a=_attr): return permissions_get(path(pid), a)
    def _s(pid, enabled, a=_attr): return permissions_set(path(pid), a, enabled)
    globals()[f'clm_file_get_{_attr}'] = _g
    globals()[f'clm_file_set_{_attr}'] = _s
del _attr, _g, _s

@dataclass(frozen=True)
class StreamInfo:
    file_id: int
    path: str
    file_size: int
    block_size: int
    block_count: int
    seek_index: int
    last_read_size: int
    auto_move: bool

@dataclass
class _Stream:
    file_id: int
    file: object
    path: str
    size: int
    block_size: int
    count: int
    index: int
    last_read_size: int = 0
    auto_move: bool = False

_streams = {}
_stream_lock = threading.RLock()


def _stream(fid):
    stream = _streams.get(fid)
    if stream is None: raise FileError(f'No open stream for ID {fid}')
    return stream


def clm_file_begin_streaming(file_id: int, path_id: int, block_size: int) -> None:
    if type(file_id) is not int or file_id < 0 or type(block_size) is not int or block_size <= 0:
        raise FileError('Invalid stream ID or block size')
    p = path(path_id)
    with _stream_lock:
        if file_id in _streams: raise FileError('Stream ID already registered')
        handle = open(p, 'rb')
        try:
            size = os.fstat(handle.fileno()).st_size
            count = (size + block_size - 1) // block_size
            _streams[file_id] = _Stream(file_id, handle, p, size, block_size, count, 0 if count else -1)
        except Exception:
            handle.close()
            raise


def clm_file_end_streaming(file_id: int) -> None:
    with _stream_lock:
        stream = _stream(file_id)
        stream.file.close()
        del _streams[file_id]

clm_file_close = clm_file_end_streaming


def clm_file_get_stream_info(file_id: int) -> StreamInfo:
    with _stream_lock:
        s = _stream(file_id)
        return StreamInfo(s.file_id, s.path, s.size, s.block_size, s.count, s.index, s.last_read_size, s.auto_move)


def clm_file_seek(file_id: int, block_index: int) -> None:
    with _stream_lock:
        s = _stream(file_id)
        if type(block_index) is not int or not 0 <= block_index < s.count:
            raise FileError('Invalid block index')
        s.index = block_index


def clm_file_seek_first(file_id: int) -> None: clm_file_seek(file_id, 0)
def clm_file_seek_last(file_id: int) -> None:
    with _stream_lock: s = _stream(file_id); index = s.count - 1
    clm_file_seek(file_id, index)
def clm_file_seek_next(file_id: int) -> None:
    with _stream_lock: s = _stream(file_id); index = s.index + 1
    clm_file_seek(file_id, index)
def clm_file_seek_previous(file_id: int) -> None:
    with _stream_lock: s = _stream(file_id); index = s.index - 1
    clm_file_seek(file_id, index)


def clm_file_seek_auto_move_enable(file_id: int) -> None:
    with _stream_lock: _stream(file_id).auto_move = True

def clm_file_seek_auto_move_disable(file_id: int) -> None:
    with _stream_lock: _stream(file_id).auto_move = False


def _load_block(file_id, storage_id, append):
    with _stream_lock:
        s = _stream(file_id)
        if s.index < 0: raise EOFError('Empty file has no blocks')
        info, old = get_values(storage_id, 'dynamic', writable=True)
        if info.size != s.block_size: raise FileError('Storage DYNAMIC width must match stream block size')
        # Read without modifying the stream seek state; the seek cursor is logical.
        s.file.seek(s.index * s.block_size)
        data = s.file.read(s.block_size)
        if not data: raise EOFError('Unexpected end of file')
        padded = data.ljust(s.block_size, b'\0')
        replace_storage(storage_id, 'dynamic', old + [padded] if append else [padded])
        s.last_read_size = len(data)
        if s.auto_move and s.index < s.count - 1:
            s.index += 1
        return len(data)


def clm_file_load_block(file_id: int, storage_id: int) -> int:
    """Overwrite with one padded DYNAMIC record; return valid bytes (not padding)."""
    return _load_block(file_id, storage_id, False)


def clm_file_load_block_append(file_id: int, storage_id: int) -> int:
    """Append one padded DYNAMIC record; return valid bytes in appended record."""
    return _load_block(file_id, storage_id, True)


def clm_file_free(storage_id: int) -> None:
    from clm_storage import clm_storage_clear
    info, _ = get_values(storage_id, 'dynamic', writable=True)
    if clm_storage_clear(storage_id) != STORAGE_SUCCESS: raise FileError('Unable to clear Storage')

__all__ = [k for k in globals() if k.startswith('clm_file_')] + ['FileError', 'StreamInfo', 'UnsupportedFilesystemOperation']
