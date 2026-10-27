/*
 * =============================================================================
 * CLM — Cross Language Modules
 * Public Domain — CC0 1.0 Universal
 * =============================================================================
 */

#pragma once

#include "test.hpp"
#include "../clm/id.hpp"

static int test_clm_id()
{
    clm_test_result result = {0};
    using namespace clm::id;

    clm_id value = CLM_ID_NOT_FOUND;
    CLM_ASSERT(is_case_sensitive(), "name matching defaults to exact case");
    CLM_ASSERT(clear() == CLM_BPA_SUCCESS &&
                   add("host", 1001) == CLM_BPA_SET_SUCCESS &&
                   map("mapped-name", 1002) == CLM_BPA_SET_SUCCESS &&
                   add("default_host", 1001) == CLM_BPA_SET_SUCCESS,
               "clear mappings and register names and aliases");
    CLM_ASSERT(add("host", 2000) == CLM_BPA_FAILURE &&
                   add("\xc0\xaf", 1003) == CLM_BPA_FAILURE &&
                   get("HOST") == CLM_ID_NOT_FOUND,
               "reject duplicate or malformed names and match case exactly");
    CLM_ASSERT(try_get("host", &value) == CLM_BPA_GET_SUCCESS &&
                   value == 1001 &&
                   try_get("missing", &value) == CLM_BPA_FAILURE,
               "provide status-based lookup");
    value = 777;
    CLM_ASSERT(try_get("missing", &value) == CLM_BPA_FAILURE &&
                   value == 777 &&
                   try_get("host", nullptr) == CLM_BPA_FAILURE,
               "leave lookup outputs unchanged on failure and reject null outputs");
    CLM_ASSERT(set("host", 2001) == CLM_BPA_SET_SUCCESS &&
                   get("host") == 2001 &&
                   get("default_host") == 1001 &&
                   set("new_name", 3001) == CLM_BPA_SET_SUCCESS,
               "update one mapping or insert a new mapping with set");
    CLM_ASSERT(erase("new_name") == CLM_BPA_SUCCESS &&
                   erase("missing") == CLM_BPA_FAILURE,
               "remove one mapping and report unknown names");

    CLM_ASSERT(add("Host", 4001) == CLM_BPA_SET_SUCCESS &&
                   set_case_sensitive(false) == CLM_BPA_FAILURE &&
                   is_case_sensitive() &&
                   get("Host") == 4001 &&
                   get("host") == 2001,
               "reject a case-fold collision without changing registry state");
    CLM_ASSERT(erase("Host") == CLM_BPA_SUCCESS &&
                   set_case_sensitive(false) == CLM_BPA_SUCCESS &&
                   !is_case_sensitive() &&
                   get("HOST") == 2001 &&
                   set("HOST", 2002) == CLM_BPA_SET_SUCCESS &&
                   get("host") == 2002,
               "enable ASCII-only case-insensitive lookup and updates");
    CLM_ASSERT(add("\xc3\x84", 6001) == CLM_BPA_SET_SUCCESS &&
                   add("\xc3\xa4", 6002) == CLM_BPA_SET_SUCCESS &&
                   get("\xc3\x84") == 6001 &&
                   get("\xc3\xa4") == 6002,
               "leave non-ASCII UTF-8 bytes case-sensitive");
    CLM_ASSERT(set_case_sensitive(true) == CLM_BPA_SUCCESS &&
                   get("HOST") == CLM_ID_NOT_FOUND &&
                   get("host") == 2002 &&
                   set_case_sensitive(false) == CLM_BPA_SUCCESS,
               "restore original registered spelling when switching modes");
    CLM_ASSERT(add("alias", 2002) == CLM_BPA_SET_SUCCESS &&
                   unmap(2002) == CLM_BPA_SUCCESS &&
                   get("host") == CLM_ID_NOT_FOUND &&
                   get("alias") == CLM_ID_NOT_FOUND &&
                   unmap_name("default_host") == CLM_BPA_SUCCESS,
               "unmap all aliases by ID or one mapping by name");

    CLM_ASSERT(gen_next() == 0 && gen_next() == 1 &&
                   gen_next() == 2,
               "generate sequential IDs starting at zero");
    gen_reset();
    CLM_ASSERT(gen_next() == 0, "reset sequential ID generation");
    CLM_ASSERT(clear() == CLM_BPA_SUCCESS &&
                   get("default_host") == CLM_ID_NOT_FOUND &&
                   !is_case_sensitive() &&
                   add("after-clear", 5001) == CLM_BPA_SET_SUCCESS &&
                   get("AFTER-CLEAR") == 5001,
               "clear mappings while retaining matching mode");
    CLM_ASSERT(add("maximum-id", CLM_ID_NOT_FOUND) == CLM_BPA_SET_SUCCESS &&
               try_get("maximum-id", &value) == CLM_BPA_GET_SUCCESS &&
               value == CLM_ID_NOT_FOUND,
               "status lookup distinguishes a registered maximum ID from missing");
    CLM_ASSERT(try_get(nullptr, &value) == CLM_BPA_FAILURE,
               "status lookup rejects null names");
    CLM_ASSERT(shutdown() == CLM_BPA_SUCCESS &&
                   get("after-clear") == CLM_ID_NOT_FOUND &&
                   add("after-shutdown", 5002) ==
                       CLM_BPA_SET_SUCCESS &&
                   get("after-shutdown") == 5002 &&
                   shutdown() == CLM_BPA_SUCCESS &&
                   set_case_sensitive(true) == CLM_BPA_SUCCESS,
               "shutdown, lazily reuse the registry, and restore default mode");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
