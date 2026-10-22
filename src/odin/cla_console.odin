package cla_console

Console_Status :: enum {
    Ok,
    Not_Implemented,
    Invalid_Argument,
}
Position :: struct { x, y: int }
Size :: struct { width, height: int }

Console_State :: struct {
    initialized: bool,
    cursor_changed: bool,
}
// State is private by convention; Odin has package-level scope.
console_state: Console_State

cla_console_init :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_restore :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_clear :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_flush :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_set_title :: proc(title: string) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_get_window_size :: proc() -> (Size, Console_Status) {
    return Size{}, .Not_Implemented
}

cla_console_set_window_size :: proc(size: Size) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_get_cursor_position :: proc() -> (Position, Console_Status) {
    return Position{}, .Not_Implemented
}

cla_console_set_cursor_position :: proc(position: Position) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_show_cursor :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_hide_cursor :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_is_cursor_visible :: proc() -> (bool, Console_Status) {
    return false, .Not_Implemented
}

cla_console_is_attached :: proc() -> (bool, Console_Status) {
    return false, .Not_Implemented
}

cla_console_set_echo_on :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_set_echo_off :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_set_line_on :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_set_line_off :: proc() -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_write :: proc(text: string) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_write_line :: proc(text: string) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_read :: proc(prompt: string) -> (string, Console_Status) {
    return "", .Not_Implemented
}

cla_console_read_line :: proc() -> (string, Console_Status) {
    return "", .Not_Implemented
}

cla_console_read_key :: proc() -> (string, Console_Status) {
    return "", .Not_Implemented
}

cla_console_read_character :: proc() -> (string, Console_Status) {
    return "", .Not_Implemented
}

cla_console_write_character :: proc(character: string) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_confirm :: proc(prompt: string) -> (bool, Console_Status) {
    return false, .Not_Implemented
}

cla_console_write_color :: proc(text: string, color: string) -> Console_Status {
    return Console_Status.Not_Implemented
}

cla_console_write_effect :: proc(text: string, delay_seconds: f64) -> Console_Status {
    return Console_Status.Not_Implemented
}
