#pragma once

#include "test.h"
#include "../clm/string_format.h"

static int clm_string_format_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - String Format");
    const clm_string_format_lex_rules rules = {
        "{}:,", "#", "/*", "*/", "\"", '\\',
        CLM_STRING_FORMAT_LEX_EMIT_NEWLINES
    };
    const char input[] = "/* ignored */{\"key\":\"value\"} # tail\n";
    clm_string_format_lexer lexer;
    clm_string_format_token token;
    clm_string_format_parser parser;
    const clm_id output_id = 76600;
    const clm_id probe_root_id = 76601;
    const clm_id probe_key_id = 76610;
    clm_string_format_writer writer;
    memset(&parser, 0, sizeof(parser));
    CLM_ASSERT(clm_string_format_lexer_init(&lexer, input, sizeof(input) - 1,
                                             &rules) == CLM_BPA_SUCCESS &&
                   clm_string_format_lexer_next(&lexer, &token) == CLM_BPA_SUCCESS &&
                   token.kind == CLM_STRING_FORMAT_TOKEN_DELIMITER &&
                   clm_string_format_lexer_next(&lexer, &token) == CLM_BPA_SUCCESS &&
                   token.kind == CLM_STRING_FORMAT_TOKEN_QUOTED,
               "skip comments and return source-span tokens");
    parser.lexer = lexer;
    CLM_ASSERT(clm_string_format_token_equals(&parser, &token, "\"key\""),
               "token spans refer to exact source bytes");
    CLM_ASSERT(clm_string_format_parser_init(&parser, "{}", 2, &rules) ==
                       CLM_BPA_SUCCESS &&
                   clm_string_format_parser_expect(&parser,
                       CLM_STRING_FORMAT_TOKEN_DELIMITER, "{", &token) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_string_format_parser_expect(&parser,
                       CLM_STRING_FORMAT_TOKEN_DELIMITER, "}", &token) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_string_format_parser_peek(&parser, &token) ==
                       CLM_BPA_GET_SUCCESS &&
                   token.kind == CLM_STRING_FORMAT_TOKEN_END,
               "parser cursor peek/take/expect reaches end of input");
    clm_id child_id = 0;
    CLM_ASSERT(clm_id_register("format_probe_child", probe_key_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_tree_new(probe_root_id) == CLM_BPA_SUCCESS &&
                   clm_string_format_internal_add_object(probe_root_id,
                       probe_key_id, false, &child_id) == CLM_BPA_SUCCESS &&
                   clm_tree_is_object(child_id) &&
                   clm_tree_delete(probe_root_id) == CLM_BPA_SUCCESS &&
                   clm_id_remove("format_probe_child") == CLM_BPA_SUCCESS,
               "resolve a mapped key and attach a fresh child object");
    clm_string_format_writer_init(&writer);
    CLM_ASSERT(clm_string_format_writer_append_quoted(&writer, "a\nb", 3) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   clm_string_new(output_id, "") &&
                   clm_string_format_writer_commit(&writer, output_id) ==
                       CLM_FORMAT_SERIALIZE_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(output_id)->data,
                          "\"a\\nb\"") == 0,
               "shared output writer escapes quoted control characters");
    clm_string_format_writer_dispose(&writer);
    CLM_ASSERT(clm_string_delete(output_id), "release format writer output");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
