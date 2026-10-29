#pragma once

#include "test.h"
#include "../clm/string_ini.h"

static int clm_string_ini_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String INI");
    const clm_id root_id = 76200;
    const clm_id output_id = 76201;
    const clm_id bad_root_id = 76202;
    const clm_id source_id = 76203;
    const clm_id root_key = 76210;
    const clm_id section_key = 76211;
    const clm_id host_key = 76212;
    const clm_id port_key = 76213;
    const char input[] =
        "ini_root_key = yes\n"
        "; comment\n"
        "[server]\n"
        "host = \"node\\n1\"\n"
        "port: 8080\n";
    clm_id section_id = 0;
    char text[64] = {0};

    CLM_ASSERT(clm_id_register("ini_root_key", root_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("server", section_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("host", host_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("port", port_key) == CLM_BPA_SET_SUCCESS,
               "register INI field names");
    const clm_int parse_status = clm_test_parse_text(source_id, input, root_id,
        clm_string_ini_loads);
    CLM_ASSERT(parse_status == CLM_FORMAT_DESERIALIZE_SUCCESS,
               "parse INI root keys, sections, assignments, and quoted values");
    CLM_ASSERT(parse_status == CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_count(root_id) == 2 &&
                   clm_tree_get_object(root_id, section_key, &section_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_get_to(root_id, root_key, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS && strcmp(text, "yes") == 0 &&
                   clm_tree_get_to(section_id, host_key, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS && strcmp(text, "node\n1") == 0 &&
                   clm_tree_get_to(section_id, port_key, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS && strcmp(text, "8080") == 0,
               "read values from the parsed INI tree");
    CLM_ASSERT(parse_status == CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_string_new(output_id, "") &&
                   clm_string_ini_dumps(root_id, output_id) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "[server]\nhost = \"node\\n1\"\n") != NULL,
               "serialize INI sections and escaped quoted values");
    CLM_ASSERT(clm_test_parse_text(source_id,
                   "[server]\nhost=one\nhost=two\n", bad_root_id,
                   clm_string_ini_loads) ==
                       CLM_FORMAT_DESERIALIZE_FAILURE &&
                   !clm_tree_is_object(bad_root_id),
               "reject duplicate INI keys and roll back the tree");
    CLM_ASSERT((clm_tree_is_object(root_id)
                   ? clm_tree_delete(root_id) == CLM_BPA_SUCCESS : true) &&
                   (!clm_bpa_is_existing(output_id) || clm_string_delete(output_id)),
               "release INI tree and output string");
    CLM_ASSERT(clm_id_remove("ini_root_key") == CLM_BPA_SUCCESS &&
                   clm_id_remove("server") == CLM_BPA_SUCCESS &&
                   clm_id_remove("host") == CLM_BPA_SUCCESS &&
                   clm_id_remove("port") == CLM_BPA_SUCCESS,
               "remove INI field mappings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
