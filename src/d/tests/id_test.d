module tests.id_test;

import clm.bpa;
import clm.id;
import tests.test;

int clmIdTests()
{
    ClmTestResult result;
    clmTestBegin("CLM - ID");

    ulong value = CLM_ID_NOT_FOUND;
    clmAssert(result, clm_id_is_case_sensitive(), "name matching is case-sensitive by default");
    clmAssert(result, clm_id_clear() == CLM_BPA_SUCCESS, "initialize and clear ID mappings");
    clmAssert(result, clm_id_register("host", 1001) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("host") == 1001, "register and resolve a name");
    clmAssert(result, clm_id_map("mapped-name", 1002) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("mapped-name") == 1002, "map an alias");
    clmAssert(result, clm_id_register("default_host", 1001) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("default_host") == 1001, "allow multiple names to share one ID");
    clmAssert(result, clm_id_register("host", 1002) == CLM_BPA_FAILURE,
        "reject duplicate name without changing its mapping");
    clmAssert(result, clm_id_register("\xC0\xAF", 1003) == CLM_BPA_FAILURE,
        "reject malformed UTF-8 names");
    clmAssert(result, clm_id_get("HOST") == CLM_ID_NOT_FOUND,
        "distinguish names by case in default mode");
    clmAssert(result, clm_id_try_get("host", value) == CLM_BPA_GET_SUCCESS &&
        value == 1001 && clm_id_try_get("missing", value) == CLM_BPA_FAILURE,
        "provide status-based lookup");

    clmAssert(result, clm_id_set("host", 2001) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("host") == 2001 && clm_id_get("default_host") == 1001,
        "update one mapping while retaining aliases");
    clmAssert(result, clm_id_set("new_name", 3001) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("new_name") == 3001, "insert through set");
    clmAssert(result, clm_id_remove("new_name") == CLM_BPA_SUCCESS &&
        clm_id_get("new_name") == CLM_ID_NOT_FOUND, "remove one mapping by name");
    clmAssert(result, clm_id_remove("missing") == CLM_BPA_FAILURE,
        "report removal of an unknown name");

    clmAssert(result, clm_id_register("Host", 4001) == CLM_BPA_SET_SUCCESS &&
        clm_id_set_case_sensitive(false) == CLM_BPA_FAILURE &&
        clm_id_is_case_sensitive() && clm_id_get("Host") == 4001 &&
        clm_id_get("host") == 2001,
        "reject case-fold collisions without changing registry state");
    clmAssert(result, clm_id_remove("Host") == CLM_BPA_SUCCESS &&
        clm_id_set_case_sensitive(false) == CLM_BPA_SUCCESS &&
        !clm_id_is_case_sensitive() && clm_id_get("HOST") == 2001,
        "enable ASCII case-insensitive lookup");
    clmAssert(result, clm_id_set("HOST", 2002) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("host") == 2002,
        "update a name without changing its registered spelling");
    clmAssert(result, clm_id_register("\xC3\x84", 6001) == CLM_BPA_SET_SUCCESS &&
        clm_id_register("\xC3\xA4", 6002) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("\xC3\x84") == 6001 && clm_id_get("\xC3\xA4") == 6002,
        "leave non-ASCII UTF-8 bytes case-sensitive");
    clmAssert(result, clm_id_register("alias", 2002) == CLM_BPA_SET_SUCCESS &&
        clm_id_unmap(2002) == CLM_BPA_SUCCESS &&
        clm_id_get("host") == CLM_ID_NOT_FOUND &&
        clm_id_get("alias") == CLM_ID_NOT_FOUND,
        "unmap every alias for a numeric ID");
    clmAssert(result, clm_id_unmap_name("default_host") == CLM_BPA_SUCCESS &&
        clm_id_get("default_host") == CLM_ID_NOT_FOUND, "unmap one name");

    clmAssert(result, clm_id_gen_next() == 0 && clm_id_gen_next() == 1 &&
        clm_id_gen_next() == 2, "generate sequential IDs starting at zero");
    clm_id_gen_reset();
    clmAssert(result, clm_id_gen_next() == 0, "reset sequential ID generation");
    clmAssert(result, clm_id_clear() == CLM_BPA_SUCCESS &&
        clm_id_get("default_host") == CLM_ID_NOT_FOUND &&
        !clm_id_is_case_sensitive(), "clear mappings while preserving matching mode");
    clmAssert(result, clm_id_register("after-clear", 5001) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("AFTER-CLEAR") == 5001, "use the registry after clearing");
    clmAssert(result, clm_id_shutdown() == CLM_BPA_SUCCESS &&
        clm_id_get("after-clear") == CLM_ID_NOT_FOUND &&
        clm_id_register("after-shutdown", 5002) == CLM_BPA_SET_SUCCESS &&
        clm_id_get("after-shutdown") == 5002 &&
        clm_id_shutdown() == CLM_BPA_SUCCESS,
        "release and lazily recreate private BPA storage");
    clmAssert(result, clm_id_set_case_sensitive(true) == CLM_BPA_SUCCESS,
        "restore default matching mode");

    clmTestSummary(result.passed, result.failed);
    return result.failed;
}
