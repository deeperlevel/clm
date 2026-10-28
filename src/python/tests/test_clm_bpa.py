"""Block and block-array allocation checks using the CLM test harness."""

import struct

from clm import bpa
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def _int_bytes(value: int) -> bytes:
    return struct.pack("=i", value)


def _read_int(value: bytes | None) -> int | None:
    return struct.unpack("=i", value)[0] if value is not None and len(value) == 4 else None


def run_tests() -> int:
    """Run BPA/BPAA checks and return the number of failed assertions."""
    failed = 0

    result = TestResult()
    clm_test_begin("CLM - Block Pool Allocation")
    clm_test_assert(
        result,
        bpa.clm_bpa_type_size(bpa.CLM_BPA_TYPE_INT) == 4
        and bpa.clm_bpa_type_size(bpa.CLM_BPA_TYPE_UUID) == 16
        and bpa.clm_bpa_type_size(bpa.CLM_BPA_TYPE_STRING) == 0
        and bpa.clm_bpa_hash_id(0x100000001) != bpa.clm_bpa_hash_id(1),
        "built-in sizes and 64-bit identifier hashing are available",
    )

    block_id = 61001
    copy_id = 61002
    struct_id = 61003
    string_id = 61004
    block_status = bpa.clm_bpa_create(block_id, bpa.CLM_BPA_TYPE_INT)
    duplicate_status = bpa.clm_bpa_create(block_id, bpa.CLM_BPA_TYPE_INT)
    set_status = bpa.clm_bpa_set(block_id, _int_bytes(123))
    get_status, value = bpa.clm_bpa_get(block_id, 4)
    bpa.clm_bpa_create(copy_id, bpa.CLM_BPA_TYPE_INT)
    copy_status = bpa.clm_bpa_copy_block(block_id, copy_id)
    copy_get_status, copy_value = bpa.clm_bpa_get(copy_id, 4)
    clm_test_assert(
        result,
        block_status == bpa.CLM_BPA_SUCCESS
        and duplicate_status == bpa.CLM_BPA_FAILURE
        and set_status == bpa.CLM_BPA_SET_SUCCESS
        and get_status == bpa.CLM_BPA_GET_SUCCESS
        and _read_int(value) == 123
        and copy_status == bpa.CLM_BPA_SUCCESS
        and copy_get_status == bpa.CLM_BPA_GET_SUCCESS
        and copy_value == value,
        "fixed-size blocks create, set, get, and copy bytes",
    )

    reference_status = bpa.clm_bpa_ref(block_id)
    first_release = bpa.clm_bpa_unref(block_id)
    remains = bpa.clm_bpa_is_existing(block_id)
    final_release = bpa.clm_bpa_unref(block_id)
    clm_test_assert(
        result,
        reference_status == bpa.CLM_BPA_SUCCESS
        and first_release == bpa.CLM_BPA_SUCCESS
        and remains
        and final_release == bpa.CLM_BPA_SUCCESS
        and not bpa.clm_bpa_is_existing(block_id),
        "block references retain resources until the final release",
    )

    clm_test_assert(
        result,
        bpa.clm_bpa_create_sized(struct_id, bpa.CLM_BPA_TYPE_STRUCT, 8) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpa_set(struct_id, struct.pack("=ii", 12, 34)) == bpa.CLM_BPA_SET_SUCCESS
        and bpa.clm_bpa_get(struct_id, 8) == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=ii", 12, 34))
        and bpa.clm_bpa_create(61005, bpa.CLM_BPA_TYPE_STRUCT) == bpa.CLM_BPA_FAILURE,
        "custom structs require an explicit size and preserve raw bytes",
    )

    clm_test_assert(
        result,
        bpa.clm_bpa_create(string_id, bpa.CLM_BPA_TYPE_STRING) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpa_set_string(string_id, b"hello\0ignored", 13) == 5
        and bpa.clm_bpa_get_string(string_id) == (5, b"hello")
        and bpa.clm_bpa_replace(string_id, b"world!\0", 7) == 6
        and bpa.clm_bpa_get_string(string_id, 6) == (bpa.CLM_BPA_FAILURE, None),
        "string blocks copy bounded NUL-terminated data and check output capacity",
    )
    clm_test_assert(
        result,
        bpa.clm_bpa_delete(copy_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpa_delete(struct_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpa_delete(string_id) == bpa.CLM_BPA_SUCCESS,
        "blocks release cleanly",
    )
    failed += clm_test_summary(result)

    result = TestResult()
    clm_test_begin("CLM - Block Arrays")
    array_id = 62001
    create_status = bpa.clm_bpaa_create(array_id, bpa.CLM_BPA_TYPE_INT, 1)
    append_statuses = [
        bpa.clm_bpaa_append_raw(array_id, _int_bytes(value))
        for value in (4, 1, 3, 2)
    ]
    count_status = bpa.clm_bpaa_get_count_to(array_id)
    capacity_status = bpa.clm_bpaa_get_capacity_to(array_id)
    clm_test_assert(
        result,
        create_status == bpa.CLM_BPA_SUCCESS
        and all(status == bpa.CLM_BPA_SUCCESS for status in append_statuses)
        and count_status == (bpa.CLM_BPA_GET_SUCCESS, 4)
        and capacity_status[0] == bpa.CLM_BPA_GET_SUCCESS
        and capacity_status[1] >= 4
        and bpa.clm_bpaa_get_first_raw(array_id, 4) == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(4))
        and bpa.clm_bpaa_get_last_raw(array_id, 4) == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(2)),
        "arrays append fixed-size items and report count and capacity",
    )

    edit_status = bpa.clm_bpaa_insert_raw(array_id, 1, _int_bytes(9))
    set_status = bpa.clm_bpaa_set_raw(array_id, 1, _int_bytes(8))
    move_status = bpa.clm_bpaa_move_to(array_id, 2)
    cursor = bpa.clm_bpaa_get_current_index_to(array_id)
    current = bpa.clm_bpaa_get_current_raw(array_id, 4)
    replaced = bpa.clm_bpaa_get_raw(array_id, 1, 4)
    remove_status = bpa.clm_bpaa_remove(array_id, 1)
    clm_test_assert(
        result,
        edit_status == bpa.CLM_BPA_SUCCESS
        and set_status == bpa.CLM_BPA_SET_SUCCESS
        and move_status == bpa.CLM_BPA_SUCCESS
        and cursor == (bpa.CLM_BPA_GET_SUCCESS, 2)
        and current == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(1))
        and replaced == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(8))
        and remove_status == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_move_last(array_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_move_next(array_id) == bpa.CLM_BPA_FAILURE,
        "array insert, replace, cursor movement, and removal preserve state",
    )

    sort_status = bpa.clm_bpaa_sort_with(
        array_id,
        lambda left, right: _read_int(left) - _read_int(right),
    )
    sorted_values = [
        _read_int(bpa.clm_bpaa_get_raw(array_id, index, 4)[1])
        for index in range(4)
    ]
    reverse_status = bpa.clm_bpaa_reverse(array_id)
    reverse_first = bpa.clm_bpaa_get_first_raw(array_id, 4)
    clm_test_assert(
        result,
        sort_status == bpa.CLM_BPA_SUCCESS
        and sorted_values == [1, 2, 3, 4]
        and reverse_status == bpa.CLM_BPA_SUCCESS
        and reverse_first == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(4))
        and bpa.clm_bpaa_shift_left(array_id, 1, 1) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_swap(array_id, 0, 1) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_copy(array_id, 0, 1) == bpa.CLM_BPA_SUCCESS,
        "arrays sort, reverse, rotate, swap, and copy fixed-width elements",
    )

    item_count = bpa.clm_bpaa_get_count_to(array_id)[1]
    capacity = bpa.clm_bpaa_get_capacity_to(array_id)[1]
    clm_test_assert(
        result,
        bpa.clm_bpaa_shrink(array_id, capacity) == bpa.CLM_BPA_FAILURE
        and bpa.clm_bpaa_shrink(array_id, item_count) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_seal(array_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_is_sealed_to(array_id) == (bpa.CLM_BPA_GET_SUCCESS, True)
        and bpa.clm_bpaa_extend(array_id, 1) == bpa.CLM_BPA_FAILURE
        and bpa.clm_bpaa_unseal(array_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_clear(array_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpaa_get_count_to(array_id) == (bpa.CLM_BPA_GET_SUCCESS, 0),
        "capacity shrinking, sealing, unsealing, and clearing follow array rules",
    )
    bpa.clm_bpaa_delete(array_id)
    failed += clm_test_summary(result)

    result = TestResult()
    clm_test_begin("CLM - Strings, Transfers, and Views")
    source_id = 63001
    text_block_id = 63002
    text_array_id = 63003
    slice_id = 63004
    value_array_id = 63005
    value_block_id = 63006
    count_block_id = 63007
    bpa.clm_bpa_create(text_block_id, bpa.CLM_BPA_TYPE_STRING)
    bpa.clm_bpa_set_string(text_block_id, "hello")
    bpa.clm_bpaa_create(text_array_id, bpa.CLM_BPA_TYPE_STRING, 1, 7)
    append_text = bpa.clm_bpaa_append_string(text_array_id, text_block_id)
    bpa.clm_bpa_set_string(text_block_id, "changed")
    text_status = bpa.clm_bpaa_get_string_raw(text_array_id, 0)
    copy_to_block = bpa.clm_bpaa_get_string(text_array_id, 0, text_block_id)
    copied_text = bpa.clm_bpa_get_string(text_block_id)
    clm_test_assert(
        result,
        append_text == bpa.CLM_BPA_SUCCESS
        and text_status == (5, b"hello")
        and copy_to_block == bpa.CLM_BPA_GET_SUCCESS
        and copied_text == (5, b"hello"),
        "string arrays own copies and transfer strings through BPA blocks",
    )

    bpa.clm_bpaa_create(value_array_id, bpa.CLM_BPA_TYPE_INT, 0)
    bpa.clm_bpa_create(value_block_id, bpa.CLM_BPA_TYPE_INT)
    bpa.clm_bpa_create(count_block_id, bpa.CLM_BPA_TYPE_UNSIGNED_INT)
    bpa.clm_bpaa_append_raw(value_array_id, _int_bytes(21))
    bpa.clm_bpaa_append_raw(value_array_id, _int_bytes(34))
    transfer_get = bpa.clm_bpaa_get(value_array_id, 0, value_block_id, 4)
    block_value = bpa.clm_bpa_get(value_block_id, 4)
    bpa.clm_bpa_set(value_block_id, _int_bytes(55))
    transfer_set = bpa.clm_bpaa_set(value_array_id, 1, value_block_id, 4)
    metadata_status = bpa.clm_bpaa_get_count(value_array_id, count_block_id)
    metadata_value = bpa.clm_bpa_get(count_block_id, 4)
    clm_test_assert(
        result,
        transfer_get == bpa.CLM_BPA_GET_SUCCESS
        and block_value == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(21))
        and transfer_set == bpa.CLM_BPA_SET_SUCCESS
        and bpa.clm_bpaa_get_raw(value_array_id, 1, 4) == (bpa.CLM_BPA_GET_SUCCESS, _int_bytes(55))
        and metadata_status == bpa.CLM_BPA_GET_SUCCESS
        and metadata_value == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=I", 2)),
        "array values and metadata transfer through compatible BPA blocks",
    )

    draw_array_id = 63008
    bpa.clm_bpaa_create(draw_array_id, bpa.CLM_BPA_TYPE_INT, 0)
    for value in (10, 20, 30):
        bpa.clm_bpaa_append_raw(draw_array_id, _int_bytes(value))
    draw_at_status, first_draw = bpa.clm_bpaa_draw_at_raw(draw_array_id, 1, 4)
    rest_draws = [bpa.clm_bpaa_draw_raw(draw_array_id, 4) for _ in range(2)]
    drawn = {_read_int(first_draw), *(_read_int(value) for status, value in rest_draws if status == bpa.CLM_BPA_GET_SUCCESS)}
    clm_test_assert(
        result,
        draw_at_status == bpa.CLM_BPA_GET_SUCCESS
        and len(drawn) == 3
        and drawn == {10, 20, 30}
        and bpa.clm_bpaa_draw_raw(draw_array_id, 4)[0] == bpa.CLM_BPA_GET_SUCCESS,
        "draw operations exhaust and automatically reshuffle the active deck",
    )

    bpa.clm_bpaa_create(source_id, bpa.CLM_BPA_TYPE_INT, 0)
    for value in (4, 1, 3, 2):
        bpa.clm_bpaa_append_raw(source_id, _int_bytes(value))
    slice_status = bpa.clm_bpaa_slice(source_id, 1, 2, slice_id)
    slice_values = [
        _read_int(bpa.clm_bpaa_get_raw(slice_id, index, 4)[1])
        for index in range(2)
    ]
    copy_slice = bpa.clm_bpaa_slice_to(source_id, 1, 2)
    borrow_status, view, view_size = bpa.clm_bpaa_slice_borrow(source_id, 1, 2)
    clm_test_assert(
        result,
        slice_status == bpa.CLM_BPA_SUCCESS
        and slice_values == [1, 3]
        and copy_slice == (bpa.CLM_BPA_SUCCESS, struct.pack("=ii", 1, 3))
        and borrow_status == bpa.CLM_BPA_SUCCESS
        and view is not None
        and bytes(view) == struct.pack("=ii", 1, 3)
        and view_size == 8,
        "array slices copy independently or expose a safe borrowed view",
    )
    if view is not None:
        view.release()

    handle_status, handle = bpa.clm_bpaa_advanced_borrow(source_id)
    direct_data = bpa.clm_bpaa_advanced_data(handle)
    blocked_resize = bpa.clm_bpaa_extend(source_id, 1)
    if direct_data is not None:
        direct_data.release()
    release_status = bpa.clm_bpaa_advanced_release(handle)
    clm_test_assert(
        result,
        handle_status == bpa.CLM_BPA_SUCCESS
        and direct_data is not None
        and handle is not None
        and handle.count == 4
        and handle.block_size == 4
        and blocked_resize == bpa.CLM_BPA_FAILURE
        and bpa.clm_bpaa_unref(source_id) == bpa.CLM_BPA_SUCCESS
        and release_status == bpa.CLM_BPA_SUCCESS
        and not bpa.clm_bpaa_is_existing(source_id),
        "advanced handles retain arrays and memoryviews guard resizing",
    )

    bpa.clm_bpaa_delete(slice_id)
    bpa.clm_bpaa_delete(text_array_id)
    bpa.clm_bpa_delete(source_id)
    bpa.clm_bpa_delete(text_block_id)
    bpa.clm_bpaa_delete(value_array_id)
    bpa.clm_bpa_delete(value_block_id)
    bpa.clm_bpa_delete(count_block_id)
    bpa.clm_bpaa_delete(draw_array_id)
    failed += clm_test_summary(result)

    failed += int(bpa.clm_bpa_shutdown() != bpa.CLM_BPA_SUCCESS)
    return failed
