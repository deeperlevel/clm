# =============================================================================
# CLM — Cross Language Modules
# An initiative by Deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm | deeparchive@outlook.com
# Public Domain — CC0 1.0 Universal | Provided AS IS, without warranty.
# Don't start from scratch, bro.
# =============================================================================
"""ID-addressed, typed sequential and multidimensional storage with explicit lifetimes."""

from __future__ import annotations
from dataclasses import dataclass, field
import math
import random
import struct
from typing import Any, Callable
from clm_block import Block
from clm_block3d import Block3D

STORAGE_TYPE_STRING = 0x01
STORAGE_TYPE_BYTE = 0x02
STORAGE_TYPE_SHORT = 0x03
STORAGE_TYPE_INTEGER = 0x04
STORAGE_TYPE_LONG_INTEGER = 0x05
STORAGE_TYPE_FLOAT = 0x06
STORAGE_TYPE_DOUBLE = 0x07
STORAGE_TYPE_DYNAMIC = 0xFE
STORAGE_TYPE_OBJECT = 0xFF

STORAGE_SUCCESS = 0x00
STORAGE_ERROR_DUPLICATE_ID = 0x01
STORAGE_ERROR_NOT_FOUND = 0x02
STORAGE_ERROR_TYPE_NOT_SUPPORTED = 0x03
STORAGE_ERROR_TYPE_NOT_MATCHED = 0x04
STORAGE_ERROR_INVALID_VALUE = 0x05
STORAGE_ERROR_INVALID_INDEX = 0x06
STORAGE_ERROR_LOCKED = 0x07
STORAGE_ERROR_DIMENSION = 0x08
STORAGE_ERROR_EMPTY = 0x09
STORAGE_ERROR_REFERENCES = 0x0A
STORAGE_ERROR_OVERFLOW = 0x0B
STORAGE_ERROR_INCOMPATIBLE = 0x0C

STORAGE_ATLAS_OVERFLOW_IGNORE = 0x01
STORAGE_ATLAS_OVERFLOW_EXTEND = 0x02
STORAGE_ATLAS_OVERFLOW_CANCEL = 0x03

_BITS = {STORAGE_TYPE_BYTE: 8, STORAGE_TYPE_SHORT: 16,
         STORAGE_TYPE_INTEGER: 32, STORAGE_TYPE_LONG_INTEGER: 64}
_TYPES = {STORAGE_TYPE_STRING, *(_BITS.keys()), STORAGE_TYPE_FLOAT,
          STORAGE_TYPE_DOUBLE, STORAGE_TYPE_DYNAMIC, STORAGE_TYPE_OBJECT}
_WIDTHS = {STORAGE_TYPE_BYTE: 1, STORAGE_TYPE_SHORT: 2, STORAGE_TYPE_INTEGER: 4,
           STORAGE_TYPE_LONG_INTEGER: 8, STORAGE_TYPE_FLOAT: 4, STORAGE_TYPE_DOUBLE: 8}


@dataclass(frozen=True)
class StorageInfo:
    id: int
    type: int
    committed: int
    reserved: int
    references: int
    index: int
    size: int
    locked: bool
    spent: int
    kind: str
    width: int = 0
    height: int = 0
    depth: int = 0
    rows: int = 0
    columns: int = 0
    layers: int = 0


@dataclass
class _Record:
    id: int
    type: int
    size: int
    kind: str = 'linear'
    data: list[Any] = field(default_factory=list)
    reserved: int = 0
    references: int = 1
    index: int = -1
    locked: bool = False
    spent: int = 0
    width: int = 0
    height: int = 0
    depth: int = 0
    rows: int = 0
    columns: int = 0
    layers: int = 0


_REGISTRY: dict[int, _Record] = {}


def _valid_int(v: Any, positive: bool = False) -> bool:
    return type(v) is int and (not positive or v > 0)


def _type_size(t: int, size: int) -> bool:
    if t not in _TYPES or not _valid_int(size, True):
        return False
    return t in (STORAGE_TYPE_STRING, STORAGE_TYPE_DYNAMIC, STORAGE_TYPE_OBJECT) or size == _WIDTHS[t]


def _valid_value(r: _Record, value: Any) -> bool:
    t = r.type
    if t == STORAGE_TYPE_OBJECT:
        return True
    if t == STORAGE_TYPE_STRING:
        return isinstance(value, str) and len(value.encode('utf-8')) <= r.size
    if t == STORAGE_TYPE_DYNAMIC:
        return isinstance(value, (bytes, bytearray, memoryview)) and len(value) == r.size
    if t in _BITS:
        bits = _BITS[t]
        return type(value) is int and -(1 << (bits - 1)) <= value < (1 << (bits - 1))
    if t in (STORAGE_TYPE_FLOAT, STORAGE_TYPE_DOUBLE):
        if type(value) is not float:
            return False
        if t == STORAGE_TYPE_FLOAT:
            try:
                struct.pack('<f', value)
            except (OverflowError, struct.error):
                return False
        return True
    return False


def _convert(r: _Record, value: Any) -> Any:
    if r.type == STORAGE_TYPE_DYNAMIC:
        return bytes(value)
    if r.type == STORAGE_TYPE_FLOAT:
        return struct.unpack('<f', struct.pack('<f', value))[0]
    return value


def _default(r: _Record) -> Any:
    if r.type == STORAGE_TYPE_STRING:
        return ''
    if r.type == STORAGE_TYPE_DYNAMIC:
        return bytes(r.size)
    if r.type == STORAGE_TYPE_OBJECT:
        return None
    if r.type in (STORAGE_TYPE_FLOAT, STORAGE_TYPE_DOUBLE):
        return 0.0
    return 0


def _lookup(sid: int, kind: str | None = None) -> tuple[int, _Record | None]:
    r = _REGISTRY.get(sid)
    if r is None:
        return STORAGE_ERROR_NOT_FOUND, None
    if kind is not None and r.kind != kind:
        return STORAGE_ERROR_INCOMPATIBLE, None
    return STORAGE_SUCCESS, r


def _mutable(sid: int, kind: str | None = None) -> tuple[int, _Record | None]:
    status, r = _lookup(sid, kind)
    if status == STORAGE_SUCCESS and r.locked:
        return STORAGE_ERROR_LOCKED, None
    return status, r


def _grow(r: _Record, count: int) -> None:
    if count > r.reserved:
        r.reserved = max(count, max(1, r.reserved * 2))


def _reset(r: _Record) -> None:
    r.spent = 0
    if r.index >= len(r.data):
        r.index = len(r.data) - 1


def _new(sid: int, t: int, size: int, reserved: int, kind: str = 'linear', **dims: int) -> int:
    if not _valid_int(sid) or not _valid_int(reserved) or reserved < 0:
        return STORAGE_ERROR_INVALID_VALUE
    if sid in _REGISTRY:
        return STORAGE_ERROR_DUPLICATE_ID
    if t not in _TYPES:
        return STORAGE_ERROR_TYPE_NOT_SUPPORTED
    if not _type_size(t, size):
        return STORAGE_ERROR_INVALID_VALUE
    r = _Record(sid, t, size, kind, reserved=reserved, **dims)
    if kind != 'linear':
        count = r.width * r.height * r.depth * r.rows * r.columns * r.layers
        r.data = [_default(r) for _ in range(count)]
        r.reserved = count
    _REGISTRY[sid] = r
    return STORAGE_SUCCESS


def clm_storage_create(sid: int, storage_type: int = STORAGE_TYPE_DYNAMIC,
                       size: int | None = None, reserved: int = 0) -> int:
    if size is None:
        size = _WIDTHS.get(storage_type, 1)
    return _new(sid, storage_type, size, reserved)


def clm_storage_get_info(sid: int) -> tuple[int, StorageInfo | None]:
    status, r = _lookup(sid)
    if status:
        return status, None
    return STORAGE_SUCCESS, StorageInfo(r.id, r.type, len(r.data), r.reserved,
                                        r.references, r.index, r.size, r.locked,
                                        r.spent, r.kind, r.width, r.height, r.depth,
                                        r.rows, r.columns, r.layers)


def clm_storage_count(sid: int) -> tuple[int, int]:
    status, r = _lookup(sid)
    return (status, len(r.data) if r else 0)


def clm_storage_reference(sid: int) -> int:
    status, r = _lookup(sid)
    if not status:
        r.references += 1
    return status


def clm_storage_unreference(sid: int) -> int:
    status, r = _lookup(sid)
    if status:
        return status
    if r.references == 0:
        return STORAGE_ERROR_REFERENCES
    r.references -= 1
    return STORAGE_SUCCESS


def clm_storage_pending_count() -> int:
    """Count registered storages eligible for disposal (references == 0).

    This does not modify or collect resources. Call only when the registry is
    not being concurrently mutated.
    """
    return sum(r.references == 0 for r in _REGISTRY.values())


def clm_storage_dispose() -> tuple[int, int]:
    disposed = [sid for sid, r in _REGISTRY.items() if r.references == 0]
    for sid in disposed:
        del _REGISTRY[sid]
    return STORAGE_SUCCESS, len(disposed)


def clm_storage_destroy(sid: int) -> int:
    if sid not in _REGISTRY:
        return STORAGE_ERROR_NOT_FOUND
    del _REGISTRY[sid]  # Explicit destroy overrides reference tracking, like clm_container.
    return STORAGE_SUCCESS


def clm_storage_clear(sid: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    r.data.clear()
    r.index = -1
    r.spent = 0
    return STORAGE_SUCCESS


def clm_storage_reserve(sid: int, reserved: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not _valid_int(reserved) or reserved < 0:
        return STORAGE_ERROR_INVALID_VALUE
    r.reserved = max(r.reserved, reserved)
    return STORAGE_SUCCESS


def clm_storage_resize(sid: int, count: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not _valid_int(count) or count < 0:
        return STORAGE_ERROR_INVALID_VALUE
    _grow(r, count)
    if count < len(r.data):
        del r.data[count:]
    else:
        r.data.extend(_default(r) for _ in range(count - len(r.data)))
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_lock(sid: int) -> int:
    status, r = _lookup(sid)
    if not status:
        r.locked = True
    return status


def clm_storage_unlock(sid: int) -> int:
    status, r = _lookup(sid)
    if not status:
        r.locked = False
    return status


def clm_storage_set_index(sid: int, index: int) -> int:
    status, r = _lookup(sid)
    if status:
        return status
    if not _valid_int(index) or not 0 <= index < len(r.data):
        return STORAGE_ERROR_INVALID_INDEX
    r.index = index
    return STORAGE_SUCCESS


def clm_storage_get_index(sid: int) -> tuple[int, int]:
    status, r = _lookup(sid)
    return status, r.index if r else -1


def _move(sid: int, target: str) -> int:
    status, r = _lookup(sid)
    if status:
        return status
    if not r.data:
        r.index = -1
        return STORAGE_ERROR_EMPTY
    if target == 'first':
        r.index = 0
    elif target == 'last':
        r.index = len(r.data) - 1
    else:
        candidate = r.index + (1 if target == 'next' else -1)
        if r.index < 0 or not 0 <= candidate < len(r.data):
            return STORAGE_ERROR_INVALID_INDEX
        r.index = candidate
    return STORAGE_SUCCESS


def clm_storage_move_first(sid: int) -> int: return _move(sid, 'first')
def clm_storage_move_last(sid: int) -> int: return _move(sid, 'last')
def clm_storage_move_next(sid: int) -> int: return _move(sid, 'next')
def clm_storage_move_previous(sid: int) -> int: return _move(sid, 'previous')
def clm_storage_iterate(sid: int) -> int: return clm_storage_move_first(sid)
def clm_storage_iterate_backward(sid: int) -> int: return clm_storage_move_last(sid)


def clm_storage_is_first(sid: int) -> tuple[int, bool]:
    status, r = _lookup(sid)
    return status, bool(r and r.data and r.index == 0)


def clm_storage_is_last(sid: int) -> tuple[int, bool]:
    status, r = _lookup(sid)
    return status, bool(r and r.data and r.index == len(r.data) - 1)


def clm_storage_get(sid: int, index: int | None = None) -> tuple[int, Any]:
    status, r = _lookup(sid)
    if status:
        return status, None
    if index is None:
        index = r.index
    if not _valid_int(index) or not 0 <= index < len(r.data):
        return STORAGE_ERROR_INVALID_INDEX, None
    return STORAGE_SUCCESS, r.data[index]


def clm_storage_insert(sid: int, index: int, value: Any) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not _valid_int(index) or not 0 <= index <= len(r.data):
        return STORAGE_ERROR_INVALID_INDEX
    if not _valid_value(r, value):
        return STORAGE_ERROR_INVALID_VALUE
    _grow(r, len(r.data) + 1)
    r.data.insert(index, _convert(r, value))
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_append(sid: int, value: Any) -> int:
    status, r = _lookup(sid, 'linear')
    if status:
        return status
    return clm_storage_insert(sid, len(r.data), value)


def clm_storage_replace(sid: int, index: int, value: Any) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not _valid_int(index) or not 0 <= index < len(r.data):
        return STORAGE_ERROR_INVALID_INDEX
    if not _valid_value(r, value):
        return STORAGE_ERROR_INVALID_VALUE
    r.data[index] = _convert(r, value)
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_remove(sid: int, index: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not _valid_int(index) or not 0 <= index < len(r.data):
        return STORAGE_ERROR_INVALID_INDEX
    r.data.pop(index)
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_swap(sid: int, a: int, b: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not all(_valid_int(v) and 0 <= v < len(r.data) for v in (a, b)):
        return STORAGE_ERROR_INVALID_INDEX
    r.data[a], r.data[b] = r.data[b], r.data[a]
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_copy(sid: int, source_index: int, destination_index: int, length: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if not all(_valid_int(v) and v >= 0 for v in (source_index, destination_index, length)):
        return STORAGE_ERROR_INVALID_INDEX
    if source_index + length > len(r.data) or destination_index + length > len(r.data):
        return STORAGE_ERROR_INVALID_INDEX
    values = r.data[source_index:source_index + length]
    r.data[destination_index:destination_index + length] = values
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_replace_all(sid: int, old: Any, new: Any) -> tuple[int, int]:
    status, r = _mutable(sid, 'linear')
    if status:
        return status, 0
    if not _valid_value(r, old) or not _valid_value(r, new):
        return STORAGE_ERROR_INVALID_VALUE, 0
    n = 0
    for i, item in enumerate(r.data):
        if item == old:
            r.data[i] = _convert(r, new)
            n += 1
    _reset(r)
    return STORAGE_SUCCESS, n


def clm_storage_delete(sid: int, value: Any) -> tuple[int, int]:
    status, r = _mutable(sid, 'linear')
    if status:
        return status, 0
    if not _valid_value(r, value):
        return STORAGE_ERROR_INVALID_VALUE, 0
    old = len(r.data)
    r.data[:] = [x for x in r.data if x != value]
    _reset(r)
    return STORAGE_SUCCESS, old - len(r.data)


def _compatible(a: _Record, b: _Record) -> bool:
    return a.type == b.type and a.size == b.size and a.kind == b.kind


def _transfer(source: int, destination: int, start: int | None, count: int | None, append: bool) -> int:
    status, a = _lookup(source, 'linear')
    if status:
        return status
    status, b = _mutable(destination, 'linear')
    if status:
        return status
    if not _compatible(a, b):
        return STORAGE_ERROR_TYPE_NOT_MATCHED
    if start is None:
        start, count = 0, len(a.data)
    if not _valid_int(start) or not _valid_int(count) or start < 0 or count < 0 or start + count > len(a.data):
        return STORAGE_ERROR_INVALID_INDEX
    values = list(a.data[start:start + count])
    if append:
        _grow(b, len(b.data) + len(values))
        b.data.extend(values)
    else:
        _grow(b, len(values))
        b.data[:] = values
    _reset(b)
    return STORAGE_SUCCESS


def clm_storage_clone(source: int, destination: int) -> int:
    return _transfer(source, destination, None, None, False)


def clm_storage_append_clone(source: int, destination: int) -> int:
    return _transfer(source, destination, None, None, True)


def clm_storage_slice(source: int, destination: int, start: int, count: int) -> int:
    return _transfer(source, destination, start, count, False)


def clm_storage_filter(source: int, predicate: Callable[[Any], bool], destination: int) -> int:
    status, a = _lookup(source, 'linear')
    if status:
        return status
    status, b = _mutable(destination, 'linear')
    if status:
        return status
    if not _compatible(a, b):
        return STORAGE_ERROR_TYPE_NOT_MATCHED
    if not callable(predicate):
        return STORAGE_ERROR_INVALID_VALUE
    # Evaluate completely before changing the destination.
    selected = [x for x in list(a.data) if predicate(x)]
    _grow(b, len(selected))
    b.data[:] = selected
    _reset(b)
    return STORAGE_SUCCESS


def clm_storage_add_sampling(sid: int, value: Any, count: int, index: int | None = None) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if index is None:
        index = len(r.data)
    if not _valid_int(count) or count < 0 or not _valid_int(index) or not 0 <= index <= len(r.data):
        return STORAGE_ERROR_INVALID_INDEX
    if not _valid_value(r, value):
        return STORAGE_ERROR_INVALID_VALUE
    _grow(r, len(r.data) + count)
    r.data[index:index] = [_convert(r, value)] * count
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_shuffle(sid: int, rng: random.Random | None = None) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    (rng if rng is not None else random).shuffle(r.data)
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_reverse(sid: int) -> int:
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    r.data.reverse()
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_sort(sid: int, comparator: Callable[[Any, Any], int] | None = None,
                     descending: bool = False) -> int:
    from functools import cmp_to_key
    status, r = _mutable(sid, 'linear')
    if status:
        return status
    if comparator is not None and not callable(comparator):
        return STORAGE_ERROR_INVALID_VALUE
    if comparator is None and r.type == STORAGE_TYPE_OBJECT:
        return STORAGE_ERROR_INVALID_VALUE
    try:
        ordered = sorted(r.data, key=cmp_to_key(comparator) if comparator else None,
                         reverse=descending)
    except (TypeError, ValueError):
        return STORAGE_ERROR_INVALID_VALUE
    r.data[:] = ordered
    _reset(r)
    return STORAGE_SUCCESS


def clm_storage_sort_desc(sid: int, comparator: Callable[[Any, Any], int] | None = None) -> int:
    return clm_storage_sort(sid, comparator, True)


def clm_storage_spend(sid: int, index: int) -> tuple[int, Any]:
    status, r = _mutable(sid, 'linear')
    if status:
        return status, None
    available = len(r.data) - r.spent
    if not _valid_int(index) or not 0 <= index < available:
        return STORAGE_ERROR_INVALID_INDEX, None
    last = available - 1
    r.data[index], r.data[last] = r.data[last], r.data[index]
    r.spent += 1
    return STORAGE_SUCCESS, r.data[last]


def clm_storage_draw(sid: int, rng: random.Random | None = None) -> tuple[int, Any]:
    status, r = _lookup(sid, 'linear')
    if status:
        return status, None
    if r.locked:
        return STORAGE_ERROR_LOCKED, None
    available = len(r.data) - r.spent
    if not available:
        return STORAGE_ERROR_EMPTY, None
    index = (rng if rng is not None else random).randrange(available)
    return clm_storage_spend(sid, index)


def clm_storage_reset_spent(sid: int) -> int:
    status, r = _mutable(sid, 'linear')
    if not status:
        r.spent = 0
    return status


# Multidimensional resources are dense, row-major typed arrays. Coordinates are
# zero-based; block dimensions and atlas block-grid dimensions are distinct.

def _make_grid(sid: int, kind: str, width: int, height: int, depth: int,
               rows: int, columns: int, layers: int,
               storage_type: int, size: int | None) -> int:
    dims = (width, height, depth, rows, columns, layers)
    if not all(_valid_int(d, True) for d in dims):
        return STORAGE_ERROR_DIMENSION
    if size is None:
        size = _WIDTHS.get(storage_type, 1)
    return _new(sid, storage_type, size, 0, kind,
                width=width, height=height, depth=depth,
                rows=rows, columns=columns, layers=layers)


def clm_storage_block_create(sid: int, width: int, height: int, size: int | None = None,
                             storage_type: int = STORAGE_TYPE_BYTE) -> int:
    return _make_grid(sid, 'block', width, height, 1, 1, 1, 1, storage_type, size)


def clm_storage_block3d_create(sid: int, width: int, height: int, depth: int,
                               size: int | None = None, storage_type: int = STORAGE_TYPE_BYTE) -> int:
    return _make_grid(sid, 'block3d', width, height, depth, 1, 1, 1, storage_type, size)


def clm_storage_atlas_create(sid: int, width: int, height: int, rows: int, columns: int,
                             size: int | None = None, storage_type: int = STORAGE_TYPE_BYTE) -> int:
    return _make_grid(sid, 'atlas', width, height, 1, rows, columns, 1, storage_type, size)


def clm_storage_atlas3d_create(sid: int, width: int, height: int, depth: int,
                               rows: int, columns: int, layers: int,
                               size: int | None = None, storage_type: int = STORAGE_TYPE_BYTE) -> int:
    return _make_grid(sid, 'atlas3d', width, height, depth, rows, columns, layers, storage_type, size)


def _grid_index(r: _Record, x: int, y: int, z: int = 0,
                column: int = 0, row: int = 0, layer: int = 0) -> int | None:
    coords = (x, y, z, column, row, layer)
    bounds = (r.width, r.height, r.depth, r.columns, r.rows, r.layers)
    if not all(_valid_int(a) and 0 <= a < bound for a, bound in zip(coords, bounds)):
        return None
    block_offset = ((layer * r.rows + row) * r.columns + column) * (r.width * r.height * r.depth)
    return block_offset + (z * r.height + y) * r.width + x


def clm_storage_grid_get(sid: int, x: int, y: int, z: int = 0,
                         column: int = 0, row: int = 0, layer: int = 0) -> tuple[int, Any]:
    status, r = _lookup(sid)
    if status:
        return status, None
    if r.kind == 'linear':
        return STORAGE_ERROR_INCOMPATIBLE, None
    index = _grid_index(r, x, y, z, column, row, layer)
    if index is None:
        return STORAGE_ERROR_INVALID_INDEX, None
    return STORAGE_SUCCESS, r.data[index]


def clm_storage_grid_set(sid: int, x: int, y: int, value: Any, z: int = 0,
                         column: int = 0, row: int = 0, layer: int = 0) -> int:
    status, r = _mutable(sid)
    if status:
        return status
    if r.kind == 'linear':
        return STORAGE_ERROR_INCOMPATIBLE
    index = _grid_index(r, x, y, z, column, row, layer)
    if index is None:
        return STORAGE_ERROR_INVALID_INDEX
    if not _valid_value(r, value):
        return STORAGE_ERROR_INVALID_VALUE
    r.data[index] = _convert(r, value)
    return STORAGE_SUCCESS


def clm_storage_block_get(sid: int, x: int, y: int) -> tuple[int, Any]:
    status, _ = _lookup(sid, 'block')
    return (status, None) if status else clm_storage_grid_get(sid, x, y)


def clm_storage_block_set(sid: int, x: int, y: int, value: Any) -> int:
    status, _ = _lookup(sid, 'block')
    return status if status else clm_storage_grid_set(sid, x, y, value)


def clm_storage_block3d_get(sid: int, x: int, y: int, z: int) -> tuple[int, Any]:
    status, _ = _lookup(sid, 'block3d')
    return (status, None) if status else clm_storage_grid_get(sid, x, y, z)


def clm_storage_block3d_set(sid: int, x: int, y: int, z: int, value: Any) -> int:
    status, _ = _lookup(sid, 'block3d')
    return status if status else clm_storage_grid_set(sid, x, y, value, z)


def clm_storage_block_destroy(sid: int) -> int:
    status, _ = _lookup(sid, 'block')
    return status if status else clm_storage_destroy(sid)


def clm_storage_block3d_destroy(sid: int) -> int:
    status, _ = _lookup(sid, 'block3d')
    return status if status else clm_storage_destroy(sid)


def clm_storage_atlas_destroy(sid: int) -> int:
    status, _ = _lookup(sid, 'atlas')
    return status if status else clm_storage_destroy(sid)


def clm_storage_atlas3d_destroy(sid: int) -> int:
    status, _ = _lookup(sid, 'atlas3d')
    return status if status else clm_storage_destroy(sid)


def _regrid(r: _Record, width: int, height: int, depth: int,
            rows: int, columns: int, layers: int) -> None:
    old = (r.width, r.height, r.depth, r.rows, r.columns, r.layers)
    old_data = r.data
    new_count = width * height * depth * rows * columns * layers
    new_data = [_default(r) for _ in range(new_count)]
    for l in range(min(layers, old[5])):
        for ry in range(min(rows, old[3])):
            for cx in range(min(columns, old[4])):
                for z in range(min(depth, old[2])):
                    for y in range(min(height, old[1])):
                        for x in range(min(width, old[0])):
                            oi = (((l * old[3] + ry) * old[4] + cx) * old[2] + z) * old[1] * old[0] + y * old[0] + x
                            ni = (((l * rows + ry) * columns + cx) * depth + z) * height * width + y * width + x
                            new_data[ni] = old_data[oi]
    r.width, r.height, r.depth = width, height, depth
    r.rows, r.columns, r.layers = rows, columns, layers
    r.data, r.reserved = new_data, new_count
    _reset(r)


def _resize_grid(sid: int, kind: str, width: int | None = None, height: int | None = None,
                 depth: int | None = None, rows: int | None = None,
                 columns: int | None = None, layers: int | None = None) -> int:
    status, r = _mutable(sid, kind)
    if status:
        return status
    dims = tuple(current if proposed is None else proposed for current, proposed in
                 zip((r.width, r.height, r.depth, r.rows, r.columns, r.layers),
                     (width, height, depth, rows, columns, layers)))
    if not all(_valid_int(d, True) for d in dims):
        return STORAGE_ERROR_DIMENSION
    _regrid(r, *dims)
    return STORAGE_SUCCESS


def clm_storage_atlas_resize(sid: int, width: int, height: int) -> int:
    return _resize_grid(sid, 'atlas', width=width, height=height)


def clm_storage_atlas_redefine(sid: int, rows: int, columns: int) -> int:
    return _resize_grid(sid, 'atlas', rows=rows, columns=columns)


def clm_storage_atlas3d_resize(sid: int, width: int, height: int, depth: int) -> int:
    return _resize_grid(sid, 'atlas3d', width=width, height=height, depth=depth)


def clm_storage_atlas3d_redefine(sid: int, rows: int, columns: int, layers: int) -> int:
    return _resize_grid(sid, 'atlas3d', rows=rows, columns=columns, layers=layers)


def _atlas_copy(atlas_id: int, coords: tuple[int, ...], block_id: int,
                into_atlas: bool, kind: str, overflow: int) -> int:
    status, a = _mutable(atlas_id, kind) if into_atlas else _lookup(atlas_id, kind)
    if status:
        return status
    block_kind = 'block3d' if kind == 'atlas3d' else 'block'
    status, b = _lookup(block_id, block_kind) if into_atlas else _mutable(block_id, block_kind)
    if status:
        return status
    if a.id == b.id or a.type != b.type or a.size != b.size:
        return STORAGE_ERROR_TYPE_NOT_MATCHED
    if overflow not in (STORAGE_ATLAS_OVERFLOW_IGNORE, STORAGE_ATLAS_OVERFLOW_EXTEND, STORAGE_ATLAS_OVERFLOW_CANCEL):
        return STORAGE_ERROR_INVALID_VALUE
    col, row, layer = (coords[0], coords[1], coords[2] if len(coords) == 3 else 0)
    if not all(_valid_int(v) and 0 <= v < bound for v, bound in zip(
            (col, row, layer), (a.columns, a.rows, a.layers))):
        return STORAGE_ERROR_INVALID_INDEX
    source, dest = (b, a) if into_atlas else (a, b)
    larger = any(sa > db for sa, db in zip((source.width, source.height, source.depth),
                                           (dest.width, dest.height, dest.depth)))
    if larger and overflow == STORAGE_ATLAS_OVERFLOW_CANCEL:
        return STORAGE_ERROR_OVERFLOW
    if larger and overflow == STORAGE_ATLAS_OVERFLOW_EXTEND:
        _regrid(dest, max(source.width, dest.width), max(source.height, dest.height),
                max(source.depth, dest.depth), dest.rows, dest.columns, dest.layers)
    w, h, d = min(a.width, b.width), min(a.height, b.height), min(a.depth, b.depth)
    # Snapshot first; block/grid values may represent shared mutable objects.
    for z in range(d):
        for y in range(h):
            for x in range(w):
                ai = _grid_index(a, x, y, z, col, row, layer)
                bi = _grid_index(b, x, y, z)
                if into_atlas:
                    a.data[ai] = b.data[bi]
                else:
                    b.data[bi] = a.data[ai]
    return STORAGE_SUCCESS


def clm_storage_atlas_get(sid: int, x: int, y: int, destination_block: int,
                          overflow: int = STORAGE_ATLAS_OVERFLOW_CANCEL) -> int:
    return _atlas_copy(sid, (x, y), destination_block, False, 'atlas', overflow)


def clm_storage_atlas_set(sid: int, x: int, y: int, source_block: int,
                          overflow: int = STORAGE_ATLAS_OVERFLOW_CANCEL) -> int:
    return _atlas_copy(sid, (x, y), source_block, True, 'atlas', overflow)


def clm_storage_atlas3d_get(sid: int, x: int, y: int, z: int, destination_block: int,
                            overflow: int = STORAGE_ATLAS_OVERFLOW_CANCEL) -> int:
    return _atlas_copy(sid, (x, y, z), destination_block, False, 'atlas3d', overflow)


def clm_storage_atlas3d_set(sid: int, x: int, y: int, z: int, source_block: int,
                            overflow: int = STORAGE_ATLAS_OVERFLOW_CANCEL) -> int:
    return _atlas_copy(sid, (x, y, z), source_block, True, 'atlas3d', overflow)


def _atlas_clone(source: int, destination: int, kind: str) -> int:
    status, a = _lookup(source, kind)
    if status:
        return status
    status, b = _mutable(destination, kind)
    if status:
        return status
    if a.type != b.type or a.size != b.size:
        return STORAGE_ERROR_TYPE_NOT_MATCHED
    if a.id == b.id:
        return STORAGE_SUCCESS
    _regrid(b, a.width, a.height, a.depth, a.rows, a.columns, a.layers)
    b.data[:] = a.data
    return STORAGE_SUCCESS


def clm_storage_atlas_clone(source: int, destination: int) -> int:
    return _atlas_clone(source, destination, 'atlas')


def clm_storage_atlas3d_clone(source: int, destination: int) -> int:
    return _atlas_clone(source, destination, 'atlas3d')


def clm_storage_atlas3d_slice(source: int, destination: int, layer: int, depth: int = 0,
                              overflow: int = STORAGE_ATLAS_OVERFLOW_CANCEL) -> int:
    """Copy one grid-layer and one z-plane from each 3D block into a 2D atlas."""
    status, a = _lookup(source, 'atlas3d')
    if status:
        return status
    status, b = _mutable(destination, 'atlas')
    if status:
        return status
    if a.type != b.type or a.size != b.size:
        return STORAGE_ERROR_TYPE_NOT_MATCHED
    if not _valid_int(layer) or not 0 <= layer < a.layers or not _valid_int(depth) or not 0 <= depth < a.depth:
        return STORAGE_ERROR_INVALID_INDEX
    if overflow not in (STORAGE_ATLAS_OVERFLOW_IGNORE, STORAGE_ATLAS_OVERFLOW_EXTEND, STORAGE_ATLAS_OVERFLOW_CANCEL):
        return STORAGE_ERROR_INVALID_VALUE
    dims_a = (a.width, a.height, a.rows, a.columns)
    dims_b = (b.width, b.height, b.rows, b.columns)
    if any(x > y for x, y in zip(dims_a, dims_b)):
        if overflow == STORAGE_ATLAS_OVERFLOW_CANCEL:
            return STORAGE_ERROR_OVERFLOW
        if overflow == STORAGE_ATLAS_OVERFLOW_EXTEND:
            _regrid(b, max(a.width, b.width), max(a.height, b.height), 1,
                    max(a.rows, b.rows), max(a.columns, b.columns), 1)
    for row in range(min(a.rows, b.rows)):
        for col in range(min(a.columns, b.columns)):
            for y in range(min(a.height, b.height)):
                for x in range(min(a.width, b.width)):
                    b.data[_grid_index(b, x, y, 0, col, row)] = a.data[_grid_index(a, x, y, depth, col, row, layer)]
    return STORAGE_SUCCESS


# Multidimensional cursors operate on the same flattened index used by StorageInfo.
def clm_storage_grid_set_position(sid: int, x: int, y: int, z: int = 0,
                                  column: int = 0, row: int = 0, layer: int = 0) -> int:
    status, r = _lookup(sid)
    if status:
        return status
    if r.kind == 'linear':
        return STORAGE_ERROR_INCOMPATIBLE
    index = _grid_index(r, x, y, z, column, row, layer)
    if index is None:
        return STORAGE_ERROR_INVALID_INDEX
    r.index = index
    return STORAGE_SUCCESS


def clm_storage_grid_get_position(sid: int) -> tuple[int, tuple[int, int, int, int, int, int] | None]:
    status, r = _lookup(sid)
    if status:
        return status, None
    if r.kind == 'linear':
        return STORAGE_ERROR_INCOMPATIBLE, None
    if not 0 <= r.index < len(r.data):
        return STORAGE_ERROR_INVALID_INDEX, None
    within, x = divmod(r.index, r.width)
    within, y = divmod(within, r.height)
    block, z = divmod(within, r.depth)
    block, column = divmod(block, r.columns)
    layer, row = divmod(block, r.rows)
    return STORAGE_SUCCESS, (x, y, z, column, row, layer)


def clm_storage_grid_get_current(sid: int) -> tuple[int, Any]:
    status, r = _lookup(sid)
    if status:
        return status, None
    if r.kind == 'linear':
        return STORAGE_ERROR_INCOMPATIBLE, None
    if not 0 <= r.index < len(r.data):
        return STORAGE_ERROR_INVALID_INDEX, None
    return STORAGE_SUCCESS, r.data[r.index]


def _grid_kind_set_position(sid: int, kind: str, *coords: int) -> int:
    status, _ = _lookup(sid, kind)
    return status if status else clm_storage_grid_set_position(sid, *coords)


def _grid_kind_get_position(sid: int, kind: str) -> tuple[int, Any]:
    status, _ = _lookup(sid, kind)
    if status:
        return status, None
    status, coords = clm_storage_grid_get_position(sid)
    if status:
        return status, None
    x, y, z, col, row, layer = coords
    if kind == 'block':
        return STORAGE_SUCCESS, (x, y)
    if kind == 'block3d':
        return STORAGE_SUCCESS, (x, y, z)
    if kind == 'atlas':
        return STORAGE_SUCCESS, (x, y, col, row)
    return STORAGE_SUCCESS, coords


def _grid_kind_move(sid: int, kind: str, move: str) -> int:
    status, _ = _lookup(sid, kind)
    return status if status else _move(sid, move)


def clm_storage_block_set_position(sid: int, x: int, y: int) -> int:
    return _grid_kind_set_position(sid, 'block', x, y)


def clm_storage_block_get_position(sid: int) -> tuple[int, Any]:
    return _grid_kind_get_position(sid, 'block')


def clm_storage_block3d_set_position(sid: int, x: int, y: int, z: int) -> int:
    return _grid_kind_set_position(sid, 'block3d', x, y, z)


def clm_storage_block3d_get_position(sid: int) -> tuple[int, Any]:
    return _grid_kind_get_position(sid, 'block3d')


def clm_storage_atlas_set_position(sid: int, x: int, y: int,
                                    column: int, row: int) -> int:
    return _grid_kind_set_position(sid, 'atlas', x, y, 0, column, row)


def clm_storage_atlas_get_position(sid: int) -> tuple[int, Any]:
    return _grid_kind_get_position(sid, 'atlas')


def clm_storage_atlas3d_set_position(sid: int, x: int, y: int, z: int,
                                      column: int, row: int, layer: int) -> int:
    return _grid_kind_set_position(sid, 'atlas3d', x, y, z, column, row, layer)


def clm_storage_atlas3d_get_position(sid: int) -> tuple[int, Any]:
    return _grid_kind_get_position(sid, 'atlas3d')


def clm_storage_block_move_first(sid: int) -> int:
    return _grid_kind_move(sid, 'block', 'first')


def clm_storage_block_move_last(sid: int) -> int:
    return _grid_kind_move(sid, 'block', 'last')


def clm_storage_block_move_next(sid: int) -> int:
    return _grid_kind_move(sid, 'block', 'next')


def clm_storage_block_move_previous(sid: int) -> int:
    return _grid_kind_move(sid, 'block', 'previous')


def clm_storage_block3d_move_first(sid: int) -> int:
    return _grid_kind_move(sid, 'block3d', 'first')


def clm_storage_block3d_move_last(sid: int) -> int:
    return _grid_kind_move(sid, 'block3d', 'last')


def clm_storage_block3d_move_next(sid: int) -> int:
    return _grid_kind_move(sid, 'block3d', 'next')


def clm_storage_block3d_move_previous(sid: int) -> int:
    return _grid_kind_move(sid, 'block3d', 'previous')


def clm_storage_atlas_move_first(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas', 'first')


def clm_storage_atlas_move_last(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas', 'last')


def clm_storage_atlas_move_next(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas', 'next')


def clm_storage_atlas_move_previous(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas', 'previous')


def clm_storage_atlas3d_move_first(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas3d', 'first')


def clm_storage_atlas3d_move_last(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas3d', 'last')


def clm_storage_atlas3d_move_next(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas3d', 'next')


def clm_storage_atlas3d_move_previous(sid: int) -> int:
    return _grid_kind_move(sid, 'atlas3d', 'previous')


def clm_storage_atlas3d_set_slice(source: int, destination: int, layer: int,
                                   depth: int = 0,
                                   overflow: int = STORAGE_ATLAS_OVERFLOW_CANCEL) -> int:
    """Copy a 2D atlas into one 3D atlas layer and per-block z plane."""
    status, a = _lookup(source, 'atlas')
    if status:
        return status
    status, b = _mutable(destination, 'atlas3d')
    if status:
        return status
    if a.type != b.type or a.size != b.size:
        return STORAGE_ERROR_TYPE_NOT_MATCHED
    if not _valid_int(layer) or not 0 <= layer < b.layers or not _valid_int(depth) or not 0 <= depth < b.depth:
        return STORAGE_ERROR_INVALID_INDEX
    if overflow not in (STORAGE_ATLAS_OVERFLOW_IGNORE, STORAGE_ATLAS_OVERFLOW_EXTEND,
                        STORAGE_ATLAS_OVERFLOW_CANCEL):
        return STORAGE_ERROR_INVALID_VALUE
    dims_a = (a.width, a.height, a.rows, a.columns)
    dims_b = (b.width, b.height, b.rows, b.columns)
    if any(x > y for x, y in zip(dims_a, dims_b)):
        if overflow == STORAGE_ATLAS_OVERFLOW_CANCEL:
            return STORAGE_ERROR_OVERFLOW
        if overflow == STORAGE_ATLAS_OVERFLOW_EXTEND:
            _regrid(b, max(a.width, b.width), max(a.height, b.height), b.depth,
                    max(a.rows, b.rows), max(a.columns, b.columns), b.layers)
    for row in range(min(a.rows, b.rows)):
        for col in range(min(a.columns, b.columns)):
            for y in range(min(a.height, b.height)):
                for x in range(min(a.width, b.width)):
                    b.data[_grid_index(b, x, y, depth, col, row, layer)] = a.data[_grid_index(a, x, y, 0, col, row)]
    return STORAGE_SUCCESS


# Optional facade; the procedural functions above are the public contract.
class Storage:
    pass

for _name, _function in list(globals().items()):
    if _name.startswith('clm_storage_') and callable(_function):
        setattr(Storage, _name[len('clm_storage_'):], staticmethod(_function))
del _name, _function
