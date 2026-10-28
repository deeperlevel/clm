"""Ordered dictionaries backed by CLM BPA blocks and arrays."""

from __future__ import annotations

import struct
from dataclasses import dataclass

from clm import bpa

CLM_DICTIONARY_INITIAL_CAPACITY = 16
_DICTIONARY_MAGIC = 0x434C4D444943544E
_METADATA_SIZE = struct.calcsize("=QQQQIIIB3x")
_INTERNAL_ID = (1 << 64) - 1


@dataclass(slots=True)
class _Dictionary:
    id: int
    value_type: int
    value_size: int
    keys_id: int
    values_id: int
    iterator: int = 0


_dictionaries: dict[int, _Dictionary] = {}


def _next_internal_id(*excluded: int) -> int | None:
    global _INTERNAL_ID
    for _ in range(len(_dictionaries) * 2 + 32):
        candidate = _INTERNAL_ID
        _INTERNAL_ID = (_INTERNAL_ID - 1) & ((1 << 64) - 1)
        if candidate not in excluded and not bpa.clm_bpa_is_existing(candidate) and not bpa.clm_bpaa_is_existing(candidate):
            return candidate
    return None


def _metadata_bytes(dictionary: _Dictionary, count: int) -> bytes:
    return struct.pack(
        "=QQQQIIIB3x",
        _DICTIONARY_MAGIC,
        dictionary.keys_id,
        dictionary.values_id,
        dictionary.id,
        dictionary.value_size,
        count,
        dictionary.iterator,
        dictionary.value_type,
    )


def _write_metadata(dictionary: _Dictionary, count: int) -> bool:
    return bpa.clm_bpa_set(dictionary.id, _metadata_bytes(dictionary, count)) == bpa.CLM_BPA_SET_SUCCESS


def _read_dictionary(identifier: int) -> _Dictionary | None:
    if type(identifier) is not int:
        return None
    dictionary = _dictionaries.get(identifier)
    if dictionary is None:
        return None
    status, data = bpa.clm_bpa_get(identifier, _METADATA_SIZE)
    if status != bpa.CLM_BPA_GET_SUCCESS or data is None or struct.unpack_from("=Q", data)[0] != _DICTIONARY_MAGIC:
        return None
    return dictionary


def _type_size(type_code: int, size: int) -> int | None:
    if type_code == bpa.CLM_BPA_TYPE_STRING:
        return 0 if size == 0 else None
    if type_code == bpa.CLM_BPA_TYPE_STRUCT:
        return size if type(size) is int and size > 0 else None
    expected = bpa.clm_bpa_type_size(type_code)
    if expected == 0 or size not in (0, expected):
        return None
    return expected


def _key_index(dictionary: _Dictionary, key: int) -> int | None:
    status, count = bpa.clm_bpaa_get_count_to(dictionary.keys_id)
    if status != bpa.CLM_BPA_GET_SUCCESS:
        return None
    for index in range(count):
        status, raw = bpa.clm_bpaa_get_raw(dictionary.keys_id, index, 8)
        if status == bpa.CLM_BPA_GET_SUCCESS and raw is not None and struct.unpack("=Q", raw)[0] == key:
            return index
    return None


def _remove_internal_arrays(dictionary: _Dictionary) -> None:
    bpa.clm_bpaa_delete(dictionary.keys_id)
    bpa.clm_bpaa_delete(dictionary.values_id)


def clm_dictionary_new(identifier: int, type_code: int, size: int = 0) -> int:
    """Create an ordered BPA-backed dictionary for the requested value type."""
    if (
        type(identifier) is not int
        or identifier < 0
        or identifier > (1 << 64) - 1
        or identifier in _dictionaries
        or bpa.clm_bpa_is_existing(identifier)
        or bpa.clm_bpaa_is_existing(identifier)
        or type(type_code) is not int
        or type(size) is not int
        or size < 0
    ):
        return bpa.CLM_BPA_FAILURE
    value_size = _type_size(type_code, size)
    if value_size is None:
        return bpa.CLM_BPA_FAILURE
    keys_id = _next_internal_id(identifier)
    if keys_id is None:
        return bpa.CLM_BPA_FAILURE
    values_id = _next_internal_id(identifier, keys_id)
    if values_id is None:
        return bpa.CLM_BPA_FAILURE

    if bpa.clm_bpaa_create(keys_id, bpa.CLM_BPA_TYPE_UNSIGNED_LONG, 0, 0) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    if type_code == bpa.CLM_BPA_TYPE_STRUCT:
        values_status = bpa.clm_bpaa_create_sized(values_id, type_code, 0, value_size, 0)
    else:
        values_status = bpa.clm_bpaa_create(values_id, type_code, 0, 0)
    if values_status != bpa.CLM_BPA_SUCCESS:
        bpa.clm_bpaa_delete(keys_id)
        return bpa.CLM_BPA_FAILURE
    if bpa.clm_bpa_create_sized(identifier, bpa.CLM_BPA_TYPE_STRUCT, _METADATA_SIZE) != bpa.CLM_BPA_SUCCESS:
        _remove_internal_arrays(_Dictionary(identifier, type_code, value_size, keys_id, values_id))
        return bpa.CLM_BPA_FAILURE

    dictionary = _Dictionary(identifier, type_code, value_size, keys_id, values_id)
    if not _write_metadata(dictionary, 0):
        bpa.clm_bpa_delete(identifier)
        _remove_internal_arrays(dictionary)
        return bpa.CLM_BPA_FAILURE
    _dictionaries[identifier] = dictionary
    return bpa.CLM_BPA_SUCCESS


def clm_dictionary_delete(identifier: int) -> int:
    """Delete a dictionary and its BPA-owned metadata and arrays."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None:
        return bpa.CLM_BPA_FAILURE
    _remove_internal_arrays(dictionary)
    del _dictionaries[identifier]
    return bpa.clm_bpa_delete(identifier)


def clm_dictionary_set_from(
    identifier: int,
    key: int,
    value: str | bytes | bytearray | memoryview,
    size: int | None = None,
) -> int:
    """Insert or replace a copied value under an unsigned 64-bit key."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None or type(key) is not int or not 0 <= key <= (1 << 64) - 1:
        return bpa.CLM_BPA_FAILURE
    if isinstance(value, str):
        try:
            raw = value.encode("utf-8") + b"\0"
        except UnicodeEncodeError:
            return bpa.CLM_BPA_FAILURE
        if size is not None:
            if type(size) is not int or size < 1:
                return bpa.CLM_BPA_FAILURE
            raw = raw[:size]
    elif isinstance(value, (bytes, bytearray, memoryview)):
        raw = bytes(value)
        if size is not None:
            if type(size) is not int or size < 0 or size > 0xFFFFFFFF:
                return bpa.CLM_BPA_FAILURE
            raw = raw[:size]
    else:
        return bpa.CLM_BPA_FAILURE

    index = _key_index(dictionary, key)
    if dictionary.value_type == bpa.CLM_BPA_TYPE_STRING:
        if not raw:
            return bpa.CLM_BPA_FAILURE
        if index is None:
            value_status = bpa.clm_bpaa_append_raw(dictionary.values_id, raw, len(raw))
        else:
            value_status = bpa.clm_bpaa_set_string_raw(dictionary.values_id, index, raw, len(raw))
        if value_status < 0:
            return bpa.CLM_BPA_FAILURE
    else:
        if len(raw) != dictionary.value_size:
            return bpa.CLM_BPA_FAILURE
        if index is None:
            value_status = bpa.clm_bpaa_append_raw(dictionary.values_id, raw, len(raw))
        else:
            value_status = bpa.clm_bpaa_set_raw(dictionary.values_id, index, raw, len(raw))
        if value_status not in (bpa.CLM_BPA_SUCCESS, bpa.CLM_BPA_SET_SUCCESS):
            return bpa.CLM_BPA_FAILURE

    if index is None:
        if bpa.clm_bpaa_append_raw(dictionary.keys_id, struct.pack("=Q", key), 8) != bpa.CLM_BPA_SUCCESS:
            bpa.clm_bpaa_remove_last(dictionary.values_id)
            return bpa.CLM_BPA_FAILURE
        count_status, count = bpa.clm_bpaa_get_count_to(dictionary.keys_id)
        if count_status != bpa.CLM_BPA_GET_SUCCESS or not _write_metadata(dictionary, count):
            bpa.clm_bpaa_remove_last(dictionary.keys_id)
            bpa.clm_bpaa_remove_last(dictionary.values_id)
            return bpa.CLM_BPA_FAILURE
        dictionary.iterator = 0
    return bpa.CLM_BPA_SET_SUCCESS


def clm_dictionary_add_from(identifier: int, key: int, value: str | bytes | bytearray | memoryview, size: int | None = None) -> int:
    """Insert a new key/value pair, failing when the key already exists."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None or _key_index(dictionary, key) is not None:
        return bpa.CLM_BPA_FAILURE
    return clm_dictionary_set_from(identifier, key, value, size)


def clm_dictionary_update_from(identifier: int, key: int, value: str | bytes | bytearray | memoryview, size: int | None = None) -> int:
    """Replace a value only when its key already exists."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None or _key_index(dictionary, key) is None:
        return bpa.CLM_BPA_FAILURE
    return clm_dictionary_set_from(identifier, key, value, size)


def clm_dictionary_set(identifier: int, key: int, source_block_id: int) -> int:
    """Copy a compatible BPA block's contents into the dictionary."""
    dictionary = _read_dictionary(identifier)
    status, value = bpa.clm_bpa_get_string(source_block_id) if dictionary and dictionary.value_type == bpa.CLM_BPA_TYPE_STRING else bpa.clm_bpa_get(source_block_id, dictionary.value_size) if dictionary else (bpa.CLM_BPA_FAILURE, None)
    if dictionary is None or status < 0 or value is None:
        return bpa.CLM_BPA_FAILURE
    source = bpa._blocks.get(source_block_id)
    if source is None or source.type != dictionary.value_type:
        return bpa.CLM_BPA_FAILURE
    data = value + b"\0" if dictionary.value_type == bpa.CLM_BPA_TYPE_STRING else value
    return clm_dictionary_set_from(identifier, key, data, len(data))


def clm_dictionary_get_to(identifier: int, key: int, buffer_size: int) -> tuple[int, bytes | None]:
    """Return a copied value as ``(GET_SUCCESS, bytes)`` or failure."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None or type(buffer_size) is not int or buffer_size < 0:
        return bpa.CLM_BPA_FAILURE, None
    index = _key_index(dictionary, key)
    if index is None:
        return bpa.CLM_BPA_FAILURE, None
    if dictionary.value_type == bpa.CLM_BPA_TYPE_STRING:
        status, value = bpa.clm_bpaa_get_string_raw(dictionary.values_id, index)
        if status < 0 or value is None or buffer_size < len(value) + 1:
            return bpa.CLM_BPA_FAILURE, None
        return bpa.CLM_BPA_GET_SUCCESS, value + b"\0"
    status, value = bpa.clm_bpaa_get_raw(dictionary.values_id, index, buffer_size)
    return (status, value) if status == bpa.CLM_BPA_GET_SUCCESS else (bpa.CLM_BPA_FAILURE, None)


def clm_dictionary_get(identifier: int, key: int, destination_block_id: int) -> int:
    """Copy a dictionary value into a compatible BPA block."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None:
        return bpa.CLM_BPA_FAILURE
    destination = bpa._blocks.get(destination_block_id)
    if destination is None or destination.type != dictionary.value_type:
        return bpa.CLM_BPA_FAILURE
    if dictionary.value_type != bpa.CLM_BPA_TYPE_STRING and destination.block_size != dictionary.value_size:
        return bpa.CLM_BPA_FAILURE
    index = _key_index(dictionary, key)
    if index is None:
        return bpa.CLM_BPA_FAILURE
    if dictionary.value_type == bpa.CLM_BPA_TYPE_STRING:
        status, value = bpa.clm_bpaa_get_string_raw(dictionary.values_id, index)
        if status < 0 or value is None or bpa.clm_bpa_set_string(destination_block_id, value, len(value) + 1) < 0:
            return bpa.CLM_BPA_FAILURE
        return bpa.CLM_BPA_GET_SUCCESS
    status, value = bpa.clm_bpaa_get_raw(dictionary.values_id, index, dictionary.value_size)
    if status != bpa.CLM_BPA_GET_SUCCESS or value is None or bpa.clm_bpa_set(destination_block_id, value) != bpa.CLM_BPA_SET_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    return bpa.CLM_BPA_GET_SUCCESS


def clm_dictionary_contains(identifier: int, key: int) -> bool:
    """Return whether the dictionary contains a key."""
    dictionary = _read_dictionary(identifier)
    return dictionary is not None and _key_index(dictionary, key) is not None


def clm_dictionary_count(identifier: int) -> int:
    """Return the number of entries, or BPA_FAILURE for an invalid dictionary."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None:
        return bpa.CLM_BPA_FAILURE
    status, count = bpa.clm_bpaa_get_count_to(dictionary.keys_id)
    return count if status == bpa.CLM_BPA_GET_SUCCESS else bpa.CLM_BPA_FAILURE


def clm_dictionary_remove(identifier: int, key: int) -> int:
    """Remove a key and its value, preserving the other insertion order."""
    dictionary = _read_dictionary(identifier)
    index = _key_index(dictionary, key) if dictionary is not None else None
    if dictionary is None or index is None:
        return bpa.CLM_BPA_FAILURE
    if bpa.clm_bpaa_remove(dictionary.values_id, index) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    if bpa.clm_bpaa_remove(dictionary.keys_id, index) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    dictionary.iterator = 0
    count = clm_dictionary_count(identifier)
    return bpa.CLM_BPA_SUCCESS if _write_metadata(dictionary, count) else bpa.CLM_BPA_FAILURE


def clm_dictionary_iterate_reset(identifier: int) -> int:
    """Reset ordered key iteration to the first entry."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None:
        return bpa.CLM_BPA_FAILURE
    dictionary.iterator = 0
    count = clm_dictionary_count(identifier)
    return bpa.CLM_BPA_SUCCESS if _write_metadata(dictionary, count) else bpa.CLM_BPA_FAILURE


def clm_dictionary_iterate_next(identifier: int) -> tuple[int, int | None]:
    """Return the next insertion-ordered key as ``(GET_SUCCESS, key)``."""
    dictionary = _read_dictionary(identifier)
    count = clm_dictionary_count(identifier)
    if dictionary is None or count < 0 or dictionary.iterator >= count:
        return bpa.CLM_BPA_FAILURE, None
    status, raw = bpa.clm_bpaa_get_raw(dictionary.keys_id, dictionary.iterator, 8)
    if status != bpa.CLM_BPA_GET_SUCCESS or raw is None:
        return bpa.CLM_BPA_FAILURE, None
    key = struct.unpack("=Q", raw)[0]
    dictionary.iterator += 1
    _write_metadata(dictionary, count)
    return bpa.CLM_BPA_GET_SUCCESS, key


def clm_dictionary_clear(identifier: int) -> int:
    """Remove all entries but retain the dictionary and array allocations."""
    dictionary = _read_dictionary(identifier)
    if dictionary is None:
        return bpa.CLM_BPA_FAILURE
    if bpa.clm_bpaa_clear(dictionary.keys_id) != bpa.CLM_BPA_SUCCESS or bpa.clm_bpaa_clear(dictionary.values_id) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    dictionary.iterator = 0
    return bpa.CLM_BPA_SUCCESS if _write_metadata(dictionary, 0) else bpa.CLM_BPA_FAILURE


def clm_dictionary_keys_to(identifier: int, capacity: int | None = None) -> tuple[int, list[int]]:
    """Copy dictionary keys in insertion order, optionally checking capacity."""
    dictionary = _read_dictionary(identifier)
    count = clm_dictionary_count(identifier)
    if dictionary is None or count < 0 or (capacity is not None and (type(capacity) is not int or capacity < count)):
        return bpa.CLM_BPA_FAILURE, []
    keys: list[int] = []
    for index in range(count):
        status, raw = bpa.clm_bpaa_get_raw(dictionary.keys_id, index, 8)
        if status != bpa.CLM_BPA_GET_SUCCESS or raw is None:
            return bpa.CLM_BPA_FAILURE, []
        keys.append(struct.unpack("=Q", raw)[0])
    return bpa.CLM_BPA_GET_SUCCESS, keys


__all__ = [name for name in globals() if name.startswith("CLM_DICTIONARY_") or name.startswith("clm_dictionary_")]
