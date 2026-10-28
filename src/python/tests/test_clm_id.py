"""ID registry checks using the CLM test reporting style."""

from clm import bpa, id as ids
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def run_tests() -> int:
    """Run ID registry checks and return the number of failed assertions."""
    result = TestResult()
    clm_test_begin("CLM - ID")

    cleared = ids.clm_id_clear()
    host = ids.clm_id_register("host", 1001)
    alias = ids.clm_id_map("mapped-name", 1002)
    same_value = ids.clm_id_register("default_host", 1001)
    clm_test_assert(
        result,
        ids.clm_id_is_case_sensitive()
        and cleared == bpa.CLM_BPA_SUCCESS
        and host == bpa.CLM_BPA_SET_SUCCESS
        and alias == bpa.CLM_BPA_SET_SUCCESS
        and same_value == bpa.CLM_BPA_SET_SUCCESS
        and ids.clm_id_get("host") == 1001
        and ids.clm_id_get("mapped-name") == 1002,
        "register names, map aliases, and allow multiple names per ID",
    )

    try_status, try_value = ids.clm_id_try_get("host")
    missing_status, missing_value = ids.clm_id_try_get("missing")
    clm_test_assert(
        result,
        ids.clm_id_register("host", 1002) == bpa.CLM_BPA_FAILURE
        and ids.clm_id_register(b"\xC0\xAF", 1003) == bpa.CLM_BPA_FAILURE
        and ids.clm_id_get("HOST") == ids.CLM_ID_NOT_FOUND
        and (try_status, try_value) == (bpa.CLM_BPA_GET_SUCCESS, 1001)
        and (missing_status, missing_value) == (bpa.CLM_BPA_FAILURE, None),
        "reject duplicate or malformed UTF-8 names and provide status lookup",
    )

    update_existing = ids.clm_id_set("host", 2001)
    insert_with_set = ids.clm_id_set("new_name", 3001)
    remove_name = ids.clm_id_remove("new_name")
    clm_test_assert(
        result,
        update_existing == bpa.CLM_BPA_SET_SUCCESS
        and ids.clm_id_get("host") == 2001
        and ids.clm_id_get("default_host") == 1001
        and insert_with_set == bpa.CLM_BPA_SET_SUCCESS
        and ids.clm_id_remove("missing") == bpa.CLM_BPA_FAILURE
        and remove_name == bpa.CLM_BPA_SUCCESS
        and ids.clm_id_get("new_name") == ids.CLM_ID_NOT_FOUND,
        "update one mapping, insert through set, and remove by name",
    )

    registered_case_alias = ids.clm_id_register("Host", 4001)
    rejected_case_mode = ids.clm_id_set_case_sensitive(False)
    preserved_policy = ids.clm_id_is_case_sensitive()
    removed_case_alias = ids.clm_id_remove("Host")
    enabled_case_mode = ids.clm_id_set_case_sensitive(False)
    insensitive_lookup = ids.clm_id_get("HOST")
    update_spelling = ids.clm_id_set("HOST", 2002)
    non_ascii = (
        ids.clm_id_register("Ä", 6001),
        ids.clm_id_register("ä", 6002),
    )
    clm_test_assert(
        result,
        registered_case_alias == bpa.CLM_BPA_SET_SUCCESS
        and rejected_case_mode == bpa.CLM_BPA_FAILURE
        and preserved_policy
        and removed_case_alias == bpa.CLM_BPA_SUCCESS
        and enabled_case_mode == bpa.CLM_BPA_SUCCESS
        and not ids.clm_id_is_case_sensitive()
        and insensitive_lookup == 2001
        and update_spelling == bpa.CLM_BPA_SET_SUCCESS
        and ids.clm_id_get("host") == 2002
        and non_ascii == (bpa.CLM_BPA_SET_SUCCESS, bpa.CLM_BPA_SET_SUCCESS)
        and ids.clm_id_get("Ä") == 6001
        and ids.clm_id_get("ä") == 6002,
        "ASCII-insensitive mode rejects collisions and leaves non-ASCII case distinct",
    )

    alias_id = ids.clm_id_register("alias", 2002)
    unmap_id = ids.clm_id_unmap(2002)
    unmap_name = ids.clm_id_unmap_name("default_host")
    clm_test_assert(
        result,
        alias_id == bpa.CLM_BPA_SET_SUCCESS
        and unmap_id == bpa.CLM_BPA_SUCCESS
        and ids.clm_id_get("host") == ids.CLM_ID_NOT_FOUND
        and ids.clm_id_get("alias") == ids.CLM_ID_NOT_FOUND
        and unmap_name == bpa.CLM_BPA_SUCCESS
        and ids.clm_id_get("default_host") == ids.CLM_ID_NOT_FOUND,
        "unmap removes one or all names associated with an ID",
    )

    ids.clm_id_gen_reset()
    generated = [ids.clm_id_gen_next() for _ in range(3)]
    ids.clm_id_gen_reset()
    reset_value = ids.clm_id_gen_next()
    clm_test_assert(
        result,
        generated == [0, 1, 2] and reset_value == 0,
        "generate sequential IDs and reset the generator",
    )

    clear_status = ids.clm_id_clear()
    mode_preserved = not ids.clm_id_is_case_sensitive()
    reuse_status = ids.clm_id_register("after-clear", 5001)
    lookup_after_clear = ids.clm_id_get("AFTER-CLEAR")
    shutdown_status = ids.clm_id_shutdown()
    lookup_after_shutdown = ids.clm_id_get("after-shutdown")
    recreate_status = ids.clm_id_register("after-shutdown", 5002)
    recreate_lookup = ids.clm_id_get("after-shutdown")
    shutdown_again = ids.clm_id_shutdown()
    restore_mode = ids.clm_id_set_case_sensitive(True)
    clm_test_assert(
        result,
        clear_status == bpa.CLM_BPA_SUCCESS
        and mode_preserved
        and reuse_status == bpa.CLM_BPA_SET_SUCCESS
        and lookup_after_clear == 5001
        and shutdown_status == bpa.CLM_BPA_SUCCESS
        and lookup_after_shutdown == ids.CLM_ID_NOT_FOUND
        and recreate_status == bpa.CLM_BPA_SET_SUCCESS
        and recreate_lookup == 5002
        and shutdown_again == bpa.CLM_BPA_SUCCESS
        and restore_mode == bpa.CLM_BPA_SUCCESS,
        "clear preserves policy and shutdown releases storage for lazy recreation",
    )

    return clm_test_summary(result)
