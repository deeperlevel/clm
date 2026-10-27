"""Shared private filesystem helpers. Not a public CLM module."""
import os
import stat
from datetime import datetime, timezone
from clm_path import clm_path_get
from clm_storage import (clm_storage_get_info, clm_storage_get, clm_storage_clear,
                         clm_storage_append, clm_storage_set_index, STORAGE_SUCCESS)

class CLMFilesystemError(RuntimeError):
    pass

class UnsupportedFilesystemOperation(CLMFilesystemError):
    pass

def path(path_id):
    p = clm_path_get(path_id)
    if not p:
        raise CLMFilesystemError('Empty mapped path')
    # Avoid silently interpreting a Windows absolute drive path on POSIX.
    if os.name != 'nt' and len(p) >= 2 and p[1] == ':' and p[0].isalpha():
        raise CLMFilesystemError('Windows drive path cannot be used on this host')
    return p

def get_values(sid, kind, *, writable=False):
    from clm_storage import STORAGE_TYPE_STRING, STORAGE_TYPE_BYTE, STORAGE_TYPE_DYNAMIC
    types = {'string': STORAGE_TYPE_STRING, 'byte': STORAGE_TYPE_BYTE, 'dynamic': STORAGE_TYPE_DYNAMIC}
    status, info = clm_storage_get_info(sid)
    if status != STORAGE_SUCCESS or info.kind != 'linear' or info.type != types[kind]:
        raise CLMFilesystemError(f'Storage {sid} must be linear {kind.upper()}')
    if writable and info.locked:
        raise CLMFilesystemError('Storage is locked')
    values = []
    for i in range(info.committed):
        status, v = clm_storage_get(sid, i)
        if status != STORAGE_SUCCESS:
            raise CLMFilesystemError(f'Unable to read storage index {i}')
        values.append(v)
    return info, values

def replace_storage(sid, kind, values, *, require_capacity=False):
    """Preflight and replace; rollback best effort for unexpected storage failures."""
    info, previous = get_values(sid, kind, writable=True)
    values = list(values)
    if require_capacity and len(values) > info.reserved:
        raise CLMFilesystemError('Destination storage has insufficient reserved capacity')
    for v in values:
        if kind == 'string' and (not isinstance(v, str) or len(v.encode('utf8')) > info.size):
            raise CLMFilesystemError('STRING value exceeds Storage element size')
        if kind == 'byte' and (type(v) is not int or not -128 <= v <= 127):
            raise CLMFilesystemError('BYTE value outside signed 8-bit range')
        if kind == 'dynamic' and (not isinstance(v, (bytes, bytearray)) or len(v) != info.size):
            raise CLMFilesystemError('DYNAMIC record width mismatch')
    def put(items):
        if clm_storage_clear(sid) != STORAGE_SUCCESS:
            raise CLMFilesystemError('Failed to clear destination Storage')
        for item in items:
            if clm_storage_append(sid, item) != STORAGE_SUCCESS:
                raise CLMFilesystemError('Failed to append to destination Storage')
    try:
        put(values)
        if info.index >= 0 and values:
            clm_storage_set_index(sid, min(info.index, len(values)-1))
    except Exception:
        put(previous)
        if info.index >= 0 and previous:
            clm_storage_set_index(sid, info.index)
        raise

def permissions_get(p, which):
    mode = os.stat(p).st_mode
    if which == 'read': return bool(mode & (stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH))
    if which == 'write': return bool(mode & (stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH))
    if which == 'execute': return bool(mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))
    if which == 'hidden':
        if os.name == 'nt':
            import ctypes
            flags = ctypes.windll.kernel32.GetFileAttributesW(str(p))
            if flags == 0xFFFFFFFF: raise OSError('GetFileAttributesW failed')
            return bool(flags & 0x2)
        return os.path.basename(os.path.normpath(p)).startswith('.')
    raise ValueError(which)

def permissions_set(p, which, enabled):
    if type(enabled) not in (bool, int) or enabled not in (0, 1):
        raise ValueError('Attribute value must be 0 or 1')
    if which == 'hidden':
        if os.name != 'nt':
            raise UnsupportedFilesystemOperation('Changing POSIX hidden state requires renaming the path')
        import ctypes
        attrs = ctypes.windll.kernel32.GetFileAttributesW(str(p))
        if attrs == 0xFFFFFFFF: raise OSError('GetFileAttributesW failed')
        new_attrs = (attrs | 0x2) if enabled else (attrs & ~0x2)
        if not ctypes.windll.kernel32.SetFileAttributesW(str(p), new_attrs):
            raise OSError('SetFileAttributesW failed')
        return
    flags = {'read': stat.S_IRUSR, 'write': stat.S_IWUSR, 'execute': stat.S_IXUSR}
    bit = flags[which]
    if os.name == 'nt' and which == 'execute':
        raise UnsupportedFilesystemOperation('Windows execute permission is not a portable file attribute')
    if os.name == 'nt' and which == 'read':
        raise UnsupportedFilesystemOperation('Windows read permission is not a portable file attribute')
    mode = stat.S_IMODE(os.stat(p).st_mode)
    os.chmod(p, (mode | bit) if enabled else (mode & ~bit))

def timestamp_get(p, field):
    st = os.stat(p)
    if field == 'creation':
        if os.name == 'nt': t = st.st_ctime
        elif hasattr(st, 'st_birthtime'): t = st.st_birthtime
        else: raise UnsupportedFilesystemOperation('Creation time unavailable on this platform')
    elif field == 'accessed': t = st.st_atime
    elif field == 'write': t = st.st_mtime
    else: raise ValueError(field)
    return int(round(t * 1000))

def timestamp_set(p, field, millis):
    if type(millis) is not int:
        raise ValueError('Timestamp must be integer Unix UTC milliseconds')
    if field == 'creation':
        raise UnsupportedFilesystemOperation('Setting creation time not supported by this portable implementation')
    st = os.stat(p)
    atime = millis / 1000 if field == 'accessed' else st.st_atime
    mtime = millis / 1000 if field == 'write' else st.st_mtime
    os.utime(p, (atime, mtime))
