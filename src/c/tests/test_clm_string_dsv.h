#pragma once

#include "test.h"
#include "../clm/string_dsv.h"

static int clm_string_dsv_tests(void)
{
	clm_test_result result = {0};
	clm_test_begin("CLM - String DSV");
	const clm_id table_id = 76600;
	const clm_id source_string_id = 76601;
	const clm_id output_string_id = 76602;
	const clm_id file_path_id = 76603;
	const clm_id file_table_id = 76604;
	const clm_id separator_string_id = 76605;
	const clm_id custom_table_id = 76606;
	const clm_id headerless_table_id = 76607;
	const clm_id failed_table_id = 76608;
	const clm_id encoded_output_id = 76609;
	const clm_id invalid_separator_table_id = 76610;
	clm_id name_column_id = 0;
	clm_id note_column_id = 0;
	clm_id generated_column_one_id = 0;
	clm_id generated_column_two_id = 0;
	clm_id bad_column_id = 0;
	const char input[] =
		"dsv_name,dsv_note\r\n"
		"Alice,\"comma, and \\\"quoted\\\"\"\r\n"
		"O'Brien,'single '' quote'\n"
		"Escapes,\"backslash \\\\ newline\\n tab\\t\"\n";
	clm_string_dsv_options options = CLM_STRING_DSV_OPTIONS_DEFAULT;
	clm_string_dsv_options headerless_options = {0, false};

	CLM_ASSERT(clm_string_new(source_string_id, input) &&
		clm_string_dsv_loads(source_string_id, table_id) == CLM_BPA_SUCCESS &&
		clm_table_column_count(table_id) == 2 &&
		clm_table_row_count(table_id) == 3 &&
		clm_id_try_get("dsv_name", &name_column_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("dsv_note", &note_column_id) == CLM_BPA_GET_SUCCESS &&
		clm_string_new(output_string_id, "") &&
		clm_table_get(table_id, 0, note_column_id, output_string_id) ==
			CLM_BPA_GET_SUCCESS,
		"parse a header and auto-register its columns");
	clm_bpa_block_node* cell_block;
	const char* cell_text;
	size_t cell_bytes;
	size_t cell_characters;
	CLM_ASSERT(clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strcmp(cell_text, "comma, and \"quoted\"") == 0 &&
		clm_table_get(table_id, 1, note_column_id, output_string_id) ==
			CLM_BPA_GET_SUCCESS &&
		clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strcmp(cell_text, "single ' quote") == 0 &&
		clm_table_get(table_id, 2, note_column_id, output_string_id) ==
			CLM_BPA_GET_SUCCESS &&
		clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strcmp(cell_text, "backslash \\ newline\n tab\t") == 0,
		"decode quoted delimiters and the supported escape forms");
	CLM_ASSERT(clm_string_dsv_dumps(table_id, output_string_id) == CLM_BPA_SUCCESS &&
		clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strstr(cell_text, "\"dsv_name\",\"dsv_note\"\n") == cell_text &&
		strstr(cell_text, "\"Alice\",\"comma, and \\\"quoted\\\"\"\n") != NULL &&
		strstr(cell_text, "\\n tab\\t\"\n") != NULL,
		"serialize quoted rows with escaped control characters");
	const clm_byte original_encoding = clm_string_get_default_encoding();
	const bool changed_encoding = clm_string_set_default_encoding(
		CLM_STRING_ENCODING_UTF16LE) == CLM_BPA_SUCCESS;
	const bool created_encoded_output = changed_encoding &&
		clm_string_new(encoded_output_id, "");
	const bool serialized_encoded_output = created_encoded_output &&
		clm_string_dsv_dumps(table_id, encoded_output_id) == CLM_BPA_SUCCESS &&
		clm_bpa_find_block(encoded_output_id)->encoding ==
			CLM_STRING_ENCODING_UTF16LE;
	const bool restored_encoding = clm_string_set_default_encoding(
		original_encoding) == CLM_BPA_SUCCESS;
	CLM_ASSERT(serialized_encoded_output && restored_encoding,
		"preserve the preferred encoding of a new CLM output string");

	CLM_ASSERT(clm_string_new(file_path_id, "clm_string_dsv_test.csv") &&
		clm_string_dsv_dump(table_id, file_path_id) == CLM_BPA_SUCCESS &&
		clm_string_dsv_load(file_path_id, file_table_id) == CLM_BPA_SUCCESS &&
		clm_table_row_count(file_table_id) == 3 &&
		clm_file_delete(file_path_id) == CLM_FILE_OPERATION_SUCCESS,
		"dump and load DSV through a CLM filename string");

	CLM_ASSERT(clm_string_set(source_string_id,
		"dsv_pair||dsv_payload\nleft||\"right||side\"\n") >= 0 &&
		clm_string_new(separator_string_id, "||"),
		"create custom-separator DSV input");
	options.separator_string_id = separator_string_id;
	CLM_ASSERT(clm_string_dsv_loads_with_options(source_string_id, custom_table_id,
			&options) == CLM_BPA_SUCCESS &&
		clm_table_column_count(custom_table_id) == 2 &&
		clm_table_row_count(custom_table_id) == 1,
		"accept a two-character CLM string separator");
	CLM_ASSERT(clm_string_set(separator_string_id, "|||") >= 0 &&
		clm_string_dsv_loads_with_options(source_string_id,
			invalid_separator_table_id, &options) == CLM_BPA_FAILURE &&
		!clm_bpa_is_existing(invalid_separator_table_id),
		"reject separators longer than two ASCII bytes");
	CLM_ASSERT(clm_string_set(source_string_id,
		"ignored,header\none,two\nthree,four\n") >= 0 &&
		clm_string_dsv_loads_with_options(source_string_id, headerless_table_id,
			&headerless_options) == CLM_BPA_SUCCESS &&
		clm_table_column_count(headerless_table_id) == 2 &&
		clm_table_row_count(headerless_table_id) == 3 &&
		clm_id_try_get("column_1", &generated_column_one_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("column_2", &generated_column_two_id) == CLM_BPA_GET_SUCCESS &&
		clm_table_get(headerless_table_id, 0, generated_column_one_id,
			output_string_id) == CLM_BPA_GET_SUCCESS &&
		clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strcmp(cell_text, "ignored") == 0 &&
		clm_table_get(headerless_table_id, 2, generated_column_two_id,
			output_string_id) == CLM_BPA_GET_SUCCESS &&
		clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strcmp(cell_text, "four") == 0 &&
		clm_string_dsv_dumps_with_options(headerless_table_id, output_string_id,
			&headerless_options) == CLM_BPA_SUCCESS &&
		clm_string_get_block(output_string_id, &cell_block, &cell_text,
			&cell_bytes, &cell_characters) == CLM_BPA_SUCCESS &&
		strcmp(cell_text,
			"\"ignored\",\"header\"\n\"one\",\"two\"\n\"three\",\"four\"\n") == 0,
		"treat every headerless record as data with generated column names");
	CLM_ASSERT(clm_string_set(source_string_id,
		"dsv_bad_column\n\"invalid\\q\"\n") >= 0 &&
		clm_string_dsv_loads(source_string_id, failed_table_id) == CLM_BPA_FAILURE &&
		!clm_bpa_is_existing(failed_table_id) &&
		clm_id_try_get("dsv_bad_column", &bad_column_id) == CLM_BPA_FAILURE,
		"reject unsupported escapes and roll back auto-registered columns");

	CLM_ASSERT(clm_table_delete(table_id) == CLM_BPA_SUCCESS &&
		clm_table_delete(file_table_id) == CLM_BPA_SUCCESS &&
		clm_table_delete(custom_table_id) == CLM_BPA_SUCCESS &&
		clm_table_delete(headerless_table_id) == CLM_BPA_SUCCESS &&
		clm_string_delete(source_string_id) &&
		clm_string_delete(output_string_id) &&
		(!clm_bpa_is_existing(encoded_output_id) ||
			clm_string_delete(encoded_output_id)) &&
		clm_string_delete(file_path_id) &&
		clm_string_delete(separator_string_id),
		"release DSV tables and CLM strings");
	CLM_ASSERT(clm_id_remove("dsv_name") == CLM_BPA_SUCCESS &&
		clm_id_remove("dsv_note") == CLM_BPA_SUCCESS &&
		clm_id_remove("dsv_pair") == CLM_BPA_SUCCESS &&
		clm_id_remove("dsv_payload") == CLM_BPA_SUCCESS &&
		clm_id_remove("column_1") == CLM_BPA_SUCCESS &&
		clm_id_remove("column_2") == CLM_BPA_SUCCESS,
		"remove DSV column mappings");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}