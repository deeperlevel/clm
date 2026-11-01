#pragma once

#include "test.hpp"
#include "../clm/clp.hpp"

static int clm_clp_tests(void)
{
	clm_test_result result = {0};
	clm_test_begin("CLM - Command-Line Parser");
	const clm_id dictionary_id = 95000;
	const clm_id argv_dictionary_id = 95001;
	const clm_id string_dictionary_id = 95002;
	const clm_id invalid_dictionary_id = 95003;
	const clm_id known_option_id = 95010;
	clm_id host_id = 0;
	clm_id port_id = 0;
	clm_id short_id = 0;
	clm_id verbose_id = 0;
	clm_id output_id = 0;
	clm_id empty_id = 0;
	clm_id mode_id = 0;
	clm_id message_id = 0;
	clm_id temporary_id = 0;
	char value[128] = {0};
	const char* arguments[] = {
		"--host=127.0.0.1", "--port", "8080", "-p=9", "--verbose",
		"output=file.txt", "--empty=", "--mode", "old", "--mode", "new"
	};
	const char* argv[] = {"app", "--known", "yes"};
	const char* invalid_arguments[] = {"--temporary", "yes", "positional"};

	CLM_ASSERT(clm_id_register("known", known_option_id) == CLM_BPA_SET_SUCCESS &&
		clm_clp_parse_tokens((clm_int)(sizeof(arguments) / sizeof(arguments[0])),
			arguments, dictionary_id) == CLM_CLP_OPERATION_SUCCESS &&
		clm_id_try_get("host", &host_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("port", &port_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("p", &short_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("verbose", &verbose_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("output", &output_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("empty", &empty_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("mode", &mode_id) == CLM_BPA_GET_SUCCESS,
		"parse options and auto-register names");
	CLM_ASSERT(clm_typed_dictionary_get_to(dictionary_id, host_id, value,
		sizeof(value)) == CLM_BPA_GET_SUCCESS &&
		strcmp(value, "127.0.0.1") == 0 &&
		clm_typed_dictionary_get_to(dictionary_id, port_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "8080") == 0 &&
		clm_typed_dictionary_get_to(dictionary_id, short_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "9") == 0 &&
		clm_typed_dictionary_get_to(dictionary_id, verbose_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "true") == 0 &&
		clm_typed_dictionary_get_to(dictionary_id, output_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "file.txt") == 0 &&
		clm_typed_dictionary_get_to(dictionary_id, empty_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "") == 0 &&
		clm_typed_dictionary_get_to(dictionary_id, mode_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "new") == 0,
		"store string values, true flags, empty values, and last repeated value");
	CLM_ASSERT(clm_clp_parse_argv(3, argv, argv_dictionary_id) ==
		CLM_CLP_OPERATION_SUCCESS &&
		clm_typed_dictionary_get_to(argv_dictionary_id, known_option_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "yes") == 0,
		"skip argv zero and use a caller-registered option ID");
	CLM_ASSERT(clm_clp_parse_string(
		"--message \"two words \\\"quoted\\\"\" --enabled", string_dictionary_id) ==
		CLM_CLP_OPERATION_SUCCESS &&
		clm_id_try_get("message", &message_id) == CLM_BPA_GET_SUCCESS &&
		clm_id_try_get("enabled", &verbose_id) == CLM_BPA_GET_SUCCESS &&
		clm_typed_dictionary_get_to(string_dictionary_id, message_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS &&
		strcmp(value, "two words \"quoted\"") == 0 &&
		clm_typed_dictionary_get_to(string_dictionary_id, verbose_id, value,
			sizeof(value)) == CLM_BPA_GET_SUCCESS && strcmp(value, "true") == 0,
		"tokenize quoted strings and parse a command-line string");
	CLM_ASSERT(clm_clp_parse_tokens(3, invalid_arguments,
		invalid_dictionary_id) == CLM_CLP_OPERATION_INVALID_SYNTAX &&
		!clm_bpa_is_existing(invalid_dictionary_id) &&
		clm_id_try_get("temporary", &temporary_id) == CLM_BPA_FAILURE,
		"reject positional arguments and roll back partial resources");
	CLM_ASSERT(clm_clp_parse_tokens(0, NULL, dictionary_id) ==
		CLM_CLP_OPERATION_DUPLICATE_ID &&
		clm_typed_dictionary_delete(dictionary_id) == CLM_BPA_SUCCESS &&
		clm_typed_dictionary_delete(argv_dictionary_id) == CLM_BPA_SUCCESS &&
		clm_typed_dictionary_delete(string_dictionary_id) == CLM_BPA_SUCCESS,
		"reject reused output IDs and release parsed dictionaries");
	CLM_ASSERT(clm_id_remove("known") == CLM_BPA_SUCCESS &&
		clm_id_remove("host") == CLM_BPA_SUCCESS &&
		clm_id_remove("port") == CLM_BPA_SUCCESS &&
		clm_id_remove("p") == CLM_BPA_SUCCESS &&
		clm_id_remove("verbose") == CLM_BPA_SUCCESS &&
		clm_id_remove("output") == CLM_BPA_SUCCESS &&
		clm_id_remove("empty") == CLM_BPA_SUCCESS &&
		clm_id_remove("mode") == CLM_BPA_SUCCESS &&
		clm_id_remove("message") == CLM_BPA_SUCCESS &&
		clm_id_remove("enabled") == CLM_BPA_SUCCESS,
		"remove parser-created and test option mappings");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}

static int test_clm_clp(void)
{
    return clm_clp_tests();
}

