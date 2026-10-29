#pragma once

#include "test.h"
#include "../clm/string_json.h"

static int clm_string_json_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String JSON");
    const clm_id root_id = 76100;
    const clm_id source_id = 76101;
    const clm_id destination_id = 76102;
    const clm_id invalid_root_id = 76103;
    const clm_id empty_root_id = 76104;
    const clm_id simple_root_id = 76105;
    const clm_id array_root_id = 76106;
    const clm_id child_array_root_id = 76107;
    const clm_id comment_root_id = 76108;
    const clm_id key_name = 76110;
    const clm_id key_count = 76111;
    const clm_id key_active = 76112;
    const clm_id key_none = 76113;
    const clm_id key_items = 76114;
    const clm_id key_flag = 76115;
    const char input[] =
        "/* settings */ {\"json_name\":\"Ada\\nLovelace\",\"json_count\":12,"
        "\"json_active\":true,\"json_none\":null,\"json_items\":[1,\"x\","
        "{\"json_flag\":false}]}";
    char text[64] = {0};
    clm_int integer = 0;
    clm_id value_id = 0;
    bool boolean = false;

    CLM_ASSERT(clm_id_register("json_name", key_name) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("json_count", key_count) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("json_active", key_active) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("json_none", key_none) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("json_items", key_items) == CLM_BPA_SET_SUCCESS &&
                   clm_id_register("json_flag", key_flag) == CLM_BPA_SET_SUCCESS,
               "register JSON object names");
    CLM_ASSERT(clm_test_parse_text(source_id, "{}", empty_root_id,
                       clm_string_json_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(empty_root_id) == CLM_BPA_SUCCESS,
               "parse and delete an empty JSON object");
    CLM_ASSERT(clm_test_parse_text(source_id, "{\"json_name\":\"v\"}",
                       simple_root_id, clm_string_json_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(simple_root_id) == CLM_BPA_SUCCESS,
               "parse a JSON object with one mapped string");
    CLM_ASSERT(clm_test_parse_text(source_id, "[1,2]", array_root_id,
                       clm_string_json_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_array_count(array_root_id) == 2 &&
                   clm_tree_delete(array_root_id) == CLM_BPA_SUCCESS,
               "parse a JSON array root with numeric elements");
    CLM_ASSERT(clm_test_parse_text(source_id, "{\"json_items\":[]}",
                       child_array_root_id, clm_string_json_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(child_array_root_id) == CLM_BPA_SUCCESS,
               "parse an empty JSON array child");
    CLM_ASSERT(clm_test_parse_text(source_id,
                       "/* note */ {\"json_name\":\"v\"}", comment_root_id,
                       clm_string_json_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_delete(comment_root_id) == CLM_BPA_SUCCESS,
               "skip a leading JSON block comment");
    CLM_ASSERT(clm_test_parse_text(source_id, input, root_id,
                       clm_string_json_loads) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_is_object(root_id),
               "parse commented JSON into a tree object");
    CLM_ASSERT(clm_tree_get_to(root_id, key_name, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS &&
                   strcmp(text, "Ada\nLovelace") == 0 &&
                   clm_tree_get_to(root_id, key_count, &integer, sizeof(integer)) ==
                       CLM_BPA_GET_SUCCESS && integer == 12,
               "decode JSON strings and preserve numeric leaf types");
    CLM_ASSERT(clm_tree_get_id(root_id, key_active, &value_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_is_boolean(value_id, &boolean) && boolean &&
                   clm_tree_get_id(root_id, key_none, &value_id) ==
                       CLM_BPA_GET_SUCCESS && clm_tree_is_null(value_id),
               "preserve JSON boolean and null node types");
    CLM_ASSERT(clm_tree_get_id(root_id, key_items, &value_id) ==
                       CLM_BPA_GET_SUCCESS && clm_tree_is_array(value_id) &&
                   clm_tree_array_count(value_id) == 3 &&
                   clm_tree_array_get_to(value_id, 0, &integer, sizeof(integer)) ==
                       CLM_BPA_GET_SUCCESS && integer == 1,
               "parse heterogeneous JSON arrays");

    CLM_ASSERT(clm_string_new(destination_id, "") &&
                   clm_string_json_dumps(root_id, destination_id) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(destination_id)->data,
                       "{\"json_name\":\"Ada\\nLovelace\",\"json_count\":12,"
                       "\"json_active\":true,\"json_none\":null,"
                       "\"json_items\":[1,\"x\",{\"json_flag\":false}]}" ) == 0,
               "dump compact JSON while preserving scalar types");
    CLM_ASSERT(clm_string_new(source_id, "") &&
                   clm_string_set(source_id, "{\"json_name\":1,}") == 16 &&
                   clm_string_json_loads(source_id, invalid_root_id) ==
                       CLM_FORMAT_DESERIALIZE_FAILURE &&
                   !clm_tree_is_object(invalid_root_id),
               "reject trailing commas and clean up failed parses");

    CLM_ASSERT(clm_tree_delete(root_id) == CLM_BPA_SUCCESS &&
                   clm_string_delete(source_id) &&
                   clm_string_delete(destination_id),
               "release JSON resources");
    CLM_ASSERT(clm_id_remove("json_name") == CLM_BPA_SUCCESS &&
                   clm_id_remove("json_count") == CLM_BPA_SUCCESS &&
                   clm_id_remove("json_active") == CLM_BPA_SUCCESS &&
                   clm_id_remove("json_none") == CLM_BPA_SUCCESS &&
                   clm_id_remove("json_items") == CLM_BPA_SUCCESS &&
                   clm_id_remove("json_flag") == CLM_BPA_SUCCESS,
               "remove JSON test field mappings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
