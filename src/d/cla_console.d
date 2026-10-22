module cla_console;

import core.sync.mutex : Mutex;

enum ConsoleStatus { ok, notImplemented, invalidArgument }
struct Position { int x; int y; }
struct Size { int width; int height; }

// Placeholder for platform-specific lifecycle state.
private __gshared bool initialized;
private __gshared bool cursorChanged;
private __gshared bool cursorVisible;


ConsoleStatus cla_console_init() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_restore() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_clear() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_flush() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_title(string title) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_get_window_size(out Size result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_window_size(Size size) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_get_cursor_position(out Position result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_cursor_position(Position position) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_show_cursor() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_hide_cursor() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_is_cursor_visible(out bool result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_is_attached(out bool result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_echo_on() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_echo_off() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_line_on() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_set_line_off() {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_write(string text) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_write_line(string text) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_read(string prompt, out string result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_read_line(out string result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_read_key(out string result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_read_character(out string result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_write_character(string character) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_confirm(string prompt, out bool result) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_write_color(string text, string color) {
    return ConsoleStatus.notImplemented;
}

ConsoleStatus cla_console_write_effect(string text, double delay_seconds) {
    return ConsoleStatus.notImplemented;
}

struct Console {
    static ConsoleStatus Init() {
        return cla_console_init();
    }
    static ConsoleStatus Restore() {
        return cla_console_restore();
    }
    static ConsoleStatus Clear() {
        return cla_console_clear();
    }
    static ConsoleStatus Flush() {
        return cla_console_flush();
    }
    static ConsoleStatus SetTitle(string title) {
        return cla_console_set_title(title);
    }
    static ConsoleStatus GetWindowSize(out Size result) {
        return cla_console_get_window_size(result);
    }
    static ConsoleStatus SetWindowSize(Size size) {
        return cla_console_set_window_size(size);
    }
    static ConsoleStatus GetCursorPosition(out Position result) {
        return cla_console_get_cursor_position(result);
    }
    static ConsoleStatus SetCursorPosition(Position position) {
        return cla_console_set_cursor_position(position);
    }
    static ConsoleStatus ShowCursor() {
        return cla_console_show_cursor();
    }
    static ConsoleStatus HideCursor() {
        return cla_console_hide_cursor();
    }
    static ConsoleStatus IsCursorVisible(out bool result) {
        return cla_console_is_cursor_visible(result);
    }
    static ConsoleStatus IsAttached(out bool result) {
        return cla_console_is_attached(result);
    }
    static ConsoleStatus SetEchoOn() {
        return cla_console_set_echo_on();
    }
    static ConsoleStatus SetEchoOff() {
        return cla_console_set_echo_off();
    }
    static ConsoleStatus SetLineOn() {
        return cla_console_set_line_on();
    }
    static ConsoleStatus SetLineOff() {
        return cla_console_set_line_off();
    }
    static ConsoleStatus Write(string text) {
        return cla_console_write(text);
    }
    static ConsoleStatus WriteLine(string text) {
        return cla_console_write_line(text);
    }
    static ConsoleStatus Read(string prompt, out string result) {
        return cla_console_read(prompt, result);
    }
    static ConsoleStatus ReadLine(out string result) {
        return cla_console_read_line(result);
    }
    static ConsoleStatus ReadKey(out string result) {
        return cla_console_read_key(result);
    }
    static ConsoleStatus ReadCharacter(out string result) {
        return cla_console_read_character(result);
    }
    static ConsoleStatus WriteCharacter(string character) {
        return cla_console_write_character(character);
    }
    static ConsoleStatus Confirm(string prompt, out bool result) {
        return cla_console_confirm(prompt, result);
    }
    static ConsoleStatus WriteColor(string text, string color) {
        return cla_console_write_color(text, color);
    }
    static ConsoleStatus WriteEffect(string text, double delay_seconds) {
        return cla_console_write_effect(text, delay_seconds);
    }
}
