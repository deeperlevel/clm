"""Dictionary checks using BPA-backed values and the CLM test harness."""

import struct

from clm import bpa, dictionary
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def run_tests() -> int:
    """Run dictionary checks and return the number of failed assertions."""
    result = TestResult()
    clm_test_begin("CLM - Dictionary")

    dictionary_id = 71000
    source_id = 71001
    destination_id = 71002
    string_source_id = 71004
    string_dictionary_id = 71010
    struct_dictionary_id = 71020
    collision_dictionary_id = 71030
    ordered_dictionary_id = 71050

    create_status = dictionary.clm_dictionary_new(dictionary_id, bpa.CLM_BPA_TYPE_INT)
    invalid_status = dictionary.clm_dictionary_new(71003, bpa.CLM_BPA_TYPE_STRING, 1)
    unrelated_status = bpa.clm_bpa_create_sized(71040, bpa.CLM_BPA_TYPE_STRUCT, 48)
    unrelated_count = dictionary.clm_dictionary_count(71040)
    unrelated_delete = dictionary.clm_dictionary_delete(71040)
    if unrelated_status == bpa.CLM_BPA_SUCCESS:
        bpa.clm_bpa_delete(71040)
    clm_test_assert(
        result,
        create_status == bpa.CLM_BPA_SUCCESS
        and dictionary.clm_dictionary_count(dictionary_id) == 0
        and dictionary.clm_dictionary_new(dictionary_id, bpa.CLM_BPA_TYPE_INT) == bpa.CLM_BPA_FAILURE
        and invalid_status == bpa.CLM_BPA_FAILURE
        and unrelated_status == bpa.CLM_BPA_SUCCESS
        and unrelated_count == bpa.CLM_BPA_FAILURE
        and unrelated_delete == bpa.CLM_BPA_FAILURE,
        "create valid dictionaries and reject invalid or unrelated BPA blocks",
    )

    inserted = all(
        dictionary.clm_dictionary_set_from(dictionary_id, 1000 + index, struct.pack("=i", index * 10))
        == bpa.CLM_BPA_SET_SUCCESS
        for index in range(40)
    )
    replacement = dictionary.clm_dictionary_set_from(dictionary_id, 1005, struct.pack("=i", 9876))
    replaced_value = dictionary.clm_dictionary_get_to(dictionary_id, 1005, 4)
    clm_test_assert(
        result,
        inserted
        and dictionary.clm_dictionary_count(dictionary_id) == 40
        and replacement == bpa.CLM_BPA_SET_SUCCESS
        and replaced_value == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 9876))
        and dictionary.clm_dictionary_count(dictionary_id) == 40,
        "insert entries through BPA arrays and replace values without changing count",
    )

    bpa.clm_bpa_create(source_id, bpa.CLM_BPA_TYPE_INT)
    bpa.clm_bpa_set(source_id, struct.pack("=i", 9876))
    set_from_block = dictionary.clm_dictionary_set(dictionary_id, 0, source_id)
    bpa.clm_bpa_create(destination_id, bpa.CLM_BPA_TYPE_INT)
    get_to_block = dictionary.clm_dictionary_get(dictionary_id, 0, destination_id)
    copied_block = bpa.clm_bpa_get(destination_id, 4)
    clm_test_assert(
        result,
        set_from_block == bpa.CLM_BPA_SET_SUCCESS
        and get_to_block == bpa.CLM_BPA_GET_SUCCESS
        and copied_block == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 9876))
        and dictionary.clm_dictionary_count(dictionary_id) == 41,
        "copy values to and from compatible BPA blocks",
    )

    removed = dictionary.clm_dictionary_remove(dictionary_id, 1005)
    missing = dictionary.clm_dictionary_get_to(dictionary_id, 1005, 4)
    reinserted = dictionary.clm_dictionary_add_from(dictionary_id, 99999, struct.pack("=i", 12345))
    clm_test_assert(
        result,
        removed == bpa.CLM_BPA_SUCCESS
        and not dictionary.clm_dictionary_contains(dictionary_id, 1005)
        and missing == (bpa.CLM_BPA_FAILURE, None)
        and reinserted == bpa.CLM_BPA_SET_SUCCESS
        and dictionary.clm_dictionary_count(dictionary_id) == 41
        and dictionary.clm_dictionary_update_from(dictionary_id, 200000, struct.pack("=i", 1)) == bpa.CLM_BPA_FAILURE
        and dictionary.clm_dictionary_add_from(dictionary_id, 1000, struct.pack("=i", 1)) == bpa.CLM_BPA_FAILURE,
        "remove and reinsert keys while add/update enforce their distinct contracts",
    )

    expected_keys = [key for key in range(1000, 1040) if key != 1005] + [0, 99999]
    keys_status, keys = dictionary.clm_dictionary_keys_to(dictionary_id, 41)
    iteration_status = dictionary.clm_dictionary_iterate_reset(dictionary_id)
    iterated: list[int] = []
    while True:
        status, key = dictionary.clm_dictionary_iterate_next(dictionary_id)
        if status != bpa.CLM_BPA_GET_SUCCESS:
            break
        assert key is not None
        iterated.append(key)
    clm_test_assert(
        result,
        keys_status == bpa.CLM_BPA_GET_SUCCESS
        and keys == expected_keys
        and iteration_status == bpa.CLM_BPA_SUCCESS
        and iterated == expected_keys
        and dictionary.clm_dictionary_iterate_next(dictionary_id)[0] == bpa.CLM_BPA_FAILURE,
        "copy and iterate all keys in insertion order",
    )

    bpa.clm_bpa_create(string_source_id, bpa.CLM_BPA_TYPE_STRING)
    bpa.clm_bpa_set_string(string_source_id, "dictionary value")
    string_new = dictionary.clm_dictionary_new(string_dictionary_id, bpa.CLM_BPA_TYPE_STRING)
    string_set = dictionary.clm_dictionary_set(string_dictionary_id, 5, string_source_id)
    string_get = dictionary.clm_dictionary_get_to(string_dictionary_id, 5, 32)
    empty_set = dictionary.clm_dictionary_set_from(string_dictionary_id, 6, b"\0", 1)
    empty_get = dictionary.clm_dictionary_get_to(string_dictionary_id, 6, 1)
    clm_test_assert(
        result,
        string_new == bpa.CLM_BPA_SUCCESS
        and string_set == bpa.CLM_BPA_SET_SUCCESS
        and string_get == (bpa.CLM_BPA_GET_SUCCESS, b"dictionary value\0")
        and empty_set == bpa.CLM_BPA_SET_SUCCESS
        and empty_get == (bpa.CLM_BPA_GET_SUCCESS, b"\0"),
        "store BPA-backed string values including empty strings",
    )

    struct_value = struct.pack("=ii", 11, 22)
    struct_status = dictionary.clm_dictionary_new(struct_dictionary_id, bpa.CLM_BPA_TYPE_STRUCT, len(struct_value))
    struct_set = dictionary.clm_dictionary_set_from(struct_dictionary_id, 7, struct_value)
    struct_get = dictionary.clm_dictionary_get_to(struct_dictionary_id, 7, len(struct_value))
    clm_test_assert(
        result,
        struct_status == bpa.CLM_BPA_SUCCESS
        and struct_set == bpa.CLM_BPA_SET_SUCCESS
        and struct_get == (bpa.CLM_BPA_GET_SUCCESS, struct_value),
        "store and retrieve fixed-size struct values",
    )

    bucket = bpa.clm_bpa_hash_id(0) & 15
    collision_keys = [key for key in range(10000) if bpa.clm_bpa_hash_id(key) & 15 == bucket][:3]
    collision_status = dictionary.clm_dictionary_new(collision_dictionary_id, bpa.CLM_BPA_TYPE_INT)
    collision_set = all(
        dictionary.clm_dictionary_set_from(collision_dictionary_id, key, struct.pack("=i", index + 1))
        == bpa.CLM_BPA_SET_SUCCESS
        for index, key in enumerate(collision_keys)
    )
    collision_removed = dictionary.clm_dictionary_remove(collision_dictionary_id, collision_keys[0]) if collision_keys else bpa.CLM_BPA_FAILURE
    collision_value = dictionary.clm_dictionary_get_to(collision_dictionary_id, collision_keys[-1], 4) if collision_keys else (bpa.CLM_BPA_FAILURE, None)
    clm_test_assert(
        result,
        collision_status == bpa.CLM_BPA_SUCCESS
        and len(collision_keys) == 3
        and collision_set
        and collision_removed == bpa.CLM_BPA_SUCCESS
        and collision_value == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 3)),
        "colliding BPA-hashed keys remain accessible after a removal",
    )

    ordered = dictionary.clm_dictionary_new(ordered_dictionary_id, bpa.CLM_BPA_TYPE_INT)
    ordered_keys = [900, 2, 77, 0]
    ordered_add = all(
        dictionary.clm_dictionary_add_from(ordered_dictionary_id, key, struct.pack("=i", 100 + index))
        == bpa.CLM_BPA_SET_SUCCESS
        for index, key in enumerate(ordered_keys)
    )
    duplicate_add = dictionary.clm_dictionary_add_from(ordered_dictionary_id, 2, struct.pack("=i", 0))
    updated = dictionary.clm_dictionary_update_from(ordered_dictionary_id, 2, struct.pack("=i", 9876))
    remove_readd = (
        dictionary.clm_dictionary_remove(ordered_dictionary_id, 2),
        dictionary.clm_dictionary_add_from(ordered_dictionary_id, 2, struct.pack("=i", 9876)),
    )
    readded_keys = dictionary.clm_dictionary_keys_to(ordered_dictionary_id)
    clm_test_assert(
        result,
        ordered == bpa.CLM_BPA_SUCCESS
        and ordered_add
        and duplicate_add == bpa.CLM_BPA_FAILURE
        and updated == bpa.CLM_BPA_SET_SUCCESS
        and remove_readd == (bpa.CLM_BPA_SUCCESS, bpa.CLM_BPA_SET_SUCCESS)
        and readded_keys == (bpa.CLM_BPA_GET_SUCCESS, [900, 77, 0, 2])
        and dictionary.clm_dictionary_clear(ordered_dictionary_id) == bpa.CLM_BPA_SUCCESS
        and dictionary.clm_dictionary_count(ordered_dictionary_id) == 0,
        "updates preserve key order while remove/readd appends and clear retains the resource",
    )

    cleanup = all(
        dictionary.clm_dictionary_delete(identifier) == bpa.CLM_BPA_SUCCESS
        for identifier in (ordered_dictionary_id, collision_dictionary_id, struct_dictionary_id, string_dictionary_id, dictionary_id)
    )
    cleanup = cleanup and bpa.clm_bpa_delete(source_id) == bpa.CLM_BPA_SUCCESS
    cleanup = cleanup and bpa.clm_bpa_delete(destination_id) == bpa.CLM_BPA_SUCCESS
    cleanup = cleanup and bpa.clm_bpa_delete(string_source_id) == bpa.CLM_BPA_SUCCESS
    clm_test_assert(result, cleanup, "delete dictionaries, backing arrays, and transfer blocks")

    return clm_test_summary(result)
