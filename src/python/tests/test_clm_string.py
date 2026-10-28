"""Unicode string and string-array checks using the CLM test harness."""

from clm import bpa
from clm import string as strings
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def _text(identifier: int) -> str | None:
    status, data = bpa.clm_bpa_get_string(identifier)
    if status < 0 or data is None:
        return None
    return data.decode("utf-8")


def run_tests() -> int:
    """Run string and string-array checks and return failed assertion count."""
    result = TestResult()
    clm_test_begin("CLM - String")

    main_id = 90001
    string = "Aé🐱"
    created = strings.clm_string_new(main_id, string)
    initial_length = strings.clm_string_length(main_id)
    inserted = strings.clm_string_insert(main_id, 1, "中")
    after_insert = _text(main_id)
    removed = strings.clm_string_remove(main_id, 1, 1)
    after_remove = _text(main_id)
    replaced = strings.clm_string_replace(main_id, "é", "中")
    after_replace = _text(main_id)
    removed_mark = strings.clm_string_replace(main_id, "!", "")
    clm_test_assert(
        result,
        created
        and initial_length == 3
        and bpa.clm_bpa_get_encoding(main_id) == (bpa.CLM_BPA_GET_SUCCESS, strings.CLM_STRING_ENCODING_UTF8)
        and inserted == 1
        and after_insert == "A中é🐱"
        and removed == 1
        and after_remove == string
        and replaced == 1
        and after_replace == "A中🐱"
        and removed_mark == 0
        and strings.clm_string_contains(main_id, "🐱")
        and strings.clm_string_replace(main_id, "", "x") == bpa.CLM_BPA_FAILURE,
        "string edits count Unicode code points and preserve UTF-8 content",
    )

    append_line = strings.clm_string_append_line(main_id, "!")
    invalid_index = strings.clm_string_remove(main_id, -1, 1)
    invalid_insert = strings.clm_string_insert(main_id, 100, "x")
    invalid_text = strings.clm_string_new(90008, "\ud800")
    clm_test_assert(
        result,
        append_line == 2
        and strings.clm_string_contains(main_id, "\n")
        and invalid_index == bpa.CLM_BPA_FAILURE
        and invalid_insert == bpa.CLM_BPA_FAILURE
        and not invalid_text
        and not bpa.clm_bpa_is_existing(90008),
        "append-line and invalid text/index validation behave consistently",
    )

    latin_id = 90100
    latin_copy_id = 90101
    unicode_id = 90102
    unicode_copy_id = 90103
    malformed_id = 90104
    latin_bytes = b"caf\xe9"
    utf16_bytes = b"A\x00\xe9\x00=\xd81\xdc"
    default_changed = strings.clm_string_set_default_encoding(strings.CLM_STRING_ENCODING_LATIN1)
    latin_created = strings.clm_string_new(latin_id, "café")
    latin_export = strings.clm_string_get_bytes(latin_id)
    latin_short = strings.clm_string_get_bytes(latin_id, 3)
    latin_query = strings.clm_string_get_bytes(latin_id, 0)
    latin_import = strings.clm_string_from_bytes(latin_copy_id, strings.CLM_STRING_ENCODING_LATIN1, latin_bytes)
    invalid_utf8 = strings.clm_string_from_bytes(malformed_id, strings.CLM_STRING_ENCODING_UTF8, b"\xc0\xaf")
    invalid_utf16 = strings.clm_string_from_bytes(malformed_id, strings.CLM_STRING_ENCODING_UTF16LE, b"\x00\xd8")
    clm_test_assert(
        result,
        default_changed == bpa.CLM_BPA_SUCCESS
        and latin_created
        and bpa.clm_bpa_get_encoding(latin_id) == (bpa.CLM_BPA_GET_SUCCESS, strings.CLM_STRING_ENCODING_LATIN1)
        and latin_export == (4, latin_bytes)
        and latin_short == (bpa.CLM_BPA_FAILURE, None)
        and latin_query == (4, None)
        and latin_import == bpa.CLM_BPA_SUCCESS
        and _text(latin_copy_id) == "café"
        and invalid_utf8 == bpa.CLM_BPA_FAILURE
        and invalid_utf16 == bpa.CLM_BPA_FAILURE
        and not bpa.clm_bpa_is_existing(malformed_id),
        "Latin-1 conversion and malformed UTF-8/UTF-16 rejection",
    )

    unicode_created = strings.clm_string_new(unicode_id, "Aé🐱")
    utf16_change = strings.clm_string_change_encoding(unicode_id, strings.CLM_STRING_ENCODING_UTF16LE)
    utf16_export = strings.clm_string_get_bytes(unicode_id)
    utf16_import = strings.clm_string_from_bytes(unicode_copy_id, strings.CLM_STRING_ENCODING_UTF16LE, utf16_bytes)
    failed_change = strings.clm_string_change_encoding(unicode_id, strings.CLM_STRING_ENCODING_LATIN1)
    borrowed_status, borrowed, borrowed_size = strings.clm_string_slice_borrow(unicode_id, 1, 2)
    slice_status = strings.clm_string_slice(unicode_id, 1, 2, 90105)
    empty_slice = strings.clm_string_slice(unicode_id, 3, 0, 90107)
    clm_test_assert(
        result,
        unicode_created
        and utf16_change == bpa.CLM_BPA_SUCCESS
        and utf16_export == (8, utf16_bytes)
        and utf16_import == bpa.CLM_BPA_SUCCESS
        and _text(unicode_copy_id) == "Aé🐱"
        and failed_change == bpa.CLM_BPA_FAILURE
        and bpa.clm_bpa_get_encoding(unicode_id)[1] == strings.CLM_STRING_ENCODING_UTF16LE
        and borrowed_status == bpa.CLM_BPA_SUCCESS
        and borrowed is not None
        and bytes(borrowed) == "é🐱".encode("utf-8")
        and borrowed_size == 6
        and slice_status == bpa.CLM_BPA_SUCCESS
        and _text(90105) == "é🐱"
        and empty_slice == bpa.CLM_BPA_SUCCESS
        and _text(90107) == "",
        "UTF-16LE export, representability, copied slices, and borrowed slices",
    )
    if borrowed is not None:
        borrowed.release()
    strings.clm_string_set_default_encoding(strings.CLM_STRING_ENCODING_UTF8)

    array_id = 90003
    pear_id, apple_id, banana_id, scratch_id = 90004, 90005, 90006, 90007
    strings.clm_string_new(pear_id, "pear")
    strings.clm_string_new(apple_id, "apple")
    strings.clm_string_new(banana_id, "banana")
    strings.clm_string_new(scratch_id, "")
    array_created = strings.clm_string_array_new(array_id, 0)
    append_values = (
        strings.clm_string_array_append(array_id, pear_id),
        strings.clm_string_array_append(array_id, apple_id),
        strings.clm_string_array_insert(array_id, 1, banana_id),
    )
    array_get = strings.clm_string_array_get(array_id, 1, scratch_id)
    clm_test_assert(
        result,
        array_created
        and append_values == (bpa.CLM_BPA_SUCCESS,) * 3
        and strings.clm_string_array_count(array_id) == 3
        and strings.clm_string_array_find(array_id, banana_id) == 1
        and array_get == bpa.CLM_BPA_GET_SUCCESS
        and _text(scratch_id) == "banana"
        and strings.clm_string_array_length_at(array_id, 1) == 6
        and strings.clm_string_array_contains_at(array_id, 1, "ana"),
        "string arrays create, insert, search, measure, and copy values",
    )

    mutations = (
        strings.clm_string_array_append_at(array_id, 1, "!"),
        strings.clm_string_array_append_line_at(array_id, 1, "."),
        strings.clm_string_array_insert_at(array_id, 1, 0, "!"),
        strings.clm_string_array_remove_at(array_id, 1, 0, 1),
        strings.clm_string_array_replace_at(array_id, 1, "a", "A"),
    )
    strings.clm_string_array_get(array_id, 1, scratch_id)
    clm_test_assert(
        result,
        mutations == (1, 2, 1, 1, 3)
        and _text(scratch_id) == "bAnAnA!.\n"
        and strings.clm_string_array_set(array_id, 0, apple_id) == bpa.CLM_BPA_SET_SUCCESS
        and strings.clm_string_array_set(array_id, 1, banana_id) == bpa.CLM_BPA_SET_SUCCESS
        and strings.clm_string_array_sort(array_id) == bpa.CLM_BPA_SUCCESS
        and strings.clm_string_array_move_last(array_id) == bpa.CLM_BPA_SUCCESS
        and strings.clm_string_array_get_current(array_id, scratch_id) == bpa.CLM_BPA_GET_SUCCESS
        and _text(scratch_id) == "banana",
        "indexed string mutations, set, sort, and cursor access",
    )

    draw_sequence = (
        strings.clm_string_array_move_first(array_id),
        strings.clm_string_array_move_next(array_id),
        strings.clm_string_array_move_previous(array_id),
        strings.clm_string_array_move_to(array_id, 2),
        strings.clm_string_array_remove(array_id, 1),
        strings.clm_string_array_shuffle(array_id),
        strings.clm_string_array_draw_at(array_id, 0, scratch_id),
        strings.clm_string_array_draw(array_id, scratch_id),
        strings.clm_string_array_draw(array_id, scratch_id),
    )
    clm_test_assert(
        result,
        draw_sequence[:6] == (bpa.CLM_BPA_SUCCESS,) * 6
        and draw_sequence[6:] == (bpa.CLM_BPA_GET_SUCCESS,) * 3
        and strings.clm_string_array_draw_at(array_id, 99, scratch_id) == bpa.CLM_BPA_FAILURE
        and strings.clm_string_array_clear(array_id) == bpa.CLM_BPA_SUCCESS
        and strings.clm_string_array_count(array_id) == 0
        and strings.clm_string_array_delete(array_id),
        "string-array cursor, draw deck, exhaustion refresh, and cleanup",
    )

    cleanup_ids = (main_id, latin_id, latin_copy_id, unicode_id, unicode_copy_id, 90105, 90107, pear_id, apple_id, banana_id, scratch_id)
    cleanup = all(strings.clm_string_delete(identifier) for identifier in cleanup_ids)
    clm_test_assert(result, cleanup, "delete string blocks without affecting other BPA types")
    return clm_test_summary(result)
