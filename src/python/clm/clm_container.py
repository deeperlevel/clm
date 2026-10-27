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

"""ID-addressed, typed containers with a procedural, non-OOP-compatible API.

Containers use Python dictionaries internally; no custom memory allocator is needed.
Container IDs are unique within this module, and keys are integer indexes.
The optional :class:`Container` facade forwards directly to the functions.
"""

from typing import Any
from dataclasses import dataclass
import math

CONTAINER_TYPE_STRING = 0x01
CONTAINER_TYPE_BYTE = 0x02
CONTAINER_TYPE_SHORT = 0x03
CONTAINER_TYPE_INTEGER = 0x04
CONTAINER_TYPE_LONG_INTEGER = 0x05
CONTAINER_TYPE_FLOAT = 0x06
CONTAINER_TYPE_DOUBLE = 0x07
CONTAINER_TYPE_DYNAMIC = 0xFE
CONTAINER_TYPE_OBJECT = 0xFF

CONTAINER_SUCCESS = 0x00
CONTAINER_ERROR_DUPLICATE_ID = 0x01
CONTAINER_ERROR_ID_NOT_FOUND = 0x02
CONTAINER_ERROR_TYPE_NOT_SUPPORTED = 0x03
CONTAINER_ERROR_TYPE_NOT_MATCHED = 0x04
CONTAINER_ERROR_NOT_FOUND = 0x05
CONTAINER_ERROR_INVALID_VALUE = 0x06


class _Containers:
    """Private holder for typed container dictionaries and their ID-to-type index."""

    indexes: dict[int, int] = {}
    references: dict[int, int] = {}
    cursors: dict[int, int] = {}  # Current key per container, when positioned.
    containers_string: dict[int, dict[int, str]] = {}
    containers_i8: dict[int, dict[int, int]] = {}
    containers_i16: dict[int, dict[int, int]] = {}
    containers_i32: dict[int, dict[int, int]] = {}
    containers_i64: dict[int, dict[int, int]] = {}
    containers_float: dict[int, dict[int, float]] = {}
    containers_double: dict[int, dict[int, float]] = {}
    containers_object: dict[int, dict[int, Any]] = {}


_TYPE_STORES: dict[int, dict[int, dict[int, Any]]] = {
    CONTAINER_TYPE_STRING: _Containers.containers_string,
    CONTAINER_TYPE_BYTE: _Containers.containers_i8,
    CONTAINER_TYPE_SHORT: _Containers.containers_i16,
    CONTAINER_TYPE_INTEGER: _Containers.containers_i32,
    CONTAINER_TYPE_LONG_INTEGER: _Containers.containers_i64,
    CONTAINER_TYPE_FLOAT: _Containers.containers_float,
    CONTAINER_TYPE_DOUBLE: _Containers.containers_double,
    CONTAINER_TYPE_OBJECT: _Containers.containers_object,
}

_INTEGER_BITS = {
    CONTAINER_TYPE_BYTE: 8,
    CONTAINER_TYPE_SHORT: 16,
    CONTAINER_TYPE_INTEGER: 32,
    CONTAINER_TYPE_LONG_INTEGER: 64,
}


def _clm_container_value_valid(container_type: int, value: Any) -> bool:
    """Validate values against the declared type; integer variants are signed."""
    if container_type == CONTAINER_TYPE_STRING:
        return isinstance(value, str)
    if container_type in _INTEGER_BITS:
        bits = _INTEGER_BITS[container_type]
        return type(value) is int and -(1 << (bits - 1)) <= value < (1 << (bits - 1))
    if container_type in (CONTAINER_TYPE_FLOAT, CONTAINER_TYPE_DOUBLE):
        if type(value) is not float:
            return False
        if container_type == CONTAINER_TYPE_FLOAT:
            if math.isfinite(value) and abs(value) > 3.4028234663852886e38:
                return False
        return True
    return container_type == CONTAINER_TYPE_OBJECT


def _clm_container_lookup(container_id: int, container_type: int):
    """Return (error code, typed dictionary) for a particular container."""
    actual_type = _Containers.indexes.get(container_id)
    if actual_type is None:
        return CONTAINER_ERROR_NOT_FOUND, None
    if actual_type != container_type:
        return CONTAINER_ERROR_TYPE_NOT_MATCHED, None
    return CONTAINER_SUCCESS, _TYPE_STORES[container_type][container_id]

def clm_container_create(container_id: int, container_type: int) -> int:
    """Create an empty typed container with a unique integer ID; return a status code."""
    if container_id in _Containers.indexes:
        return CONTAINER_ERROR_DUPLICATE_ID
    if container_type not in _TYPE_STORES:
        return CONTAINER_ERROR_TYPE_NOT_SUPPORTED
    _TYPE_STORES[container_type][container_id] = {}
    _Containers.indexes[container_id] = container_type
    _Containers.references[container_id] = 1
    return CONTAINER_SUCCESS

def _clm_container_add(container_id: int, container_type: int, key: int, value: Any) -> int:
    """Insert a new indexed value, rejecting missing IDs, type mismatches, and duplicate keys."""
    status, values = _clm_container_lookup(container_id, container_type)
    if status != CONTAINER_SUCCESS:
        return status
    if not _clm_container_value_valid(container_type, value):
        return CONTAINER_ERROR_INVALID_VALUE
    if key in values:
        return CONTAINER_ERROR_DUPLICATE_ID
    values[key] = value
    return CONTAINER_SUCCESS

def _clm_container_edit(container_id: int, container_type: int, key: int, value: Any) -> int:
    """Replace an existing indexed value without inserting a missing key."""
    status, values = _clm_container_lookup(container_id, container_type)
    if status != CONTAINER_SUCCESS:
        return status
    if key not in values:
        return CONTAINER_ERROR_ID_NOT_FOUND
    if not _clm_container_value_valid(container_type, value):
        return CONTAINER_ERROR_INVALID_VALUE
    values[key] = value
    return CONTAINER_SUCCESS

def _clm_container_delete(container_id: int, container_type: int, key: int) -> int:
    """Delete an existing entry; do not remove the container itself."""
    status, values = _clm_container_lookup(container_id, container_type)
    if status != CONTAINER_SUCCESS:
        return status
    if key not in values:
        return CONTAINER_ERROR_ID_NOT_FOUND
    del values[key]
    if _Containers.cursors.get(container_id) == key:
        _Containers.cursors.pop(container_id, None)
    return CONTAINER_SUCCESS

def _clm_container_get(container_id: int, container_type: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value); a missing entry returns (ID_NOT_FOUND, None)."""
    status, values = _clm_container_lookup(container_id, container_type)
    if status != CONTAINER_SUCCESS:
        return status, None
    if key not in values:
        return CONTAINER_ERROR_ID_NOT_FOUND, None
    return CONTAINER_SUCCESS, values[key]

def clm_container_contains(container_id: int, key: int) -> tuple[int, bool]:
    """Check whether an index exists; return (status, exists)."""
    container_type = _Containers.indexes.get(container_id)
    if container_type is None:
        return CONTAINER_ERROR_NOT_FOUND, False
    return CONTAINER_SUCCESS, key in _TYPE_STORES[container_type][container_id]

def clm_container_count(container_id: int) -> tuple[int, int]:
    """Return (status, number of entries) for a container."""
    container_type = _Containers.indexes.get(container_id)
    if container_type is None:
        return CONTAINER_ERROR_NOT_FOUND, 0
    return CONTAINER_SUCCESS, len(_TYPE_STORES[container_type][container_id])

def clm_container_clear(container_id: int) -> int:
    """Remove all entries without deleting the container's ID or declared type."""
    container_type = _Containers.indexes.get(container_id)
    if container_type is None:
        return CONTAINER_ERROR_NOT_FOUND
    _TYPE_STORES[container_type][container_id].clear()
    _Containers.cursors.pop(container_id, None)
    return CONTAINER_SUCCESS

def clm_container_destroy(container_id: int) -> int:
    """Remove a container and release its Python references; its ID becomes reusable."""
    container_type = _Containers.indexes.pop(container_id, None)
    if container_type is None:
        return CONTAINER_ERROR_NOT_FOUND
    del _TYPE_STORES[container_type][container_id]
    _Containers.cursors.pop(container_id, None)
    _Containers.references.pop(container_id, None)
    return CONTAINER_SUCCESS

def _clm_container_move(container_id: int, direction: str) -> tuple[int, int | None]:
    """Move the container's cursor in insertion order and return (status, key).

    The cursor is per container ID. Missing positions and empty containers return
    ID_NOT_FOUND; missing container IDs return NOT_FOUND. Moving past either end
    leaves the cursor on its last valid entry.
    """
    container_type = _Containers.indexes.get(container_id)
    if container_type is None:
        return CONTAINER_ERROR_NOT_FOUND, None

    values = _TYPE_STORES[container_type][container_id]
    if not values:
        _Containers.cursors.pop(container_id, None)
        return CONTAINER_ERROR_ID_NOT_FOUND, None

    if direction == "first":
        key = next(iter(values))
    elif direction == "last":
        key = next(reversed(values))
    else:
        current = _Containers.cursors.get(container_id)
        if current is None or current not in values:
            return CONTAINER_ERROR_ID_NOT_FOUND, None

        # Keep the shared API procedural. No Python iterator escapes this call.
        keys = list(values)
        position = keys.index(current) + (1 if direction == "next" else -1)
        if not 0 <= position < len(keys):
            return CONTAINER_ERROR_ID_NOT_FOUND, None
        key = keys[position]

    _Containers.cursors[container_id] = key
    return CONTAINER_SUCCESS, key

def clm_container_first(container_id: int) -> tuple[int, int | None]:
    """Position at the first inserted entry and return (status, entry_key)."""
    return _clm_container_move(container_id, "first")

def clm_container_previous(container_id: int) -> tuple[int, int | None]:
    """Move to the previous entry and return (status, entry_key).

    Call ``first`` or ``last`` first to initialize the cursor. At the beginning,
    return ID_NOT_FOUND without advancing the cursor.
    """
    return _clm_container_move(container_id, "previous")

def clm_container_next(container_id: int) -> tuple[int, int | None]:
    """Move to the next entry and return (status, entry_key).

    Call ``first`` or ``last`` first to initialize the cursor. At the end,
    return ID_NOT_FOUND without advancing the cursor.
    """
    return _clm_container_move(container_id, "next")

def clm_container_last(container_id: int) -> tuple[int, int | None]:
    """Position at the last inserted entry and return (status, entry_key)."""
    return _clm_container_move(container_id, "last")

def clm_container_add_string(container_id: int, key: int, value: str) -> int:
    """Add a new entry in a string container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_STRING, key, value)

def clm_container_edit_string(container_id: int, key: int, value: str) -> int:
    """Update an existing entry in a string container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_STRING, key, value)

def clm_container_delete_string(container_id: int, key: int) -> int:
    """Delete an existing entry in a string container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_STRING, key)

def clm_container_get_string(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in a string container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_STRING, key)

def clm_container_add_byte(container_id: int, key: int, value: int) -> int:
    """Add a new entry in a byte container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_BYTE, key, value)

def clm_container_edit_byte(container_id: int, key: int, value: int) -> int:
    """Update an existing entry in a byte container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_BYTE, key, value)

def clm_container_delete_byte(container_id: int, key: int) -> int:
    """Delete an existing entry in a byte container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_BYTE, key)

def clm_container_get_byte(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in a byte container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_BYTE, key)

def clm_container_add_short(container_id: int, key: int, value: int) -> int:
    """Add a new entry in a short container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_SHORT, key, value)

def clm_container_edit_short(container_id: int, key: int, value: int) -> int:
    """Update an existing entry in a short container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_SHORT, key, value)

def clm_container_delete_short(container_id: int, key: int) -> int:
    """Delete an existing entry in a short container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_SHORT, key)

def clm_container_get_short(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in a short container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_SHORT, key)

def clm_container_add_integer(container_id: int, key: int, value: int) -> int:
    """Add a new entry in an integer container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_INTEGER, key, value)

def clm_container_edit_integer(container_id: int, key: int, value: int) -> int:
    """Update an existing entry in an integer container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_INTEGER, key, value)

def clm_container_delete_integer(container_id: int, key: int) -> int:
    """Delete an existing entry in an integer container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_INTEGER, key)

def clm_container_get_integer(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in an integer container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_INTEGER, key)

def clm_container_add_long(container_id: int, key: int, value: int) -> int:
    """Add a new entry in a long container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_LONG_INTEGER, key, value)

def clm_container_edit_long(container_id: int, key: int, value: int) -> int:
    """Update an existing entry in a long container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_LONG_INTEGER, key, value)

def clm_container_delete_long(container_id: int, key: int) -> int:
    """Delete an existing entry in a long container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_LONG_INTEGER, key)

def clm_container_get_long(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in a long container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_LONG_INTEGER, key)

def clm_container_add_float(container_id: int, key: int, value: float) -> int:
    """Add a new entry in a float container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_FLOAT, key, value)

def clm_container_edit_float(container_id: int, key: int, value: float) -> int:
    """Update an existing entry in a float container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_FLOAT, key, value)

def clm_container_delete_float(container_id: int, key: int) -> int:
    """Delete an existing entry in a float container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_FLOAT, key)

def clm_container_get_float(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in a float container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_FLOAT, key)

def clm_container_add_double(container_id: int, key: int, value: float) -> int:
    """Add a new entry in a double container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_DOUBLE, key, value)

def clm_container_edit_double(container_id: int, key: int, value: float) -> int:
    """Update an existing entry in a double container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_DOUBLE, key, value)

def clm_container_delete_double(container_id: int, key: int) -> int:
    """Delete an existing entry in a double container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_DOUBLE, key)

def clm_container_get_double(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in a double container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_DOUBLE, key)

def clm_container_add_object(container_id: int, key: int, value: Any) -> int:
    """Add a new entry in an object container."""
    return _clm_container_add(container_id, CONTAINER_TYPE_OBJECT, key, value)

def clm_container_edit_object(container_id: int, key: int, value: Any) -> int:
    """Update an existing entry in an object container."""
    return _clm_container_edit(container_id, CONTAINER_TYPE_OBJECT, key, value)

def clm_container_delete_object(container_id: int, key: int) -> int:
    """Delete an existing entry in an object container."""
    return _clm_container_delete(container_id, CONTAINER_TYPE_OBJECT, key)

def clm_container_get_object(container_id: int, key: int) -> tuple[int, Any]:
    """Retrieve an entry as (status, value) in an object container."""
    return _clm_container_get(container_id, CONTAINER_TYPE_OBJECT, key)



@dataclass(frozen=True)
class ContainerInfo:
    """Detached, read-only snapshot; reserved=-1 means capacity is not exposed."""
    id: int
    type: int
    committed: int
    reserved: int
    references: int
    index: int


def _clm_container_values(container_id: int):
    kind = _Containers.indexes.get(container_id)
    if kind is None:
        return None
    return _TYPE_STORES[kind][container_id]


def _clm_container_position(container_id: int) -> int:
    values = _clm_container_values(container_id)
    if values is None:
        return -1
    key = _Containers.cursors.get(container_id)
    if key is None or key not in values:
        return -1
    return list(values).index(key)


def clm_container_get_info(container_id: int) -> tuple[int, ContainerInfo | None]:
    """Return a detached metadata snapshot, or (NOT_FOUND, None)."""
    kind = _Containers.indexes.get(container_id)
    if kind is None:
        return CONTAINER_ERROR_NOT_FOUND, None
    values = _TYPE_STORES[kind][container_id]
    return CONTAINER_SUCCESS, ContainerInfo(
        container_id, kind, len(values), -1,
        _Containers.references[container_id], _clm_container_position(container_id)
    )


def clm_container_reference(container_id: int) -> int:
    """Retain one explicit owner of a container, including zero-reference containers."""
    if container_id not in _Containers.indexes:
        return CONTAINER_ERROR_NOT_FOUND
    _Containers.references[container_id] += 1
    return CONTAINER_SUCCESS


def clm_container_unreference(container_id: int) -> int:
    """Release one explicit owner, without destroying the container."""
    refs = _Containers.references.get(container_id)
    if refs is None:
        return CONTAINER_ERROR_NOT_FOUND
    if refs == 0:
        return CONTAINER_ERROR_INVALID_VALUE
    _Containers.references[container_id] = refs - 1
    return CONTAINER_SUCCESS


def clm_container_pending_count() -> int:
    """Count registered containers eligible for disposal (references == 0).

    This is a read-only snapshot; it neither decrements references nor disposes.
    Do not call concurrently with mutations of the module registry.
    """
    return sum(refs == 0 for refs in _Containers.references.values())


def clm_container_dispose() -> tuple[int, int]:
    """Destroy zero-reference containers; return (status, destroyed_count).

    Do not run concurrently with registry mutation. Explicit destroy() remains
    unconditional for backward compatibility.
    """
    eligible = [cid for cid, refs in tuple(_Containers.references.items()) if refs == 0]
    disposed = 0
    for cid in eligible:
        if _Containers.references.get(cid) == 0:
            if clm_container_destroy(cid) == CONTAINER_SUCCESS:
                disposed += 1
    return CONTAINER_SUCCESS, disposed


def clm_container_iterate(container_id: int) -> int:
    """Position the shared cursor at the first key; return status only."""
    return clm_container_first(container_id)[0]


def clm_container_iterate_backward(container_id: int) -> int:
    """Position the shared cursor at the last key; return status only."""
    return clm_container_last(container_id)[0]


def clm_container_move_first(container_id: int) -> int:
    return clm_container_first(container_id)[0]


def clm_container_move_last(container_id: int) -> int:
    return clm_container_last(container_id)[0]


def clm_container_move_next(container_id: int) -> int:
    return clm_container_next(container_id)[0]


def clm_container_move_previous(container_id: int) -> int:
    return clm_container_previous(container_id)[0]


def clm_container_get(container_id: int) -> tuple[int, Any]:
    """Get the value at the current cursor without moving it."""
    values = _clm_container_values(container_id)
    if values is None:
        return CONTAINER_ERROR_NOT_FOUND, None
    key = _Containers.cursors.get(container_id)
    if key is None or key not in values:
        return CONTAINER_ERROR_ID_NOT_FOUND, None
    return CONTAINER_SUCCESS, values[key]


def clm_container_get_key(container_id: int) -> tuple[int, int | None]:
    """Get the entry key at the current cursor without moving it."""
    values = _clm_container_values(container_id)
    if values is None:
        return CONTAINER_ERROR_NOT_FOUND, None
    key = _Containers.cursors.get(container_id)
    if key is None or key not in values:
        return CONTAINER_ERROR_ID_NOT_FOUND, None
    return CONTAINER_SUCCESS, key


def clm_container_is_first(container_id: int) -> tuple[int, bool]:
    status, index = clm_container_get_index(container_id)
    return status, status == CONTAINER_SUCCESS and index == 0


def clm_container_is_last(container_id: int) -> tuple[int, bool]:
    status, index = clm_container_get_index(container_id)
    if status != CONTAINER_SUCCESS:
        return status, False
    return CONTAINER_SUCCESS, index == len(_clm_container_values(container_id)) - 1


def clm_container_get_count(container_id: int) -> tuple[int, int]:
    return clm_container_count(container_id)


def clm_container_get_index(container_id: int) -> tuple[int, int]:
    """Get positional index (zero-based), or -1 if unpositioned."""
    if container_id not in _Containers.indexes:
        return CONTAINER_ERROR_NOT_FOUND, -1
    return CONTAINER_SUCCESS, _clm_container_position(container_id)


def clm_container_set_index(container_id: int, index: int) -> int:
    """Position the cursor by insertion-order index; reject invalid indices."""
    values = _clm_container_values(container_id)
    if values is None:
        return CONTAINER_ERROR_NOT_FOUND
    if type(index) is not int or not 0 <= index < len(values):
        return CONTAINER_ERROR_INVALID_VALUE
    _Containers.cursors[container_id] = list(values)[index]
    return CONTAINER_SUCCESS


def clm_container_slice(
    source: int, destination: int, start: int, count: int
) -> int:
    """Copy a keyed insertion-order range, replacing destination atomically.

    Original entry keys are retained. Object values are shallow references.
    Neither cursor changes on error; destination cursor resets on success.
    """
    src = _clm_container_values(source)
    dst = _clm_container_values(destination)
    if src is None or dst is None:
        return CONTAINER_ERROR_NOT_FOUND
    if source == destination or type(start) is not int or type(count) is not int:
        return CONTAINER_ERROR_INVALID_VALUE
    if _Containers.indexes[source] != _Containers.indexes[destination]:
        return CONTAINER_ERROR_TYPE_NOT_MATCHED
    if start < 0 or count < 0 or start > len(src) or count > len(src) - start:
        return CONTAINER_ERROR_INVALID_VALUE
    try:
        selected = dict(list(src.items())[start:start + count])
    except MemoryError:
        return CONTAINER_ERROR_INVALID_VALUE
    _TYPE_STORES[_Containers.indexes[destination]][destination] = selected
    _Containers.cursors.pop(destination, None)
    return CONTAINER_SUCCESS


class Container:
    """Convenience facade for the procedural CLM container API."""

    create = staticmethod(clm_container_create)
    add_string = staticmethod(clm_container_add_string)
    edit_string = staticmethod(clm_container_edit_string)
    delete_string = staticmethod(clm_container_delete_string)
    get_string = staticmethod(clm_container_get_string)
    add_byte = staticmethod(clm_container_add_byte)
    edit_byte = staticmethod(clm_container_edit_byte)
    delete_byte = staticmethod(clm_container_delete_byte)
    get_byte = staticmethod(clm_container_get_byte)
    add_short = staticmethod(clm_container_add_short)
    edit_short = staticmethod(clm_container_edit_short)
    delete_short = staticmethod(clm_container_delete_short)
    get_short = staticmethod(clm_container_get_short)
    add_integer = staticmethod(clm_container_add_integer)
    edit_integer = staticmethod(clm_container_edit_integer)
    delete_integer = staticmethod(clm_container_delete_integer)
    get_integer = staticmethod(clm_container_get_integer)
    add_long = staticmethod(clm_container_add_long)
    edit_long = staticmethod(clm_container_edit_long)
    delete_long = staticmethod(clm_container_delete_long)
    get_long = staticmethod(clm_container_get_long)
    add_float = staticmethod(clm_container_add_float)
    edit_float = staticmethod(clm_container_edit_float)
    delete_float = staticmethod(clm_container_delete_float)
    get_float = staticmethod(clm_container_get_float)
    add_double = staticmethod(clm_container_add_double)
    edit_double = staticmethod(clm_container_edit_double)
    delete_double = staticmethod(clm_container_delete_double)
    get_double = staticmethod(clm_container_get_double)
    add_object = staticmethod(clm_container_add_object)
    edit_object = staticmethod(clm_container_edit_object)
    delete_object = staticmethod(clm_container_delete_object)
    get_object = staticmethod(clm_container_get_object)
    contains = staticmethod(clm_container_contains)
    count = staticmethod(clm_container_count)
    clear = staticmethod(clm_container_clear)
    destroy = staticmethod(clm_container_destroy)
    first = staticmethod(clm_container_first)
    previous = staticmethod(clm_container_previous)
    next = staticmethod(clm_container_next)
    last = staticmethod(clm_container_last)

    get_info = staticmethod(clm_container_get_info)
    reference = staticmethod(clm_container_reference)
    unreference = staticmethod(clm_container_unreference)
    dispose = staticmethod(clm_container_dispose)
    pending_count = staticmethod(clm_container_pending_count)
    iterate = staticmethod(clm_container_iterate)
    iterate_backward = staticmethod(clm_container_iterate_backward)
    move_first = staticmethod(clm_container_move_first)
    move_last = staticmethod(clm_container_move_last)
    move_next = staticmethod(clm_container_move_next)
    move_previous = staticmethod(clm_container_move_previous)
    get = staticmethod(clm_container_get)
    get_key = staticmethod(clm_container_get_key)
    is_first = staticmethod(clm_container_is_first)
    is_last = staticmethod(clm_container_is_last)
    get_count = staticmethod(clm_container_get_count)
    get_index = staticmethod(clm_container_get_index)
    set_index = staticmethod(clm_container_set_index)
    slice = staticmethod(clm_container_slice)
