#pragma once

#include "test.h"
#include "../clm/string_toml.h"

static int clm_string_toml_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String TOML");
    const clm_id root_id = 76400;
    const clm_id output_id = 76401;
    const clm_id bad_root_id = 76402;
    const clm_id simple_root_id = 76403;
    const clm_id scalar_root_id = 76404;
    const clm_id array_root_id = 76405;
    const clm_id source_id = 76406;
    const clm_id section_key = 76410;
    const clm_id host_key = 76411;
    const clm_id port_key = 76412;
    const clm_id enabled_key = 76413;
    const clm_id ports_key = 76414;
    const char input[] =
        "[server]\n"
        "host = \"node\"\n"
        "port = 8080\n"
        "enabled = true\n"
        "ports = [80, 81]\n";
    clm_id section_id = 0;
    clm_id ports_id = 0;
    clm_int port = 0;
    bool enabled = false;

    CLM_ASSERT(clm_id_register("server", section_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("host", host_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("port", port_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("enabled", enabled_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("ports", ports_key) == CLM_BPA_SET_SUCCESS,
               "register TOML names");
    CLM_ASSERT(clm_test_parse_text(source_id, "[server]\n", simple_root_id,
                       clm_string_toml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(simple_root_id) == CLM_BPA_SUCCESS,
               "parse an empty TOML table");
    CLM_ASSERT(clm_test_parse_text(source_id,
                       "[server]\nhost = \"node\"\nport = 8080\n",
                       scalar_root_id, clm_string_toml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(scalar_root_id) == CLM_BPA_SUCCESS,
               "parse TOML string and integer assignments");
    CLM_ASSERT(clm_test_parse_text(source_id,
                       "[server]\nports = [80, 81]\n", array_root_id,
                       clm_string_toml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(array_root_id) == CLM_BPA_SUCCESS,
               "parse a TOML inline array");
    CLM_ASSERT(clm_test_parse_text(source_id,
                       "[server]\nports = []\n", simple_root_id,
                       clm_string_toml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(simple_root_id) == CLM_BPA_SUCCESS,
               "parse an empty TOML array child");
    CLM_ASSERT(clm_test_parse_text(source_id, input, root_id,
                       clm_string_toml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_get_object(root_id, section_key, &section_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_get_to(section_id, port_key, &port, sizeof(port)) ==
                       CLM_BPA_GET_SUCCESS && port == 8080 &&
                   clm_tree_get_id(section_id, enabled_key, &ports_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_is_boolean(ports_id, &enabled) && enabled &&
                   clm_tree_get_object(section_id, ports_key, &ports_id) ==
                       CLM_BPA_FAILURE,
               "parse TOML sections, typed scalars, and arrays");
    clm_id array_id = 0;
    CLM_ASSERT(clm_tree_get_id(section_id, ports_key, &array_id) ==
                       CLM_BPA_GET_SUCCESS && clm_tree_is_array(array_id) &&
                   clm_tree_array_count(array_id) == 2 &&
                   clm_tree_array_get_to(array_id, 1, &port, sizeof(port)) ==
                       CLM_BPA_GET_SUCCESS && port == 81,
               "preserve TOML inline arrays as tree arrays");
    CLM_ASSERT(clm_string_new(output_id, "") &&
                   clm_string_toml_dumps(root_id, output_id) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "[\"server\"]\n") != NULL &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "\"port\" = 8080\n") != NULL,
               "serialize TOML table and typed values");
    CLM_ASSERT(clm_test_parse_text(source_id, "[bad]\nvalue = null\n",
                       bad_root_id, clm_string_toml_loads) ==
                       CLM_FORMAT_DESERIALIZE_FAILURE &&
                   !clm_tree_is_object(bad_root_id),
               "reject unsupported TOML null and roll back");
    CLM_ASSERT(clm_tree_delete(root_id) == CLM_BPA_SUCCESS &&
                   clm_string_delete(output_id),
               "release TOML resources");
    CLM_ASSERT(clm_id_remove("server") == CLM_BPA_SUCCESS &&
                   clm_id_remove("host") == CLM_BPA_SUCCESS &&
                   clm_id_remove("port") == CLM_BPA_SUCCESS &&
                   clm_id_remove("enabled") == CLM_BPA_SUCCESS &&
                   clm_id_remove("ports") == CLM_BPA_SUCCESS,
               "remove TOML mappings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
