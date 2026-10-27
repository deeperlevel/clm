package tests

import (
	"fmt"
	"os"
	"runtime"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

type CLMTestResult struct {
	Passed int
	Failed int
}

var clmTestsPassed int
var clmTestsFailed int
var clmTestLogFile *os.File
var clmTestLogFailed bool
var clmTestLogClosed bool

const (
	clmTestDividerSingle = "-------------------------------------------------------------"
	clmTestDividerDouble = "============================================================="
)

func CLMTestLogOpen(path string) bool {
	if path == "" || clmTestLogFile != nil {
		return false
	}
	file, err := os.Create(path)
	if err != nil {
		clmTestLogFailed = true
		return false
	}
	clmTestLogFile = file
	clmTestLogFailed = false
	clmTestLogClosed = false
	return true
}

func CLMTestLogClose() bool {
	if clmTestLogFile == nil {
		return false
	}
	success := !clmTestLogFailed
	if clm.ConsoleFlushTo(clmTestLogFile) != clm.ConsoleSuccess {
		success = false
	}
	if clmTestLogFile.Close() != nil {
		success = false
	}
	clmTestLogFile = nil
	clmTestLogClosed = true
	return success && !clmTestLogFailed
}

func CLMTestWrite(text string) {
	clm.ConsoleWrite(text)
	clmTestLogWrite(text)
}

func CLMTestWriteLine(text string) {
	clm.ConsoleWriteLine(text)
	clmTestLogWriteLine(text)
}

func CLMTestWriteColor(text string, color string) {
	clm.ConsoleWriteColor(text, color)
	clmTestLogWrite(text)
}

func CLMTestWriteFormat(format string, arguments ...any) {
	clm.ConsoleWriteFormat(format, arguments...)
	clmTestLogWriteFormat(format, arguments...)
}

func CLMTestWriteLineFormat(format string, arguments ...any) {
	clm.ConsoleWriteLineFormat(format, arguments...)
	clmTestLogWriteLineFormat(format, arguments...)
}

func CLMTestBegin(title string) {
	CLMTestWriteLine("")
	CLMTestWriteLine(clmTestDividerDouble)
	CLMTestWriteColor(title, clm.ConsoleColorYellow)
	CLMTestWriteLine("")
	CLMTestWriteLine(clmTestDividerDouble)
}

func (r *CLMTestResult) Assert(condition bool, message string) {
	if condition {
		r.Passed++
		CLMTestWrite("    ")
		CLMTestWriteColor("PASSED:", clm.ConsoleColorGreen)
		CLMTestWrite(" ")
		CLMTestWriteLine(message)
		return
	}

	r.Failed++

	_, file, line, ok := runtime.Caller(1)
	if ok {
		message = fmt.Sprintf("%s:%d: %s", file, line, message)
	}
	CLMTestWrite("    ")
	CLMTestWriteColor("FAILED:", clm.ConsoleColorRed)
	CLMTestWrite(" ")
	CLMTestWriteLine(message)
}

func CLMTestSummary(title string, result CLMTestResult) {
	clmTestsPassed += result.Passed
	clmTestsFailed += result.Failed

	CLMTestWriteLine("")
	CLMTestWriteLine(clmTestDividerDouble)
	CLMTestWriteColor(title, clm.ConsoleColorYellow)
	CLMTestWriteLine("")
	CLMTestWriteLine(clmTestDividerDouble)
	CLMTestWriteLine(clmTestDividerSingle)
	CLMTestWrite("    ")
	CLMTestWriteColor("PASSED:", clm.ConsoleColorGreen)
	CLMTestWriteFormat(" %d  |  ", result.Passed)
	CLMTestWriteColor("FAILED:", clm.ConsoleColorRed)
	CLMTestWriteFormat(" %d  |  ", result.Failed)
	CLMTestWriteColor("TOTAL:", clm.ConsoleColorCyan)
	CLMTestWriteLineFormat(" %d", result.Passed+result.Failed)
}

func CLMOverallTestSummary(title string) {
	CLMTestWriteLine("")
	CLMTestWriteLine(clmTestDividerDouble)
	CLMTestWriteColor(title, clm.ConsoleColorYellow)
	CLMTestWriteLine("")
	CLMTestWriteLine(clmTestDividerDouble)
	CLMTestWrite("  ")
	CLMTestWriteColor("PASSED:", clm.ConsoleColorGreen)
	CLMTestWriteFormat(" %d  |  ", clmTestsPassed)
	CLMTestWriteColor("FAILED:", clm.ConsoleColorRed)
	CLMTestWriteFormat(" %d  |  ", clmTestsFailed)
	CLMTestWriteColor("TOTAL:", clm.ConsoleColorCyan)
	CLMTestWriteLineFormat(" %d", clmTestsPassed+clmTestsFailed)
	CLMTestWriteLine(clmTestDividerDouble)
	CLMTestWriteLine("")
}

func clmTestLogWrite(text string) {
	clmTestEnsureLog()
	if clmTestLogFile != nil && clm.ConsoleWriteTo(clmTestLogFile, text) != clm.ConsoleSuccess {
		clmTestLogFailed = true
	}
}

func clmTestLogWriteLine(text string) {
	clmTestEnsureLog()
	if clmTestLogFile == nil {
		return
	}
	if clm.ConsoleWriteLineTo(clmTestLogFile, text) != clm.ConsoleSuccess ||
		clm.ConsoleFlushTo(clmTestLogFile) != clm.ConsoleSuccess {
		clmTestLogFailed = true
	}
}

func clmTestLogWriteFormat(format string, arguments ...any) {
	clmTestEnsureLog()
	if clmTestLogFile != nil && clm.ConsoleWriteFormatTo(clmTestLogFile, format, arguments...) != clm.ConsoleSuccess {
		clmTestLogFailed = true
	}
}

func clmTestLogWriteLineFormat(format string, arguments ...any) {
	clmTestEnsureLog()
	if clmTestLogFile == nil {
		return
	}
	if clm.ConsoleWriteLineFormatTo(clmTestLogFile, format, arguments...) != clm.ConsoleSuccess ||
		clm.ConsoleFlushTo(clmTestLogFile) != clm.ConsoleSuccess {
		clmTestLogFailed = true
	}
}

func clmTestEnsureLog() {
	if clmTestLogFile == nil && !clmTestLogClosed && !clmTestLogFailed {
		CLMTestLogOpen("main.log")
	}
}
