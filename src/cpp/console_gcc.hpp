#pragma once

#include <iostream>

#include "size.hpp"
#include "position.hpp"

namespace cla {
 static void cla_console_init()
 {
  // Not implemented
 }

 static void cla_console_restore()
 {
  // Not implemented
 }

 static void cla_console_clear()
 {
  // Not implemented
 }

 static void cla_console_flush()
 {
  std::cout << std::endl;
 }

 static void cla_console_set_title(const char* title)
 {
  // Not implemented
 }

 static void cla_console_get_window_size(Size* out_value)
 {
  // Not implemented
 }

 static void cla_console_set_window_size(Size size)
 {
  // Not implemented
 }

 static void cla_console_get_cursor_position(Position* out_value)
 {
  // Not implemented
 }

 static void cla_console_set_cursor_position(Position position)
 {
  // Not implemented
 }

 static void cla_console_show_cursor()
 {
  // Not implemented
 }

 static void cla_console_hide_cursor()
 {
  // Not implemented
 }

 static void cla_console_is_cursor_visible(bool* out_value)
 {
  // Not implemented
 }

 static void cla_console_is_attached(bool* out_value)
 {
  // Not implemented
 }

 static void cla_console_set_echo_on()
 {
  // Not implemented
 }

 static void cla_console_set_echo_off()
 {
  // Not implemented
 }

 static void cla_console_set_line_on()
 {
  // Not implemented
 }

 static void cla_console_set_line_off()
 {
  // Not implemented
 }

 static void cla_console_write(const char* text)
 {
  std::cout << text;
 }

 static void cla_console_write_line(const char* text)
 {
  std::cout << text << "\n";
 }

 static void cla_console_read(const char* prompt, char* out_value) {
  std::cout << prompt;

  *out_value = static_cast<char>(std::getchar());

  if (*out_value != '\n' && *out_value != EOF) {
   int c;
   while ((c = std::getchar()) != '\n' && c != EOF);
  }
 }

 static void cla_console_read_line(std::string& out_value)
 {
  std::getline(std::cin, out_value);
 }

 static void cla_console_read_key(const char* out_value)
 {
  // Not implemented
 }

 static void cla_console_read_character(const char* out_value)
 {
  // Not implemented
 }

 static void cla_console_write_character(const char* character)
 {
  std::cout << character;
 }

 static void cla_console_confirm(const char* prompt, bool* out_value)
 {
  // Not implemented
 }

 static void cla_console_write_color(const char* text, const char* color)
 {
  // Not implemented
 }

 static void cla_console_write_effect(const char* text, const float delay_seconds)
 {
  // Not implemented
 }

 class Console {
 public:

  static void init() {
   cla_console_init();
  }
  static void restore() {
   cla_console_restore();
  }
  static void clear() {
   cla_console_clear();
  }
  static void flush() {
   cla_console_flush();
  }
  static void set_title(const char* title) {
   cla_console_set_title(title);
  }
  static void get_window_size(Size* out_value) {
   cla_console_get_window_size(out_value);
  }
  static void set_window_size(Size size) {
   cla_console_set_window_size(size);
  }
  static void get_cursor_position(Position* out_value) {
   cla_console_get_cursor_position(out_value);
  }
  static void set_cursor_position(Position position) {
   cla_console_set_cursor_position(position);
  }
  static void show_cursor() {
   cla_console_show_cursor();
  }
  static void hide_cursor() {
   cla_console_hide_cursor();
  }
  static void is_cursor_visible(bool* out_value) {
   cla_console_is_cursor_visible(out_value);
  }
  static void is_attached(bool* out_value) {
   cla_console_is_attached(out_value);
  }
  static void set_echo_on() {
   cla_console_set_echo_on();
  }
  static void set_echo_off() {
   cla_console_set_echo_off();
  }
  static void set_line_on() {
   cla_console_set_echo_on();
  }
  static void set_line_off() {
   cla_console_set_line_off();
  }
  static void write(const char* text) {
   cla_console_write(text);
  }
  static void write_line(const char* text) {
   cla_console_write_line(text);
  }
  static void read(const char* prompt, char* out_value) {
   cla_console_read(prompt, out_value);
  }
  static void read_line(std::string& out_value) {
   cla_console_read_line(out_value);
  }
  static void read_key(const char* out_value) {
   cla_console_read_key(out_value);
  }
  static void read_character(const char* out_value) {
   return cla_console_read_character(out_value);
  }
  static void write_character(const char* character) {
   return cla_console_write_character(character);
  }
  static void confirm(const char* prompt, bool* out_value) {
   return cla_console_confirm(prompt, out_value);
  }
  static void write_color(const char* text, const char* color) {
   return cla_console_write_color(text, color);
  }
  static void write_effect(const char* text, const float delay_seconds) {
   return cla_console_write_effect(text, delay_seconds);
  }
 };
}
