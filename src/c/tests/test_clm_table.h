#pragma once

#include "test.h"
#include "../clm/table.h"

static int clm_table_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Table");

    const clm_id table_id = 75000;
    const clm_id source_string_id = 75001;
    const clm_id destination_string_id = 75002;
    const clm_id host_column_id = 75100;
    const clm_id port_column_id = 75101;
    const clm_id note_column_id = 75102;
    const clm_id host_alias_id = 75100;
    char text[32] = {0};
    clm_id column_id = 0;

    CLM_ASSERT(clm_id_register("table_host", host_column_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_id_register("table_port", port_column_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_id_register("table_note", note_column_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_id_register("table_host_alias", host_alias_id) ==
                       CLM_BPA_SET_SUCCESS,
               "register table column names");
    CLM_ASSERT(clm_table_new(table_id) == CLM_BPA_SUCCESS &&
                   clm_table_column_count(table_id) == 0 &&
                   clm_table_row_count(table_id) == 0 &&
                   clm_table_new(table_id) == CLM_BPA_FAILURE,
               "create an empty table and reject a duplicate ID");
    CLM_ASSERT(clm_table_add_column(table_id, "table_unknown") ==
                       CLM_BPA_FAILURE &&
                   clm_table_add_column(table_id, "table_host") ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_table_add_column(table_id, "table_port") ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_table_add_column(table_id, "table_host_alias") ==
                       CLM_BPA_FAILURE &&
                   clm_table_column_count(table_id) == 2,
               "add mapped columns and reject unknown or aliased IDs");
    CLM_ASSERT(clm_table_column_id_at(table_id, 0, &column_id) ==
                       CLM_BPA_GET_SUCCESS && column_id == host_column_id &&
                   clm_table_column_name_to(table_id, 1, text, sizeof(text)) == 10 &&
                   strcmp(text, "table_port") == 0 &&
                   clm_table_column_name_to(table_id, 1, text, 2) ==
                       CLM_BPA_FAILURE,
               "preserve column order, IDs, and names");

    const clm_int first_row = clm_table_append_row(table_id);
    const clm_int second_row = clm_table_append_row(table_id);
    CLM_ASSERT(first_row == 0 && second_row == 1 &&
                   clm_table_row_count(table_id) == 2 &&
                   !clm_table_cell_contains(table_id, first_row, host_column_id) &&
                   clm_table_get(table_id, first_row, host_column_id,
                                 destination_string_id) == CLM_BPA_FAILURE,
               "append sparse rows with missing cells");
    CLM_ASSERT(clm_bpa_create(source_string_id, CLM_BPA_TYPE_STRING) ==
                       CLM_BPA_SUCCESS &&
                   clm_bpa_set_string(source_string_id, "alpha", 6) == 5 &&
                   clm_bpa_create(destination_string_id, CLM_BPA_TYPE_STRING) ==
                       CLM_BPA_SUCCESS &&
                   clm_table_set(table_id, first_row, host_column_id,
                                 source_string_id) == CLM_BPA_SET_SUCCESS &&
                   clm_table_cell_contains(table_id, first_row, host_column_id),
               "copy a BPA string into a present table cell");
    CLM_ASSERT(clm_bpa_set_string(source_string_id, "changed", 8) == 7 &&
                   clm_table_get(table_id, first_row, host_column_id,
                                 destination_string_id) == CLM_BPA_GET_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(destination_string_id)->data,
                          "alpha") == 0,
               "table cell owns an independent copy of its source");
    CLM_ASSERT(clm_table_set_from(table_id, first_row, port_column_id, "") ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_table_cell_contains(table_id, first_row, port_column_id) &&
                   clm_table_get(table_id, first_row, port_column_id,
                                 destination_string_id) == CLM_BPA_GET_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(destination_string_id)->data,
                          "") == 0,
               "distinguish a present empty string from a missing cell");

    const clm_int inserted_row = clm_table_insert_row(table_id, 0);
    CLM_ASSERT(inserted_row == 0 && clm_table_row_count(table_id) == 3 &&
                   !clm_table_cell_contains(table_id, inserted_row, host_column_id) &&
                   clm_table_cell_contains(table_id, 1, host_column_id),
               "insert a row and shift existing row indexes");
    const char invalid_utf8[] = {(char)0xC0, (char)0xAF, '\0'};
    CLM_ASSERT(clm_table_set_from(table_id, inserted_row, host_column_id,
                                  invalid_utf8) == CLM_BPA_FAILURE &&
                   clm_table_set_from(table_id, 99, host_column_id, "bad row") ==
                       CLM_BPA_FAILURE &&
                   clm_table_set_from(table_id, inserted_row, 75999, "bad column") ==
                       CLM_BPA_FAILURE,
               "reject invalid UTF-8 and unknown row or column");

    CLM_ASSERT(clm_table_add_column(table_id, "table_note") ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_table_column_count(table_id) == 3 &&
                   !clm_table_cell_contains(table_id, first_row + 1, note_column_id) &&
                   clm_table_set_from(table_id, 1, note_column_id, "keep") ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_table_remove_column(table_id, port_column_id) ==
                       CLM_BPA_SUCCESS &&
                   clm_table_column_count(table_id) == 2 &&
                   !clm_table_cell_contains(table_id, 1, port_column_id),
               "add sparse columns and remove a column with its cells");
    CLM_ASSERT(clm_table_remove_row(table_id, 0) == CLM_BPA_SUCCESS &&
                   clm_table_row_count(table_id) == 2 &&
                   clm_table_cell_contains(table_id, 0, host_column_id) &&
                   clm_table_remove_cell(table_id, 0, host_column_id) ==
                       CLM_BPA_SUCCESS &&
                   !clm_table_cell_contains(table_id, 0, host_column_id),
               "remove rows and make individual cells missing");
    CLM_ASSERT(clm_table_clear_rows(table_id) == CLM_BPA_SUCCESS &&
                   clm_table_row_count(table_id) == 0 &&
                   clm_table_column_count(table_id) == 2 &&
                   clm_table_append_row(table_id) == 0,
               "clear and reuse rows without removing the schema");
    CLM_ASSERT(clm_table_delete(table_id) == CLM_BPA_SUCCESS &&
                   clm_table_row_count(table_id) == CLM_BPA_FAILURE &&
                   clm_bpa_delete(source_string_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(destination_string_id) == CLM_BPA_SUCCESS,
               "delete table resources and caller-owned strings");
    CLM_ASSERT(clm_id_remove("table_host") == CLM_BPA_SUCCESS &&
                   clm_id_remove("table_port") == CLM_BPA_SUCCESS &&
                   clm_id_remove("table_note") == CLM_BPA_SUCCESS &&
                   clm_id_remove("table_host_alias") == CLM_BPA_SUCCESS,
               "remove table test column mappings");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
