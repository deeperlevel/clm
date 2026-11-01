#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "base.h"
#include "stringtify.h"

#include <stdbool.h>
#include <stdio.h>

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
clm_console_status clm_console_write_to(FILE* stream, const char* text);

/** Initializes the standard-C console interface; currently performs no setup.
 * @return CLM_CONSOLE_SUCCESS.
 */
clm_console_status clm_console_init(void);

/** Restores console state; currently performs no platform-specific operation.
 * @return CLM_CONSOLE_SUCCESS.
 */
clm_console_status clm_console_restore(void);

/** Emits ANSI clear-screen and home-cursor controls to a stream.
 * The stream must be connected to an ANSI-compatible terminal for control
 * effects; redirected output may contain the escape sequence literally.
 * @param stream Destination stream.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL stream, or FAILURE on I/O error.
 */
clm_console_status clm_console_clear_to(FILE* stream);

/** Clears stdout and flushes it.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
clm_console_status clm_console_clear(void);

/** Flushes stdout.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
clm_console_status clm_console_flush(void);

/** Emits an ANSI cursor-position sequence to a stream.
 * Position coordinates are zero-based in the API and converted to ANSI's
 * one-based row/column coordinates.
 * @param stream Destination stream.
 * @param position Zero-based x/y cursor position.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL stream or negative coordinate,
 *         or FAILURE on formatting/I/O error.
 */
clm_console_status clm_console_set_cursor_position_to(FILE* stream, clm_position position);

/** Moves the cursor on stdout and flushes the stream.
 * @param position Zero-based x/y cursor position.
 * @return SUCCESS, INVALID_ARGUMENT for negative coordinates, or FAILURE.
 */
clm_console_status clm_console_set_cursor_position(clm_position position);

/** Emits an ANSI sequence to show or hide the cursor on a stream.
 * @param stream Destination stream.
 * @param visible true to show the cursor; false to hide it.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL stream, or FAILURE on I/O error.
 */
clm_console_status clm_console_set_cursor_visible_to(FILE* stream, bool visible);

/** Shows the cursor on stdout and flushes the stream.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
clm_console_status clm_console_show_cursor(void);

/** Hides the cursor on stdout and flushes the stream.
 * @return CLM_CONSOLE_SUCCESS, or CLM_CONSOLE_FAILURE on I/O error.
 */
clm_console_status clm_console_hide_cursor(void);

/** Writes text to stdout without a newline.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on I/O error.
 */
clm_console_status clm_console_write(const char* text);

/** Writes text to stderr without a newline.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on I/O error.
 */
clm_console_status clm_console_error(const char* text);

/** Formats variadic arguments and writes the result to a stream.
 * The va_list is consumed only through copies; the caller retains its state.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param arguments Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
clm_console_status clm_console_vwrite_format_to(FILE* stream, const char* format, va_list arguments);

/** Formats and writes a line to a stream, appending one LF character.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param arguments Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
clm_console_status clm_console_vwrite_line_format_to(FILE* stream, const char* format, va_list arguments);

/** Writes printf-style formatted output to a stream without a newline.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
clm_console_status clm_console_write_format_to(FILE* stream, const char* format, ...);

/** Writes printf-style formatted output to a stream followed by LF.
 * @param stream Destination stream.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for NULL stream/format, or FAILURE.
 */
clm_console_status clm_console_write_line_format_to(FILE* stream, const char* format, ...);

/** Writes printf-style formatted output to stdout without a newline.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL format, or FAILURE.
 */
clm_console_status clm_console_write_format(const char* format, ...);

/** Writes printf-style formatted output to stderr without a newline.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL format, or FAILURE.
 */
clm_console_status clm_console_error_format(const char* format, ...);

/** Writes printf-style formatted output to stdout followed by LF, then flushes.
 * @param format printf-style format string.
 * @param ... Values matching the format specifiers.
 * @return SUCCESS, INVALID_ARGUMENT for a NULL format, or FAILURE.
 */
clm_console_status clm_console_write_line_format(const char* format, ...);

/** Writes text and a newline to a stream without flushing it.
 * @param stream Destination stream.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL inputs, or FAILURE on I/O error.
 */
clm_console_status clm_console_write_line_to(FILE* stream, const char* text);

/** Writes text and a newline to stdout, then flushes it.
 * @param text NUL-terminated text.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on I/O error.
 */
clm_console_status clm_console_write_line(const char* text);

/** Reads one line from a stream, stripping LF and an optional preceding CR.
 * Lines that exceed capacity are drained and reported as failures.
 * @param stream Input stream.
 * @param buffer Receives the NUL-terminated line.
 * @param capacity Buffer size including the terminator; must be 2..INT_MAX.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on EOF,
 *         I/O error, or an overlong line.
 */
clm_console_status clm_console_read_line_from(FILE* stream, char* buffer, size_t capacity);

/** Reads a line from stdin into shared internal storage.
 * The returned pointer is overwritten by the next line or character read.
 * Input capacity is CLM_CONSOLE_INPUT_CAPACITY bytes including the terminator.
 * @param out_value Receives the shared line pointer; must not be NULL.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on input error/EOF.
 */
clm_console_status clm_console_read_line(const char** out_value);

/** Writes an optional prompt to stdout, flushes it, then reads a line.
 * @param prompt Optional prompt; NULL suppresses prompt output.
 * @param out_value Receives a pointer to shared input storage.
 * @return SUCCESS, INVALID_ARGUMENT for NULL output, or FAILURE on I/O/input.
 */
clm_console_status clm_console_read(const char* prompt, const char** out_value);

/** Reads one byte from a stream.
 * This reads a byte, not a decoded Unicode character.
 * @param stream Input stream.
 * @param out_character Receives the byte converted to char.
 * @return SUCCESS, INVALID_ARGUMENT for NULL arguments, or FAILURE on EOF/I/O.
 */
clm_console_status clm_console_read_character_from(FILE* stream, char* out_character);

/** Reads one byte from stdin into shared, NUL-terminated storage.
 * Standard C input may be line-buffered; this is not raw key input. The pointer
 * is overwritten by the next line or character read.
 * @param out_value Receives the shared one-byte string; must not be NULL.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on EOF/input error.
 */
clm_console_status clm_console_read_character(const char** out_value);

/** Reads one byte from stdin; equivalent to clm_console_read_character.
 * @param out_value Receives a pointer to shared one-byte storage.
 * @return SUCCESS, INVALID_ARGUMENT for NULL, or FAILURE on EOF/input error.
 */
clm_console_status clm_console_read_key(const char** out_value);

/** Writes one complete, valid UTF-8 encoded Unicode scalar value to a stream.
 * @param stream Destination stream.
 * @param character NUL-terminated UTF-8 sequence containing exactly one scalar.
 * @return SUCCESS, INVALID_ARGUMENT for malformed/multiple sequences or NULL,
 *         or FAILURE on I/O error.
 */
clm_console_status clm_console_write_character_to(FILE* stream, const char* character);

/** Writes one UTF-8 scalar value to stdout and flushes it.
 * @param character NUL-terminated UTF-8 sequence containing exactly one scalar.
 * @return SUCCESS, INVALID_ARGUMENT for malformed input, or FAILURE.
 */
clm_console_status clm_console_write_character(const char* character);

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
clm_console_status clm_console_confirm_from(FILE* input, FILE* output, const char* prompt, bool* out_value);

/** Prompts on stdin/stdout for a yes/no response.
 * @param prompt Optional prompt text.
 * @param out_value Receives the parsed answer; must not be NULL.
 * @return SUCCESS, INVALID_ARGUMENT for invalid response/input, or FAILURE.
 */
clm_console_status clm_console_confirm(const char* prompt, bool* out_value);

/** Writes ANSI color prefix, text, and reset sequence to a stream.
 * ANSI interpretation depends on the destination terminal.
 * @param stream Destination stream.
 * @param text Text to write.
 * @param color ANSI color/style sequence.
 * @return SUCCESS, INVALID_ARGUMENT for NULL arguments, or FAILURE on I/O.
 */
clm_console_status clm_console_write_color_to(FILE* stream, const char* text, const char* color);

/** Writes ANSI-colored text to stdout and flushes it.
 * @param text Text to write.
 * @param color ANSI color/style sequence.
 * @return SUCCESS, INVALID_ARGUMENT for NULL arguments, or FAILURE on I/O.
 */
clm_console_status clm_console_write_color(const char* text, const char* color);

/** Busy-waits for a duration using the C standard clock.
 * This consumes CPU while waiting and is intended for simple presentation
 * effects, not timing-sensitive work or efficient delays.
 * @param seconds Finite nonnegative duration in seconds.
 * @return SUCCESS, INVALID_ARGUMENT for negative/non-finite input, or FAILURE
 *         if the clock cannot be read.
 */
clm_console_status clm_console_wait(double seconds);

/** Writes text one byte at a time, waiting between bytes.
 * UTF-8 multibyte sequences are emitted byte-by-byte. Waiting uses a busy loop.
 * @param stream Destination stream.
 * @param text NUL-terminated text.
 * @param delay_seconds Finite nonnegative delay between bytes.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on I/O
 *         or clock error.
 */
clm_console_status clm_console_write_effect_to(FILE* stream, const char* text, double delay_seconds);

/** Writes text to stdout one byte at a time with a busy-wait delay.
 * @param text NUL-terminated text.
 * @param delay_seconds Finite nonnegative delay between bytes.
 * @return SUCCESS, INVALID_ARGUMENT for invalid arguments, or FAILURE on I/O
 *         or clock error.
 */
clm_console_status clm_console_write_effect(const char* text, double delay_seconds);

/**
 * Enables or disables ANSI coloring for CLM object writes.
 *
 * When enabled, console object writes use the console's ANSI
 * stringtify adapter.
 *
 * When disabled, object writes use the default uncolored formatter,
 * unless an interceptor provides its own representation.
 *
 * Parameters:
 *   value - Nonzero enables coloring; zero disables it.
 *
 * Default:
 *   Enabled (1).
 *
 * Thread safety:
 *   The setting is shared across console users. Each object write
 *   captures the current setting before conversion.
 */
void clm_console_set_color_on_object_writes(clm_int value);

/**
 * Registers a global stringtify interceptor for console object writes.
 *
 * The interceptor takes precedence over the console color adapter.
 * Returning CLM_STRINGTIFY_USE_DEFAULT allows normal rendering,
 * including the color adapter when enabled.
 *
 * Passing NULL clears the current interceptor.
 *
 * The callback must remain valid while it may be invoked.
 */
void clm_console_intercept_object_writes(
    clm_stringtify_interceptor_fn interceptor, void *user);

/**
 * Clears the global interceptor for console object writes.
 *
 * Subsequent writes use the normal console formatting behavior.
 * Does not free the callback or its application-owned resources.
 */
void clm_console_do_not_intercept_object_writes(void);

/**
 * Writes a BPA block to standard output.
 *
 * UTF-8 string blocks are written as raw text, preserving line breaks.
 * Other BPA types are converted using clm_stringtify.
 *
 * Parameters:
 *   id - ID of the BPA block to write.
 *
 * Returns:
 *   SUCCESS when output completes, otherwise an error status.
 *
 * Ownership:
 *   Does not modify or release the supplied block.
 */
clm_console_status clm_console_write_block(clm_id id);

/**
 * Writes a formatted CLM dictionary to standard output.
 *
 * Uses readable presentation with indentation.
 * A custom stringtify adapter may later add ANSI coloring.
 *
 * The dictionary remains owned by the caller.
 */
clm_console_status clm_console_write_dictionary(clm_id id);

/**
 * Writes a formatted CLM storage object to standard output.
 *
 * Displays its stored elements through clm_stringtify.
 * The storage object remains owned by the caller.
 */
clm_console_status clm_console_write_storage(clm_id id);

/**
 * Writes a CLM table to standard output.
 *
 * Uses aligned columns and visible table borders.
 * The table remains owned by the caller.
 */
clm_console_status clm_console_write_table(clm_id id);

/**
 * Writes a CLM tree to standard output.
 *
 * Uses an indented, human-readable representation.
 * The tree remains owned by the caller.
 */
clm_console_status clm_console_write_tree(clm_id id);

/**
 * Writes the debug representation of any supported BPA block.
 *
 * Unlike write_block(), string values are quoted and escaped.
 * Useful when examining block content rather than displaying text.
 */
clm_console_status clm_console_inspect_block(clm_id id);

/**
 * Writes a CLM tree to standard output in YAML format.
 *
 * Uses clm_string_yaml through a per-call interceptor for indented output.
 * Bypasses the global interceptor and ANSI coloring without changing settings.
 * The root must be an object or array; invalid roots return INVALID_ARGUMENT.
 * The tree remains owned by the caller.
 */
clm_console_status clm_console_write_tree_as_yaml(clm_id id);

/**
 * Writes a CLM tree to standard output in JSON format.
 *
 * Uses clm_string_json through a per-call interceptor with two-space indentation.
 * Bypasses the global interceptor and ANSI coloring without changing settings.
 * The root must be an object or array; invalid roots return INVALID_ARGUMENT.
 * The tree remains owned by the caller.
 */
clm_console_status clm_console_write_tree_as_json(clm_id id);