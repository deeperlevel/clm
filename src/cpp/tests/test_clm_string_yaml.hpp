#pragma once

#include "test.hpp"
#include "../clm/string_yaml.hpp"

static int clm_string_yaml_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String YAML");
    const clm_id root_id = 76500;
    const clm_id output_id = 76501;
    const clm_id bad_root_id = 76502;
    const clm_id simple_root_id = 76503;
    const clm_id nested_root_id = 76504;
    const clm_id array_root_id = 76505;
    const clm_id source_id = 76506;
    const clm_id server_key = 76510;
    const clm_id host_key = 76511;
    const clm_id ports_key = 76512;
    const clm_id active_key = 76513;
    const char input[] =
        "server:\n"
        "  host: node\n"
        "  ports:\n"
        "    - 80\n"
        "    - 81\n"
        "  active: true\n";
    clm_id server_id = 0;
    clm_id ports_id = 0;
    clm_id active_id = 0;
    clm_int port = 0;
    bool active = false;

    CLM_ASSERT(clm_id_register("server", server_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("host", host_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("ports", ports_key) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("active", active_key) == CLM_BPA_SET_SUCCESS,
               "register YAML mapping names");
    CLM_ASSERT(clm_test_parse_text(source_id, "server: value\n", simple_root_id,
                       clm_string_yaml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(simple_root_id) == CLM_BPA_SUCCESS,
               "parse a single YAML mapping");
    CLM_ASSERT(clm_test_parse_text(source_id, "server:\n  host: node\n",
                       nested_root_id, clm_string_yaml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(nested_root_id) == CLM_BPA_SUCCESS,
               "parse a nested YAML mapping");
    CLM_ASSERT(clm_test_parse_text(source_id, "ports:\n  - 80\n  - 81\n",
                       array_root_id, clm_string_yaml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(array_root_id) == CLM_BPA_SUCCESS,
               "parse a YAML block sequence");
    CLM_ASSERT(clm_test_parse_text(source_id, input, root_id,
                       clm_string_yaml_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_get_object(root_id, server_key, &server_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_get_to(server_id, host_key, (char[16]){0}, 16) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_get_object(server_id, ports_key, &ports_id) ==
                       CLM_BPA_FAILURE &&
                   clm_tree_get_id(server_id, ports_key, &ports_id) ==
                       CLM_BPA_GET_SUCCESS && clm_tree_is_array(ports_id) &&
                   clm_tree_array_count(ports_id) == 2 &&
                   clm_tree_array_get_to(ports_id, 0, &port, sizeof(port)) ==
                       CLM_BPA_GET_SUCCESS && port == 80 &&
                   clm_tree_get_id(server_id, active_key, &active_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_is_boolean(active_id, &active) && active,
               "parse nested YAML mappings, sequences, and typed scalars");
    CLM_ASSERT(clm_string_new(output_id, "") &&
                   clm_string_yaml_dumps(root_id, output_id) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "\"host\": \"node\"\n") != NULL &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "- \"80\"\n") != NULL &&
                   strstr((const char*)clm_bpa_find_block(output_id)->data,
                          "\"active\": \"true\"\n") != NULL,
               "dump simple YAML with all scalar values quoted");
    CLM_ASSERT(clm_test_parse_text(source_id, "root:\n\tchild: value\n",
                       bad_root_id, clm_string_yaml_loads) ==
                       CLM_FORMAT_DESERIALIZE_FAILURE &&
                   !clm_tree_is_object(bad_root_id),
               "reject tab indentation and clean up failed YAML parse");
    CLM_ASSERT(clm_tree_delete(root_id) == CLM_BPA_SUCCESS &&
                   clm_string_delete(output_id),
               "release YAML resources");
    CLM_ASSERT(clm_id_remove("server") == CLM_BPA_SUCCESS &&
                   clm_id_remove("host") == CLM_BPA_SUCCESS &&
                   clm_id_remove("ports") == CLM_BPA_SUCCESS &&
                   clm_id_remove("active") == CLM_BPA_SUCCESS,
               "remove YAML field mappings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}


static int test_clm_string_yaml(void)
{
    return clm_string_yaml_tests();
}

