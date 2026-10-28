"""Fixed-dimension BPA-backed grids and atlases."""

from __future__ import annotations

import struct
from dataclasses import dataclass

from clm import bpa

CLM_STORAGE_MAGIC = 0x434C4D53544F5247
_MAX_COUNT = (1 << 31) - 1
_METADATA_SIZE = 64
_INTERNAL_ID = (1 << 64) - 1


@dataclass(slots=True)
class StorageInfo:
    """Dimensions and element metadata for a storage resource."""

    columns: int
    rows: int
    layers: int
    frame_columns: int
    frame_rows: int
    element_size: int
    type: int
    atlas: bool
    data_id: int


_storage: dict[int, StorageInfo] = {}


def _next_data_id(forbidden: int) -> int | None:
    global _INTERNAL_ID
    for _ in range(len(_storage) + 32):
        candidate = _INTERNAL_ID
        _INTERNAL_ID = (_INTERNAL_ID - 1) & ((1 << 64) - 1)
        if candidate != forbidden and not bpa.clm_bpa_is_existing(candidate) and not bpa.clm_bpaa_is_existing(candidate):
            return candidate
    return None


def _metadata_bytes(info: StorageInfo) -> bytes:
    data = struct.pack(
        "=QIIIIIIIB3xQ",
        CLM_STORAGE_MAGIC,
        info.columns,
        info.rows,
        info.layers,
        info.frame_columns,
        info.frame_rows,
        info.element_size,
        info.type,
        int(info.atlas),
        info.data_id,
    )
    return data.ljust(_METADATA_SIZE, b"\0")


def _read(identifier: int) -> StorageInfo | None:
    if type(identifier) is not int:
        return None
    info = _storage.get(identifier)
    if info is None:
        return None
    status, data = bpa.clm_bpa_get(identifier, _METADATA_SIZE)
    if status != bpa.CLM_BPA_GET_SUCCESS or data is None or struct.unpack_from("=Q", data)[0] != CLM_STORAGE_MAGIC:
        return None
    array = bpa._arrays.get(info.data_id)
    if array is None or array.type != info.type or array.block_size != info.element_size:
        return None
    return info


def _new(
    identifier: int,
    type_code: int,
    struct_size: int,
    columns: int,
    rows: int,
    layers: int,
    frame_columns: int,
    frame_rows: int,
    atlas: bool,
) -> int:
    dimensions = (columns, rows, layers)
    if (
        type(identifier) is not int
        or identifier < 0
        or identifier > (1 << 64) - 1
        or identifier in _storage
        or bpa.clm_bpa_is_existing(identifier)
        or bpa.clm_bpaa_is_existing(identifier)
        or type(type_code) is not int
        or type(struct_size) is not int
        or struct_size < 0
        or any(type(value) is not int or value <= 0 or value > 0xFFFFFFFF for value in dimensions)
        or type(frame_columns) is not int
        or type(frame_rows) is not int
        or (atlas and (frame_columns <= 0 or frame_rows <= 0 or columns % frame_columns or rows % frame_rows))
        or (not atlas and (frame_columns != 0 or frame_rows != 0))
        or type_code == bpa.CLM_BPA_TYPE_STRING
    ):
        return bpa.CLM_BPA_FAILURE
    if type_code == bpa.CLM_BPA_TYPE_STRUCT:
        element_size = struct_size
        if element_size <= 0 or element_size > 0xFFFFFFFF:
            return bpa.CLM_BPA_FAILURE
    else:
        element_size = bpa.clm_bpa_type_size(type_code)
        if element_size == 0 or struct_size not in (0, element_size):
            return bpa.CLM_BPA_FAILURE
    count = columns * rows * layers
    if count <= 0 or count > _MAX_COUNT or count * element_size > 0x7FFFFFFFFFFFFFFF:
        return bpa.CLM_BPA_FAILURE
    data_id = _next_data_id(identifier)
    if data_id is None:
        return bpa.CLM_BPA_FAILURE
    if bpa.clm_bpaa_create_sized(data_id, type_code, count, element_size, 0) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    array = bpa._arrays.get(data_id)
    if array is None:
        bpa.clm_bpaa_delete(data_id)
        return bpa.CLM_BPA_FAILURE
    array.count = count
    array.deck_size = count
    info = StorageInfo(columns, rows, layers, frame_columns, frame_rows, element_size, type_code, atlas, data_id)
    if bpa.clm_bpa_create_sized(identifier, bpa.CLM_BPA_TYPE_STRUCT, _METADATA_SIZE) != bpa.CLM_BPA_SUCCESS:
        bpa.clm_bpaa_delete(data_id)
        return bpa.CLM_BPA_FAILURE
    if bpa.clm_bpa_set(identifier, _metadata_bytes(info)) != bpa.CLM_BPA_SET_SUCCESS:
        bpa.clm_bpa_delete(identifier)
        bpa.clm_bpaa_delete(data_id)
        return bpa.CLM_BPA_FAILURE
    _storage[identifier] = info
    return bpa.CLM_BPA_SUCCESS


def clm_storage_new(identifier: int, type_code: int, struct_size: int, columns: int, rows: int, layers: int) -> int:
    """Create a zero-initialized row-major 3D grid of fixed-size values."""
    return _new(identifier, type_code, struct_size, columns, rows, layers, 0, 0, False)


def clm_storage_new_2d(identifier: int, type_code: int, struct_size: int, columns: int, rows: int) -> int:
    """Create a zero-initialized 2D grid with one layer."""
    return clm_storage_new(identifier, type_code, struct_size, columns, rows, 1)


def clm_storage_atlas_new(identifier: int, type_code: int, struct_size: int, columns: int, rows: int, frame_columns: int, frame_rows: int) -> int:
    """Create a 2D atlas whose dimensions are divisible by its frame size."""
    return _new(identifier, type_code, struct_size, columns, rows, 1, frame_columns, frame_rows, True)


def clm_storage_atlas_new_3d(identifier: int, type_code: int, struct_size: int, columns: int, rows: int, layers: int, frame_columns: int, frame_rows: int) -> int:
    """Create a layered atlas with identical frame grids in every layer."""
    return _new(identifier, type_code, struct_size, columns, rows, layers, frame_columns, frame_rows, True)


def clm_storage_get_info(identifier: int) -> tuple[int, StorageInfo | None]:
    """Return storage metadata as ``(GET_SUCCESS, info)``."""
    info = _read(identifier)
    return (bpa.CLM_BPA_GET_SUCCESS, info) if info is not None else (bpa.CLM_BPA_FAILURE, None)


def clm_storage_delete(identifier: int) -> int:
    """Delete storage metadata and its associated BPAA payload array."""
    info = _read(identifier)
    if info is None:
        return bpa.CLM_BPA_FAILURE
    del _storage[identifier]
    array_status = bpa.clm_bpaa_delete(info.data_id)
    block_status = bpa.clm_bpa_delete(identifier)
    return bpa.CLM_BPA_SUCCESS if array_status == block_status == bpa.CLM_BPA_SUCCESS else bpa.CLM_BPA_FAILURE


def _index(info: StorageInfo, layer: int, row: int, column: int) -> int | None:
    if any(type(value) is not int for value in (layer, row, column)):
        return None
    if layer < 0 or layer >= info.layers or row < 0 or row >= info.rows or column < 0 or column >= info.columns:
        return None
    return (layer * info.rows + row) * info.columns + column


def clm_storage_get_to(identifier: int, layer: int, row: int, column: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Copy one element into Python-owned bytes using layer/row/column coordinates."""
    info = _read(identifier)
    if info is None or type(buffer_size) is not int or buffer_size < info.element_size:
        return bpa.CLM_BPA_FAILURE, None
    index = _index(info, layer, row, column)
    return bpa.clm_bpaa_get_raw(info.data_id, index, buffer_size) if index is not None else (bpa.CLM_BPA_FAILURE, None)


def clm_storage_set_from(identifier: int, layer: int, row: int, column: int, value: bytes | bytearray | memoryview, value_size: int | None = None) -> int:
    """Replace one cell from exact-size fixed-width bytes."""
    info = _read(identifier)
    if info is None or not isinstance(value, (bytes, bytearray, memoryview)):
        return bpa.CLM_BPA_FAILURE
    data = bytes(value)
    if value_size is not None and (type(value_size) is not int or value_size != len(data)):
        return bpa.CLM_BPA_FAILURE
    if len(data) != info.element_size:
        return bpa.CLM_BPA_FAILURE
    index = _index(info, layer, row, column)
    return bpa.clm_bpaa_set_raw(info.data_id, index, data, len(data)) if index is not None else bpa.CLM_BPA_FAILURE


def clm_storage_get(identifier: int, layer: int, row: int, column: int, destination_block_id: int) -> int:
    """Copy a cell into a compatible BPA destination block."""
    info = _read(identifier)
    index = _index(info, layer, row, column) if info is not None else None
    if info is None or index is None:
        return bpa.CLM_BPA_FAILURE
    return bpa.clm_bpaa_get(info.data_id, index, destination_block_id, info.element_size)


def clm_storage_set(identifier: int, layer: int, row: int, column: int, source_block_id: int) -> int:
    """Copy a compatible BPA source block into one storage cell."""
    info = _read(identifier)
    index = _index(info, layer, row, column) if info is not None else None
    if info is None or index is None:
        return bpa.CLM_BPA_FAILURE
    return bpa.clm_bpaa_set(info.data_id, index, source_block_id, info.element_size)


def clm_storage_fill_from(identifier: int, value: bytes | bytearray | memoryview, value_size: int | None = None) -> int:
    """Fill every grid element with a copy of one fixed-width payload."""
    info = _read(identifier)
    if info is None or not isinstance(value, (bytes, bytearray, memoryview)):
        return bpa.CLM_BPA_FAILURE
    data = bytes(value)
    if len(data) != info.element_size or (value_size is not None and value_size != len(data)):
        return bpa.CLM_BPA_FAILURE
    array = bpa._arrays[info.data_id]
    if not isinstance(array.data, bytearray):
        return bpa.CLM_BPA_FAILURE
    for offset in range(0, len(array.data), info.element_size):
        array.data[offset : offset + info.element_size] = data
    array.deck_size = array.count
    return bpa.CLM_BPA_SUCCESS


def clm_storage_slice(source_id: int, layer: int, row: int, column: int, layers: int, rows: int, columns: int, destination_id: int) -> int:
    """Copy a bounded volume into a new regular grid, independent of the source."""
    source = _read(source_id)
    if source is None or any(type(value) is not int for value in (layer, row, column, layers, rows, columns)):
        return bpa.CLM_BPA_FAILURE
    if layers <= 0 or rows <= 0 or columns <= 0 or layer < 0 or row < 0 or column < 0:
        return bpa.CLM_BPA_FAILURE
    if layer >= source.layers or row >= source.rows or column >= source.columns or layers > source.layers - layer or rows > source.rows - row or columns > source.columns - column:
        return bpa.CLM_BPA_FAILURE
    if clm_storage_new(destination_id, source.type, source.element_size, columns, rows, layers) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    destination = _read(destination_id)
    if destination is None:
        clm_storage_delete(destination_id)
        return bpa.CLM_BPA_FAILURE
    for z in range(layers):
        for y in range(rows):
            for x in range(columns):
                _, value = clm_storage_get_to(source_id, layer + z, row + y, column + x, source.element_size)
                if value is None or clm_storage_set_from(destination_id, z, y, x, value) != bpa.CLM_BPA_SET_SUCCESS:
                    clm_storage_delete(destination_id)
                    return bpa.CLM_BPA_FAILURE
    return bpa.CLM_BPA_SUCCESS


def _frame_bounds(identifier: int, layer: int, frame_row: int, frame_column: int) -> tuple[StorageInfo, int, int] | None:
    info = _read(identifier)
    if info is None or not info.atlas or any(type(value) is not int for value in (layer, frame_row, frame_column)):
        return None
    frame_rows = info.rows // info.frame_rows
    frame_columns = info.columns // info.frame_columns
    if layer < 0 or layer >= info.layers or frame_row < 0 or frame_row >= frame_rows or frame_column < 0 or frame_column >= frame_columns:
        return None
    return info, frame_row * info.frame_rows, frame_column * info.frame_columns


def clm_storage_atlas_get_frame_to(identifier: int, layer: int, frame_row: int, frame_column: int, buffer_size: int | None = None) -> tuple[int, bytes | None]:
    """Copy one tightly packed atlas frame into bytes."""
    bounds = _frame_bounds(identifier, layer, frame_row, frame_column)
    if bounds is None:
        return bpa.CLM_BPA_FAILURE, None
    info, row_start, column_start = bounds
    required = info.frame_rows * info.frame_columns * info.element_size
    if buffer_size is not None and (type(buffer_size) is not int or buffer_size < required):
        return bpa.CLM_BPA_FAILURE, None
    result = bytearray()
    for row_offset in range(info.frame_rows):
        for column_offset in range(info.frame_columns):
            _, value = clm_storage_get_to(identifier, layer, row_start + row_offset, column_start + column_offset, info.element_size)
            if value is None:
                return bpa.CLM_BPA_FAILURE, None
            result.extend(value)
    return bpa.CLM_BPA_GET_SUCCESS, bytes(result)


def clm_storage_atlas_set_frame_from(identifier: int, layer: int, frame_row: int, frame_column: int, value: bytes | bytearray | memoryview, buffer_size: int | None = None) -> int:
    """Copy tightly packed row-major bytes into an atlas frame."""
    bounds = _frame_bounds(identifier, layer, frame_row, frame_column)
    if bounds is None or not isinstance(value, (bytes, bytearray, memoryview)):
        return bpa.CLM_BPA_FAILURE
    info, row_start, column_start = bounds
    data = bytes(value)
    required = info.frame_rows * info.frame_columns * info.element_size
    if len(data) != required or (buffer_size is not None and buffer_size != required):
        return bpa.CLM_BPA_FAILURE
    position = 0
    for row_offset in range(info.frame_rows):
        for column_offset in range(info.frame_columns):
            cell = data[position : position + info.element_size]
            if clm_storage_set_from(identifier, layer, row_start + row_offset, column_start + column_offset, cell) != bpa.CLM_BPA_SET_SUCCESS:
                return bpa.CLM_BPA_FAILURE
            position += info.element_size
    return bpa.CLM_BPA_SET_SUCCESS


def clm_storage_atlas_slice_frame(identifier: int, layer: int, frame_row: int, frame_column: int, destination_id: int) -> int:
    """Copy one atlas frame into an independent 2D regular grid."""
    bounds = _frame_bounds(identifier, layer, frame_row, frame_column)
    if bounds is None:
        return bpa.CLM_BPA_FAILURE
    info, row_start, column_start = bounds
    return clm_storage_slice(identifier, layer, row_start, column_start, 1, info.frame_rows, info.frame_columns, destination_id)


__all__ = [name for name in globals() if name.startswith("CLM_STORAGE_") or name.startswith("clm_storage_") or name == "StorageInfo"]
