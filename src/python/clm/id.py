"""UTF-8 name-to-ID registry backed by BPA dictionaries."""

from __future__ import annotations

import struct

from clm import bpa, dictionary

ID_STORAGE_ID = 0x0F000001
CLM_ID_INDEX_STORAGE_ID = 0x0F000002
CLM_ID_NAME_BUFFER_ID = 0x0F000003
CLM_ID_NAMES_STORAGE_ID = 0x0F000004
CLM_ID_VALUES_STORAGE_ID = 0x0F000005
CLM_ID_NOT_FOUND = (1 << 64) - 1

clm_id_generator_value = 0
clm_id_next_entry_value = 0
clm_id_active_index = ID_STORAGE_ID
clm_id_generator_exhausted = False
clm_id_case_sensitive = True


def _valid_name(name: object) -> bytes | None:
    if isinstance(name, str):
        try:
            data = name.encode("utf-8", "strict")
        except UnicodeEncodeError:
            return None
    elif isinstance(name, bytes):
        data = name
        try:
            data.decode("utf-8", "strict")
        except UnicodeDecodeError:
            return None
    else:
        return None
    if not data or b"\0" in data:
        return None
    return data


def _fold_ascii(data: bytes, case_sensitive: bool) -> bytes:
    if case_sensitive:
        return data
    return bytes(byte + 32 if 65 <= byte <= 90 else byte for byte in data)


def _hash_name(data: bytes, case_sensitive: bool) -> int:
    value = 2166136261
    for byte in _fold_ascii(data, case_sensitive):
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def _all_reserved_ids_available() -> bool:
    reserved = (
        ID_STORAGE_ID,
        CLM_ID_INDEX_STORAGE_ID,
        CLM_ID_NAME_BUFFER_ID,
        CLM_ID_NAMES_STORAGE_ID,
        CLM_ID_VALUES_STORAGE_ID,
    )
    return all(not bpa.clm_bpa_is_existing(identifier) and not bpa.clm_bpaa_is_existing(identifier) for identifier in reserved)


def _ensure_storage() -> int:
    global clm_id_active_index, clm_id_next_entry_value
    required = (ID_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_VALUES_STORAGE_ID)
    if all(dictionary.clm_dictionary_count(identifier) >= 0 for identifier in required):
        scratch = bpa._blocks.get(CLM_ID_NAME_BUFFER_ID)
        if scratch is not None and scratch.type == bpa.CLM_BPA_TYPE_STRING:
            return bpa.CLM_BPA_SUCCESS
    if not _all_reserved_ids_available():
        return bpa.CLM_BPA_FAILURE

    created: list[int] = []
    specs = (
        (ID_STORAGE_ID, bpa.CLM_BPA_TYPE_UNSIGNED_LONG, 0),
        (CLM_ID_INDEX_STORAGE_ID, bpa.CLM_BPA_TYPE_UNSIGNED_LONG, 0),
        (CLM_ID_NAMES_STORAGE_ID, bpa.CLM_BPA_TYPE_STRING, 0),
        (CLM_ID_VALUES_STORAGE_ID, bpa.CLM_BPA_TYPE_UNSIGNED_LONG, 0),
    )
    for identifier, type_code, size in specs:
        if dictionary.clm_dictionary_new(identifier, type_code, size) != bpa.CLM_BPA_SUCCESS:
            for previous in reversed(created):
                dictionary.clm_dictionary_delete(previous)
            return bpa.CLM_BPA_FAILURE
        created.append(identifier)
    if bpa.clm_bpa_create(CLM_ID_NAME_BUFFER_ID, bpa.CLM_BPA_TYPE_STRING) != bpa.CLM_BPA_SUCCESS:
        for previous in reversed(created):
            dictionary.clm_dictionary_delete(previous)
        return bpa.CLM_BPA_FAILURE
    clm_id_active_index = ID_STORAGE_ID
    clm_id_next_entry_value = 0
    return bpa.CLM_BPA_SUCCESS


def _index_lookup(index_id: int, name: bytes, case_sensitive: bool) -> tuple[bool, int, int | None] | None:
    count = dictionary.clm_dictionary_count(index_id)
    if count < 0:
        return None
    folded = _fold_ascii(name, case_sensitive)
    start = _hash_name(name, case_sensitive)
    for probe in range(count + 1):
        slot = (start + probe) & CLM_ID_NOT_FOUND
        if not dictionary.clm_dictionary_contains(index_id, slot):
            return False, slot, None
        status, raw = dictionary.clm_dictionary_get_to(index_id, slot, 8)
        if status != bpa.CLM_BPA_GET_SUCCESS or raw is None:
            return None
        entry_id = struct.unpack("=Q", raw)[0]
        if entry_id == CLM_ID_NOT_FOUND:
            continue
        status, stored = dictionary.clm_dictionary_get_to(CLM_ID_NAMES_STORAGE_ID, entry_id, 0xFFFFFFFF)
        if status != bpa.CLM_BPA_GET_SUCCESS or stored is None:
            return None
        stored_name = stored.split(b"\0", 1)[0]
        if _fold_ascii(stored_name, case_sensitive) == folded:
            return True, slot, entry_id
    return None


def _allocate_entry_id() -> int | None:
    global clm_id_next_entry_value
    count = dictionary.clm_dictionary_count(CLM_ID_NAMES_STORAGE_ID)
    if count < 0:
        return None
    for offset in range(count + 1):
        candidate = (clm_id_next_entry_value + offset) & CLM_ID_NOT_FOUND
        if candidate != CLM_ID_NOT_FOUND and not dictionary.clm_dictionary_contains(CLM_ID_NAMES_STORAGE_ID, candidate):
            clm_id_next_entry_value = (candidate + 1) & CLM_ID_NOT_FOUND
            return candidate
    return None


def _store_index(index_id: int, slot: int, entry_id: int) -> int:
    payload = struct.pack("=Q", entry_id)
    if dictionary.clm_dictionary_contains(index_id, slot):
        return dictionary.clm_dictionary_update_from(index_id, slot, payload, len(payload))
    return dictionary.clm_dictionary_add_from(index_id, slot, payload, len(payload))


def clm_id_register(name: str | bytes, identifier: int) -> int:
    """Register a nonempty UTF-8 name and numeric ID; duplicate names fail."""
    global clm_id_active_index
    raw_name = _valid_name(name)
    if raw_name is None or type(identifier) is not int or not 0 <= identifier <= CLM_ID_NOT_FOUND:
        return bpa.CLM_BPA_FAILURE
    if _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    lookup = _index_lookup(clm_id_active_index, raw_name, clm_id_case_sensitive)
    if lookup is None or lookup[0]:
        return bpa.CLM_BPA_FAILURE
    entry_id = _allocate_entry_id()
    if entry_id is None:
        return bpa.CLM_BPA_FAILURE
    if dictionary.clm_dictionary_set_from(CLM_ID_NAMES_STORAGE_ID, entry_id, raw_name + b"\0", len(raw_name) + 1) != bpa.CLM_BPA_SET_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    payload = struct.pack("=Q", identifier)
    if dictionary.clm_dictionary_set_from(CLM_ID_VALUES_STORAGE_ID, entry_id, payload, len(payload)) != bpa.CLM_BPA_SET_SUCCESS:
        dictionary.clm_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry_id)
        return bpa.CLM_BPA_FAILURE
    if _store_index(clm_id_active_index, lookup[1], entry_id) != bpa.CLM_BPA_SET_SUCCESS:
        dictionary.clm_dictionary_remove(CLM_ID_VALUES_STORAGE_ID, entry_id)
        dictionary.clm_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry_id)
        return bpa.CLM_BPA_FAILURE
    return bpa.CLM_BPA_SET_SUCCESS


def clm_id_map(name: str | bytes, identifier: int) -> int:
    """Alias for ``clm_id_register``."""
    return clm_id_register(name, identifier)


def clm_id_get(name: str | bytes) -> int:
    """Resolve a name, returning ``CLM_ID_NOT_FOUND`` when absent or invalid."""
    raw_name = _valid_name(name)
    if raw_name is None or _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return CLM_ID_NOT_FOUND
    lookup = _index_lookup(clm_id_active_index, raw_name, clm_id_case_sensitive)
    if lookup is None or not lookup[0] or lookup[2] is None:
        return CLM_ID_NOT_FOUND
    status, raw = dictionary.clm_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, lookup[2], 8)
    return struct.unpack("=Q", raw)[0] if status == bpa.CLM_BPA_GET_SUCCESS and raw is not None else CLM_ID_NOT_FOUND


def clm_id_try_get(name: str | bytes) -> tuple[int, int | None]:
    """Resolve a name as ``(GET_SUCCESS, id)`` so every ID value is representable."""
    raw_name = _valid_name(name)
    if raw_name is None or _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE, None
    lookup = _index_lookup(clm_id_active_index, raw_name, clm_id_case_sensitive)
    if lookup is None or not lookup[0] or lookup[2] is None:
        return bpa.CLM_BPA_FAILURE, None
    status, raw = dictionary.clm_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, lookup[2], 8)
    if status != bpa.CLM_BPA_GET_SUCCESS or raw is None:
        return bpa.CLM_BPA_FAILURE, None
    return bpa.CLM_BPA_GET_SUCCESS, struct.unpack("=Q", raw)[0]


def clm_id_set(name: str | bytes, identifier: int) -> int:
    """Register a new name or update an existing name's ID."""
    raw_name = _valid_name(name)
    if raw_name is None or type(identifier) is not int or not 0 <= identifier <= CLM_ID_NOT_FOUND:
        return bpa.CLM_BPA_FAILURE
    if _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    lookup = _index_lookup(clm_id_active_index, raw_name, clm_id_case_sensitive)
    if lookup is None:
        return bpa.CLM_BPA_FAILURE
    if not lookup[0]:
        return clm_id_register(name, identifier)
    assert lookup[2] is not None
    payload = struct.pack("=Q", identifier)
    return dictionary.clm_dictionary_update_from(CLM_ID_VALUES_STORAGE_ID, lookup[2], payload, len(payload))


def clm_id_remove(name: str | bytes) -> int:
    """Remove a name mapping without affecting other aliases for its ID."""
    raw_name = _valid_name(name)
    if raw_name is None or _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    lookup = _index_lookup(clm_id_active_index, raw_name, clm_id_case_sensitive)
    if lookup is None or not lookup[0] or lookup[2] is None:
        return bpa.CLM_BPA_FAILURE
    entry_id = lookup[2]
    result = dictionary.clm_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry_id)
    result_values = dictionary.clm_dictionary_remove(CLM_ID_VALUES_STORAGE_ID, entry_id)
    if result != bpa.CLM_BPA_SUCCESS or result_values != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    return _rebuild_index(clm_id_active_index, clm_id_case_sensitive)


def clm_id_unmap(identifier: int) -> int:
    """Remove every registered name that maps to ``identifier``."""
    if type(identifier) is not int or not 0 <= identifier <= CLM_ID_NOT_FOUND or _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    status, keys = dictionary.clm_dictionary_keys_to(CLM_ID_VALUES_STORAGE_ID)
    if status != bpa.CLM_BPA_GET_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    matching: list[str] = []
    target = struct.pack("=Q", identifier)
    for entry_id in keys:
        value_status, value = dictionary.clm_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry_id, 8)
        if value_status == bpa.CLM_BPA_GET_SUCCESS and value == target:
            name_status, name = dictionary.clm_dictionary_get_to(CLM_ID_NAMES_STORAGE_ID, entry_id, 1 << 20)
            if name_status == bpa.CLM_BPA_GET_SUCCESS and name is not None:
                matching.append(name[:-1].decode("utf-8"))
    if not matching:
        return bpa.CLM_BPA_FAILURE
    for name in matching:
        if clm_id_remove(name) != bpa.CLM_BPA_SUCCESS:
            return bpa.CLM_BPA_FAILURE
    return bpa.CLM_BPA_SUCCESS


def clm_id_unmap_name(name: str | bytes) -> int:
    """Alias for ``clm_id_remove``."""
    return clm_id_remove(name)


def clm_id_is_case_sensitive() -> bool:
    """Return whether name matching is case-sensitive."""
    return clm_id_case_sensitive


def _rebuild_index(index_id: int, case_sensitive: bool) -> int:
    if dictionary.clm_dictionary_clear(index_id) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    status, entry_ids = dictionary.clm_dictionary_keys_to(CLM_ID_NAMES_STORAGE_ID)
    if status != bpa.CLM_BPA_GET_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    for entry_id in entry_ids:
        status, raw_name = dictionary.clm_dictionary_get_to(CLM_ID_NAMES_STORAGE_ID, entry_id, 1 << 20)
        if status != bpa.CLM_BPA_GET_SUCCESS or raw_name is None:
            return bpa.CLM_BPA_FAILURE
        raw_name = raw_name[:-1]
        lookup = _index_lookup(index_id, raw_name, case_sensitive)
        if lookup is None or lookup[0]:
            dictionary.clm_dictionary_clear(index_id)
            return bpa.CLM_BPA_FAILURE
        if _store_index(index_id, lookup[1], entry_id) != bpa.CLM_BPA_SET_SUCCESS:
            dictionary.clm_dictionary_clear(index_id)
            return bpa.CLM_BPA_FAILURE
    return bpa.CLM_BPA_SUCCESS


def clm_id_set_case_sensitive(enabled: bool) -> int:
    """Change case policy, rejecting changes that would merge existing names."""
    global clm_id_active_index, clm_id_case_sensitive
    if not isinstance(enabled, bool) or _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    if enabled == clm_id_case_sensitive:
        return bpa.CLM_BPA_SUCCESS
    next_index = CLM_ID_INDEX_STORAGE_ID if clm_id_active_index == ID_STORAGE_ID else ID_STORAGE_ID
    if _rebuild_index(next_index, enabled) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    clm_id_active_index = next_index
    clm_id_case_sensitive = enabled
    return bpa.CLM_BPA_SUCCESS


def clm_id_clear() -> int:
    """Clear all mappings while preserving case policy and generator state."""
    global clm_id_active_index, clm_id_next_entry_value
    if _ensure_storage() != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    for identifier in (ID_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_VALUES_STORAGE_ID):
        if dictionary.clm_dictionary_clear(identifier) != bpa.CLM_BPA_SUCCESS:
            return bpa.CLM_BPA_FAILURE
    clm_id_active_index = ID_STORAGE_ID
    clm_id_next_entry_value = 0
    return bpa.CLM_BPA_SUCCESS


def clm_id_shutdown() -> int:
    """Release private BPA dictionaries; subsequent operations recreate them."""
    global clm_id_active_index, clm_id_next_entry_value
    if not _dictionaries_exist():
        return bpa.CLM_BPA_FAILURE
    status = bpa.CLM_BPA_SUCCESS
    for identifier in (CLM_ID_VALUES_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, ID_STORAGE_ID):
        if dictionary.clm_dictionary_delete(identifier) != bpa.CLM_BPA_SUCCESS:
            status = bpa.CLM_BPA_FAILURE
    if bpa.clm_bpa_delete(CLM_ID_NAME_BUFFER_ID) != bpa.CLM_BPA_SUCCESS:
        status = bpa.CLM_BPA_FAILURE
    clm_id_active_index = ID_STORAGE_ID
    clm_id_next_entry_value = 0
    return status


def _dictionaries_exist() -> bool:
    return all(dictionary.clm_dictionary_count(identifier) >= 0 for identifier in (ID_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_VALUES_STORAGE_ID)) and bpa.clm_bpa_is_existing(CLM_ID_NAME_BUFFER_ID)


def clm_id_gen_reset() -> None:
    """Reset sequential ID generation so the next call returns zero."""
    global clm_id_generator_value, clm_id_generator_exhausted
    clm_id_generator_value = 0
    clm_id_generator_exhausted = False


def clm_id_gen_next() -> int:
    """Return the next unsigned 64-bit sequential ID, or NOT_FOUND when exhausted."""
    global clm_id_generator_value, clm_id_generator_exhausted
    if clm_id_generator_exhausted:
        return CLM_ID_NOT_FOUND
    result = clm_id_generator_value
    if result == CLM_ID_NOT_FOUND - 1:
        clm_id_generator_exhausted = True
    else:
        clm_id_generator_value += 1
    return result


__all__ = [name for name in globals() if name.startswith("CLM_ID_") or name.startswith("ID_STORAGE_") or name.startswith("clm_id_")]
