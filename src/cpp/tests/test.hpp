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

#include "../clm/console.hpp"

typedef struct {
    int passed;
    int failed;
} clm_test_result;

static int clm_tests_passed = 0;
static int clm_tests_failed = 0;
static std::FILE* clm_test_log_file = nullptr;
static int clm_test_log_failed = 0;

#define DIVIDER_SINGLE "-------------------------------------------------------------"
#define DIVIDER_DOUBLE "============================================================="

static inline int clm_test_log_open(const char* path)
{
    if (path == nullptr || clm_test_log_file != nullptr)
        return 0;
#if defined(_MSC_VER)
    std::FILE* stream = nullptr;
    if (fopen_s(&stream, path, "w") != 0)
        return 0;
#else
    std::FILE* stream = std::fopen(path, "w");
    if (stream == nullptr)
        return 0;
#endif
    clm_test_log_file = stream;
    clm_test_log_failed = 0;
    return 1;
}

static inline int clm_test_log_close()
{
    int success = !clm_test_log_failed;
    if (clm_test_log_file == nullptr)
        return 0;
    if (std::fflush(clm_test_log_file) == EOF)
        success = 0;
    if (std::fclose(clm_test_log_file) == EOF)
        success = 0;
    clm_test_log_file = nullptr;
    return success && !clm_test_log_failed;
}

static inline void clm_test_write(const char* text)
{
    clm::console::write(text);
    if (clm_test_log_file != nullptr &&
        clm::console::write_to(clm_test_log_file, text) !=
            CLM_CONSOLE_SUCCESS)
        clm_test_log_failed = 1;
}

static inline void clm_test_write_line(const char* text)
{
    clm::console::write_line(text);
    if (clm_test_log_file != nullptr &&
        (clm::console::write_line_to(clm_test_log_file, text) !=
             CLM_CONSOLE_SUCCESS ||
         std::fflush(clm_test_log_file) == EOF))
        clm_test_log_failed = 1;
}

static inline void clm_test_write_color(const char* text, const char* color)
{
    clm::console::write_color(text, color);
    if (clm_test_log_file != nullptr &&
        clm::console::write_to(clm_test_log_file, text) !=
            CLM_CONSOLE_SUCCESS)
        clm_test_log_failed = 1;
}

static inline void clm_test_write_format(const char* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    clm::console::vwrite_format_to(stdout, format, arguments);
    if (clm_test_log_file != nullptr &&
        clm::console::vwrite_format_to(
            clm_test_log_file, format, arguments) != CLM_CONSOLE_SUCCESS)
        clm_test_log_failed = 1;
    va_end(arguments);
}

static inline void clm_test_write_line_format(const char* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    clm::console::vwrite_line_format_to(stdout, format, arguments);
    if (clm_test_log_file != nullptr &&
        (clm::console::vwrite_line_format_to(
             clm_test_log_file, format, arguments) != CLM_CONSOLE_SUCCESS ||
         std::fflush(clm_test_log_file) == EOF))
        clm_test_log_failed = 1;
    va_end(arguments);
}

static inline void clm_test_begin(const char* title)
{
    clm_test_write_line("");
    clm_test_write_line(DIVIDER_DOUBLE);
    clm_test_write_color(title, CONSOLE_COLOR_YELLOW);
    clm_test_write_line("");
    clm_test_write_line(DIVIDER_DOUBLE);
}

static inline void clm_test_assert(
    clm_test_result& result,
    const bool condition,
    const char* description)
{
    if (condition) {
        clm_test_write("    ");
        clm_test_write_color("PASSED:", CONSOLE_COLOR_GREEN);
        clm_test_write(" ");
        clm_test_write_line(description);
        ++result.passed;
    } else {
        ++result.failed;
        clm_test_write("    ");
        clm_test_write_color("FAILED:", CONSOLE_COLOR_RED);
        clm_test_write(" ");
        clm_test_write_line(description);
    }
}

    #define CLM_TEST_ASSERT_SELECT(_1, _2, NAME, ...) NAME
    #define CLM_TEST_ASSERT_ONE(condition) \
    clm_test_assert(result, (condition), #condition)
    #define CLM_TEST_ASSERT_TWO(condition, description) \
    clm_test_assert(result, (condition), (description))
    #define CLM_ASSERT(...) \
    CLM_TEST_ASSERT_SELECT(__VA_ARGS__, CLM_TEST_ASSERT_TWO, \
                           CLM_TEST_ASSERT_ONE, unused)(__VA_ARGS__)

static inline void clm_test_summary(const int passed, const int failed)
{
    clm_tests_passed += passed;
    clm_tests_failed += failed;

    clm_test_write_line(DIVIDER_SINGLE);
    clm_test_write("    ");
    clm_test_write_color("PASSED:", CONSOLE_COLOR_GREEN);
    clm_test_write_format(" %d  |  ", passed);
    clm_test_write_color("FAILED:", CONSOLE_COLOR_RED);
    clm_test_write_format(" %d  |  ", failed);
    clm_test_write_color("TOTAL:", CONSOLE_COLOR_CYAN);
    clm_test_write_line_format(" %d", passed + failed);
}

static inline void clm_overall_test_summary(const char* title)
{
    clm_test_write_line("");
    clm_test_write_line(DIVIDER_DOUBLE);
    clm_test_write_color(title, CONSOLE_COLOR_YELLOW);
    clm_test_write_line("");
    clm_test_write_line(DIVIDER_DOUBLE);
    clm_test_write("  ");
    clm_test_write_color("PASSED:", CONSOLE_COLOR_GREEN);
    clm_test_write_format(" %d  |  ", clm_tests_passed);
    clm_test_write_color("FAILED:", CONSOLE_COLOR_RED);
    clm_test_write_format(" %d  |  ", clm_tests_failed);
    clm_test_write_color("TOTAL:", CONSOLE_COLOR_CYAN);
    clm_test_write_line_format(" %d", clm_tests_passed + clm_tests_failed);
    clm_test_write_line(DIVIDER_DOUBLE);
    clm_test_write_line("");
}
