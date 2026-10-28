"""Unicode string and string-array operations stored in CLM BPA resources."""

from __future__ import annotations

import random

from clm import bpa

CLM_STRING_ENCODING_UTF8 = 1
CLM_STRING_ENCODING_LATIN1 = 2
CLM_STRING_ENCODING_UTF16LE = 3
clm_string_global_default_encoding = CLM_STRING_ENCODING_UTF8


def _encode_text(text: object) -> bytes | None:
    if not isinstance(text, str) or "\0" in text:
        return None
    try:
        return text.encode("utf-8", "strict")
    except UnicodeEncodeError:
        return None


def _decode_utf8(data: bytes) -> str | None:
    if b"\0" in data:
        return None
    try:
        return data.decode("utf-8", "strict")
    except UnicodeDecodeError:
        return None


def _block_text(identifier: int) -> tuple[object, str] | None:
    block = bpa._blocks.get(identifier) if type(identifier) is int else None
    if block is None or block.type != bpa.CLM_BPA_TYPE_STRING:
        return None
    status, data = bpa.clm_bpa_get_string(identifier)
    if status < 0 or data is None:
        return None
    text = _decode_utf8(data)
    return (block, text) if text is not None else None


def _supported_encoding(encoding: int) -> bool:
    return type(encoding) is int and encoding in (
        CLM_STRING_ENCODING_UTF8,
        CLM_STRING_ENCODING_LATIN1,
        CLM_STRING_ENCODING_UTF16LE,
    )


def clm_string_set_default_encoding(encoding: int) -> int:
    """Set the preferred external encoding for subsequently created strings."""
    global clm_string_global_default_encoding
    if not _supported_encoding(encoding):
        return bpa.CLM_BPA_FAILURE
    clm_string_global_default_encoding = encoding
    return bpa.CLM_BPA_SUCCESS


def clm_string_get_default_encoding() -> int:
    """Return the preferred external encoding for newly created strings."""
    return clm_string_global_default_encoding


def clm_string_new(identifier: int, text: str) -> bool:
    """Create a BPA string containing validated UTF-8 text."""
    data = _encode_text(text)
    if data is None or bpa.clm_bpa_create(identifier, bpa.CLM_BPA_TYPE_STRING) != bpa.CLM_BPA_SUCCESS:
        return False
    if bpa.clm_bpa_set_string(identifier, data + b"\0", len(data) + 1) < 0:
        bpa.clm_bpa_delete(identifier)
        return False
    bpa.clm_bpa_set_encoding(identifier, clm_string_global_default_encoding)
    return True


def clm_string_delete(identifier: int) -> bool:
    """Delete an existing string BPA block without affecting other block types."""
    if _block_text(identifier) is None:
        return False
    return bpa.clm_bpa_delete(identifier) == bpa.CLM_BPA_SUCCESS


def clm_string_set(identifier: int, text: str) -> int:
    """Replace a string's text and return its Unicode code-point count."""
    data = _encode_text(text)
    if data is None or _block_text(identifier) is None:
        return bpa.CLM_BPA_FAILURE
    return len(text) if bpa.clm_bpa_replace(identifier, data + b"\0", len(data) + 1) >= 0 else bpa.CLM_BPA_FAILURE


def clm_string_append(identifier: int, suffix: str) -> int:
    """Append validated text and return the number of appended code points."""
    current = _block_text(identifier)
    if current is None or _encode_text(suffix) is None:
        return bpa.CLM_BPA_FAILURE
    return len(suffix) if clm_string_set(identifier, current[1] + suffix) >= 0 else bpa.CLM_BPA_FAILURE


def clm_string_append_line(identifier: int, suffix: str) -> int:
    """Append text and one LF; return the number of appended code points."""
    current = _block_text(identifier)
    if current is None or _encode_text(suffix) is None:
        return bpa.CLM_BPA_FAILURE
    return len(suffix) + 1 if clm_string_set(identifier, current[1] + suffix + "\n") >= 0 else bpa.CLM_BPA_FAILURE


def clm_string_insert(identifier: int, index: int, inserted: str) -> int:
    """Insert text at a zero-based Unicode code-point index."""
    current = _block_text(identifier)
    if current is None or type(index) is not int or index < 0 or index > len(current[1]) or _encode_text(inserted) is None:
        return bpa.CLM_BPA_FAILURE
    return len(inserted) if clm_string_set(identifier, current[1][:index] + inserted + current[1][index:]) >= 0 else bpa.CLM_BPA_FAILURE


def clm_string_remove(identifier: int, index: int, length: int) -> int:
    """Remove a Unicode code-point range and return its length."""
    current = _block_text(identifier)
    if current is None or type(index) is not int or type(length) is not int or index < 0 or length < 0 or index > len(current[1]) or length > len(current[1]) - index:
        return bpa.CLM_BPA_FAILURE
    if length == 0:
        return 0
    return length if clm_string_set(identifier, current[1][:index] + current[1][index + length :]) >= 0 else bpa.CLM_BPA_FAILURE


def clm_string_contains(identifier: int, search: str) -> bool:
    """Return whether the string contains a valid UTF-8 search string."""
    current = _block_text(identifier)
    return current is not None and _encode_text(search) is not None and search in current[1]


def clm_string_length(identifier: int) -> int:
    """Return Unicode code-point length, or BPA_FAILURE for invalid IDs."""
    current = _block_text(identifier)
    return len(current[1]) if current is not None else bpa.CLM_BPA_FAILURE


def clm_string_replace(identifier: int, search: str, replacement: str) -> int:
    """Replace all nonoverlapping matches; an empty search is invalid."""
    current = _block_text(identifier)
    if current is None or not isinstance(search, str) or not search or _encode_text(search) is None or _encode_text(replacement) is None:
        return bpa.CLM_BPA_FAILURE
    count = current[1].count(search)
    if count == 0:
        return 0
    updated = current[1].replace(search, replacement)
    return count if clm_string_set(identifier, updated) >= 0 else bpa.CLM_BPA_FAILURE


def clm_string_get_bytes(identifier: int, buffer_size: int | None = None) -> tuple[int, bytes | None]:
    """Export preferred-encoding bytes as ``(byte_count, data)``.

    ``buffer_size=0`` queries the required length and returns ``None`` for data.
    """
    current = _block_text(identifier)
    if current is None:
        return bpa.CLM_BPA_FAILURE, None
    encoding_status, encoding = bpa.clm_bpa_get_encoding(identifier)
    if encoding_status != bpa.CLM_BPA_GET_SUCCESS or not _supported_encoding(encoding):
        return bpa.CLM_BPA_FAILURE, None
    codec = {
        CLM_STRING_ENCODING_UTF8: "utf-8",
        CLM_STRING_ENCODING_LATIN1: "latin-1",
        CLM_STRING_ENCODING_UTF16LE: "utf-16-le",
    }[encoding]
    try:
        data = current[1].encode(codec, "strict")
    except UnicodeEncodeError:
        return bpa.CLM_BPA_FAILURE, None
    if buffer_size is not None:
        if type(buffer_size) is not int or buffer_size < 0:
            return bpa.CLM_BPA_FAILURE, None
        if buffer_size == 0:
            return len(data), None
        if buffer_size < len(data):
            return bpa.CLM_BPA_FAILURE, None
    return len(data), data


def clm_string_from_bytes(identifier: int, encoding: int, buffer: bytes | bytearray | memoryview, buffer_size: int | None = None) -> int:
    """Decode exact external bytes into an existing or newly created BPA string."""
    if not _supported_encoding(encoding) or not isinstance(buffer, (bytes, bytearray, memoryview)):
        return bpa.CLM_BPA_FAILURE
    try:
        data = bytes(buffer)
    except (TypeError, ValueError):
        return bpa.CLM_BPA_FAILURE
    if buffer_size is not None:
        if type(buffer_size) is not int or buffer_size < 0 or buffer_size > len(data):
            return bpa.CLM_BPA_FAILURE
        data = data[:buffer_size]
    codec = {
        CLM_STRING_ENCODING_UTF8: "utf-8",
        CLM_STRING_ENCODING_LATIN1: "latin-1",
        CLM_STRING_ENCODING_UTF16LE: "utf-16-le",
    }[encoding]
    try:
        text = data.decode(codec, "strict")
    except UnicodeDecodeError:
        return bpa.CLM_BPA_FAILURE
    if "\0" in text or _encode_text(text) is None:
        return bpa.CLM_BPA_FAILURE
    existing = bpa._blocks.get(identifier) if type(identifier) is int else None
    created = existing is None
    if created:
        if not clm_string_new(identifier, text):
            return bpa.CLM_BPA_FAILURE
    else:
        if existing.type != bpa.CLM_BPA_TYPE_STRING or clm_string_set(identifier, text) < 0:
            return bpa.CLM_BPA_FAILURE
    return bpa.clm_bpa_set_encoding(identifier, encoding)


def clm_string_change_encoding(identifier: int, new_encoding: int) -> int:
    """Change a string's preferred export encoding if its text is representable."""
    current = _block_text(identifier)
    if current is None or not _supported_encoding(new_encoding):
        return bpa.CLM_BPA_FAILURE
    codec = {1: "utf-8", 2: "latin-1", 3: "utf-16-le"}[new_encoding]
    try:
        current[1].encode(codec, "strict")
    except UnicodeEncodeError:
        return bpa.CLM_BPA_FAILURE
    return bpa.clm_bpa_set_encoding(identifier, new_encoding)


def clm_string_slice(source_id: int, start: int, length: int, destination_id: int) -> int:
    """Copy a strict Unicode code-point range into a new independent string."""
    source = _block_text(source_id)
    if source is None or type(start) is not int or type(length) is not int or start < 0 or length < 0 or start > len(source[1]) or length > len(source[1]) - start or bpa.clm_bpa_is_existing(destination_id):
        return bpa.CLM_BPA_FAILURE
    if not clm_string_new(destination_id, source[1][start : start + length]):
        return bpa.CLM_BPA_FAILURE
    _, encoding = bpa.clm_bpa_get_encoding(source_id)
    if clm_string_change_encoding(destination_id, encoding) != bpa.CLM_BPA_SUCCESS:
        clm_string_delete(destination_id)
        return bpa.CLM_BPA_FAILURE
    return bpa.CLM_BPA_SUCCESS


def clm_string_slice_borrow(source_id: int, start: int, length: int) -> tuple[int, memoryview | None, int]:
    """Return a safe borrowed UTF-8 byte snapshot for a code-point range."""
    source = _block_text(source_id)
    if source is None or type(start) is not int or type(length) is not int or start < 0 or length < 0 or start > len(source[1]) or length > len(source[1]) - start:
        return bpa.CLM_BPA_FAILURE, None, 0
    data = source[1][start : start + length].encode("utf-8")
    return bpa.CLM_BPA_SUCCESS, memoryview(data), len(data)


def _array(identifier: int) -> object | None:
    array = bpa._arrays.get(identifier) if type(identifier) is int else None
    if array is None or array.type != bpa.CLM_BPA_TYPE_STRING or array.encoding != CLM_STRING_ENCODING_UTF8:
        return None
    return array


def _array_text(identifier: int, index: int) -> str | None:
    array = _array(identifier)
    if array is None or type(index) is not int or index < 0 or index >= array.count:
        return None
    status, data = bpa.clm_bpaa_get_string_raw(identifier, index)
    return _decode_utf8(data) if status >= 0 and data is not None else None


def _replace_array_text(identifier: int, index: int, text: str) -> bool:
    data = _encode_text(text)
    if data is None:
        return False
    return bpa.clm_bpaa_set_string_raw(identifier, index, data + b"\0", len(data) + 1) >= 0


def clm_string_array_new(identifier: int, reserved: int = 0) -> bool:
    """Create a BPA-backed string array using canonical UTF-8 storage."""
    return bpa.clm_bpaa_create(identifier, bpa.CLM_BPA_TYPE_STRING, reserved, CLM_STRING_ENCODING_UTF8) == bpa.CLM_BPA_SUCCESS


def clm_string_array_length_at(identifier: int, index: int) -> int:
    """Return the indexed string's Unicode code-point length."""
    text = _array_text(identifier, index)
    return len(text) if text is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_contains_at(identifier: int, index: int, search: str) -> bool:
    """Return whether the indexed string contains a valid search value."""
    text = _array_text(identifier, index)
    return text is not None and _encode_text(search) is not None and search in text


def clm_string_array_append_at(identifier: int, index: int, suffix: str) -> int:
    """Append text to one array string and return appended code-point count."""
    text = _array_text(identifier, index)
    if text is None or _encode_text(suffix) is None or not _replace_array_text(identifier, index, text + suffix):
        return bpa.CLM_BPA_FAILURE
    return len(suffix)


def clm_string_array_append_line_at(identifier: int, index: int, suffix: str) -> int:
    """Append text and LF to one array string."""
    text = _array_text(identifier, index)
    if text is None or _encode_text(suffix) is None or not _replace_array_text(identifier, index, text + suffix + "\n"):
        return bpa.CLM_BPA_FAILURE
    return len(suffix) + 1


def clm_string_array_insert_at(identifier: int, index: int, character_index: int, inserted: str) -> int:
    """Insert text at a Unicode code-point index in one array item."""
    text = _array_text(identifier, index)
    if text is None or type(character_index) is not int or character_index < 0 or character_index > len(text) or _encode_text(inserted) is None:
        return bpa.CLM_BPA_FAILURE
    return len(inserted) if _replace_array_text(identifier, index, text[:character_index] + inserted + text[character_index:]) else bpa.CLM_BPA_FAILURE


def clm_string_array_remove_at(identifier: int, index: int, character_index: int, length: int) -> int:
    """Remove a Unicode code-point range from one array item."""
    text = _array_text(identifier, index)
    if text is None or type(character_index) is not int or type(length) is not int or character_index < 0 or length < 0 or character_index > len(text) or length > len(text) - character_index:
        return bpa.CLM_BPA_FAILURE
    return length if _replace_array_text(identifier, index, text[:character_index] + text[character_index + length :]) else bpa.CLM_BPA_FAILURE


def clm_string_array_replace_at(identifier: int, index: int, search: str, replacement: str) -> int:
    """Replace all nonoverlapping matches in one array item."""
    text = _array_text(identifier, index)
    if text is None or not isinstance(search, str) or not search or _encode_text(search) is None or _encode_text(replacement) is None:
        return bpa.CLM_BPA_FAILURE
    count = text.count(search)
    return count if count == 0 or _replace_array_text(identifier, index, text.replace(search, replacement)) else bpa.CLM_BPA_FAILURE


def clm_string_array_delete(identifier: int) -> bool:
    """Delete a BPA-backed string array."""
    return _array(identifier) is not None and bpa.clm_bpaa_delete(identifier) == bpa.CLM_BPA_SUCCESS


def clm_string_array_count(identifier: int) -> int:
    """Return string-array count or BPA_FAILURE for invalid IDs."""
    status, count = bpa.clm_bpaa_get_count_to(identifier) if _array(identifier) is not None else (bpa.CLM_BPA_FAILURE, 0)
    return count if status == bpa.CLM_BPA_GET_SUCCESS else bpa.CLM_BPA_FAILURE


def clm_string_array_get(identifier: int, index: int, destination_string_id: int) -> int:
    """Copy an indexed string to an existing BPA string block."""
    if _array(identifier) is None:
        return bpa.CLM_BPA_FAILURE
    status = bpa.clm_bpaa_get_string(identifier, index, destination_string_id)
    if status == bpa.CLM_BPA_GET_SUCCESS:
        bpa.clm_bpa_set_encoding(destination_string_id, CLM_STRING_ENCODING_UTF8)
    return status


def clm_string_array_set(identifier: int, index: int, source_string_id: int) -> int:
    """Replace an array item with an existing BPA string block's value."""
    source = _block_text(source_string_id)
    return bpa.clm_bpaa_set_string(identifier, index, source_string_id) if source is not None and _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_insert(identifier: int, index: int, source_string_id: int) -> int:
    """Insert a copy of an existing BPA string block into an array."""
    source = _block_text(source_string_id)
    return bpa.clm_bpaa_insert_string(identifier, index, source_string_id) if source is not None and _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_append(identifier: int, source_string_id: int) -> int:
    """Append a copy of an existing BPA string block to an array."""
    source = _block_text(source_string_id)
    return bpa.clm_bpaa_append_string(identifier, source_string_id) if source is not None and _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_remove(identifier: int, index: int) -> int:
    """Remove an array string by index."""
    return bpa.clm_bpaa_remove(identifier, index) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_clear(identifier: int) -> int:
    """Clear all strings while retaining the BPAA array resource."""
    return bpa.clm_bpaa_clear(identifier) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_find(identifier: int, source_string_id: int) -> int:
    """Return the first exact string match or BPA_FAILURE."""
    source = _block_text(source_string_id)
    array = _array(identifier)
    if source is None or array is None:
        return bpa.CLM_BPA_FAILURE
    assert isinstance(array.data, list)
    encoded = source[1].encode("utf-8")
    try:
        return array.data.index(encoded)
    except ValueError:
        return bpa.CLM_BPA_FAILURE


def clm_string_array_sort(identifier: int) -> int:
    """Sort array strings lexicographically by their UTF-8 bytes."""
    array = _array(identifier)
    if array is None:
        return bpa.CLM_BPA_FAILURE
    assert isinstance(array.data, list)
    array.data.sort()
    array.index = 0
    bpa._reset_deck(array)
    return bpa.CLM_BPA_SUCCESS


def clm_string_array_shuffle(identifier: int) -> int:
    """Shuffle array strings and reset the draw deck."""
    array = _array(identifier)
    if array is None:
        return bpa.CLM_BPA_FAILURE
    assert isinstance(array.data, list)
    random.shuffle(array.data)
    array.index = 0
    bpa._reset_deck(array)
    return bpa.CLM_BPA_SUCCESS


def clm_string_array_move_first(identifier: int) -> int:
    """Move the string-array cursor to the first item."""
    return bpa.clm_bpaa_move_first(identifier) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_move_previous(identifier: int) -> int:
    """Move the string-array cursor one item backward."""
    return bpa.clm_bpaa_move_previous(identifier) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_move_next(identifier: int) -> int:
    """Move the string-array cursor one item forward."""
    return bpa.clm_bpaa_move_next(identifier) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_move_last(identifier: int) -> int:
    """Move the string-array cursor to the final item."""
    return bpa.clm_bpaa_move_last(identifier) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_move_to(identifier: int, index: int) -> int:
    """Move the string-array cursor to an item index."""
    return bpa.clm_bpaa_move_to(identifier, index) if _array(identifier) is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_get_current(identifier: int, destination_string_id: int) -> int:
    """Copy the current array string into an existing BPA string block."""
    array = _array(identifier)
    return clm_string_array_get(identifier, array.index, destination_string_id) if array is not None else bpa.CLM_BPA_FAILURE


def clm_string_array_draw_at(identifier: int, deck_index: int, destination_string_id: int) -> int:
    """Copy a selected active-deck item and remove it from that deck."""
    array = _array(identifier)
    if array is None or type(deck_index) is not int or deck_index < 0 or deck_index >= array.deck_size:
        return bpa.CLM_BPA_FAILURE
    if clm_string_array_get(identifier, deck_index, destination_string_id) != bpa.CLM_BPA_GET_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    assert isinstance(array.data, list)
    last = array.deck_size - 1
    array.data[deck_index], array.data[last] = array.data[last], array.data[deck_index]
    array.deck_size -= 1
    return bpa.CLM_BPA_GET_SUCCESS


def clm_string_array_draw(identifier: int, destination_string_id: int) -> int:
    """Draw the next item, reshuffling after the active deck is exhausted."""
    array = _array(identifier)
    if array is None or array.count == 0:
        return bpa.CLM_BPA_FAILURE
    if array.deck_size == 0 and clm_string_array_shuffle(identifier) != bpa.CLM_BPA_SUCCESS:
        return bpa.CLM_BPA_FAILURE
    return clm_string_array_draw_at(identifier, 0, destination_string_id)


__all__ = [name for name in globals() if name.startswith("CLM_STRING_") or name.startswith("clm_string_")]
