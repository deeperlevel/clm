module tests.test;

import std.format : format;
import std.stdio : File, stdout;
import clm.console;

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com
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

struct ClmTestResult {
    int passed;
    int failed;
}

__gshared int clmTestsPassed = 0;
__gshared int clmTestsFailed = 0;

private __gshared File clmTestLogFile;
private __gshared bool clmTestLogFailed;
__gshared bool clmTestConsoleFailed;

enum string DIVIDER_SINGLE = "-------------------------------------------------------------";
enum string DIVIDER_DOUBLE = "=============================================================";

private void recordConsoleStatus(ConsoleStatus status)
{
    if (status != ConsoleStatus.ok)
        clmTestConsoleFailed = true;
}

private void recordLogStatus(ConsoleStatus status)
{
    if (status != ConsoleStatus.ok)
        clmTestLogFailed = true;
}

bool clmTestLogOpen(string path)
{
    if (path is null || path.length == 0 || clmTestLogFile.isOpen)
        return false;
    try
    {
        clmTestLogFile = File(path, "w");
        clmTestLogFailed = false;
        return true;
    }
    catch (Exception)
    {
        clmTestLogFile = File.init;
        return false;
    }
}

bool clmTestLogClose()
{
    if (!clmTestLogFile.isOpen)
        return false;
    try
    {
        clmTestLogFile.flush();
    }
    catch (Exception)
    {
        clmTestLogFailed = true;
    }
    try
    {
        clmTestLogFile.close();
    }
    catch (Exception)
    {
        clmTestLogFailed = true;
    }
    return !clmTestLogFailed;
}

void clmTestWrite(string text)
{
    recordConsoleStatus(clm_console_write(text));
    if (clmTestLogFile.isOpen)
        recordLogStatus(clm_console_write_to(clmTestLogFile, text));
}

void clmTestWriteLine(string text)
{
    recordConsoleStatus(clm_console_write_line(text));
    if (clmTestLogFile.isOpen)
    {
        recordLogStatus(clm_console_write_line_to(clmTestLogFile, text));
        try
        {
            clmTestLogFile.flush();
        }
        catch (Exception)
        {
            clmTestLogFailed = true;
        }
    }
}

void clmTestWriteColor(string text, string color)
{
    recordConsoleStatus(clm_console_write_color(text, color));
    if (clmTestLogFile.isOpen)
        recordLogStatus(clm_console_write_to(clmTestLogFile, text));
}

void clmTestWriteFormat(Args...)(string formatString, Args args)
{
    auto text = format(formatString, args);
    recordConsoleStatus(clm_console_write_to(stdout, text));
    if (clmTestLogFile.isOpen)
        recordLogStatus(clm_console_write_to(clmTestLogFile, text));
}

void clmTestWriteLineFormat(Args...)(string formatString, Args args)
{
    auto text = format(formatString, args);
    recordConsoleStatus(clm_console_write_line(text));
    if (clmTestLogFile.isOpen)
    {
        recordLogStatus(clm_console_write_line_to(clmTestLogFile, text));
        try
        {
            clmTestLogFile.flush();
        }
        catch (Exception)
        {
            clmTestLogFailed = true;
        }
    }
}

void clmTestBegin(string title)
{
    clmTestWriteLine("");
    clmTestWriteLine(DIVIDER_DOUBLE);
    clmTestWriteColor(title, CONSOLE_COLOR_YELLOW);
    clmTestWriteLine("");
    clmTestWriteLine(DIVIDER_DOUBLE);
}

void clmAssert(ref ClmTestResult result, bool condition, string description = "")
{
    if (condition)
    {
        clmTestWrite("    ");
        clmTestWriteColor("PASSED:", CONSOLE_COLOR_GREEN);
        clmTestWrite(" ");
        clmTestWriteLine(description);
        ++result.passed;
    }
    else
    {
        ++result.failed;
        clmTestWrite("    ");
        clmTestWriteColor("FAILED:", CONSOLE_COLOR_RED);
        clmTestWrite(" ");
        clmTestWriteLine(description);
    }
}

void clmTestSummary(const int passed, const int failed)
{
    clmTestsPassed += passed;
    clmTestsFailed += failed;

    clmTestWriteLine(DIVIDER_SINGLE);
    clmTestWrite("    ");
    clmTestWriteColor("PASSED:", CONSOLE_COLOR_GREEN);
    clmTestWriteFormat(" %d  |  ", passed);
    clmTestWriteColor("FAILED:", CONSOLE_COLOR_RED);
    clmTestWriteFormat(" %d  |  ", failed);
    clmTestWriteColor("TOTAL:", CONSOLE_COLOR_CYAN);
    clmTestWriteLineFormat(" %d", passed + failed);
}

void overallTestSummary(string title)
{
    clmTestWriteLine("");
    clmTestWriteLine(DIVIDER_DOUBLE);
    clmTestWriteColor(title, CONSOLE_COLOR_YELLOW);
    clmTestWriteLine("");
    clmTestWriteLine(DIVIDER_DOUBLE);
    clmTestWrite("  ");
    clmTestWriteColor("PASSED:", CONSOLE_COLOR_GREEN);
    clmTestWriteFormat(" %d  |  ", clmTestsPassed);
    clmTestWriteColor("FAILED:", CONSOLE_COLOR_RED);
    clmTestWriteFormat(" %d  |  ", clmTestsFailed);
    clmTestWriteColor("TOTAL:", CONSOLE_COLOR_CYAN);
    clmTestWriteLineFormat(" %d", clmTestsPassed + clmTestsFailed);
    clmTestWriteLine(DIVIDER_DOUBLE);
    clmTestWriteLine("");
}
