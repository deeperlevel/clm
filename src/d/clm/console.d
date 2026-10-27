module clm.console;

import std.datetime.stopwatch : AutoStart, StopWatch;
import std.format : dFormat = format;
import std.math : isFinite;
import std.stdio : File, stdin, stdout;
import clm.base : Position_i32;

enum ConsoleStatus : int
{
    ok = 0,
    success = ok,
    failure = -1,
    invalidArgument = -2
}

enum size_t CONSOLE_INPUT_CAPACITY = 4096;

enum string CONSOLE_COLOR_RESET = "\x1b[0m";
enum string CONSOLE_COLOR_BLACK = "\x1b[30m";
enum string CONSOLE_COLOR_RED = "\x1b[31m";
enum string CONSOLE_COLOR_GREEN = "\x1b[32m";
enum string CONSOLE_COLOR_YELLOW = "\x1b[33m";
enum string CONSOLE_COLOR_BLUE = "\x1b[34m";
enum string CONSOLE_COLOR_MAGENTA = "\x1b[35m";
enum string CONSOLE_COLOR_CYAN = "\x1b[36m";
enum string CONSOLE_COLOR_WHITE = "\x1b[37m";
enum string CONSOLE_COLOR_BLACK_BOLD = "\x1b[1;30m";
enum string CONSOLE_COLOR_RED_BOLD = "\x1b[1;31m";
enum string CONSOLE_COLOR_GREEN_BOLD = "\x1b[1;32m";
enum string CONSOLE_COLOR_YELLOW_BOLD = "\x1b[1;33m";
enum string CONSOLE_COLOR_BLUE_BOLD = "\x1b[1;34m";
enum string CONSOLE_COLOR_MAGENTA_BOLD = "\x1b[1;35m";
enum string CONSOLE_COLOR_CYAN_BOLD = "\x1b[1;36m";
enum string CONSOLE_COLOR_WHITE_BOLD = "\x1b[1;37m";
enum string CONSOLE_COLOR_BLACK_BG = "\x1b[40m";
enum string CONSOLE_COLOR_RED_BG = "\x1b[41m";
enum string CONSOLE_COLOR_GREEN_BG = "\x1b[42m";
enum string CONSOLE_COLOR_YELLOW_BG = "\x1b[43m";
enum string CONSOLE_COLOR_BLUE_BG = "\x1b[44m";
enum string CONSOLE_COLOR_MAGENTA_BG = "\x1b[45m";
enum string CONSOLE_COLOR_CYAN_BG = "\x1b[46m";
enum string CONSOLE_COLOR_WHITE_BG = "\x1b[47m";

private ConsoleStatus write(File stream, string text)
{
    if (!stream.isOpen || text is null)
        return ConsoleStatus.invalidArgument;
    try
    {
        stream.rawWrite(text);
        return ConsoleStatus.ok;
    }
    catch (Exception)
    {
        return ConsoleStatus.failure;
    }
}

private ConsoleStatus flush(File stream)
{
    if (!stream.isOpen)
        return ConsoleStatus.invalidArgument;
    try
    {
        stream.flush();
        return ConsoleStatus.ok;
    }
    catch (Exception)
    {
        return ConsoleStatus.failure;
    }
}

private ConsoleStatus readByte(File stream, out ubyte value)
{
    value = 0;
    if (!stream.isOpen)
        return ConsoleStatus.invalidArgument;
    try
    {
        ubyte[1] byteBuffer;
        auto readBytes = stream.rawRead(byteBuffer[]);
        if (readBytes.length == 0)
            return ConsoleStatus.failure;
        value = readBytes[0];
        return ConsoleStatus.ok;
    }
    catch (Exception)
    {
        return ConsoleStatus.failure;
    }
}

ConsoleStatus clm_console_init()
{
    return ConsoleStatus.ok;
}

ConsoleStatus clm_console_restore()
{
    return ConsoleStatus.ok;
}

ConsoleStatus cla_console_init()
{
    return clm_console_init();
}

ConsoleStatus cla_console_restore()
{
    return clm_console_restore();
}

ConsoleStatus clm_console_write_to(File stream, string text)
{
    return write(stream, text);
}

ConsoleStatus clm_console_write(string text)
{
    return clm_console_write_to(stdout, text);
}

ConsoleStatus clm_console_write_line_to(File stream, string text)
{
    auto status = write(stream, text);
    if (status != ConsoleStatus.ok)
        return status;
    return write(stream, "\n");
}

ConsoleStatus clm_console_write_line(string text)
{
    auto status = clm_console_write_line_to(stdout, text);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_vwrite_format_to(Args...)(File stream, string formatString, Args args)
{
    if (!stream.isOpen || formatString is null)
        return ConsoleStatus.invalidArgument;
    try
    {
        return write(stream, dFormat(formatString, args));
    }
    catch (Exception)
    {
        return ConsoleStatus.failure;
    }
}

ConsoleStatus clm_console_vwrite_line_format_to(Args...)(File stream, string formatString, Args args)
{
    auto status = clm_console_vwrite_format_to(stream, formatString, args);
    if (status != ConsoleStatus.ok)
        return status;
    return write(stream, "\n");
}

ConsoleStatus clm_console_write_format_to(Args...)(File stream, string formatString, Args args)
{
    return clm_console_vwrite_format_to(stream, formatString, args);
}

ConsoleStatus clm_console_write_line_format_to(Args...)(File stream, string formatString, Args args)
{
    return clm_console_vwrite_line_format_to(stream, formatString, args);
}

ConsoleStatus clm_console_write_format(Args...)(string formatString, Args args)
{
    return clm_console_write_format_to(stdout, formatString, args);
}

ConsoleStatus clm_console_write_line_format(Args...)(string formatString, Args args)
{
    auto status = clm_console_write_line_format_to(stdout, formatString, args);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_read_line_from(File stream, out string value, size_t capacity = CONSOLE_INPUT_CAPACITY)
{
    value = null;
    if (!stream.isOpen || capacity < 2 || capacity > int.max)
        return ConsoleStatus.invalidArgument;

    ubyte[] bytes;
    bool tooLong;
    bool receivedByte;
    while (true)
    {
        ubyte byteValue;
        auto status = readByte(stream, byteValue);
        if (status != ConsoleStatus.ok)
        {
            if (!stream.eof && status == ConsoleStatus.failure)
            {
                bytes = null;
                return ConsoleStatus.failure;
            }
            if (!receivedByte)
                return ConsoleStatus.failure;
            break;
        }
        receivedByte = true;
        if (byteValue == '\n')
            break;
        if (bytes.length == capacity - 1)
        {
            tooLong = true;
            while (byteValue != '\n')
            {
                status = readByte(stream, byteValue);
                if (status != ConsoleStatus.ok)
                    break;
            }
            break;
        }
        bytes ~= byteValue;
    }

    if (tooLong)
        return ConsoleStatus.failure;
    if (bytes.length != 0 && bytes[$ - 1] == '\r')
        bytes = bytes[0 .. $ - 1];
    value = cast(string)bytes;
    return ConsoleStatus.ok;
}

ConsoleStatus clm_console_read_line(out string value)
{
    return clm_console_read_line_from(stdin, value);
}

ConsoleStatus clm_console_read(string prompt, out string value)
{
    value = null;
    if (prompt !is null && clm_console_write_to(stdout, prompt) != ConsoleStatus.ok)
        return ConsoleStatus.failure;
    if (flush(stdout) != ConsoleStatus.ok)
        return ConsoleStatus.failure;
    return clm_console_read_line(value);
}

ConsoleStatus clm_console_read_character_from(File stream, out char character)
{
    character = '\0';
    ubyte byteValue;
    auto status = readByte(stream, byteValue);
    if (status == ConsoleStatus.ok)
        character = cast(char)byteValue;
    return status;
}

ConsoleStatus clm_console_read_character(out string value)
{
    value = null;
    char character;
    auto status = clm_console_read_character_from(stdin, character);
    if (status == ConsoleStatus.ok)
        value = [character].idup;
    return status;
}

ConsoleStatus clm_console_read_key(out string value)
{
    return clm_console_read_character(value);
}

private bool isUtf8Scalar(string character)
{
    if (character.length == 0)
        return false;
    auto bytes = cast(const(ubyte)[])character;
    size_t length;
    if (bytes[0] <= 0x7f)
        length = 1;
    else if (bytes[0] >= 0xc2 && bytes[0] <= 0xdf)
        length = 2;
    else if (bytes[0] >= 0xe0 && bytes[0] <= 0xef)
        length = 3;
    else if (bytes[0] >= 0xf0 && bytes[0] <= 0xf4)
        length = 4;
    else
        return false;
    if (bytes.length != length)
        return false;
    foreach (index; 1 .. length)
    {
        if ((bytes[index] & 0xc0) != 0x80)
            return false;
    }
    return !((length == 3 && bytes[0] == 0xe0 && bytes[1] < 0xa0) ||
        (length == 3 && bytes[0] == 0xed && bytes[1] >= 0xa0) ||
        (length == 4 && bytes[0] == 0xf0 && bytes[1] < 0x90) ||
        (length == 4 && bytes[0] == 0xf4 && bytes[1] >= 0x90));
}

ConsoleStatus clm_console_write_character_to(File stream, string character)
{
    if (character is null || !isUtf8Scalar(character))
        return ConsoleStatus.invalidArgument;
    return write(stream, character);
}

ConsoleStatus clm_console_write_character(string character)
{
    auto status = clm_console_write_character_to(stdout, character);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_confirm_from(File input, File output, string prompt, out bool value)
{
    if (!input.isOpen || !output.isOpen)
        return ConsoleStatus.invalidArgument;
    if (prompt !is null && clm_console_write_to(output, prompt) != ConsoleStatus.ok)
        return ConsoleStatus.failure;
    if (clm_console_write_to(output, " [y/n]: ") != ConsoleStatus.ok ||
        flush(output) != ConsoleStatus.ok)
        return ConsoleStatus.failure;

    string response;
    auto status = clm_console_read_line_from(input, response);
    if (status != ConsoleStatus.ok)
        return status;
    size_t first;
    while (first < response.length && isAsciiWhitespace(response[first]))
        ++first;
    size_t last = response.length;
    while (last > first && isAsciiWhitespace(response[last - 1]))
        --last;
    auto answer = response[first .. last];
    if (answer.length == 1 && (answer[0] == 'y' || answer[0] == 'Y'))
    {
        value = true;
        return ConsoleStatus.ok;
    }
    if (answer.length == 3 &&
        (answer[0] == 'y' || answer[0] == 'Y') &&
        (answer[1] == 'e' || answer[1] == 'E') &&
        (answer[2] == 's' || answer[2] == 'S'))
    {
        value = true;
        return ConsoleStatus.ok;
    }
    if ((answer.length == 1 && (answer[0] == 'n' || answer[0] == 'N')) ||
        (answer.length == 2 &&
         (answer[0] == 'n' || answer[0] == 'N') &&
         (answer[1] == 'o' || answer[1] == 'O')))
    {
        value = false;
        return ConsoleStatus.ok;
    }
    return ConsoleStatus.invalidArgument;
}

private bool isAsciiWhitespace(char character)
{
    return character == ' ' || character == '\t' || character == '\n' ||
        character == '\v' || character == '\f' || character == '\r';
}

ConsoleStatus clm_console_confirm(string prompt, out bool value)
{
    return clm_console_confirm_from(stdin, stdout, prompt, value);
}

ConsoleStatus clm_console_clear_to(File stream)
{
    return write(stream, "\x1b[2J\x1b[H");
}

ConsoleStatus clm_console_clear()
{
    auto status = clm_console_clear_to(stdout);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_flush()
{
    return flush(stdout);
}

ConsoleStatus clm_console_set_cursor_position_to(File stream, Position_i32 position)
{
    if (position.x < 0 || position.y < 0)
        return ConsoleStatus.invalidArgument;
    try
    {
        auto sequence = dFormat("\x1b[%d;%dH",
            cast(uint)position.y + 1, cast(uint)position.x + 1);
        return write(stream, sequence);
    }
    catch (Exception)
    {
        return ConsoleStatus.failure;
    }
}

ConsoleStatus clm_console_set_cursor_position(Position_i32 position)
{
    auto status = clm_console_set_cursor_position_to(stdout, position);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_set_cursor_visible_to(File stream, bool visible)
{
    return write(stream, visible ? "\x1b[?25h" : "\x1b[?25l");
}

ConsoleStatus clm_console_show_cursor()
{
    auto status = clm_console_set_cursor_visible_to(stdout, true);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_hide_cursor()
{
    auto status = clm_console_set_cursor_visible_to(stdout, false);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_write_color_to(File stream, string text, string color)
{
    if (!stream.isOpen || text is null || color is null)
        return ConsoleStatus.invalidArgument;
    if (write(stream, color) != ConsoleStatus.ok ||
        write(stream, text) != ConsoleStatus.ok ||
        write(stream, CONSOLE_COLOR_RESET) != ConsoleStatus.ok)
        return ConsoleStatus.failure;
    return ConsoleStatus.ok;
}

ConsoleStatus clm_console_write_color(string text, string color)
{
    auto status = clm_console_write_color_to(stdout, text, color);
    return status == ConsoleStatus.ok ? flush(stdout) : status;
}

ConsoleStatus clm_console_wait(double seconds)
{
    if (!isFinite(seconds) || seconds < 0.0)
        return ConsoleStatus.invalidArgument;
    if (seconds == 0.0)
        return ConsoleStatus.ok;
    auto timer = StopWatch(AutoStart.yes);
    while (cast(double)timer.peek().total!"nsecs" / 1_000_000_000.0 < seconds)
    {
    }
    return ConsoleStatus.ok;
}

ConsoleStatus clm_console_write_effect_to(File stream, string text, double delaySeconds)
{
    if (!stream.isOpen || text is null || !isFinite(delaySeconds) || delaySeconds < 0.0)
        return ConsoleStatus.invalidArgument;
    auto bytes = cast(const(ubyte)[])text;
    foreach (index, byteValue; bytes)
    {
        try
        {
            stream.rawWrite(bytes[index .. index + 1]);
            stream.flush();
        }
        catch (Exception)
        {
            return ConsoleStatus.failure;
        }
        if (index + 1 < bytes.length &&
            clm_console_wait(delaySeconds) != ConsoleStatus.ok)
            return ConsoleStatus.failure;
    }
    return ConsoleStatus.ok;
}

ConsoleStatus clm_console_write_effect(string text, double delaySeconds)
{
    return clm_console_write_effect_to(stdout, text, delaySeconds);
}
