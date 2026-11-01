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

#include "console.h"
#include "bpa.h"
#include "string_format.h"
#include "tree.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

clm_console_status clm_console_write_to(
    FILE* stream,
    const char* text) {
    if (stream == NULL || text == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    return fputs(text, stream) == EOF
        ? CLM_CONSOLE_FAILURE : CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_init(void) {
    return CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_restore(void) {
    return CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_clear_to(FILE* stream) {
    return clm_console_write_to(stream, "\x1b[2J\x1b[H");
}

clm_console_status clm_console_clear(void) {
    const clm_console_status status = clm_console_clear_to(stdout);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_flush(void) {
    return fflush(stdout) == 0
        ? CLM_CONSOLE_SUCCESS : CLM_CONSOLE_FAILURE;
}

clm_console_status clm_console_set_cursor_position_to(
    FILE* stream,
    const clm_position position) {
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

clm_console_status clm_console_set_cursor_position(
    const clm_position position) {
    const clm_console_status status =
        clm_console_set_cursor_position_to(stdout, position);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_set_cursor_visible_to(
    FILE* stream,
    const bool visible) {
    return clm_console_write_to(
        stream,
        visible ? "\x1b[?25h" : "\x1b[?25l");
}

clm_console_status clm_console_show_cursor(void) {
    const clm_console_status status =
        clm_console_set_cursor_visible_to(stdout, true);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_hide_cursor(void) {
    const clm_console_status status =
        clm_console_set_cursor_visible_to(stdout, false);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_write(const char* text) {
    return clm_console_write_to(stdout, text);
}

clm_console_status clm_console_error(const char* text) {
    return clm_console_write_to(stderr, text);
}

clm_console_status clm_console_vwrite_format_to(
    FILE* stream,
    const char* format,
    va_list arguments) {
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

clm_console_status clm_console_vwrite_line_format_to(
    FILE* stream,
    const char* format,
    va_list arguments) {
    clm_console_status status =
        clm_console_vwrite_format_to(stream, format, arguments);
    if (status != CLM_CONSOLE_SUCCESS)
        return status;
    return fputc('\n', stream) == EOF
        ? CLM_CONSOLE_FAILURE : CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_write_format_to(
    FILE* stream,
    const char* format,
    ...) {
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_format_to(stream, format, arguments);
    va_end(arguments);
    return status;
}

clm_console_status clm_console_write_line_format_to(
    FILE* stream,
    const char* format,
    ...) {
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_line_format_to(stream, format, arguments);
    va_end(arguments);
    return status;
}

clm_console_status clm_console_write_format(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_format_to(stdout, format, arguments);
    va_end(arguments);
    return status;
}

clm_console_status clm_console_error_format(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_format_to(stderr, format, arguments);
    va_end(arguments);
    return status;
}

clm_console_status clm_console_write_line_format(
    const char* format,
    ...) {
    va_list arguments;
    va_start(arguments, format);
    const clm_console_status status = clm_console_vwrite_line_format_to(stdout, format, arguments);
    va_end(arguments);
    if (status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF)
        return CLM_CONSOLE_FAILURE;
    return status;
}

clm_console_status clm_console_write_line_to(
    FILE* stream,
    const char* text) {
    clm_console_status status = clm_console_write_to(stream, text);
    if (status != CLM_CONSOLE_SUCCESS)
        return status;
    return fputc('\n', stream) == EOF
        ? CLM_CONSOLE_FAILURE : CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_write_line(const char* text) {
    const clm_console_status status = clm_console_write_line_to(stdout, text);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_read_line_from(
    FILE* stream,
    char* buffer,
    const size_t capacity) {
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

clm_console_status clm_console_read_line(const char** out_value) {
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

clm_console_status clm_console_read(
    const char* prompt,
    const char** out_value) {
    if (out_value == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (prompt != NULL &&
        clm_console_write_to(stdout, prompt) != CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    if (fflush(stdout) == EOF)
        return CLM_CONSOLE_FAILURE;
    return clm_console_read_line(out_value);
}

clm_console_status clm_console_read_character_from(
    FILE* stream,
    char* out_character) {
    if (stream == NULL || out_character == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    int character = fgetc(stream);
    if (character == EOF)
        return CLM_CONSOLE_FAILURE;
    *out_character = (char)character;
    return CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_read_character(
    const char** out_value) {
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

clm_console_status clm_console_read_key(const char** out_value) {
    return clm_console_read_character(out_value);
}

clm_console_status clm_console_write_character_to(
    FILE* stream,
    const char* character) {
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

clm_console_status clm_console_write_character(
    const char* character) {
    const clm_console_status status =
        clm_console_write_character_to(stdout, character);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_confirm_from(
    FILE* input,
    FILE* output,
    const char* prompt,
    bool* out_value) {
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

clm_console_status clm_console_confirm(
    const char* prompt,
    bool* out_value) {
    return clm_console_confirm_from(stdin, stdout, prompt, out_value);
}

clm_console_status clm_console_write_color_to(
    FILE* stream,
    const char* text,
    const char* color) {
    if (stream == NULL || text == NULL || color == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (clm_console_write_to(stream, color) != CLM_CONSOLE_SUCCESS ||
        clm_console_write_to(stream, text) != CLM_CONSOLE_SUCCESS ||
        clm_console_write_to(stream, CONSOLE_COLOR_RESET) != CLM_CONSOLE_SUCCESS)
        return CLM_CONSOLE_FAILURE;
    return CLM_CONSOLE_SUCCESS;
}

clm_console_status clm_console_write_color(
    const char* text,
    const char* color) {
    const clm_console_status status = clm_console_write_color_to(stdout, text, color);
    return status == CLM_CONSOLE_SUCCESS && fflush(stdout) == EOF
        ? CLM_CONSOLE_FAILURE : status;
}

clm_console_status clm_console_wait(
    const double seconds) {
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

clm_console_status clm_console_write_effect_to(
    FILE* stream,
    const char* text,
    const double delay_seconds) {
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

clm_console_status clm_console_write_effect(
    const char* text,
    const double delay_seconds) {
    /* clock() keeps the implementation within standard C, at the cost of busy-waiting. */
    return clm_console_write_effect_to(stdout, text, delay_seconds);
}

static atomic_flag clm_console_object_config_lock = ATOMIC_FLAG_INIT;
static bool clm_console_object_color_enabled = true;
static clm_stringtify_interceptor_fn clm_console_object_interceptor;
static void* clm_console_object_interceptor_user;

static void clm_console_object_config_acquire(void)
{
    while (atomic_flag_test_and_set_explicit(&clm_console_object_config_lock,
        memory_order_acquire)) { }
}

static void clm_console_object_config_release(void)
{
    atomic_flag_clear_explicit(&clm_console_object_config_lock,
        memory_order_release);
}

void clm_console_set_color_on_object_writes(clm_int value)
{
    clm_console_object_config_acquire();
    clm_console_object_color_enabled = value != 0;
    clm_console_object_config_release();
}

void clm_console_intercept_object_writes(
    clm_stringtify_interceptor_fn interceptor,
    void* user)
{
    clm_console_object_config_acquire();
    clm_console_object_interceptor = interceptor;
    clm_console_object_interceptor_user = interceptor != NULL ? user : NULL;
    clm_console_object_config_release();
}

void clm_console_do_not_intercept_object_writes(void)
{
    clm_console_intercept_object_writes(NULL, NULL);
}

static const char* clm_console_stringtify_type_name(clm_byte type)
{
    switch (type) {
    case CLM_BPA_TYPE_STRUCT: return "struct";
    case CLM_BPA_TYPE_SIGNED_BYTE: return "i8";
    case CLM_BPA_TYPE_BYTE: return "u8";
    case CLM_BPA_TYPE_SHORT: return "i16";
    case CLM_BPA_TYPE_UNSIGNED_SHORT: return "u16";
    case CLM_BPA_TYPE_INT: return "i32";
    case CLM_BPA_TYPE_UNSIGNED_INT: return "u32";
    case CLM_BPA_TYPE_LONG: return "i64";
    case CLM_BPA_TYPE_UNSIGNED_LONG: return "u64";
    case CLM_BPA_TYPE_FLOAT: return "f32";
    case CLM_BPA_TYPE_DOUBLE: return "f64";
    case CLM_BPA_TYPE_STRING: return "string";
    case CLM_BPA_TYPE_UUID: return "uuid";
    case CLM_BPA_TYPE_DATETIME: return "datetime";
    default: return "unknown";
    }
}

static clm_stringtify_status clm_console_stringtify_inline_value(
    const clm_stringtify_value* value,
    clm_string_format_writer* writer)
{
    if (value == NULL || writer == NULL || value->data == NULL || value->size == 0)
        return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
    char number[128];
    int length = -1;
    switch (value->type) {
    case CLM_BPA_TYPE_SIGNED_BYTE: {
        clm_sbyte number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%d", (int)number_value);
        break;
    }
    case CLM_BPA_TYPE_BYTE: {
        clm_byte number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%u", (unsigned)number_value);
        break;
    }
    case CLM_BPA_TYPE_SHORT: {
        clm_short number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%d", (int)number_value);
        break;
    }
    case CLM_BPA_TYPE_UNSIGNED_SHORT: {
        clm_ushort number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%u", (unsigned)number_value);
        break;
    }
    case CLM_BPA_TYPE_INT: {
        clm_int number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%d", number_value);
        break;
    }
    case CLM_BPA_TYPE_UNSIGNED_INT: {
        clm_uint number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%u", number_value);
        break;
    }
    case CLM_BPA_TYPE_LONG: {
        clm_long number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%lld", (long long)number_value);
        break;
    }
    case CLM_BPA_TYPE_UNSIGNED_LONG: {
        clm_ulong number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        length = snprintf(number, sizeof(number), "%llu",
            (unsigned long long)number_value);
        break;
    }
    case CLM_BPA_TYPE_FLOAT: {
        clm_float number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        if (!isfinite(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        length = snprintf(number, sizeof(number), "%.9g", (double)number_value);
        break;
    }
    case CLM_BPA_TYPE_DOUBLE: {
        clm_double number_value;
        if (value->size != sizeof(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        memcpy(&number_value, value->data, sizeof(number_value));
        if (!isfinite(number_value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
        length = snprintf(number, sizeof(number), "%.17g", number_value);
        break;
    }
    default:
        return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
    }
    if (length < 0 || (size_t)length >= sizeof(number))
        return CLM_STRINGTIFY_FAILURE;
    return clm_string_format_writer_append(writer, number, (size_t)length) ==
        CLM_FORMAT_SERIALIZE_SUCCESS ? CLM_STRINGTIFY_SUCCESS
        : CLM_STRINGTIFY_FAILURE;
}

static clm_stringtify_status clm_console_stringtify_color_adapter(
    clm_id node_id,
    clm_id field_id,
    const clm_stringtify_value* value,
    const clm_stringtify_options* options,
    clm_stringtify_writer* output,
    void* user)
{
    (void)node_id;
    (void)field_id;
    (void)user;
    if (value == NULL || options == NULL || output == NULL)
        return CLM_STRINGTIFY_INVALID_ARGUMENT;
    if (!options->ansi_colors)
        return CLM_STRINGTIFY_USE_DEFAULT;

    clm_string_format_writer text;
    clm_string_format_writer_init(&text);
    clm_stringtify_status status = CLM_STRINGTIFY_SUCCESS;
    bool boolean_value = false;
    if (value->id != 0 && clm_tree_is_null(value->id)) {
        status = clm_string_format_writer_append_cstr(&text, "null") ==
            CLM_FORMAT_SERIALIZE_SUCCESS ? CLM_STRINGTIFY_SUCCESS : CLM_STRINGTIFY_FAILURE;
    } else if (value->id != 0 && clm_tree_is_boolean(value->id, &boolean_value)) {
        status = clm_string_format_writer_append_cstr(&text,
            boolean_value ? "true" : "false") == CLM_FORMAT_SERIALIZE_SUCCESS
            ? CLM_STRINGTIFY_SUCCESS : CLM_STRINGTIFY_FAILURE;
    } else if (value->type == CLM_BPA_TYPE_STRING) {
        status = clm_string_format_writer_append_quoted(&text,
            value->data != NULL ? (const char*)value->data : "", value->size) ==
            CLM_FORMAT_SERIALIZE_SUCCESS ? CLM_STRINGTIFY_SUCCESS : CLM_STRINGTIFY_FAILURE;
    } else if (value->id != 0) {
        status = clm_string_format_writer_append_leaf_text(&text, value->id) ==
            CLM_FORMAT_SERIALIZE_SUCCESS ? CLM_STRINGTIFY_SUCCESS
            : CLM_STRINGTIFY_USE_DEFAULT;
    } else {
        status = clm_console_stringtify_inline_value(value, &text);
    }
    if (status != CLM_STRINGTIFY_SUCCESS) {
        clm_string_format_writer_dispose(&text);
        return status;
    }

    const char* color = value->type == CLM_BPA_TYPE_STRING
        ? CONSOLE_COLOR_GREEN
        : value->type == CLM_BPA_TYPE_FLOAT || value->type == CLM_BPA_TYPE_DOUBLE
            ? CONSOLE_COLOR_YELLOW
            : value->type == CLM_BPA_TYPE_STRUCT || value->type == CLM_BPA_TYPE_UUID ||
              value->type == CLM_BPA_TYPE_DATETIME
                ? CONSOLE_COLOR_MAGENTA : CONSOLE_COLOR_CYAN;
    if (options->style == CLM_STRINGTIFY_STYLE_DETAILED) {
        status = clm_stringtify_writer_append_cstr(output, "(");
        if (status == CLM_STRINGTIFY_SUCCESS)
            status = clm_stringtify_writer_append_cstr(output,
                clm_console_stringtify_type_name(value->type));
        if (status == CLM_STRINGTIFY_SUCCESS)
            status = clm_stringtify_writer_append_cstr(output, ") ");
    }
    if (status == CLM_STRINGTIFY_SUCCESS)
        status = clm_stringtify_writer_append_cstr(output, color);
    if (status == CLM_STRINGTIFY_SUCCESS)
        status = clm_stringtify_writer_append(output, text.data, text.length);
    if (status == CLM_STRINGTIFY_SUCCESS)
        status = clm_stringtify_writer_append_cstr(output, CONSOLE_COLOR_RESET);
    clm_string_format_writer_dispose(&text);
    return status;
}

static clm_console_status clm_console_map_stringtify_status(
    const clm_stringtify_status status)
{
    if (status == CLM_STRINGTIFY_SUCCESS)
        return CLM_CONSOLE_SUCCESS;
    if (status == CLM_STRINGTIFY_INVALID_ARGUMENT)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    return CLM_CONSOLE_FAILURE;
}

typedef clm_stringtify_status (*clm_console_stringtify_fn)(
    clm_id, const clm_stringtify_options*, char*, size_t, size_t*);

static clm_console_status clm_console_write_stringtified_with_interceptor(
    const clm_id id,
    clm_console_stringtify_fn stringify,
    const bool table,
    const bool detailed,
    clm_stringtify_interceptor_fn format_interceptor)
{
    bool color_enabled;
    clm_stringtify_interceptor_fn interceptor;
    void* interceptor_user;
    clm_console_object_config_acquire();
    color_enabled = clm_console_object_color_enabled;
    interceptor = clm_console_object_interceptor;
    interceptor_user = clm_console_object_interceptor_user;
    clm_console_object_config_release();

    clm_stringtify_options options;
    clm_stringtify_options_init(&options);
    options.style = detailed ? CLM_STRINGTIFY_STYLE_DETAILED
        : CLM_STRINGTIFY_STYLE_PRETTY;
    options.ansi_colors = color_enabled;
    options.adapter = color_enabled ? clm_console_stringtify_color_adapter : NULL;
    options.interceptor = format_interceptor != NULL ? format_interceptor : interceptor;
    options.interceptor_user = format_interceptor != NULL ? NULL : interceptor_user;
    options.table_borders = table;
    options.align_columns = table;

    size_t required = 0;
    clm_stringtify_status status = stringify(id, &options, NULL, 0, &required);
    if (status != CLM_STRINGTIFY_SUCCESS)
        return clm_console_map_stringtify_status(status);
    if (required == 0)
        return CLM_CONSOLE_FAILURE;
    char* text = (char*)malloc(required);
    if (text == NULL)
        return CLM_CONSOLE_FAILURE;
    status = stringify(id, &options, text, required, &required);
    const clm_console_status result = status == CLM_STRINGTIFY_SUCCESS
        ? clm_console_write_to(stdout, text)
        : clm_console_map_stringtify_status(status);
    free(text);
    return result;
}

static clm_console_status clm_console_write_stringtified(
    const clm_id id,
    clm_console_stringtify_fn stringify,
    const bool table,
    const bool detailed)
{
    return clm_console_write_stringtified_with_interceptor(id, stringify,
        table, detailed, NULL);
}

clm_console_status clm_console_write_tree_as_yaml(const clm_id id)
{
    return clm_console_write_stringtified_with_interceptor(id,
        clm_stringtify_tree_to_string_with_options, false, false,
        clm_stringtify_tree_yaml_interceptor);
}

clm_console_status clm_console_write_tree_as_json(const clm_id id)
{
    return clm_console_write_stringtified_with_interceptor(id,
        clm_stringtify_tree_to_string_with_options, false, false,
        clm_stringtify_tree_json_interceptor);
}

clm_console_status clm_console_write_block(const clm_id id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL)
        return CLM_CONSOLE_INVALID_ARGUMENT;
    if (block->type == CLM_BPA_TYPE_STRING) {
        if (block->length != 0 && block->data == NULL)
            return CLM_CONSOLE_FAILURE;
        return fwrite(block->data, 1, block->length, stdout) == block->length
            ? CLM_CONSOLE_SUCCESS : CLM_CONSOLE_FAILURE;
    }
    return clm_console_write_stringtified(id,
        clm_stringtify_block_to_string_with_options, false, true);
}

clm_console_status clm_console_write_dictionary(const clm_id id)
{
    return clm_console_write_stringtified(id,
        clm_stringtify_dictionary_to_string_with_options, false, false);
}

clm_console_status clm_console_write_storage(const clm_id id)
{
    return clm_console_write_stringtified(id,
        clm_stringtify_storage_to_string_with_options, false, false);
}

clm_console_status clm_console_write_table(const clm_id id)
{
    return clm_console_write_stringtified(id,
        clm_stringtify_table_to_string_with_options, true, false);
}

clm_console_status clm_console_write_tree(const clm_id id)
{
    return clm_console_write_stringtified(id,
        clm_stringtify_tree_to_string_with_options, false, false);
}

clm_console_status clm_console_inspect_block(const clm_id id)
{
    return clm_console_write_stringtified(id,
        clm_stringtify_block_to_string_with_options, false, true);
}
