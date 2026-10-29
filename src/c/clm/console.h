#pragma once

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

#include "base.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** Result codes returned by console operations. */
typedef enum clm_console_status {
    /** Operation completed successfully. */
    CLM_CONSOLE_SUCCESS = 0,
    /** I/O, formatting, allocation, or clock operation failed. */
    CLM_CONSOLE_FAILURE = -1,
    /** A required argument or value is invalid. */
    CLM_CONSOLE_INVALID_ARGUMENT = -2
} clm_console_status;

/** Capacity of the shared line/key input buffers, including the NUL terminator. */
#define CLM_CONSOLE_INPUT_CAPACITY 4096

/* ANSI SGR reset */
#define CONSOLE_COLOR_RESET           "\x1b[0m"

/* Regular Text Colors */
#define CONSOLE_COLOR_BLACK           "\x1b[30m"
#define CONSOLE_COLOR_RED             "\x1b[31m"
#define CONSOLE_COLOR_GREEN           "\x1b[32m"
#define CONSOLE_COLOR_YELLOW          "\x1b[33m"
#define CONSOLE_COLOR_BLUE            "\x1b[34m"
#define CONSOLE_COLOR_MAGENTA         "\x1b[35m"
#define CONSOLE_COLOR_CYAN            "\x1b[36m"
#define CONSOLE_COLOR_WHITE           "\x1b[37m"

/* Bold Text Colors */
#define CONSOLE_COLOR_BLACK_BOLD      "\x1b[1;30m"
#define CONSOLE_COLOR_RED_BOLD        "\x1b[1;31m"
#define CONSOLE_COLOR_GREEN_BOLD      "\x1b[1;32m"
#define CONSOLE_COLOR_YELLOW_BOLD     "\x1b[1;33m"
#define CONSOLE_COLOR_BLUE_BOLD       "\x1b[1;34m"
#define CONSOLE_COLOR_MAGENTA_BOLD    "\x1b[1;35m"
#define CONSOLE_COLOR_CYAN_BOLD       "\x1b[1;36m"
#define CONSOLE_COLOR_WHITE_BOLD      "\x1b[1;37m"

/* Background Colors */
#define CONSOLE_COLOR_BLACK_BG        "\x1b[40m"
#define CONSOLE_COLOR_RED_BG          "\x1b[41m"
#define CONSOLE_COLOR_GREEN_BG        "\x1b[42m"
#define CONSOLE_COLOR_YELLOW_BG       "\x1b[43m"
#define CONSOLE_COLOR_BLUE_BG         "\x1b[44m"
#define CONSOLE_COLOR_MAGENTA_BG      "\x1b[45m"
#define CONSOLE_COLOR_CYAN_BG         "\x1b[46m"
#define CONSOLE_COLOR_WHITE_BG        "\x1b[47m"

/** Writes a NUL-terminated string to a stream without adding a newline.
 * @param stream Destination stream.
 * @param text Text to write.
 * @return SUCCESS, INVALID_ARGUMENT for NULL inputs, or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_write_to(
    FILE* stream,
    const char* text)
{
    if (stream == NULL || text == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    return fputs(text, stream) == EOF
        ? CLM_CONSOLE_FAILURE : CLM_CONSOLE_SUCCESS;
}

/** Initializes the standard-C console interface; currently performs no setup.
 * @return CLM_CONSOLE_SUCCESS.
 */
static inline clm_console_status clm_console_init(void)
{
    return CLM_CONSOLE_SUCCESS;
}

/** Restores console state; currently performs no platform-specific operation.
 * @return CLM_CONSOLE_SUCCESS.
 */
static inline clm_console_status clm_console_restore(void)
{
    return CLM_CONSOLE_SUCCESS;
}

/** Emits ANSI clear-screen and home-cursor controls to a stream.
 * The stream must be connected to an ANSI-compatible terminal for control
 * effects; redirected output may contain the escape sequence literally.
 * @param stream Destination stream.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL stream, or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_clear_to(FILE* stream)
{
    return clm_console_write_to(stream, "\x1b[2J\x1b[H");
}

/** Clears stdout and flushes it.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
static inline clm_console_status clm_console_clear(void)
{
    const clm_console_status status = clm_console_clear_to(stdout);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Flushes stdout.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
static inline clm_console_status clm_console_flush(void)
{
    return fflush(stdout) == 0
        ? CLM_CONSOLE_SUCCESS : CLM_CONSOLE_FAILURE;
}

/** Emits an ANSI cursor-position sequence to a stream.
 * Position coordinates are zero-based in the API and converted to ANSI's
 * one-based row/column coordinates.
 * @param stream Destination stream.
 * @param position Zero-based x/y cursor position.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL stream or negative coordinate,
 *         or FAILURE on formatting/I/O error.
 */
static inline clm_console_status clm_console_set_cursor_position_to(
    FILE* stream,
    const clm_position position)
{
    char sequence[64];
    if (stream == NULL || position.x < 0 || position.y < 0)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    const int written = snprintf(
        sequence,
        sizeof(sequence),
        "\x1b[%u;%uH",
        (unsigned int)((clm_uint)position.y + 1u),
        (unsigned int)((clm_uint)position.x + 1u));
    if (written < 0 || (size_t)written >= sizeof(sequence))
        return CLM_CONSOLE_FAILURE;
    return clm_console_write_to(stream, sequence);
}

/** Moves the cursor on stdout and flushes the stream.
 * @param position Zero-based x/y cursor position.
 * @return SUCCESS, INVALID_ARGUMENT for negative coordinates, or FAILURE.
 */
static inline clm_console_status clm_console_set_cursor_position(
    const clm_position position)
{
    const clm_console_status status =
        clm_console_set_cursor_position_to(stdout, position);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Emits an ANSI sequence to show or hide the cursor on a stream.
 * @param stream Destination stream.
 * @param visible true to show the cursor; false to hide it.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL stream, or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_set_cursor_visible_to(
    FILE* stream,
    const bool visible)
{
    return clm_console_write_to(
        stream,
        visible ? "\x1b[?25h" : "\x1b[?25l");
}

/** Shows the cursor on stdout and flushes the stream.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
static inline clm_console_status clm_console_show_cursor(void)
{
    const clm_console_status status =
        clm_console_set_cursor_visible_to(stdout, true);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Hides the cursor on stdout and flushes the stream.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
static inline clm_console_status clm_console_hide_cursor(void)
{
    const clm_console_status status =
        clm_console_set_cursor_visible_to(stdout, false);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Writes text to stdout without a newline.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_write(const char* text)
{
    return clm_console_write_to(stdout, text);
}

/** Formats variadic arguments and writes the result to a stream.
 * The va_list is consumed only through copies; the caller retains its state.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param arguments Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
static inline clm_console_status clm_console_vwrite_format_to(
    FILE* stream,
    const char* format,
    va_list arguments)
{
    va_list measure_arguments;
    va_list write_arguments;

    if (stream == NULL || format == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;

    va_copy(measure_arguments, arguments);
    int length = vsnprintf(NULL, 0, format, measure_arguments);
    va_end(measure_arguments);
    if (length < 0)
        return CLM_CONSOLE_FAILURE;

    char* buffer = (char*)malloc((size_t)length + 1);
    if (buffer == NULL)
        return CLM_CONSOLE_FAILURE;

    va_copy(write_arguments, arguments);
    if (vsnprintf(buffer, (size_t)length + 1, format, write_arguments) != length) {
        va_end(write_arguments);
        free(buffer);
        return CLM_CONSOLE_FAILURE;
    }
    va_end(write_arguments);

    const clm_console_status status = fwrite(buffer, 1, (size_t)length, stream) == (size_t)length
                                    ? CLM_CONSOLE_SUCCESS
                                    : CLM_CONSOLE_FAILURE;
    free(buffer);
    return status;
}

/** Formats and writes a line to a stream, appending one LF character.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param arguments Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
static inline clm_console_status clm_console_vwrite_line_format_to(
    FILE* stream,
    const char* format,
    va_list arguments)
{
    clm_console_status status =
        clm_console_vwrite_format_to(stream, format, arguments);
    if (status != CLM_CONSOLE_SUCCESS)
        return status;
    return fputc('\n', stream) == EOF
        ? CLM_CONSOLE_FAILURE : CLM_CONSOLE_SUCCESS;
}

/** Writes printf-style formatted output to a stream without a newline.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
static inline clm_console_status clm_console_write_format_to(
    FILE* stream,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_format_to(stream, format, arguments);
    va_end(arguments);
    return status;
}

/** Writes printf-style formatted output to a stream followed by LF.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
static inline clm_console_status clm_console_write_line_format_to(
    FILE* stream,
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_line_format_to(stream, format, arguments);
    va_end(arguments);
    return status;
}

/** Writes printf-style formatted output to stdout without a newline.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL format, or FAILURE.
 */
static inline clm_console_status clm_console_write_format(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_format_to(stdout, format, arguments);
    va_end(arguments);
    return status;
}

/** Writes printf-style formatted output to stdout followed by LF, then flushes.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL format, or FAILURE.
 */
static inline clm_console_status clm_console_write_line_format(
    const char* format,
    ...)
{
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_line_format_to(stdout, format, arguments);
    va_end(arguments);
    if (status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF)
        return CLM_CONSOLE_FAILURE;
    return status;
}

/** Writes text and a newline to a stream without flushing it.
 * @param stream Destination stream.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL inputs, or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_write_line_to(
    FILE* stream,
    const char* text)
{
    clm_console_status status = clm_console_write_to(stream, text);
    if (status != CLM_CONSOLE_SUCCESS)
        return status;
    return fputc('\n', stream) == EOF
        ? CLM_CONSOLE_FAILURE : CLM_CONSOLE_SUCCESS;
}

/** Writes text and a newline to stdout, then flushes it.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_write_line(const char* text)
{
    const clm_console_status status = clm_console_write_line_to(stdout, text);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Reads one line from a stream, stripping LF and an optional preceding CR.
 * Lines that exceed capacity are drained and reported as failures.
 * @param stream Input stream.
 * @param buffer Receives the NUL-terminated line.
 * @param capacity Buffer size including the terminator; must be 2..INT_MAX.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on EOF,
 *         I/O error, or an overlong line.
 */
static inline clm_console_status clm_console_read_line_from(
    FILE* stream,
    char* buffer,
    const size_t capacity)
{
    if (stream == NULL || buffer == NULL || capacity < 2 || capacity > INT_MAX)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (fgets(buffer, (int)capacity, stream) == NULL)
        return CLM_CONSOLE_FAILURE;
    size_t length = strlen(buffer);
    if (length != 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
        if (length != 0 && buffer[length - 1] == '\r')
            buffer[--length] = '\0';
        return CLM_CONSOLE_SUCCESS;
    }

    int next = fgetc(stream);
    if (next != EOF && next != '\n') {
        do {
            next = fgetc(stream);
        } while (next != EOF && next != '\n');
        buffer[0] = '\0';
        return CLM_CONSOLE_FAILURE;
    }
    if (next == EOF && ferror(stream)) {
        buffer[0] = '\0';
        return CLM_CONSOLE_FAILURE;
    }
    if (length != 0 && buffer[length - 1] == '\r')
        buffer[length - 1] = '\0';
    return CLM_CONSOLE_SUCCESS;
}

/** Reads a line from stdin into shared internal storage.
 * The returned pointer is overwritten by the next line or character read.
 * Input capacity is CLM_CONSOLE_INPUT_CAPACITY bytes including the terminator.
 * @param out_value Receives the shared line pointer; must not be NULL.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on input error/EOF.
 */
static inline clm_console_status clm_console_read_line(const char** out_value)
{
    /* The returned text is shared storage, overwritten by the next read. */
    static char buffer[CLM_CONSOLE_INPUT_CAPACITY];
    if (out_value == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (clm_console_read_line_from(stdin, buffer, sizeof(buffer)) !=
        CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    *out_value = buffer;
    return CLM_CONSOLE_SUCCESS;
}

/** Writes an optional prompt to stdout, flushes it, then reads a line.
 * @param prompt Optional prompt; NULL suppresses prompt output.
 * @param out_value Receives a pointer to shared input storage.
 * @return SUCCESS, INVALID_ARGUMENT for NULL output, or FAILURE on I/O/input.
 */
static inline clm_console_status clm_console_read(
    const char* prompt,
    const char** out_value)
{
    if (out_value == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (prompt != NULL &&
        clm_console_write_to(stdout, prompt) != CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    if (fflush(stdout) == EOF)
        return CLM_CONSOLE_FAILURE;
    return clm_console_read_line(out_value);
}

/** Reads one byte from a stream.
 * This reads a byte, not a decoded Unicode character.
 * @param stream Input stream.
 * @param out_character Receives the byte converted to char.
 * @return SUCCESS, INVALID_ARGUMENT for NULL arguments, or FAILURE on EOF/I/O.
 */
static inline clm_console_status clm_console_read_character_from(
    FILE* stream,
    char* out_character)
{
    if (stream == NULL || out_character == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    int character = fgetc(stream);
    if (character == EOF)
        return CLM_CONSOLE_FAILURE;
    *out_character = (char)character;
    return CLM_CONSOLE_SUCCESS;
}

/** Reads one byte from stdin into shared, NUL-terminated storage.
 * Standard C input may be line-buffered; this is not raw key input. The pointer
 * is overwritten by the next line or character read.
 * @param out_value Receives the shared one-byte string; must not be NULL.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on EOF/input error.
 */
static inline clm_console_status clm_console_read_character(
    const char** out_value)
{
    /* The returned character is shared storage, overwritten by the next read. */
    static char buffer[2];
    if (out_value == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (clm_console_read_character_from(stdin, &buffer[0]) !=
        CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    buffer[1] = '\0';
    *out_value = buffer;
    return CLM_CONSOLE_SUCCESS;
}

/** Reads one byte from stdin; equivalent to clm_console_read_character.
 * @param out_value Receives a pointer to shared one-byte storage.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on EOF/input error.
 */
static inline clm_console_status clm_console_read_key(const char** out_value)
{
    return clm_console_read_character(out_value);
}

/** Writes one complete, valid UTF-8 encoded Unicode scalar value to a stream.
 * @param stream Destination stream.
 * @param character NUL-terminated UTF-8 sequence containing exactly one scalar.
 * @return SUCCESS, INVALID_ARGUMENT for malformed/multiple sequences or NULL,
 *         or FAILURE on I/O error.
 */
static inline clm_console_status clm_console_write_character_to(
    FILE* stream,
    const char* character)
{
    const unsigned char* bytes = (const unsigned char*)character;
    size_t length;
    if (stream == NULL || character == NULL || character[0] == '\0')
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (bytes[0] <= 0x7f)
        length = 1;
    else if (bytes[0] >= 0xc2 && bytes[0] <= 0xdf)
        length = 2;
    else if (bytes[0] >= 0xe0 && bytes[0] <= 0xef)
        length = 3;
    else if (bytes[0] >= 0xf0 && bytes[0] <= 0xf4)
        length = 4;
    else
        return CLM_CONSOLE_INVALID_ARGUMENT;
    for (size_t index = 1; index < length; ++index) {
        if (bytes[index] == '\0' ||
            (bytes[index] & 0xc0) != 0x80)
            return CLM_CONSOLE_INVALID_ARGUMENT;
    }
    if ((length == 3 && bytes[0] == 0xe0 && bytes[1] < 0xa0) ||
        (length == 3 && bytes[0] == 0xed && bytes[1] >= 0xa0) ||
        (length == 4 && bytes[0] == 0xf0 && bytes[1] < 0x90) ||
        (length == 4 && bytes[0] == 0xf4 && bytes[1] >= 0x90) ||
        bytes[length] != '\0')
        return CLM_CONSOLE_INVALID_ARGUMENT;
    return fwrite(bytes, 1, length, stream) == length
        ? CLM_CONSOLE_SUCCESS : CLM_CONSOLE_FAILURE;
}

/** Writes one UTF-8 scalar value to stdout and flushes it.
 * @param character NUL-terminated UTF-8 sequence containing exactly one scalar.
 * @return SUCCESS, INVALID_ARGUMENT for malformed input, or FAILURE.
 */
static inline clm_console_status clm_console_write_character(
    const char* character)
{
    const clm_console_status status =
        clm_console_write_character_to(stdout, character);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Prompts for and parses a yes/no response from supplied streams.
 * Leading/trailing whitespace is ignored; accepted answers are y/yes/n/no,
 * case-insensitively.
 * @param input Stream from which the response line is read.
 * @param output Stream to which prompt text is written and flushed.
 * @param prompt Optional prompt text; NULL suppresses only the custom text.
 * @param out_value Receives true for yes and false for no.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments/response, or FAILURE
 *         on I/O error or an overlong input line.
 */
static inline clm_console_status clm_console_confirm_from(
    FILE* input,
    FILE* output,
    const char* prompt,
    bool* out_value)
{
    char response[CLM_CONSOLE_INPUT_CAPACITY];
    if (input == NULL || output == NULL || out_value == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (prompt != NULL && clm_console_write_to(output, prompt) != CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    if (clm_console_write_to(output, " [y/n]: ") != CLM_CONSOLE_SUCCESS ||
        fflush(output) == EOF)
        return CLM_CONSOLE_FAILURE;
    if (clm_console_read_line_from(input, response, sizeof(response)) !=
        CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    const char* cursor = response;
    while (*cursor != '\0' && isspace((unsigned char)*cursor))
        ++cursor;
    size_t length = strlen(cursor);
    while (length != 0 && isspace((unsigned char)cursor[length - 1]))
        --length;
    if ((length == 1 &&
         (cursor[0] == 'y' || cursor[0] == 'Y' ||
          cursor[0] == 'n' || cursor[0] == 'N')) ||
        (length == 3 &&
         (cursor[0] == 'y' || cursor[0] == 'Y') &&
         (cursor[1] == 'e' || cursor[1] == 'E') &&
         (cursor[2] == 's' || cursor[2] == 'S'))) {
        *out_value = cursor[0] == 'y' || cursor[0] == 'Y';
        return CLM_CONSOLE_SUCCESS;
    }
    if (length == 2 &&
        (cursor[0] == 'n' || cursor[0] == 'N') &&
        (cursor[1] == 'o' || cursor[1] == 'O')) {
        *out_value = false;
        return CLM_CONSOLE_SUCCESS;
    }
    return CLM_CONSOLE_INVALID_ARGUMENT;
}

/** Prompts on stdin/stdout for a yes/no response.
 * @param prompt Optional prompt text.
 * @param out_value Receives the parsed answer; must not be NULL.
 * @return SUCCESS, INVALID_ARGUMENT for invalid response/input, or FAILURE.
 */
static inline clm_console_status clm_console_confirm(
    const char* prompt,
    bool* out_value)
{
    return clm_console_confirm_from(stdin, stdout, prompt, out_value);
}

/** Writes ANSI color prefix, text, and reset sequence to a stream.
 * ANSI interpretation depends on the destination terminal.
 * @param stream Destination stream.
 * @param text Text to write.
 * @param color ANSI color/style sequence.
 * @return SUCCESS, INVALID_ARGUMENT for NULL arguments, or FAILURE on I/O.
 */
static inline clm_console_status clm_console_write_color_to(
    FILE* stream,
    const char* text,
    const char* color)
{
    if (stream == NULL || text == NULL || color == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (clm_console_write_to(stream, color) != CLM_CONSOLE_SUCCESS ||
        clm_console_write_to(stream, text) != CLM_CONSOLE_SUCCESS ||
        clm_console_write_to(stream, CONSOLE_COLOR_RESET) != CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    return CLM_CONSOLE_SUCCESS;
}

/** Writes ANSI-colored text to stdout and flushes it.
 * @param text Text to write.
 * @param color ANSI color/style sequence.
 * @return SUCCESS, INVALID_ARGUMENT for NULL arguments, or FAILURE on I/O.
 */
static inline clm_console_status clm_console_write_color(
    const char* text,
    const char* color)
{
    const clm_console_status status = clm_console_write_color_to(stdout, text, color);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

/** Busy-waits for a duration using the C standard clock.
 * This consumes CPU while waiting and is intended for simple presentation
 * effects, not timing-sensitive work or efficient delays.
 * @param seconds Finite nonnegative duration in seconds.
 * @return SUCCESS, INVALID_ARGUMENT for negative/non-finite input, or FAILURE
 *         if the clock cannot be read.
 */
static inline clm_console_status clm_console_wait(
    const double seconds)
{
    clock_t current;
    if (!isfinite(seconds) || seconds < 0.0)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (seconds == 0.0)
        return CLM_CONSOLE_SUCCESS;
    const clock_t start = clock();
    if (start == (clock_t)-1)
        return CLM_CONSOLE_FAILURE;
    do {
        current = clock();
        if (current == (clock_t)-1 || current < start)
            return CLM_CONSOLE_FAILURE;
    } while (((double)current - (double)start) / (double)CLOCKS_PER_SEC <
             seconds);
    return CLM_CONSOLE_SUCCESS;
}

/** Writes text one byte at a time, waiting between bytes.
 * UTF-8 multibyte sequences are emitted byte-by-byte. Waiting uses a busy loop.
 * @param stream Destination stream.
 * @param text NUL-terminated text.
 * @param delay_seconds Finite nonnegative delay between bytes.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on I/O
 *         or clock error.
 */
static inline clm_console_status clm_console_write_effect_to(
    FILE* stream,
    const char* text,
    const double delay_seconds)
{
    if (stream == NULL || text == NULL || !isfinite(delay_seconds) ||
        delay_seconds < 0.0)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    for (const unsigned char* character = (const unsigned char*)text; *character != '\0'; ++character) {
        if (fputc(*character, stream) == EOF || fflush(stream) == EOF)
            return CLM_CONSOLE_FAILURE;
        if (character[1] != '\0' &&
            clm_console_wait(delay_seconds) != CLM_CONSOLE_SUCCESS)
            return CLM_CONSOLE_FAILURE;
    }
    return CLM_CONSOLE_SUCCESS;
}

/** Writes text to stdout one byte at a time with a busy-wait delay.
 * @param text NUL-terminated text.
 * @param delay_seconds Finite nonnegative delay between bytes.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on I/O
 *         or clock error.
 */
static inline clm_console_status clm_console_write_effect(
    const char* text,
    const double delay_seconds)
{
    /* clock() keeps the implementation within standard C, at the cost of busy-waiting. */
    return clm_console_write_effect_to(stdout, text, delay_seconds);
}
