#pragma once
#include <stdbool.h>
#include <stdio.h>

typedef enum cla_console_status {
 CLA_CONSOLE_OK = 0,
 CLA_CONSOLE_NOT_IMPLEMENTED = 1,
 CLA_CONSOLE_INVALID_ARGUMENT = 2
} cla_console_status;
typedef struct cla_console_position { int x, y; } cla_console_position;
typedef struct cla_console_size { int width, height; } cla_console_size;
/* Output string pointers refer to implementation-managed storage; specify ownership before implementation. */

static inline cla_console_status cla_console_init(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_restore(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_clear(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_flush(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_title(const char * title) {
 (void)title;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_get_window_size(const cla_console_size * out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_window_size(cla_console_size size) {
 (void)size;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_get_cursor_position(const cla_console_position * out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_cursor_position(cla_console_position position) {
 (void)position;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_show_cursor(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_hide_cursor(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_is_cursor_visible(const bool * out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_is_attached(const bool * out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_echo_on(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_echo_off(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_line_on(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_set_line_off(void) {
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_write(const char * text) {
 (void)text;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static int cla_console_write_line(const char* text) {
 return printf("%s\n", text);
}

static inline cla_console_status cla_console_read(const char * prompt, const char ** out_value) {
 (void)prompt;
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_read_line(const char ** out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_read_key(const char ** out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_read_character(const char ** out_value) {
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_write_character(const char * character) {
 (void)character;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_confirm(const char * prompt, const bool * out_value) {
 (void)prompt;
 (void)out_value;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_write_color(const char * text, const char * color) {
 (void)text;
 (void)color;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}

static inline cla_console_status cla_console_write_effect(const char * text, double delay_seconds) {
 (void)text;
 (void)delay_seconds;
 return CLA_CONSOLE_NOT_IMPLEMENTED;
}