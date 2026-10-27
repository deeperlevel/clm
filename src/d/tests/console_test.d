module tests.console_test;

import std.stdio : File;
import clm;
import tests.test;

int clmConsoleTests()
{
    ClmTestResult result;
    clmTestBegin("CLM - Console");
    auto output = File.tmpfile();
    auto input = File.tmpfile();

    clmAssert(result, clm_console_init() == ConsoleStatus.ok, "initializes console");
    clmAssert(result, clm_console_restore() == ConsoleStatus.ok, "restores console");
    clmAssert(result, clm_console_write(null) == ConsoleStatus.invalidArgument, "rejects null text");
    clmAssert(result, clm_console_write_character("") == ConsoleStatus.invalidArgument, "rejects empty character");
    clmAssert(result, clm_console_wait(-1.0) == ConsoleStatus.invalidArgument, "rejects negative delay");
    clmAssert(result, clm_console_clear_to(output) == ConsoleStatus.ok, "writes clear-screen sequence");
    clmAssert(result, clm_console_set_cursor_position_to(
        output, Position_i32(2, 4)) == ConsoleStatus.ok, "writes cursor position");
    clmAssert(result, clm_console_set_cursor_position_to(
        output, Position_i32(-1, 0)) == ConsoleStatus.invalidArgument, "rejects negative cursor position");
    clmAssert(result, clm_console_set_cursor_visible_to(output, false) == ConsoleStatus.ok, "hides cursor");
    clmAssert(result, clm_console_set_cursor_visible_to(output, true) == ConsoleStatus.ok, "shows cursor");
    clmAssert(result, clm_console_write_to(output, "text") == ConsoleStatus.ok, "writes text");
    clmAssert(result, clm_console_write_line_to(output, "line") == ConsoleStatus.ok, "writes a line");
    clmAssert(result, clm_console_write_format_to(output, " value=%d %s", 42, "ok") == ConsoleStatus.ok, "writes formatted text");
    clmAssert(result, clm_console_write_line_format_to(output, "line:%02d", 7) == ConsoleStatus.ok, "writes formatted line");
    clmAssert(result, clm_console_write_character_to(output, "é") == ConsoleStatus.ok, "writes one UTF-8 character");
    clmAssert(result, clm_console_write_character_to(output, "XY") == ConsoleStatus.invalidArgument, "rejects multiple characters");
    clmAssert(result, clm_console_write_color_to(output, "red", CONSOLE_COLOR_RED) == ConsoleStatus.ok, "writes colored text");
    clmAssert(result, clm_console_write_effect_to(output, "go", 0.0) == ConsoleStatus.ok, "writes text effect");
    output.rewind();
    auto written = output.rawRead(new ubyte[256]);
    auto actual = cast(string)written;
    auto expected =
        "\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25h" ~
        "textline\n value=42 okline:07\né" ~
        "\x1b[31mred\x1b[0mgo";
    clmAssert(result, actual == expected, "writes expected ANSI and text output");
    output.close();

    input.rawWrite("hello\r\nlast");
    input.rewind();
    string line;
    clmAssert(result, clm_console_read_line_from(input, line) == ConsoleStatus.ok && line == "hello", "reads and strips CRLF line");
    clmAssert(result, clm_console_read_line_from(input, line) == ConsoleStatus.ok && line == "last", "reads final line without newline");
    clmAssert(result, clm_console_read_line_from(input, line) == ConsoleStatus.failure, "reports end of input");
    input.close();

    input = File.tmpfile();
    input.rawWrite("123456789\nok\n");
    input.rewind();
    clmAssert(result, clm_console_read_line_from(input, line, 8) == ConsoleStatus.failure, "rejects overlong line");
    clmAssert(result, clm_console_read_line_from(input, line, 8) == ConsoleStatus.ok && line == "ok", "drains overlong line");
    input.close();

    input = File.tmpfile();
    output = File.tmpfile();
    input.rawWrite(" yes \n");
    input.rewind();
    bool confirmed;
    clmAssert(result, clm_console_confirm_from(input, output, "Continue?", confirmed) == ConsoleStatus.ok && confirmed, "parses affirmative confirmation");
    input.close();
    output.close();

    clmTestSummary(result.passed, result.failed);
    return result.failed;
}
