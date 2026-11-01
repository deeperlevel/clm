#pragma once

#include "test.hpp"
#include "../clm/id.hpp"

static int clm_id_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - ID");

    clm_id value = clm::id::CLM_ID_NOT_FOUND;
    CLM_ASSERT(clm::id::is_case_sensitive(),
               "name matching is case-sensitive by default");
    CLM_ASSERT(clm::id::clear() == CLM_BPA_SUCCESS,
               "initialize and clear ID mappings");
    CLM_ASSERT(clm::id::add("host", 1001) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("host") == 1001,
               "register and resolve a name");
    CLM_ASSERT(clm::id::map("mapped-name", 1002) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("mapped-name") == 1002,
               "map alias for registration");
    CLM_ASSERT(clm::id::add("default_host", 1001) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("default_host") == 1001,
               "allow multiple names to share one ID");
    size_t name_length = 0;
    char name_buffer[32] = {0};
    CLM_ASSERT(clm::id::get_name(1001, NULL, 0, &name_length) ==
                   CLM_BPA_GET_SUCCESS && name_length == 4 &&
                   clm::id::get_name(1001, name_buffer, sizeof(name_buffer),
                                   &name_length) == CLM_BPA_GET_SUCCESS &&
                   strcmp(name_buffer, "host") == 0 &&
                   clm::id::get_name(1001, name_buffer, 2, &name_length) ==
                       CLM_BPA_FAILURE,
               "resolve the first registered spelling for a mapped ID");
    CLM_ASSERT(clm::id::add("host", 1002) == CLM_BPA_FAILURE,
               "reject duplicate name without changing its mapping");
    CLM_ASSERT(clm::id::add("\xc0\xaf", 1003) == CLM_BPA_FAILURE,
               "reject malformed UTF-8 names");
    CLM_ASSERT(clm::id::get("HOST") == clm::id::CLM_ID_NOT_FOUND,
               "distinguish names by case in default mode");
    CLM_ASSERT(clm::id::try_get("host", &value) == CLM_BPA_GET_SUCCESS && value == 1001 &&
               clm::id::try_get("missing", &value) == CLM_BPA_FAILURE,
               "provide status-based lookup");

    CLM_ASSERT(clm::id::set("host", 2001) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("host") == 2001 &&
               clm::id::get("default_host") == 1001,
               "update one mapping while retaining aliases");
    CLM_ASSERT(clm::id::set("new_name", 3001) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("new_name") == 3001,
               "insert through set");
    CLM_ASSERT(clm::id::remove("new_name") == CLM_BPA_SUCCESS &&
               clm::id::get("new_name") == clm::id::CLM_ID_NOT_FOUND,
               "remove one mapping by name");
    CLM_ASSERT(clm::id::remove("missing") == CLM_BPA_FAILURE,
               "report removal of an unknown name");

    CLM_ASSERT(clm::id::add("Host", 4001) == CLM_BPA_SET_SUCCESS &&
               clm::id::set_case_sensitive(false) == CLM_BPA_FAILURE &&
               clm::id::is_case_sensitive() &&
               clm::id::get("Host") == 4001 &&
               clm::id::get("host") == 2001,
               "reject case-fold collisions without changing registry state");
    CLM_ASSERT(clm::id::remove("Host") == CLM_BPA_SUCCESS &&
               clm::id::set_case_sensitive(false) == CLM_BPA_SUCCESS &&
               !clm::id::is_case_sensitive() &&
               clm::id::get("HOST") == 2001,
               "enable ASCII case-insensitive lookup");
    CLM_ASSERT(clm::id::set("HOST", 2002) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("host") == 2002,
               "update a name without changing its registered spelling");
    CLM_ASSERT(clm::id::add("\xc3\x84", 6001) == CLM_BPA_SET_SUCCESS &&
               clm::id::add("\xc3\xa4", 6002) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("\xc3\x84") == 6001 &&
               clm::id::get("\xc3\xa4") == 6002,
               "leave non-ASCII UTF-8 bytes case-sensitive");

    CLM_ASSERT(clm::id::add("alias", 2002) == CLM_BPA_SET_SUCCESS &&
               clm::id::unmap(2002) == CLM_BPA_SUCCESS &&
               clm::id::get("host") == clm::id::CLM_ID_NOT_FOUND &&
               clm::id::get("alias") == clm::id::CLM_ID_NOT_FOUND,
               "unmap every alias for a numeric ID");
    CLM_ASSERT(clm::id::unmap_name("default_host") == CLM_BPA_SUCCESS &&
               clm::id::get("default_host") == clm::id::CLM_ID_NOT_FOUND,
               "unmap a single name");

    CLM_ASSERT(clm::id::gen_next() == 0 &&
               clm::id::gen_next() == 1 &&
               clm::id::gen_next() == 2,
               "generate sequential IDs starting at zero");
    clm::id::gen_reset();
    CLM_ASSERT(clm::id::gen_next() == 0,
               "reset sequential ID generation");

    CLM_ASSERT(clm::id::clear() == CLM_BPA_SUCCESS &&
               clm::id::get("default_host") == clm::id::CLM_ID_NOT_FOUND &&
               !clm::id::is_case_sensitive(),
               "clear mappings while preserving matching mode");
    CLM_ASSERT(clm::id::add("after-clear", 5001) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("AFTER-CLEAR") == 5001,
               "use the registry after clearing");
    CLM_ASSERT(clm::id::shutdown() == CLM_BPA_SUCCESS &&
               clm::id::get("after-clear") == clm::id::CLM_ID_NOT_FOUND &&
               clm::id::add("after-shutdown", 5002) == CLM_BPA_SET_SUCCESS &&
               clm::id::get("after-shutdown") == 5002 &&
               clm::id::shutdown() == CLM_BPA_SUCCESS,
               "release and lazily recreate private BPA storage");

    CLM_ASSERT(clm::id::set_case_sensitive(true) == CLM_BPA_SUCCESS,
               "restore default matching mode");
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}


static int test_clm_id(void)
{
    return clm_id_tests();
}

