/*
* =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#pragma once

#include "test.hpp"
#include "../clm/console.hpp"

#include <cmath>
#include <cstring>

static inline std::FILE* clm_console_test_tmpfile()
{
#if defined(_MSC_VER)
    std::FILE* stream = nullptr;
    return tmpfile_s(&stream) == 0 ? stream : nullptr;
#else
    return std::tmpfile();
#endif
}

static inline bool clm_console_test_stream_equals(
    std::FILE* stream,
    const char* expected)
{
    char actual[256];
    const std::size_t expected_length = std::strlen(expected);
    const std::size_t actual_length =
        std::fread(actual, 1, sizeof(actual), stream);
    return actual_length == expected_length &&
           std::memcmp(actual, expected, expected_length) == 0 &&
           std::ferror(stream) == 0;
}

static inline int test_clm_console()
{
    clm_test_result result{0, 0};
    std::FILE* output = clm_console_test_tmpfile();
    std::FILE* input = clm_console_test_tmpfile();
    clm_test_begin("CLM - Console");

    CLM_ASSERT(clm::console::init() == CLM_CONSOLE_SUCCESS);
    CLM_ASSERT(clm::console::restore() == CLM_CONSOLE_SUCCESS);
    CLM_ASSERT(clm::console::flush() == CLM_CONSOLE_SUCCESS);
    CLM_ASSERT(clm::console::write(nullptr) ==
               CLM_CONSOLE_INVALID_ARGUMENT);
    CLM_ASSERT(clm::console::write_line(nullptr) ==
               CLM_CONSOLE_INVALID_ARGUMENT);
    CLM_ASSERT(clm::console::write_character(nullptr) ==
               CLM_CONSOLE_INVALID_ARGUMENT);
    CLM_ASSERT(clm::console::write_effect(nullptr, 0.0) ==
               CLM_CONSOLE_INVALID_ARGUMENT);
    CLM_ASSERT(clm::console::wait(-1.0) ==
               CLM_CONSOLE_INVALID_ARGUMENT);
    CLM_ASSERT(clm::console::wait(NAN) ==
               CLM_CONSOLE_INVALID_ARGUMENT);

    CLM_ASSERT(output != nullptr);
    if (output != nullptr) {
        CLM_ASSERT(clm::console::clear_to(output) ==
                   CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::set_cursor_position_to(
                       output, {2, 4}) == CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::set_cursor_position_to(
                       output, {-1, 0}) == CLM_CONSOLE_INVALID_ARGUMENT);
        CLM_ASSERT(clm::console::set_cursor_visible_to(
                       output, false) == CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::set_cursor_visible_to(
                       output, true) == CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_to(output, "text") ==
                   CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_line_to(output, "line") ==
                   CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_format_to(
                       output, " value=%d %s", 42, "ok") ==
                   CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_line_format_to(
                       output, "line:%02d", 7) == CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_format_to(
                       output, nullptr) == CLM_CONSOLE_INVALID_ARGUMENT);
        CLM_ASSERT(clm::console::write_line_format_to(
                       nullptr, "%d", 1) == CLM_CONSOLE_INVALID_ARGUMENT);
        CLM_ASSERT(clm::console::write_character_to(
                       output, "X") == CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_character_to(
                       output, "") == CLM_CONSOLE_INVALID_ARGUMENT);
        CLM_ASSERT(clm::console::write_character_to(
                       output, "XY") == CLM_CONSOLE_INVALID_ARGUMENT);
        CLM_ASSERT(clm::console::write_character_to(
                       output, "\xC3\xA9") == CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_color_to(
                       output, "red", CONSOLE_COLOR_RED) ==
                   CLM_CONSOLE_SUCCESS);
        CLM_ASSERT(clm::console::write_effect_to(
                       output, "go", 0.0) == CLM_CONSOLE_SUCCESS);
        std::rewind(output);
        CLM_ASSERT(clm_console_test_stream_equals(
            output,
            "\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25h"
            "textline\n value=42 okline:07\nX\xC3\xA9"
            "\x1b[31mred\x1b[0mgo"));
        std::fclose(output);
    }

    CLM_ASSERT(input != nullptr);
    if (input != nullptr) {
        char line[16];
        std::fputs("hello\r\nlast", input);
        std::rewind(input);
        CLM_ASSERT(clm::console::read_line_from(
                       input, line, sizeof(line)) == CLM_CONSOLE_SUCCESS &&
                   std::strcmp(line, "hello") == 0);
        CLM_ASSERT(clm::console::read_line_from(
                       input, line, sizeof(line)) == CLM_CONSOLE_SUCCESS &&
                   std::strcmp(line, "last") == 0);
        CLM_ASSERT(clm::console::read_line_from(
                       input, line, sizeof(line)) == CLM_CONSOLE_FAILURE);
        std::fclose(input);
    }

    input = clm_console_test_tmpfile();
    CLM_ASSERT(input != nullptr);
    if (input != nullptr) {
        char line[8];
        std::fputs("123456789\nok\n", input);
        std::rewind(input);
        CLM_ASSERT(clm::console::read_line_from(
                       input, line, sizeof(line)) == CLM_CONSOLE_FAILURE);
        CLM_ASSERT(clm::console::read_line_from(
                       input, line, sizeof(line)) == CLM_CONSOLE_SUCCESS &&
                   std::strcmp(line, "ok") == 0);
        std::fclose(input);
    }

    input = clm_console_test_tmpfile();
    CLM_ASSERT(input != nullptr);
    if (input != nullptr) {
        char character = '\0';
        std::fputs("Q", input);
        std::rewind(input);
        CLM_ASSERT(clm::console::read_character_from(
                       input, &character) == CLM_CONSOLE_SUCCESS &&
                   character == 'Q');
        std::fclose(input);
    }

    input = clm_console_test_tmpfile();
    output = clm_console_test_tmpfile();
    CLM_ASSERT(input != nullptr && output != nullptr);
    if (input != nullptr && output != nullptr) {
        bool confirmed = false;
        std::fputs(" yes \n", input);
        std::rewind(input);
        CLM_ASSERT(clm::console::confirm_from(
                       input, output, "Continue?", &confirmed) ==
                       CLM_CONSOLE_SUCCESS &&
                   confirmed);
        std::fclose(input);
        std::fclose(output);
    } else {
        if (input != nullptr)
            std::fclose(input);
        if (output != nullptr)
            std::fclose(output);
    }

    input = clm_console_test_tmpfile();
    output = clm_console_test_tmpfile();
    CLM_ASSERT(input != nullptr && output != nullptr);
    if (input != nullptr && output != nullptr) {
        bool confirmed = true;
        std::fputs("no\n", input);
        std::rewind(input);
        CLM_ASSERT(clm::console::confirm_from(
                       input, output, nullptr, &confirmed) ==
                       CLM_CONSOLE_SUCCESS &&
                   !confirmed);
        std::fclose(input);
        std::fclose(output);
    } else {
        if (input != nullptr)
            std::fclose(input);
        if (output != nullptr)
            std::fclose(output);
    }

    input = clm_console_test_tmpfile();
    output = clm_console_test_tmpfile();
    CLM_ASSERT(input != nullptr && output != nullptr);
    if (input != nullptr && output != nullptr) {
        bool confirmed = true;
        std::fputs("yesterday\n", input);
        std::rewind(input);
        CLM_ASSERT(clm::console::confirm_from(
                       input, output, nullptr, &confirmed) ==
                       CLM_CONSOLE_INVALID_ARGUMENT &&
                   confirmed);
        std::fclose(input);
        std::fclose(output);
    } else {
        if (input != nullptr)
            std::fclose(input);
        if (output != nullptr)
            std::fclose(output);
    }

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
