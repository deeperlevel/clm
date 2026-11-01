#pragma once

#include "test.h"
#include "../clm/console.h"
#include "../clm/string_json.h"
#include "../clm/string_yaml.h"
#include "../clm/tree.h"
#include <math.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

static FILE* clm_console_test_tmpfile(void)
{
#if defined(_MSC_VER)
    FILE* stream = NULL;
    return tmpfile_s(&stream) == 0 ? stream : NULL;
#else
    return tmpfile();
#endif
}

static int clm_console_test_stream_equals(FILE* stream, const char* expected)
{
    char actual[256];
    const size_t expected_length = strlen(expected);
    const size_t actual_length = fread(actual, 1, sizeof(actual), stream);
    return actual_length == expected_length &&
           memcmp(actual, expected, expected_length) == 0 &&
           ferror(stream) == 0;
}

static int clm_console_test_capture_tree(
    clm_console_status (*write_tree)(clm_id), clm_id id, const char* expected)
{
    FILE* output = clm_console_test_tmpfile();
    if (output == NULL || fflush(stdout) != 0) {
        if (output != NULL)
            fclose(output);
        return 0;
    }
#if defined(_WIN32)
    const int destination = _fileno(stdout);
    const int saved = _dup(destination);
    const int redirected = saved >= 0 ? _dup2(_fileno(output), destination) : -1;
#else
    const int destination = fileno(stdout);
    const int saved = dup(destination);
    const int redirected = saved >= 0 ? dup2(fileno(output), destination) : -1;
#endif
    int success = 0;
    if (redirected >= 0) {
        const clm_console_status status = write_tree(id);
        success = fflush(stdout) == 0 && status == CLM_CONSOLE_SUCCESS;
    }
    if (saved >= 0) {
#if defined(_WIN32)
        if (_dup2(saved, destination) < 0)
            success = 0;
        _close(saved);
#else
        if (dup2(saved, destination) < 0)
            success = 0;
        close(saved);
#endif
    }
    rewind(output);
    success = success && clm_console_test_stream_equals(output, expected);
    fclose(output);
    return success;
}

static clm_stringtify_status clm_console_test_tree_interceptor(
    clm_id id, const clm_stringtify_options* options,
    clm_stringtify_writer* writer, void* user)
{
    (void)id;
    (void)options;
    ++*(int*)user;
    return clm_stringtify_writer_append_cstr(writer, "intercepted");
}

static int clm_console_tests(void)
{
    clm_test_result result = {0};
    FILE* output = clm_console_test_tmpfile();
    FILE* input = clm_console_test_tmpfile();
    clm_test_begin("CLM - Console");

    CLM_ASSERT(clm_console_init() == CLM_CONSOLE_SUCCESS, "clm_console_init() == CLM_CONSOLE_SUCCESS");
    CLM_ASSERT(clm_console_restore() == CLM_CONSOLE_SUCCESS, "clm_console_restore() == CLM_CONSOLE_SUCCESS");
    CLM_ASSERT(clm_console_flush() == CLM_CONSOLE_SUCCESS, "clm_console_flush() == CLM_CONSOLE_SUCCESS");
    CLM_ASSERT(clm_console_write(NULL) == CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_write(NULL) == CLM_CONSOLE_INVALID_ARGUMENT");
    CLM_ASSERT(clm_console_error(NULL) == CLM_CONSOLE_INVALID_ARGUMENT &&
                   clm_console_error_format(NULL) == CLM_CONSOLE_INVALID_ARGUMENT,
               "stderr writers reject NULL arguments");
    CLM_ASSERT(clm_console_error("console error test") == CLM_CONSOLE_SUCCESS &&
                   clm_console_error_format(":%d %s", 42, "ok") == CLM_CONSOLE_SUCCESS,
               "write plain and formatted diagnostics to stderr");
    CLM_ASSERT(clm_console_write_line(NULL) == CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_write_line(NULL) == CLM_CONSOLE_INVALID_ARGUMENT");
    CLM_ASSERT(clm_console_write_character(NULL) == CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_write_character(NULL) == CLM_CONSOLE_INVALID_ARGUMENT");
    CLM_ASSERT(clm_console_write_effect(NULL, 0.0) == CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_write_effect(NULL, 0.0) == CLM_CONSOLE_INVALID_ARGUMENT");
    CLM_ASSERT(clm_console_wait(-1.0) == CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_wait(-1.0) == CLM_CONSOLE_INVALID_ARGUMENT");
    CLM_ASSERT(clm_console_wait(NAN) == CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_wait(NAN) == CLM_CONSOLE_INVALID_ARGUMENT");

    CLM_ASSERT(clm_console_write_tree_as_yaml(0) == CLM_CONSOLE_INVALID_ARGUMENT &&
                   clm_console_write_tree_as_json(0) == CLM_CONSOLE_INVALID_ARGUMENT,
               "format writers reject missing tree IDs");
    const clm_id source_id = 0x434F4E530001;
    const clm_id root_id = 0x434F4E530002;
    const clm_id yaml_id = 0x434F4E530003;
    const int parsed = clm_test_parse_text(source_id,
        "[1,\"a\\\"[,]{}\",{},[],[true,null]]", root_id, clm_string_json_loads) ==
        CLM_FORMAT_DESERIALIZE_SUCCESS;
    CLM_ASSERT(parsed, "create a console format test tree");
    if (parsed) {
        int intercepted = 0;
        clm_console_intercept_object_writes(clm_console_test_tree_interceptor, &intercepted);
        CLM_ASSERT(clm_console_test_capture_tree(clm_console_write_tree_as_json, root_id,
                       "[\n  1,\n  \"a\\\"[,]{}\",\n  {},\n  [],\n  [\n    true,\n    null\n  ]\n]"),
                   "JSON interceptor preserves escapes and indents nested containers");
        const int serialized = clm_string_new(yaml_id, "") &&
            clm_string_yaml_dumps(root_id, yaml_id) == CLM_FORMAT_SERIALIZE_SUCCESS;
        CLM_ASSERT(serialized, "serialize expected YAML output");
        if (serialized) {
            CLM_ASSERT(clm_console_test_capture_tree(clm_console_write_tree_as_yaml,
                           root_id, (const char*)clm_bpa_find_block(yaml_id)->data),
                       "YAML interceptor writes the YAML serializer output");
        }
        CLM_ASSERT(intercepted == 0 && clm_tree_array_count(root_id) == 5,
                   "format interceptors bypass global interception and preserve the tree");
        CLM_ASSERT(clm_console_test_capture_tree(clm_console_write_tree, root_id, "intercepted") &&
                       intercepted == 2,
                   "format writes leave the global interceptor and user context unchanged");
        clm_console_do_not_intercept_object_writes();
        (void)clm_string_delete(yaml_id);
        CLM_ASSERT(clm_tree_delete(root_id) == CLM_BPA_SUCCESS,
                   "release console format test tree");
    }

    CLM_ASSERT(output != NULL, "output != NULL");
    if (output != NULL) {
        CLM_ASSERT(clm_console_clear_to(output) == CLM_CONSOLE_SUCCESS, "clm_console_clear_to(output) == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_set_cursor_position_to(
                       output, (clm_position){.x = 2, .y = 4}) == CLM_CONSOLE_SUCCESS, "clm_console_set_cursor_position_to( output, (clm_position){.x = 2, .y = 4}) == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_set_cursor_position_to(
                       output, (clm_position){.x = -1, .y = 0}) ==
                       CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_set_cursor_position_to( output, (clm_position){.x = -1, .y = 0}) == CLM_CONSOLE_INVALID_ARGUMENT");
        CLM_ASSERT(clm_console_set_cursor_visible_to(output, false) ==
                       CLM_CONSOLE_SUCCESS, "clm_console_set_cursor_visible_to(output, false) == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_set_cursor_visible_to(output, true) ==
                       CLM_CONSOLE_SUCCESS, "clm_console_set_cursor_visible_to(output, true) == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_to(output, "text") == CLM_CONSOLE_SUCCESS, "clm_console_write_to(output, \"text\") == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_line_to(output, "line") == CLM_CONSOLE_SUCCESS, "clm_console_write_line_to(output, \"line\") == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_format_to(output, " value=%d %s", 42, "ok") ==
                       CLM_CONSOLE_SUCCESS,
                   "clm_console_write_format_to(output, \" value=%d %s\", 42, \"ok\") == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_line_format_to(output, "line:%02d", 7) ==
                       CLM_CONSOLE_SUCCESS,
                   "clm_console_write_line_format_to(output, \"line:%02d\", 7) == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_format_to(output, NULL) ==
                       CLM_CONSOLE_INVALID_ARGUMENT,
                   "clm_console_write_format_to(output, NULL) == CLM_CONSOLE_INVALID_ARGUMENT");
        CLM_ASSERT(clm_console_write_line_format_to(NULL, "%d", 1) ==
                       CLM_CONSOLE_INVALID_ARGUMENT,
                   "clm_console_write_line_format_to(NULL, \"%d\", 1) == CLM_CONSOLE_INVALID_ARGUMENT");
        CLM_ASSERT(clm_console_write_character_to(output, "X") ==
                       CLM_CONSOLE_SUCCESS, "clm_console_write_character_to(output, \"X\") == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_character_to(output, "") ==
                       CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_write_character_to(output, \"\") == CLM_CONSOLE_INVALID_ARGUMENT");
        CLM_ASSERT(clm_console_write_character_to(output, "XY") ==
                       CLM_CONSOLE_INVALID_ARGUMENT, "clm_console_write_character_to(output, \"XY\") == CLM_CONSOLE_INVALID_ARGUMENT");
        CLM_ASSERT(clm_console_write_character_to(output, "\xC3\xA9") ==
                       CLM_CONSOLE_SUCCESS, "clm_console_write_character_to(output, \"\\xC3\\xA9\") == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_color_to(
                       output, "red", CONSOLE_COLOR_RED) == CLM_CONSOLE_SUCCESS, "clm_console_write_color_to( output, \"red\", CONSOLE_COLOR_RED) == CLM_CONSOLE_SUCCESS");
        CLM_ASSERT(clm_console_write_effect_to(output, "go", 0.0) ==
                       CLM_CONSOLE_SUCCESS, "clm_console_write_effect_to(output, \"go\", 0.0) == CLM_CONSOLE_SUCCESS");
        rewind(output);
        CLM_ASSERT(clm_console_test_stream_equals(
                       output,
                       "\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25h"
                       "textline\n value=42 okline:07\nX\xC3\xA9"
                       "\x1b[31mred\x1b[0mgo"), "clm_console_test_stream_equals( output, expected ANSI and formatted text)");
        fclose(output);
    }

    CLM_ASSERT(input != NULL, "input != NULL");
    if (input != NULL) {
        char line[16];
        fputs("hello\r\nlast", input);
        rewind(input);
        CLM_ASSERT(clm_console_read_line_from(input, line, sizeof(line)) ==
                       CLM_CONSOLE_SUCCESS && strcmp(line, "hello") == 0, "clm_console_read_line_from(input, line, sizeof(line)) == CLM_CONSOLE_SUCCESS && strcmp(line, \"hello\") == 0");
        CLM_ASSERT(clm_console_read_line_from(input, line, sizeof(line)) ==
                       CLM_CONSOLE_SUCCESS && strcmp(line, "last") == 0, "clm_console_read_line_from(input, line, sizeof(line)) == CLM_CONSOLE_SUCCESS && strcmp(line, \"last\") == 0");
        CLM_ASSERT(clm_console_read_line_from(input, line, sizeof(line)) ==
                       CLM_CONSOLE_FAILURE, "clm_console_read_line_from(input, line, sizeof(line)) == CLM_CONSOLE_FAILURE");
        fclose(input);
    }

    input = clm_console_test_tmpfile();
    CLM_ASSERT(input != NULL, "input != NULL");
    if (input != NULL) {
        char line[8];
        fputs("123456789\nok\n", input);
        rewind(input);
        CLM_ASSERT(clm_console_read_line_from(input, line, sizeof(line)) ==
                       CLM_CONSOLE_FAILURE, "clm_console_read_line_from(input, line, sizeof(line)) == CLM_CONSOLE_FAILURE");
        CLM_ASSERT(clm_console_read_line_from(input, line, sizeof(line)) ==
                       CLM_CONSOLE_SUCCESS && strcmp(line, "ok") == 0, "clm_console_read_line_from(input, line, sizeof(line)) == CLM_CONSOLE_SUCCESS && strcmp(line, \"ok\") == 0");
        fclose(input);
    }

    input = clm_console_test_tmpfile();
    CLM_ASSERT(input != NULL, "input != NULL");
    if (input != NULL) {
        char character = '\0';
        fputs("Q", input);
        rewind(input);
        CLM_ASSERT(clm_console_read_character_from(input, &character) ==
                       CLM_CONSOLE_SUCCESS && character == 'Q', "clm_console_read_character_from(input, &character) == CLM_CONSOLE_SUCCESS && character == 'Q'");
        fclose(input);
    }

    input = clm_console_test_tmpfile();
    output = clm_console_test_tmpfile();
    CLM_ASSERT(input != NULL && output != NULL, "input != NULL && output != NULL");
    if (input != NULL && output != NULL) {
        bool confirmed = false;
        fputs(" yes \n", input);
        rewind(input);
        CLM_ASSERT(clm_console_confirm_from(input, output, "Continue?", &confirmed) ==
                       CLM_CONSOLE_SUCCESS && confirmed, "clm_console_confirm_from(input, output, \"Continue?\", &confirmed) == CLM_CONSOLE_SUCCESS && confirmed");
        fclose(input);
        fclose(output);
    } else {
        if (input != NULL)
            fclose(input);
        if (output != NULL)
            fclose(output);
    }

    input = clm_console_test_tmpfile();
    output = clm_console_test_tmpfile();
    CLM_ASSERT(input != NULL && output != NULL, "input != NULL && output != NULL");
    if (input != NULL && output != NULL) {
        bool confirmed = true;
        fputs("no\n", input);
        rewind(input);
        CLM_ASSERT(clm_console_confirm_from(input, output, NULL, &confirmed) ==
                       CLM_CONSOLE_SUCCESS && !confirmed, "clm_console_confirm_from(input, output, NULL, &confirmed) == CLM_CONSOLE_SUCCESS && !confirmed");
        fclose(input);
        fclose(output);
    } else {
        if (input != NULL)
            fclose(input);
        if (output != NULL)
            fclose(output);
    }

    input = clm_console_test_tmpfile();
    output = clm_console_test_tmpfile();
    CLM_ASSERT(input != NULL && output != NULL, "input != NULL && output != NULL");
    if (input != NULL && output != NULL) {
        bool confirmed = true;
        fputs("yesterday\n", input);
        rewind(input);
        CLM_ASSERT(clm_console_confirm_from(input, output, NULL, &confirmed) ==
                       CLM_CONSOLE_INVALID_ARGUMENT && confirmed, "clm_console_confirm_from(input, output, NULL, &confirmed) == CLM_CONSOLE_INVALID_ARGUMENT && confirmed");
        fclose(input);
        fclose(output);
    } else {
        if (input != NULL)
            fclose(input);
        if (output != NULL)
            fclose(output);
    }

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
