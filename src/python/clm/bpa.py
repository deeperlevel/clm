"""Python-managed implementation of the CLM block-pool allocation API.

Fixed-width payloads use ``bytearray`` storage and string payloads use immutable
``bytes`` values. Python owns allocation and reclamation; the C slab pool is not
replicated. Registry operations are process-global and are not thread-safe.
"""

from __future__ import annotations

import random
import struct
from dataclasses import dataclass, field
from functools import cmp_to_key
from typing import Callable, TypeAlias

CLM_BPA_FAILURE = -1
CLM_BPA_SUCCESS = 0
CLM_BPA_GET_SUCCESS = 1
CLM_BPA_SET_SUCCESS = 2

CLM_BPA_TYPE_STRUCT = 0
CLM_BPA_TYPE_SIGNED_BYTE = 1
CLM_BPA_TYPE_BYTE = 2
CLM_BPA_TYPE_SHORT = 3
CLM_BPA_TYPE_UNSIGNED_SHORT = 4
CLM_BPA_TYPE_INT = 5
CLM_BPA_TYPE_UNSIGNED_INT = 6
CLM_BPA_TYPE_LONG = 7
CLM_BPA_TYPE_UNSIGNED_LONG = 8
CLM_BPA_TYPE_FLOAT = 9
CLM_BPA_TYPE_DOUBLE = 10
CLM_BPA_TYPE_STRING = 11
CLM_BPA_TYPE_UUID = 12
CLM_BPA_TYPE_DATETIME = 13

CLM_BPA_INITIAL_CAPACITY = 16
CLM_BPA_POOL_SLAB_SLOTS = 32
CLM_BPA_POOL_MAX_SLOT = 4096
CLM_BPA_POOL_CLASSES = 10
_MAX_ID = (1 << 64) - 1
_MAX_REFCOUNT = (1 << 16) - 1

_TYPE_FORMATS: dict[int, str] = {
    CLM_BPA_TYPE_SIGNED_BYTE: "b",
    CLM_BPA_TYPE_BYTE: "B",
    CLM_BPA_TYPE_SHORT: "h",
    CLM_BPA_TYPE_UNSIGNED_SHORT: "H",
    CLM_BPA_TYPE_INT: "i",
    CLM_BPA_TYPE_UNSIGNED_INT: "I",
    CLM_BPA_TYPE_LONG: "q",
    CLM_BPA_TYPE_UNSIGNED_LONG: "Q",
    CLM_BPA_TYPE_FLOAT: "f",
    CLM_BPA_TYPE_DOUBLE: "d",
    CLM_BPA_TYPE_DATETIME: "q",
}
_TYPE_SIZES = {kind: struct.calcsize("=" + fmt) for kind, fmt in _TYPE_FORMATS.items()}
_TYPE_SIZES[CLM_BPA_TYPE_UUID] = 16

RawValue: TypeAlias = bytes | bytearray | memoryview
CompareFunction: TypeAlias = Callable[[bytes, bytes], int]
PredicateFunction: TypeAlias = Callable[[bytes, bytes], bool]


@dataclass(slots=True)
class _Block:
    id: int
    type: int
    block_size: int
    refcount: int = 1
    encoding: int = 0
    data: bytearray | None = None
    text: bytes | None = None

    @property
    def length(self) -> int:
        return len(self.text) if self.text is not None else 0


@dataclass(slots=True)
class _Array:
    id: int
    type: int
    block_size: int
    encoding: int
    capacity: int
    refcount: int = 1
    count: int = 0
    index: int = 0
    sealed: bool = False
    deck_size: int = 0
    data: bytearray | list[bytes] = field(default_factory=bytearray)


@dataclass(slots=True)
class ArrayBorrow:
    """Retained handle for advanced array operations; release it when finished."""

    _array: _Array
    _released: bool = False

    @property
    def id(self) -> int:
        return self._array.id

    @property
    def type(self) -> int:
        return self._array.type

    @property
    def refcount(self) -> int:
        return self._array.refcount

    @property
    def count(self) -> int:
        return self._array.count

    @property
    def capacity(self) -> int:
        return self._array.capacity

    @property
    def block_size(self) -> int:
        return self._array.block_size

    @property
    def encoding(self) -> int:
        return self._array.encoding

    @property
    def index(self) -> int:
        return self._array.index

    @property
    def sealed(self) -> bool:
        return self._array.sealed

    @property
    def deck_size(self) -> int:
        return self._array.deck_size


_blocks: dict[int, _Block] = {}
_arrays: dict[int, _Array] = {}


def _valid_id(value: object) -> bool:
    return type(value) is int and 0 <= value <= _MAX_ID


def _valid_type(value: object) -> bool:
    return type(value) is int and (
        value == CLM_BPA_TYPE_STRUCT
        or value == CLM_BPA_TYPE_STRING
        or value == CLM_BPA_TYPE_UUID
        or value == CLM_BPA_TYPE_DATETIME
        or value in _TYPE_FORMATS
    )


def _valid_size(value: object, minimum: int = 0) -> bool:
    return type(value) is int and minimum <= value <= 0xFFFFFFFF


def _bounded_bytes(value: object, size: int | None = None) -> bytes | None:
    if isinstance(value, str):
        try:
            data = value.encode("utf-8")
        except UnicodeEncodeError:
            return None
    elif isinstance(value, (bytes, bytearray, memoryview)):
        try:
            data = bytes(value)
        except (TypeError, ValueError):
            return None
    else:
        return None
    if size is not None:
        if not _valid_size(size, 1):
            return None
        data = data[:size]
    terminator = data.find(b"\0")
    if terminator >= 0:
        data = data[:terminator]
    return data


def _fixed_bytes(value: object, size: int) -> bytes | None:
    if not isinstance(value, (bytes, bytearray, memoryview)):
        return None
    try:
        data = bytes(value)
    except (TypeError, ValueError):
        return None
    return data if len(data) == size else None


def _resolve_size(type_code: int, block_size: int) -> int | None:
    if type_code == CLM_BPA_TYPE_STRING:
        return 0 if block_size == 0 else None
    if type_code == CLM_BPA_TYPE_STRUCT:
        return block_size if block_size > 0 else None
    resolved = _TYPE_SIZES.get(type_code)
    if resolved is None or block_size not in (0, resolved):
        return None
    return resolved


def _item(array: _Array, index: int) -> bytes:
    if array.type == CLM_BPA_TYPE_STRING:
        assert isinstance(array.data, list)
        return array.data[index]
    assert isinstance(array.data, bytearray)
    start = index * array.block_size
    return bytes(array.data[start : start + array.block_size])


def _write_item(array: _Array, index: int, value: bytes) -> None:
    if array.type == CLM_BPA_TYPE_STRING:
        assert isinstance(array.data, list)
        array.data[index] = value
        return
    assert isinstance(array.data, bytearray)
    start = index * array.block_size
    array.data[start : start + array.block_size] = value


def _reset_deck(array: _Array) -> None:
    array.deck_size = array.count


def _ensure_capacity(array: _Array, needed: int) -> bool:
    if needed <= array.capacity:
        return True
    if array.sealed or needed > 0xFFFFFFFF:
        return False
    capacity = max(4, array.capacity)
    while capacity < needed:
        capacity = min(0xFFFFFFFF, capacity * 2)
        if capacity < needed and capacity == 0xFFFFFFFF:
            return False
    if array.type != CLM_BPA_TYPE_STRING:
        try:
            assert isinstance(array.data, bytearray)
            array.data.extend(b"\0" * ((capacity - array.capacity) * array.block_size))
        except (MemoryError, OverflowError):
            return False
    array.capacity = capacity
    return True


def _array_for(value: int | ArrayBorrow) -> _Array | None:
    if isinstance(value, ArrayBorrow):
        if value._released or _arrays.get(value._array.id) is not value._array:
            return None
        return value._array
    if not _valid_id(value):
        return None
    return _arrays.get(value)


def _remove_array(array: _Array) -> int:
    if _arrays.get(array.id) is not array:
        return CLM_BPA_FAILURE
    del _arrays[array.id]
    return CLM_BPA_SUCCESS


def clm_bpa_type_size(type_code: int) -> int:
    """Return the fixed storage size for a built-in type, or zero otherwise."""
    return _TYPE_SIZES.get(type_code, 0) if type(type_code) is int else 0


def clm_bpa_pool_class_for(size: int) -> int:
    """Return the C-compatible size-class index; oversized values use fallback."""
    if not _valid_size(size, 1) or size > CLM_BPA_POOL_MAX_SLOT:
        return CLM_BPA_POOL_CLASSES
    slot_size = 8
    index = 0
    while slot_size < size and index + 1 < CLM_BPA_POOL_CLASSES:
        slot_size *= 2
        index += 1
    return index if slot_size >= size else CLM_BPA_POOL_CLASSES


def clm_bpa_pool_alloc(size: int) -> bytearray | None:
    """Return zeroed Python-managed bytes; no custom slab pool is maintained."""
    if not _valid_size(size, 1):
        return None
    try:
        return bytearray(size)
    except (MemoryError, OverflowError):
        return None


def clm_bpa_pool_free(value: object, size: int | None = None) -> None:
    """Release a reference to a Python-managed allocation; reclamation is automatic."""
    del value, size


def clm_bpa_pool_cleanup() -> int:
    """No-op compatibility function; Python releases unused storage automatically."""
    return CLM_BPA_SUCCESS


def clm_bpa_pool_stats(size: int) -> tuple[int, int, int]:
    """Return failure because Python does not expose per-size allocator slab stats."""
    if clm_bpa_pool_class_for(size) == CLM_BPA_POOL_CLASSES:
        return CLM_BPA_FAILURE, 0, 0
    return CLM_BPA_FAILURE, 0, 0


def clm_bpa_hash_id(identifier: int) -> int:
    """Mix an unsigned 64-bit ID with the SplitMix64 finalizer."""
    if not _valid_id(identifier):
        return 0
    value = identifier
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & _MAX_ID
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & _MAX_ID
    return value ^ (value >> 31)


def clm_bpa_is_existing(identifier: int) -> bool:
    """Return whether an individual block ID is registered."""
    return _valid_id(identifier) and identifier in _blocks


def clm_bpa_get_encoding(identifier: int) -> tuple[int, int]:
    """Return a block's encoding metadata as ``(status, encoding)``."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    return (CLM_BPA_GET_SUCCESS, block.encoding) if block is not None else (CLM_BPA_FAILURE, 0)


def clm_bpa_set_encoding(identifier: int, encoding: int) -> int:
    """Set a block's one-byte encoding metadata."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None or not _valid_size(encoding) or encoding > 0xFF:
        return CLM_BPA_FAILURE
    block.encoding = encoding
    return CLM_BPA_SUCCESS


def clm_bpaa_is_existing(identifier: int) -> bool:
    """Return whether an array ID is registered."""
    return _valid_id(identifier) and identifier in _arrays


def clm_bpa_create_sized(identifier: int, type_code: int, block_size: int) -> int:
    """Create a zero-initialized block, requiring size for STRUCT blocks."""
    if not _valid_id(identifier) or not _valid_type(type_code) or not _valid_size(block_size):
        return CLM_BPA_FAILURE
    if identifier in _blocks:
        return CLM_BPA_FAILURE
    size = _resolve_size(type_code, block_size)
    if size is None:
        return CLM_BPA_FAILURE
    block = _Block(identifier, type_code, size)
    if type_code != CLM_BPA_TYPE_STRING:
        try:
            block.data = bytearray(size)
        except (BufferError, MemoryError, OverflowError):
            return CLM_BPA_FAILURE
    _blocks[identifier] = block
    return CLM_BPA_SUCCESS


def clm_bpa_create(identifier: int, type_code: int) -> int:
    """Create a built-in or string block; STRUCT requires ``create_sized``."""
    if type_code == CLM_BPA_TYPE_STRUCT:
        return CLM_BPA_FAILURE
    return clm_bpa_create_sized(identifier, type_code, 0)


def clm_bpa_delete(identifier: int) -> int:
    """Immediately delete a block regardless of its reference count."""
    if not _valid_id(identifier) or identifier not in _blocks:
        return CLM_BPA_FAILURE
    del _blocks[identifier]
    return CLM_BPA_SUCCESS


def clm_bpa_ref(identifier: int) -> int:
    """Increment a block's reference count."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None or block.refcount == _MAX_REFCOUNT:
        return CLM_BPA_FAILURE
    block.refcount += 1
    return CLM_BPA_SUCCESS


def clm_bpa_unref(identifier: int) -> int:
    """Release a block reference and remove it when the count reaches zero."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None or block.refcount <= 0:
        return CLM_BPA_FAILURE
    block.refcount -= 1
    return clm_bpa_delete(identifier) if block.refcount == 0 else CLM_BPA_SUCCESS


def clm_bpa_get(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Return fixed block contents as ``(GET_SUCCESS, bytes)`` or failure."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if (
        block is None
        or block.type == CLM_BPA_TYPE_STRING
        or not _valid_size(buffer_size)
        or buffer_size < block.block_size
        or block.data is None
    ):
        return CLM_BPA_FAILURE, None
    return CLM_BPA_GET_SUCCESS, bytes(block.data)


def clm_bpa_set(identifier: int, buffer: RawValue, buffer_size: int | None = None) -> int:
    """Copy exact-size raw bytes into a fixed-size block."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None or block.type == CLM_BPA_TYPE_STRING or block.data is None:
        return CLM_BPA_FAILURE
    size = len(buffer) if buffer_size is None and isinstance(buffer, (bytes, bytearray, memoryview)) else buffer_size
    if not _valid_size(size) or size != block.block_size:
        return CLM_BPA_FAILURE
    value = _fixed_bytes(buffer, block.block_size)
    if value is None:
        return CLM_BPA_FAILURE
    block.data[:] = value
    return CLM_BPA_SET_SUCCESS


def clm_bpa_get_string(identifier: int, buffer_size: int | None = None) -> tuple[int, bytes | None]:
    """Return string bytes as ``(length, value)``; failure is ``(-1, None)``."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None or block.type != CLM_BPA_TYPE_STRING or block.text is None:
        return CLM_BPA_FAILURE, None
    if buffer_size is not None and (not _valid_size(buffer_size) or buffer_size < len(block.text) + 1):
        return CLM_BPA_FAILURE, None
    return len(block.text), block.text


def clm_bpa_set_string(identifier: int, buffer: str | RawValue, buffer_size: int | None = None) -> int:
    """Set bounded string bytes and return their length, or BPA_FAILURE."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    value = _bounded_bytes(buffer, buffer_size)
    if block is None or block.type != CLM_BPA_TYPE_STRING or value is None:
        return CLM_BPA_FAILURE
    block.text = value
    return len(value)


def clm_bpa_replace(identifier: int, buffer: str | RawValue, buffer_size: int | None = None) -> int:
    """Replace a fixed block or string block using the matching set operation."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None:
        return CLM_BPA_FAILURE
    if block.type == CLM_BPA_TYPE_STRING:
        return clm_bpa_set_string(identifier, buffer, buffer_size)
    return clm_bpa_set(identifier, buffer, buffer_size)


def clm_bpa_borrow(identifier: int) -> tuple[int, memoryview | None, int]:
    """Return a safe memoryview and byte count; the view keeps storage alive."""
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None:
        return CLM_BPA_FAILURE, None, 0
    if block.type == CLM_BPA_TYPE_STRING:
        if block.text is None:
            return CLM_BPA_FAILURE, None, 0
        value = memoryview(block.text + b"\0")
        return CLM_BPA_GET_SUCCESS, value, len(value)
    if block.data is None:
        return CLM_BPA_FAILURE, None, 0
    value = memoryview(block.data)
    return CLM_BPA_GET_SUCCESS, value, len(value)


def clm_bpa_copy_block(source_id: int, destination_id: int) -> int:
    """Copy compatible block contents without exposing an intermediate buffer."""
    source = _blocks.get(source_id) if _valid_id(source_id) else None
    destination = _blocks.get(destination_id) if _valid_id(destination_id) else None
    if (
        source is None
        or destination is None
        or source.type != destination.type
        or source.block_size != destination.block_size
    ):
        return CLM_BPA_FAILURE
    if source.type == CLM_BPA_TYPE_STRING:
        if source.text is None:
            return CLM_BPA_FAILURE
        destination.text = bytes(source.text)
    elif source.data is not None and destination.data is not None:
        destination.data[:] = source.data
    else:
        return CLM_BPA_FAILURE
    return CLM_BPA_SUCCESS


def clm_bpaa_create_sized(
    identifier: int,
    type_code: int,
    reserved: int = 0,
    block_size: int = 0,
    encoding: int = 0,
) -> int:
    """Create an array with initial capacity and optional STRUCT item size."""
    if (
        not _valid_id(identifier)
        or identifier in _arrays
        or not _valid_type(type_code)
        or not _valid_size(reserved)
        or not _valid_size(block_size)
        or not _valid_size(encoding, 0)
        or encoding > 0xFF
    ):
        return CLM_BPA_FAILURE
    size = _resolve_size(type_code, block_size)
    if size is None:
        return CLM_BPA_FAILURE
    array = _Array(identifier, type_code, size, encoding, reserved)
    if type_code == CLM_BPA_TYPE_STRING:
        array.data = []
    else:
        try:
            array.data = bytearray(reserved * size)
        except (BufferError, MemoryError, OverflowError):
            return CLM_BPA_FAILURE
    _arrays[identifier] = array
    return CLM_BPA_SUCCESS


def clm_bpaa_create(identifier: int, type_code: int, reserved: int = 0, encoding: int = 0) -> int:
    """Create a built-in or string array; STRUCT requires ``create_sized``."""
    if type_code == CLM_BPA_TYPE_STRUCT:
        return CLM_BPA_FAILURE
    return clm_bpaa_create_sized(identifier, type_code, reserved, 0, encoding)


def clm_bpaa_delete(identifier: int) -> int:
    """Immediately delete an array regardless of its reference count."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return _remove_array(array) if array is not None else CLM_BPA_FAILURE


def clm_bpaa_ref(identifier: int) -> int:
    """Increment an array's reference count."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.refcount == _MAX_REFCOUNT:
        return CLM_BPA_FAILURE
    array.refcount += 1
    return CLM_BPA_SUCCESS


def clm_bpaa_unref(identifier: int) -> int:
    """Release an array reference and remove it when the count reaches zero."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.refcount <= 0:
        return CLM_BPA_FAILURE
    array.refcount -= 1
    return _remove_array(array) if array.refcount == 0 else CLM_BPA_SUCCESS


def _array_insert(array: _Array, index: int, value: bytes) -> int:
    if type(index) is not int or index < 0 or index > array.count:
        return CLM_BPA_FAILURE
    if array.type == CLM_BPA_TYPE_STRING:
        if not _ensure_capacity(array, array.count + 1):
            return CLM_BPA_FAILURE
        assert isinstance(array.data, list)
        array.data.insert(index, bytes(value))
    else:
        if len(value) != array.block_size or not _ensure_capacity(array, array.count + 1):
            return CLM_BPA_FAILURE
        assert isinstance(array.data, bytearray)
        start = index * array.block_size
        array.data[start + array.block_size : (array.count + 1) * array.block_size] = array.data[
            start : array.count * array.block_size
        ]
        array.data[start : start + array.block_size] = value
    array.count += 1
    array.index = index
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_append_raw(identifier: int, buffer: str | RawValue, buffer_size: int | None = None) -> int:
    """Append one fixed-width payload or bounded string to an array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None:
        return CLM_BPA_FAILURE
    if array.type == CLM_BPA_TYPE_STRING:
        value = _bounded_bytes(buffer, buffer_size)
        return CLM_BPA_FAILURE if value is None else _array_insert(array, array.count, value)
    size = len(buffer) if buffer_size is None and isinstance(buffer, (bytes, bytearray, memoryview)) else buffer_size
    value = _fixed_bytes(buffer, array.block_size)
    if value is None or size != array.block_size:
        return CLM_BPA_FAILURE
    return _array_insert(array, array.count, value)


def clm_bpaa_insert_raw(
    identifier: int,
    index: int,
    buffer: str | RawValue,
    buffer_size: int | None = None,
) -> int:
    """Insert fixed-width bytes or a bounded string at an array index."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or type(index) is not int:
        return CLM_BPA_FAILURE
    if array.type == CLM_BPA_TYPE_STRING:
        value = _bounded_bytes(buffer, buffer_size)
    else:
        size = len(buffer) if buffer_size is None and isinstance(buffer, (bytes, bytearray, memoryview)) else buffer_size
        value = _fixed_bytes(buffer, array.block_size)
        if size != array.block_size:
            value = None
    return CLM_BPA_FAILURE if value is None else _array_insert(array, index, value)


def clm_bpaa_get_raw(identifier: int, index: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Return one fixed-width item as ``(GET_SUCCESS, bytes)``."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.type == CLM_BPA_TYPE_STRING
        or type(index) is not int
        or not _valid_size(buffer_size)
        or index < 0
        or index >= array.count
        or buffer_size < array.block_size
    ):
        return CLM_BPA_FAILURE, None
    return CLM_BPA_GET_SUCCESS, _item(array, index)


def clm_bpaa_get_string_raw(
    identifier: int,
    index: int,
    buffer_size: int | None = None,
) -> tuple[int, bytes | None]:
    """Return one string as ``(length, bytes)`` or ``(BPA_FAILURE, None)``."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.type != CLM_BPA_TYPE_STRING
        or type(index) is not int
        or index < 0
        or index >= array.count
    ):
        return CLM_BPA_FAILURE, None
    value = _item(array, index)
    if buffer_size is not None and (not _valid_size(buffer_size) or buffer_size < len(value) + 1):
        return CLM_BPA_FAILURE, None
    return len(value), value


def clm_bpaa_set_raw(identifier: int, index: int, buffer: RawValue, buffer_size: int | None = None) -> int:
    """Replace one fixed-width array item with an exact-size byte payload."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.type == CLM_BPA_TYPE_STRING
        or type(index) is not int
        or index < 0
        or index >= array.count
    ):
        return CLM_BPA_FAILURE
    size = len(buffer) if buffer_size is None and isinstance(buffer, (bytes, bytearray, memoryview)) else buffer_size
    value = _fixed_bytes(buffer, array.block_size)
    if value is None or size != array.block_size:
        return CLM_BPA_FAILURE
    _write_item(array, index, value)
    _reset_deck(array)
    return CLM_BPA_SET_SUCCESS


def clm_bpaa_set_string_raw(
    identifier: int,
    index: int,
    buffer: str | RawValue,
    buffer_size: int | None = None,
) -> int:
    """Replace one string item and return its byte length, or failure."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    value = _bounded_bytes(buffer, buffer_size)
    if (
        array is None
        or array.type != CLM_BPA_TYPE_STRING
        or type(index) is not int
        or index < 0
        or index >= array.count
        or value is None
    ):
        return CLM_BPA_FAILURE
    _write_item(array, index, value)
    _reset_deck(array)
    return len(value)


def clm_bpaa_get_first_raw(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Return the first fixed-width item, if present."""
    return clm_bpaa_get_raw(identifier, 0, buffer_size)


def clm_bpaa_get_last_raw(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Return the last fixed-width item, if present."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.count == 0:
        return CLM_BPA_FAILURE, None
    return clm_bpaa_get_raw(identifier, array.count - 1, buffer_size)


def clm_bpaa_get_current_raw(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Return the fixed-width item at the array cursor."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None:
        return CLM_BPA_FAILURE, None
    return clm_bpaa_get_raw(identifier, array.index, buffer_size)


def clm_bpaa_get_count_to(identifier: int) -> tuple[int, int]:
    """Return an array's item count as ``(GET_SUCCESS, count)``."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return (CLM_BPA_GET_SUCCESS, array.count) if array is not None else (CLM_BPA_FAILURE, 0)


def clm_bpaa_get_length_to(identifier: int) -> tuple[int, int]:
    """Return the current string item's byte length."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.type != CLM_BPA_TYPE_STRING or array.index >= array.count:
        return CLM_BPA_FAILURE, 0
    return CLM_BPA_GET_SUCCESS, len(_item(array, array.index))


def clm_bpaa_get_capacity_to(identifier: int) -> tuple[int, int]:
    """Return an array's reserved slot capacity."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return (CLM_BPA_GET_SUCCESS, array.capacity) if array is not None else (CLM_BPA_FAILURE, 0)


def clm_bpaa_extend(identifier: int, additional_capacity: int) -> int:
    """Increase an unsealed array's capacity by a positive number of slots."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.sealed or not _valid_size(additional_capacity, 1):
        return CLM_BPA_FAILURE
    new_capacity = array.capacity + additional_capacity
    if new_capacity > 0xFFFFFFFF:
        return CLM_BPA_FAILURE
    if array.type != CLM_BPA_TYPE_STRING:
        try:
            assert isinstance(array.data, bytearray)
            array.data.extend(b"\0" * (additional_capacity * array.block_size))
        except (BufferError, MemoryError, OverflowError):
            return CLM_BPA_FAILURE
    array.capacity = new_capacity
    return CLM_BPA_SUCCESS


def clm_bpaa_shrink(identifier: int, new_capacity: int) -> int:
    """Reduce an unsealed array's capacity without discarding live items."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.sealed
        or not _valid_size(new_capacity)
        or new_capacity >= array.capacity
        or new_capacity < array.count
    ):
        return CLM_BPA_FAILURE
    if array.type != CLM_BPA_TYPE_STRING:
        assert isinstance(array.data, bytearray)
        try:
            del array.data[new_capacity * array.block_size :]
        except BufferError:
            return CLM_BPA_FAILURE
    array.capacity = new_capacity
    return CLM_BPA_SUCCESS


def clm_bpaa_remove(identifier: int, index: int) -> int:
    """Remove an initialized item and retain the array's reserved capacity."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or type(index) is not int or index < 0 or index >= array.count:
        return CLM_BPA_FAILURE
    if array.type == CLM_BPA_TYPE_STRING:
        assert isinstance(array.data, list)
        array.data.pop(index)
    else:
        assert isinstance(array.data, bytearray)
        start = index * array.block_size
        end = (array.count - 1) * array.block_size
        array.data[start:end] = array.data[start + array.block_size : end + array.block_size]
        tail = (array.count - 1) * array.block_size
        array.data[tail : tail + array.block_size] = b"\0" * array.block_size
    array.count -= 1
    if array.count == 0:
        array.index = 0
    elif array.index > index:
        array.index -= 1
    elif array.index >= array.count:
        array.index = array.count - 1
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_remove_first(identifier: int) -> int:
    """Remove the first array item."""
    return clm_bpaa_remove(identifier, 0)


def clm_bpaa_remove_last(identifier: int) -> int:
    """Remove the last array item."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return clm_bpaa_remove(identifier, array.count - 1) if array and array.count else CLM_BPA_FAILURE


def clm_bpaa_clear(identifier: int) -> int:
    """Remove all initialized items while retaining capacity and array state."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None:
        return CLM_BPA_FAILURE
    if array.type == CLM_BPA_TYPE_STRING:
        assert isinstance(array.data, list)
        array.data.clear()
    else:
        assert isinstance(array.data, bytearray)
        array.data[:] = b"\0" * len(array.data)
    array.count = 0
    array.index = 0
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_slice(source_id: int, start: int, length: int, destination_id: int) -> int:
    """Create an independent array copy of a bounded source range."""
    source = _arrays.get(source_id) if _valid_id(source_id) else None
    if (
        source is None
        or not _valid_id(destination_id)
        or type(start) is not int
        or type(length) is not int
        or start < 0
        or length < 0
        or start > source.count
        or length > source.count - start
        or destination_id in _arrays
    ):
        return CLM_BPA_FAILURE
    status = clm_bpaa_create_sized(
        destination_id,
        source.type,
        length,
        source.block_size if source.type == CLM_BPA_TYPE_STRUCT else 0,
        source.encoding,
    )
    if status != CLM_BPA_SUCCESS:
        return status
    destination = _arrays[destination_id]
    if source.type == CLM_BPA_TYPE_STRING:
        destination.data = [bytes(_item(source, i)) for i in range(start, start + length)]
    else:
        for index in range(length):
            _write_item(destination, index, _item(source, start + index))
    destination.count = length
    _reset_deck(destination)
    return CLM_BPA_SUCCESS


def clm_bpaa_slice_borrow(
    source_id: int,
    start: int,
    length: int,
) -> tuple[int, memoryview | None, int]:
    """Borrow a contiguous fixed-width slice as a safe memoryview."""
    source = _arrays.get(source_id) if _valid_id(source_id) else None
    if (
        source is None
        or source.type == CLM_BPA_TYPE_STRING
        or type(start) is not int
        or type(length) is not int
        or start < 0
        or length < 0
        or start > source.count
        or length > source.count - start
    ):
        return CLM_BPA_FAILURE, None, 0
    assert isinstance(source.data, bytearray)
    first = start * source.block_size
    last = first + length * source.block_size
    view = memoryview(source.data)[first:last]
    return CLM_BPA_SUCCESS, view if length else None, len(view)


def clm_bpaa_slice_to(
    source_id: int,
    start: int,
    length: int,
    buffer_size: int | None = None,
) -> tuple[int, bytes | None]:
    """Copy a fixed-width slice and return ``(status, bytes)``."""
    source = _arrays.get(source_id) if _valid_id(source_id) else None
    if (
        source is None
        or source.type == CLM_BPA_TYPE_STRING
        or type(start) is not int
        or type(length) is not int
        or start < 0
        or length < 0
        or start > source.count
        or length > source.count - start
    ):
        return CLM_BPA_FAILURE, None
    required = length * source.block_size
    if buffer_size is not None and (not _valid_size(buffer_size) or buffer_size < required):
        return CLM_BPA_FAILURE, None
    return CLM_BPA_SUCCESS, b"".join(_item(source, i) for i in range(start, start + length))


def clm_bpaa_reset(identifier: int) -> int:
    """Move an array cursor to index zero."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None:
        return CLM_BPA_FAILURE
    array.index = 0
    return CLM_BPA_SUCCESS


def clm_bpaa_move_first(identifier: int) -> int:
    """Move an array cursor to the first element."""
    return clm_bpaa_reset(identifier)


def clm_bpaa_move_previous(identifier: int) -> int:
    """Move the cursor one element toward the beginning."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.index == 0:
        return CLM_BPA_FAILURE
    array.index -= 1
    return CLM_BPA_SUCCESS


def clm_bpaa_move_next(identifier: int) -> int:
    """Move the cursor one element toward the end."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.index + 1 >= array.count:
        return CLM_BPA_FAILURE
    array.index += 1
    return CLM_BPA_SUCCESS


def clm_bpaa_move_last(identifier: int) -> int:
    """Move the cursor to the final element."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.count == 0:
        return CLM_BPA_FAILURE
    array.index = array.count - 1
    return CLM_BPA_SUCCESS


def clm_bpaa_move_to(identifier: int, index: int) -> int:
    """Move the cursor to an initialized element."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or type(index) is not int or index < 0 or index >= array.count:
        return CLM_BPA_FAILURE
    array.index = index
    return CLM_BPA_SUCCESS


def clm_bpaa_get_current_index_to(identifier: int) -> tuple[int, int]:
    """Return the current cursor position as ``(status, index)``."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return (CLM_BPA_GET_SUCCESS, array.index) if array is not None else (CLM_BPA_FAILURE, 0)


def clm_bpaa_seal(identifier: int) -> int:
    """Prevent capacity changes and automatic growth for an array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None:
        return CLM_BPA_FAILURE
    array.sealed = True
    return CLM_BPA_SUCCESS


def clm_bpaa_is_sealed_to(identifier: int) -> tuple[int, bool]:
    """Return an array's sealed state as ``(status, sealed)``."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return (CLM_BPA_GET_SUCCESS, array.sealed) if array is not None else (CLM_BPA_FAILURE, False)


def clm_bpaa_unseal(identifier: int) -> int:
    """Allow capacity changes and growth for an array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None:
        return CLM_BPA_FAILURE
    array.sealed = False
    return CLM_BPA_SUCCESS


def clm_bpaa_sort_with(identifier: int, compare: CompareFunction | None) -> int:
    """Sort fixed-width items using a three-way comparator over byte values."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.type == CLM_BPA_TYPE_STRING or not callable(compare):
        return CLM_BPA_FAILURE
    values = [_item(array, index) for index in range(array.count)]
    try:
        values.sort(key=cmp_to_key(compare))
    except (TypeError, ValueError):
        return CLM_BPA_FAILURE
    for index, value in enumerate(values):
        _write_item(array, index, value)
    array.index = 0
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_sort(identifier: int, predicate: PredicateFunction | None) -> int:
    """Sort fixed-width items using a predicate that identifies out-of-order pairs."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.type == CLM_BPA_TYPE_STRING or not callable(predicate):
        return CLM_BPA_FAILURE
    values = [_item(array, index) for index in range(array.count)]
    try:
        for end in range(len(values) - 1, 0, -1):
            changed = False
            for index in range(end):
                if predicate(values[index], values[index + 1]):
                    values[index], values[index + 1] = values[index + 1], values[index]
                    changed = True
            if not changed:
                break
    except (TypeError, ValueError):
        return CLM_BPA_FAILURE
    for index, value in enumerate(values):
        _write_item(array, index, value)
    array.index = 0
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_reverse(identifier: int) -> int:
    """Reverse the order of fixed-width array items."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.type == CLM_BPA_TYPE_STRING:
        return CLM_BPA_FAILURE
    values = [_item(array, index) for index in range(array.count)]
    for index, value in enumerate(reversed(values)):
        _write_item(array, index, value)
    array.index = 0
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def _rotate(array: _Array, index: int, positions: int, right: bool) -> int:
    if (
        array.type == CLM_BPA_TYPE_STRING
        or type(index) is not int
        or type(positions) is not int
        or index < 0
        or index >= array.count
        or positions < 0
    ):
        return CLM_BPA_FAILURE
    suffix = [_item(array, item_index) for item_index in range(index, array.count)]
    shift = positions % len(suffix)
    if shift:
        suffix = suffix[-shift:] + suffix[:-shift] if right else suffix[shift:] + suffix[:shift]
        for offset, value in enumerate(suffix, index):
            _write_item(array, offset, value)
        array.index = 0
        _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_shift_left(identifier: int, index: int, positions: int) -> int:
    """Rotate the fixed-width suffix left by ``positions`` places."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return _rotate(array, index, positions, False) if array is not None else CLM_BPA_FAILURE


def clm_bpaa_shift_right(identifier: int, index: int, positions: int) -> int:
    """Rotate the fixed-width suffix right by ``positions`` places."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return _rotate(array, index, positions, True) if array is not None else CLM_BPA_FAILURE


def clm_bpaa_swap(identifier: int, index1: int, index2: int) -> int:
    """Swap two initialized fixed-width elements."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.type == CLM_BPA_TYPE_STRING
        or type(index1) is not int
        or type(index2) is not int
        or index1 < 0
        or index2 < 0
        or index1 >= array.count
        or index2 >= array.count
    ):
        return CLM_BPA_FAILURE
    first, second = _item(array, index1), _item(array, index2)
    _write_item(array, index1, second)
    _write_item(array, index2, first)
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_copy(identifier: int, source_index: int, destination_index: int) -> int:
    """Copy one fixed-width element over another in the same array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.type == CLM_BPA_TYPE_STRING
        or type(source_index) is not int
        or type(destination_index) is not int
        or source_index < 0
        or destination_index < 0
        or source_index >= array.count
        or destination_index >= array.count
    ):
        return CLM_BPA_FAILURE
    _write_item(array, destination_index, _item(array, source_index))
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_shuffle(identifier: int) -> int:
    """Shuffle fixed-width items and reset the draw deck."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.type == CLM_BPA_TYPE_STRING:
        return CLM_BPA_FAILURE
    values = [_item(array, index) for index in range(array.count)]
    random.shuffle(values)
    for index, value in enumerate(values):
        _write_item(array, index, value)
    array.index = 0
    _reset_deck(array)
    return CLM_BPA_SUCCESS


def clm_bpaa_draw_at_raw(identifier: int, index: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Draw a selected position from the active deck as ``(status, bytes)``."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if (
        array is None
        or array.type == CLM_BPA_TYPE_STRING
        or type(index) is not int
        or not _valid_size(buffer_size)
        or buffer_size < array.block_size
        or index < 0
        or index >= array.deck_size
    ):
        return CLM_BPA_FAILURE, None
    last = array.deck_size - 1
    result = _item(array, index)
    if index != last:
        tail = _item(array, last)
        _write_item(array, index, tail)
        _write_item(array, last, result)
    array.deck_size -= 1
    return CLM_BPA_GET_SUCCESS, result


def clm_bpaa_draw_raw(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Draw the next item, shuffling the array when the active deck is empty."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.type == CLM_BPA_TYPE_STRING or array.count == 0:
        return CLM_BPA_FAILURE, None
    if array.deck_size == 0:
        clm_bpaa_shuffle(identifier)
    return clm_bpaa_draw_at_raw(identifier, 0, buffer_size)


def clm_bpaa_advanced_borrow(identifier: int) -> tuple[int, ArrayBorrow | None]:
    """Retain an array and return a handle for advanced direct operations."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.refcount == _MAX_REFCOUNT:
        return CLM_BPA_FAILURE, None
    array.refcount += 1
    return CLM_BPA_SUCCESS, ArrayBorrow(array)


def clm_bpaa_advanced_release(borrowed: ArrayBorrow | None) -> int:
    """Release an advanced array handle; repeated release fails."""
    if not isinstance(borrowed, ArrayBorrow) or borrowed._released:
        return CLM_BPA_FAILURE
    array = borrowed._array
    if _arrays.get(array.id) is not array or array.refcount <= 0:
        borrowed._released = True
        return CLM_BPA_FAILURE
    borrowed._released = True
    array.refcount -= 1
    return _remove_array(array) if array.refcount == 0 else CLM_BPA_SUCCESS


def clm_bpaa_advanced_data(borrowed: ArrayBorrow | None) -> memoryview | None:
    """Return a memoryview over fixed-width array storage, or ``None`` for strings."""
    array = _array_for(borrowed) if borrowed is not None else None
    return memoryview(array.data) if array is not None and isinstance(array.data, bytearray) else None


def clm_bpaa_advanced_get_raw(
    borrowed: ArrayBorrow | None,
    index: int,
    buffer_size: int,
) -> tuple[int, bytes | None]:
    """Read an item through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None:
        return CLM_BPA_FAILURE, None
    return clm_bpaa_get_raw(borrowed._array.id, index, buffer_size)


def clm_bpaa_advanced_set_raw(
    borrowed: ArrayBorrow | None,
    index: int,
    buffer: RawValue,
    buffer_size: int | None = None,
) -> int:
    """Replace an item through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None:
        return CLM_BPA_FAILURE
    return clm_bpaa_set_raw(borrowed._array.id, index, buffer, buffer_size)


def clm_bpaa_advanced_insert_raw(
    borrowed: ArrayBorrow | None,
    index: int,
    buffer: RawValue,
    buffer_size: int | None = None,
) -> int:
    """Insert an item through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None or borrowed._array.type == CLM_BPA_TYPE_STRING:
        return CLM_BPA_FAILURE
    return clm_bpaa_insert_raw(borrowed._array.id, index, buffer, buffer_size)


def clm_bpaa_advanced_append_raw(
    borrowed: ArrayBorrow | None,
    buffer: RawValue,
    buffer_size: int | None = None,
) -> int:
    """Append an item through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None or borrowed._array.type == CLM_BPA_TYPE_STRING:
        return CLM_BPA_FAILURE
    return clm_bpaa_append_raw(borrowed._array.id, buffer, buffer_size)


def clm_bpaa_advanced_remove(borrowed: ArrayBorrow | None, index: int) -> int:
    """Remove an item through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None:
        return CLM_BPA_FAILURE
    array = borrowed._array
    if array.type == CLM_BPA_TYPE_STRING:
        return CLM_BPA_FAILURE
    return clm_bpaa_remove(array.id, index)


def clm_bpaa_advanced_clear(borrowed: ArrayBorrow | None) -> int:
    """Clear a fixed-width array through a retained handle."""
    if borrowed is None or _array_for(borrowed) is None or borrowed._array.type == CLM_BPA_TYPE_STRING:
        return CLM_BPA_FAILURE
    return clm_bpaa_clear(borrowed._array.id)


def clm_bpaa_advanced_extend(borrowed: ArrayBorrow | None, additional_capacity: int) -> int:
    """Reserve additional capacity through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None:
        return CLM_BPA_FAILURE
    return clm_bpaa_extend(borrowed._array.id, additional_capacity)


def clm_bpaa_advanced_shrink(borrowed: ArrayBorrow | None, new_capacity: int) -> int:
    """Shrink capacity through a retained array handle."""
    if borrowed is None or _array_for(borrowed) is None:
        return CLM_BPA_FAILURE
    return clm_bpaa_shrink(borrowed._array.id, new_capacity)


def _compatible(array: _Array | None, block: _Block | None, expected_size: int) -> bool:
    return bool(
        array is not None
        and block is not None
        and array.type != CLM_BPA_TYPE_STRING
        and block.type != CLM_BPA_TYPE_STRING
        and (array.type == CLM_BPA_TYPE_STRUCT or array.type == block.type)
        and array.block_size == block.block_size == expected_size
    )


def clm_bpaa_get(identifier: int, index: int, block_id: int, expected_size: int) -> int:
    """Copy an array element into a compatible block and return its status."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if not _compatible(array, block, expected_size) or type(index) is not int or index < 0 or index >= array.count:
        return CLM_BPA_FAILURE
    assert block.data is not None
    block.data[:] = _item(array, index)
    return CLM_BPA_GET_SUCCESS


def clm_bpaa_set(identifier: int, index: int, block_id: int, expected_size: int) -> int:
    """Copy a compatible block into an initialized fixed-width array item."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if not _compatible(array, block, expected_size) or type(index) is not int or index < 0 or index >= array.count or block.data is None:
        return CLM_BPA_FAILURE
    return clm_bpaa_set_raw(identifier, index, block.data, expected_size)


def clm_bpaa_insert(identifier: int, index: int, block_id: int, expected_size: int) -> int:
    """Insert a compatible fixed-width block into an array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if not _compatible(array, block, expected_size) or block.data is None:
        return CLM_BPA_FAILURE
    return clm_bpaa_insert_raw(identifier, index, block.data, expected_size)


def clm_bpaa_append(identifier: int, block_id: int, expected_size: int) -> int:
    """Append a compatible fixed-width block to an array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return clm_bpaa_insert(identifier, array.count, block_id, expected_size) if array is not None else CLM_BPA_FAILURE


def clm_bpaa_get_string(identifier: int, index: int, block_id: int) -> int:
    """Copy a string-array item into a string block and return GET_SUCCESS."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if (
        array is None
        or block is None
        or array.type != CLM_BPA_TYPE_STRING
        or block.type != CLM_BPA_TYPE_STRING
        or type(index) is not int
        or index < 0
        or index >= array.count
    ):
        return CLM_BPA_FAILURE
    block.text = bytes(_item(array, index))
    return CLM_BPA_GET_SUCCESS


def clm_bpaa_set_string(identifier: int, index: int, block_id: int) -> int:
    """Copy a string block into an existing string-array item."""
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if block is None or block.type != CLM_BPA_TYPE_STRING or block.text is None:
        return CLM_BPA_FAILURE
    length = clm_bpaa_set_string_raw(identifier, index, block.text, len(block.text) + 1)
    return CLM_BPA_FAILURE if length < 0 else CLM_BPA_SET_SUCCESS


def clm_bpaa_insert_string(identifier: int, index: int, block_id: int) -> int:
    """Insert a copy of a string block into a string array."""
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if block is None or block.type != CLM_BPA_TYPE_STRING or block.text is None:
        return CLM_BPA_FAILURE
    return clm_bpaa_insert_raw(identifier, index, block.text, len(block.text) + 1)


def clm_bpaa_append_string(identifier: int, block_id: int) -> int:
    """Append a copy of a string block to a string array."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return clm_bpaa_insert_string(identifier, array.count, block_id) if array is not None else CLM_BPA_FAILURE


def clm_bpaa_draw(identifier: int, block_id: int, expected_size: int) -> int:
    """Draw a fixed-width item into a compatible block and return its status."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if not _compatible(array, block, expected_size):
        return CLM_BPA_FAILURE
    status, value = clm_bpaa_draw_raw(identifier, expected_size)
    if status != CLM_BPA_GET_SUCCESS or value is None or block.data is None:
        return CLM_BPA_FAILURE
    block.data[:] = value
    return CLM_BPA_GET_SUCCESS


def clm_bpaa_draw_at(identifier: int, index: int, block_id: int, expected_size: int) -> int:
    """Draw a selected item into a compatible block and return its status."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    block = _blocks.get(block_id) if _valid_id(block_id) else None
    if not _compatible(array, block, expected_size):
        return CLM_BPA_FAILURE
    status, value = clm_bpaa_draw_at_raw(identifier, index, expected_size)
    if status != CLM_BPA_GET_SUCCESS or value is None or block.data is None:
        return CLM_BPA_FAILURE
    block.data[:] = value
    return CLM_BPA_GET_SUCCESS


def clm_bpa_get_to(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpa_get_to``; returns the copied bytes."""
    return clm_bpa_get(identifier, buffer_size)


def clm_bpa_set_from(identifier: int, source: RawValue, size: int | None = None) -> int:
    """Python form of ``clm_bpa_set_from``."""
    return clm_bpa_set(identifier, source, size)


def clm_bpa_get_string_to(identifier: int, buffer_size: int | None = None) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpa_get_string_to``."""
    return clm_bpa_get_string(identifier, buffer_size)


def clm_bpa_set_string_from(identifier: int, source: str | RawValue, size: int | None = None) -> int:
    """Python form of ``clm_bpa_set_string_from``."""
    return clm_bpa_set_string(identifier, source, size)


def clm_bpaa_get_to(identifier: int, index: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_get_to``."""
    return clm_bpaa_get_raw(identifier, index, buffer_size)


def clm_bpaa_get_first_to(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_get_first_to``."""
    return clm_bpaa_get_first_raw(identifier, buffer_size)


def clm_bpaa_get_last_to(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_get_last_to``."""
    return clm_bpaa_get_last_raw(identifier, buffer_size)


def clm_bpaa_get_current_to(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_get_current_to``."""
    return clm_bpaa_get_current_raw(identifier, buffer_size)


def clm_bpaa_get_string_to(identifier: int, index: int, buffer_size: int | None = None) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_get_string_to``."""
    return clm_bpaa_get_string_raw(identifier, index, buffer_size)


def clm_bpaa_set_from(identifier: int, index: int, source: RawValue, size: int | None = None) -> int:
    """Python form of ``clm_bpaa_set_from``."""
    return clm_bpaa_set_raw(identifier, index, source, size)


def clm_bpaa_set_string_from(identifier: int, index: int, source: str | RawValue, size: int | None = None) -> int:
    """Python form of ``clm_bpaa_set_string_from``."""
    return clm_bpaa_set_string_raw(identifier, index, source, size)


def clm_bpaa_insert_from(identifier: int, index: int, source: str | RawValue, size: int | None = None) -> int:
    """Python form of ``clm_bpaa_insert_from``."""
    return clm_bpaa_insert_raw(identifier, index, source, size)


def clm_bpaa_append_from(identifier: int, source: str | RawValue, size: int | None = None) -> int:
    """Python form of ``clm_bpaa_append_from``."""
    return clm_bpaa_append_raw(identifier, source, size)


def clm_bpaa_draw_to(identifier: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_draw_to``."""
    return clm_bpaa_draw_raw(identifier, buffer_size)


def clm_bpaa_draw_at_to(identifier: int, index: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Python form of ``clm_bpaa_draw_at_to``."""
    return clm_bpaa_draw_at_raw(identifier, index, buffer_size)


def _write_typed_block(identifier: int, type_code: int, fmt: str, value: int) -> int:
    block = _blocks.get(identifier) if _valid_id(identifier) else None
    if block is None or block.type != type_code or block.block_size != struct.calcsize("=" + fmt):
        return CLM_BPA_FAILURE
    try:
        encoded = struct.pack("=" + fmt, value)
    except (struct.error, OverflowError, TypeError):
        return CLM_BPA_FAILURE
    assert block.data is not None
    block.data[:] = encoded
    return CLM_BPA_GET_SUCCESS


def clm_bpaa_write_u32(destination_id: int, value: int) -> int:
    """Write an unsigned 32-bit value to a matching BPA block."""
    return _write_typed_block(destination_id, CLM_BPA_TYPE_UNSIGNED_INT, "I", value)


def clm_bpaa_write_i32(destination_id: int, value: int) -> int:
    """Write a signed 32-bit value to a matching BPA block."""
    return _write_typed_block(destination_id, CLM_BPA_TYPE_INT, "i", value)


def clm_bpaa_write_byte(destination_id: int, value: int) -> int:
    """Write an unsigned byte to a matching BPA block."""
    return _write_typed_block(destination_id, CLM_BPA_TYPE_BYTE, "B", value)


def clm_bpaa_get_count(identifier: int, destination_id: int) -> int:
    """Write an array's count to an unsigned-int BPA block."""
    status, count = clm_bpaa_get_count_to(identifier)
    return clm_bpaa_write_u32(destination_id, count) if status == CLM_BPA_GET_SUCCESS else CLM_BPA_FAILURE


def clm_bpaa_get_capacity(identifier: int, destination_id: int) -> int:
    """Write an array's capacity to an unsigned-int BPA block."""
    status, capacity = clm_bpaa_get_capacity_to(identifier)
    return clm_bpaa_write_u32(destination_id, capacity) if status == CLM_BPA_GET_SUCCESS else CLM_BPA_FAILURE


def clm_bpaa_get_length(identifier: int, destination_id: int) -> int:
    """Write the current string's byte length to an unsigned-int BPA block."""
    status, length = clm_bpaa_get_length_to(identifier)
    return clm_bpaa_write_u32(destination_id, length) if status == CLM_BPA_GET_SUCCESS else CLM_BPA_FAILURE


def clm_bpaa_get_current_index(identifier: int, destination_id: int) -> int:
    """Write an array cursor index to a signed-int BPA block."""
    status, index = clm_bpaa_get_current_index_to(identifier)
    return clm_bpaa_write_i32(destination_id, index) if status == CLM_BPA_GET_SUCCESS else CLM_BPA_FAILURE


def clm_bpaa_is_sealed(identifier: int, destination_id: int) -> int:
    """Write an array's sealed state as an unsigned byte to a BPA block."""
    status, sealed = clm_bpaa_is_sealed_to(identifier)
    return clm_bpaa_write_byte(destination_id, int(sealed)) if status == CLM_BPA_GET_SUCCESS else CLM_BPA_FAILURE


def clm_bpaa_get_first(identifier: int, destination_id: int, expected_size: int) -> int:
    """Copy an array's first item after checking block compatibility."""
    return clm_bpaa_get(identifier, 0, destination_id, expected_size)


def clm_bpaa_get_last(identifier: int, destination_id: int, expected_size: int) -> int:
    """Copy an array's last item after checking block compatibility."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    if array is None or array.count == 0:
        return CLM_BPA_FAILURE
    return clm_bpaa_get(identifier, array.count - 1, destination_id, expected_size)


def clm_bpaa_get_current(identifier: int, destination_id: int, expected_size: int) -> int:
    """Copy the cursor item after checking block compatibility."""
    array = _arrays.get(identifier) if _valid_id(identifier) else None
    return clm_bpaa_get(identifier, array.index, destination_id, expected_size) if array is not None else CLM_BPA_FAILURE


def clm_bpa_shutdown() -> int:
    """Clear all registered blocks and arrays; outstanding Python views stay safe."""
    _arrays.clear()
    _blocks.clear()
    return CLM_BPA_SUCCESS


__all__ = [name for name in globals() if name.startswith("CLM_BPA_") or name.startswith("CLM_BPA_TYPE_") or name.startswith("clm_bpa_") or name.startswith("clm_bpaa_") or name in {"ArrayBorrow", "CompareFunction", "PredicateFunction"}]