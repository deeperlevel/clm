#pragma once

#include "test.hpp"
#include "../clm/string_env.hpp"

static int clm_string_env_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String ENV");
    const clm_id root_id = 76300;
    const clm_id output_id = 76301;
    const clm_id bad_root_id = 76302;
    const clm_id source_id = 76303;
    const clm_id host_key = 76310;
    const clm_id port_key = 76311;
    const char input[] =
        "# settings\n"
        "export HOST=\"server one\"\n"
        "PORT=8080\n";
    char text[64] = {0};

    CLM_ASSERT(clm_id_register("HOST", host_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("PORT", port_key) == CLM_BPA_SET_SUCCESS,
               "register ENV variable names");
    CLM_ASSERT(clm_test_parse_text(source_id, input, root_id,
                       clm_string_env_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_count(root_id) == 2 &&
                   clm_tree_get_to(root_id, host_key, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS && strcmp(text, "server one") == 0 &&
                   clm_tree_get_to(root_id, port_key, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS && strcmp(text, "8080") == 0,
               "parse ENV comments, export prefix, and values as strings");
    CLM_ASSERT(clm_string_new(output_id, "") &&
                   clm_string_env_dumps(root_id, output_id) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "HOST=\"server one\"\n") != NULL &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "PORT=\"8080\"\n") != NULL,
               "serialize ENV assignments with quoted string values");
    CLM_ASSERT(clm_test_parse_text(source_id, "INVALID-NAME=value\n",
                       bad_root_id, clm_string_env_loads) ==
                       CLM_FORMAT_DESERIALIZE_FAILURE &&
                   !clm_tree_is_object(bad_root_id),
               "reject invalid ENV names and clean up parse failure");
    CLM_ASSERT(clm_tree_delete(root_id) == CLM_BPA_SUCCESS &&
                   clm_string_delete(output_id),
               "release ENV resources");
    CLM_ASSERT(clm_id_remove("HOST") == CLM_BPA_SUCCESS &&
                   clm_id_remove("PORT") == CLM_BPA_SUCCESS,
               "remove ENV mappings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}


static int test_clm_string_env(void)
{
    return clm_string_env_tests();
}

