#pragma once

#include "test.h"
#include "../clm/string_format.h"

static clm_int clm_tree_test_parse_callback(
    clm_string_format_parser* parser,
    clm_id root_object_id)
{
    (void)parser;
    const clm_int value = 17;
    return clm_tree_set_from(root_object_id, 77, CLM_BPA_TYPE_INT,
                             &value, sizeof(value)) == CLM_BPA_SET_SUCCESS
        ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
}

static clm_int clm_tree_test_parse_failure_callback(
    clm_string_format_parser* parser,
    clm_id root_object_id)
{
    (void)parser;
    (void)root_object_id;
    return CLM_FORMAT_DESERIALIZE_FAILURE;
}

static clm_int clm_tree_test_serialize_callback(
    clm_id root_object_id,
    clm_id destination_string_id)
{
    return clm_tree_contains(root_object_id, 77) &&
           clm_string_set(destination_string_id, "serialized") == 10
        ? CLM_FORMAT_SERIALIZE_SUCCESS : CLM_FORMAT_SERIALIZE_FAILURE;
}

static int clm_tree_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Tree");

    const clm_id root_id = 74000;
    const clm_id object_id = 74001;
    const clm_id nested_object_id = 74002;
    const clm_id borrowed_root_id = 74010;
    const clm_id borrowed_value_id = 74011;
    const clm_id remove_root_id = 74020;
    const clm_id remove_object_id = 74021;
    const clm_id remove_nested_id = 74022;
    const clm_id cycle_a_id = 74030;
    const clm_id cycle_b_id = 74031;
    const clm_id destination_id = 74040;
    const clm_id format_root_id = 74050;
    const clm_id failed_format_root_id = 74051;
    const clm_id format_output_id = 74052;
    const clm_id array_id = 74060;
    const clm_id array_object_id = 74061;
    const clm_id nested_array_id = 74062;
    const char host[] = "Server111";
    const clm_int port = 8080;
    const clm_int replacement = 202;
    clm_id child_id = 0;
    clm_byte type = 0;
    clm_int port_result = 0;
    char text[32] = {0};

    CLM_ASSERT(clm_tree_delete(74999) == CLM_BPA_FAILURE,
               "reject deletion of a non-tree ID safely");
    CLM_ASSERT(clm_tree_new(root_id) == CLM_BPA_SUCCESS &&
                   clm_tree_count(root_id) == 0 &&
                   clm_tree_is_object(root_id),
               "create an empty tree object");
    CLM_ASSERT(clm_tree_add_object(root_id, 10, object_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_add_object(object_id, 11, nested_object_id) ==
                       CLM_BPA_SET_SUCCESS,
               "create nested child objects");
    CLM_ASSERT(clm_tree_set_from(object_id, 12, CLM_BPA_TYPE_STRING,
                                 host, sizeof(host)) == CLM_BPA_SET_SUCCESS &&
                   clm_tree_set_from(object_id, 13, CLM_BPA_TYPE_INT,
                                     &port, sizeof(port)) == CLM_BPA_SET_SUCCESS &&
                   clm_tree_set_from(nested_object_id, 14, CLM_BPA_TYPE_STRING,
                                     "secret", sizeof("secret")) ==
                       CLM_BPA_SET_SUCCESS,
               "store independently typed leaf values");
    CLM_ASSERT(clm_tree_get_object(root_id, 10, &child_id) ==
                       CLM_BPA_GET_SUCCESS && child_id == object_id &&
                   clm_tree_get_type(object_id, 12, &type) == CLM_BPA_GET_SUCCESS &&
                   type == CLM_BPA_TYPE_STRING &&
                   clm_tree_get_to(object_id, 12, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS && strcmp(text, host) == 0 &&
                   clm_tree_get_to(object_id, 13, &port_result,
                                   sizeof(port_result)) == CLM_BPA_GET_SUCCESS &&
                   port_result == port,
               "navigate child objects and retrieve typed leaves");
    CLM_ASSERT(clm_tree_get_object(object_id, 13, &child_id) ==
                       CLM_BPA_FAILURE &&
                   clm_tree_add_object(root_id, 10, 74003) == CLM_BPA_FAILURE &&
                   clm_bpa_is_existing(74003) == false,
               "reject non-object traversal and duplicate child keys");

    clm_id owned_host_id = 0;
    clm_id owned_secret_id = 0;
    CLM_ASSERT(clm_tree_get_id(object_id, 12, &owned_host_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_tree_get_id(nested_object_id, 14, &owned_secret_id) ==
                       CLM_BPA_GET_SUCCESS,
               "capture IDs for the child subtree and owned leaves");
    CLM_ASSERT(clm_tree_set_from(root_id, 10, CLM_BPA_TYPE_INT,
                                 &replacement, sizeof(replacement)) ==
                   CLM_BPA_SET_SUCCESS,
               "replace an object child with a scalar leaf");
    CLM_ASSERT(!clm_bpa_is_existing(object_id) &&
                   !clm_bpa_is_existing(nested_object_id) &&
                   !clm_bpa_is_existing(owned_host_id) &&
                   !clm_bpa_is_existing(owned_secret_id),
               "replacement releases the child subtree and its owned leaves");
    CLM_ASSERT(clm_tree_get_to(root_id, 10, &port_result,
                               sizeof(port_result)) == CLM_BPA_GET_SUCCESS &&
                   port_result == replacement &&
                   clm_tree_delete(root_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_is_existing(root_id) == false,
               "delete a tree and its owned leaf values");

    CLM_ASSERT(clm_bpa_create(borrowed_value_id, CLM_BPA_TYPE_INT) ==
                       CLM_BPA_SUCCESS &&
                   clm_bpa_set(borrowed_value_id, &port, sizeof(port)) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_new(borrowed_root_id) == CLM_BPA_SUCCESS &&
                   clm_tree_set(borrowed_root_id, 1, borrowed_value_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_delete(borrowed_root_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_is_existing(borrowed_value_id) &&
                   clm_bpa_delete(borrowed_value_id) == CLM_BPA_SUCCESS,
               "tree deletion preserves borrowed leaf blocks");

    CLM_ASSERT(clm_tree_new(remove_root_id) == CLM_BPA_SUCCESS &&
                   clm_tree_add_object(remove_root_id, 20, remove_object_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_add_object(remove_object_id, 21, remove_nested_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_set_from(remove_nested_id, 22, CLM_BPA_TYPE_STRING,
                                     "remove me", sizeof("remove me")) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_remove(remove_root_id, 20) == CLM_BPA_SUCCESS &&
                   !clm_bpa_is_existing(remove_object_id) &&
                   !clm_bpa_is_existing(remove_nested_id) &&
                   clm_tree_count(remove_root_id) == 0 &&
                   clm_tree_delete(remove_root_id) == CLM_BPA_SUCCESS,
               "removing a child object recursively deletes its subtree");

    CLM_ASSERT(clm_tree_new(cycle_a_id) == CLM_BPA_SUCCESS &&
                   clm_tree_new(cycle_b_id) == CLM_BPA_SUCCESS &&
                   clm_dictionary_set(cycle_a_id, 1, cycle_b_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_set(cycle_b_id, 2, cycle_a_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_delete(cycle_a_id) == CLM_BPA_FAILURE &&
                   clm_bpa_is_existing(cycle_a_id) &&
                   clm_bpa_is_existing(cycle_b_id),
               "reject cyclic object references without partial deletion");
    CLM_ASSERT(clm_dictionary_remove(cycle_a_id, 1) == CLM_BPA_SUCCESS &&
                   clm_dictionary_remove(cycle_b_id, 2) == CLM_BPA_SUCCESS &&
                   clm_tree_delete(cycle_a_id) == CLM_BPA_SUCCESS &&
                   clm_tree_delete(cycle_b_id) == CLM_BPA_SUCCESS,
               "delete objects after breaking a cycle");

    CLM_ASSERT(clm_bpa_create(destination_id, CLM_BPA_TYPE_STRING) ==
                       CLM_BPA_SUCCESS &&
                   clm_tree_new(root_id) == CLM_BPA_SUCCESS &&
                   clm_tree_set_from(root_id, 30, CLM_BPA_TYPE_STRING,
                                     "copy", sizeof("copy")) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_get(root_id, 30, destination_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_bpa_find_block(destination_id) != NULL &&
                   strcmp((const char*)clm_bpa_find_block(destination_id)->data,
                          "copy") == 0 &&
                   clm_tree_delete(root_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(destination_id) == CLM_BPA_SUCCESS,
               "copy a leaf into a compatible existing BPA block");

    const clm_string_format_lex_rules format_rules = {0};
    CLM_ASSERT(clm_string_format_parse_tree(
                       "", 0, &format_rules, format_root_id,
                       clm_tree_test_parse_callback) ==
                       CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_tree_count(format_root_id) == 1 &&
                   clm_string_format_parse_tree(
                       "", 0, &format_rules, failed_format_root_id,
                       clm_tree_test_parse_failure_callback) ==
                       CLM_FORMAT_DESERIALIZE_FAILURE &&
                   !clm_tree_is_object(failed_format_root_id),
               "parse wrapper creates a tree root and rolls it back on failure");
    CLM_ASSERT(clm_string_new(format_output_id, "") &&
                   clm_string_format_serialize_tree(
                       format_root_id, format_output_id,
                       clm_tree_test_serialize_callback) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   clm_bpa_find_block(format_output_id) != NULL &&
                   strcmp((const char*)clm_bpa_find_block(format_output_id)->data,
                          "serialized") == 0,
               "serialize wrapper validates and delegates for tree data");
    CLM_ASSERT(clm_tree_delete(format_root_id) == CLM_BPA_SUCCESS &&
                   clm_string_delete(format_output_id),
               "release format callback test resources");

    clm_int array_number = 23;
    clm_id array_element_id = 0;
    clm_byte array_element_type = 0;
    CLM_ASSERT(clm_tree_new_array(array_id) == CLM_BPA_SUCCESS &&
                   clm_tree_array_append_from(array_id, CLM_BPA_TYPE_INT,
                                              &array_number,
                                              sizeof(array_number)) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_array_append_object(array_id, array_object_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_set_from(array_object_id, 91, CLM_BPA_TYPE_STRING,
                                     "inside", sizeof("inside")) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_array_append_array(array_id, nested_array_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_array_append_null(nested_array_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_array_count(array_id) == 3 &&
                   clm_tree_array_get_type(array_id, 0, &array_element_type) ==
                       CLM_BPA_GET_SUCCESS &&
                   array_element_type == CLM_BPA_TYPE_INT &&
                   clm_tree_array_get_object(array_id, 1, &child_id) ==
                       CLM_BPA_GET_SUCCESS && child_id == array_object_id &&
                   clm_tree_array_get_array(array_id, 2, &child_id) ==
                       CLM_BPA_GET_SUCCESS && child_id == nested_array_id &&
                   clm_tree_array_count(nested_array_id) == 1 &&
                   clm_tree_array_get_id(nested_array_id, 0,
                                         &array_element_id) == CLM_BPA_GET_SUCCESS &&
                   clm_tree_is_null(array_element_id) &&
                   clm_tree_array_get_to(nested_array_id, 0, text,
                                         sizeof(text)) == CLM_BPA_FAILURE,
               "support ordered typed arrays, nested containers, and null leaves");
    CLM_ASSERT(clm_tree_delete(array_id) == CLM_BPA_SUCCESS &&
                   !clm_bpa_is_existing(array_id) &&
                   !clm_bpa_is_existing(array_object_id) &&
                   !clm_bpa_is_existing(nested_array_id) &&
                   !clm_bpa_is_existing(array_element_id),
               "recursively release array containers and owned leaves");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
