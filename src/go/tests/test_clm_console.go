package tests

import (
	"bytes"
	"errors"
	"io"
	"math"
	"strings"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
)

type failedWriter struct{}

func (failedWriter) Write([]byte) (int, error) { return 0, errors.New("write failed") }

type shortWriter struct{}

func (shortWriter) Write([]byte) (int, error) { return 0, nil }

func ConsoleTests() CLMTestResult {
	result := CLMTestResult{}
	CLMTestBegin("CLM - Console")
	result.Assert(clm.ConsoleInit() == 0 && clm.ConsoleRestore() == 0 && clm.ConsoleFlush() == 0, "console lifecycle")
	result.Assert(clm.ConsoleWriteTo(nil, "text") == clm.ConsoleInvalidArgument && clm.ConsoleWriteLineTo(nil, "text") == clm.ConsoleInvalidArgument && clm.ConsoleWriteLineFormatTo(nil, "%d", 1) == clm.ConsoleInvalidArgument, "reject nil streams")
	result.Assert(clm.ConsoleWriteCharacter("") == clm.ConsoleInvalidArgument && clm.ConsoleWait(-1) == clm.ConsoleInvalidArgument && clm.ConsoleWait(math.NaN()) == clm.ConsoleInvalidArgument, "reject invalid character and delay")
	var output bytes.Buffer
	result.Assert(clm.ConsoleClearTo(&output) == 0, "clear sequence")
	result.Assert(clm.ConsoleSetCursorPositionTo(&output, clm.Position{X: 2, Y: 4}) == 0 && clm.ConsoleSetCursorPositionTo(&output, clm.Position{X: -1}) == clm.ConsoleInvalidArgument, "cursor position")
	result.Assert(clm.ConsoleSetCursorVisibleTo(&output, false) == 0 && clm.ConsoleSetCursorVisibleTo(&output, true) == 0, "cursor visibility")
	result.Assert(clm.ConsoleWriteTo(&output, "text") == 0 && clm.ConsoleWriteLineTo(&output, "line") == 0, "write text and line")
	result.Assert(clm.ConsoleWriteFormatTo(&output, " value=%d %s", 42, "ok") == 0 && clm.ConsoleWriteLineFormatTo(&output, "line:%02d", 7) == 0, "formatted output")
	result.Assert(clm.ConsoleWriteCharacterTo(&output, "X") == 0 && clm.ConsoleWriteCharacterTo(&output, "") == clm.ConsoleInvalidArgument && clm.ConsoleWriteCharacterTo(&output, "XY") == clm.ConsoleInvalidArgument && clm.ConsoleWriteCharacterTo(&output, "\xc3\xa9") == 0, "single UTF-8 character")
	result.Assert(clm.ConsoleWriteColorTo(&output, "red", clm.ConsoleColorRed) == 0 && clm.ConsoleWriteEffectTo(&output, "go", 0) == 0, "color and effect output")
	result.Assert(output.String() == "\x1b[2J\x1b[H\x1b[5;3H\x1b[?25l\x1b[?25htextline\n value=42 okline:07\nX\xc3\xa9\x1b[31mred\x1b[0mgo", "exact ANSI and formatted bytes")
	input := strings.NewReader("hello\r\nlast")
	line, status := clm.ConsoleReadLineFrom(input)
	result.Assert(status == 0 && line == "hello", "read CRLF line")
	line, status = clm.ConsoleReadLineFrom(input)
	result.Assert(status == 0 && line == "last", "read final unterminated line")
	_, status = clm.ConsoleReadLineFrom(input)
	result.Assert(status == clm.ConsoleFailure, "report EOF")
	input = strings.NewReader(strings.Repeat("1", clm.ConsoleInputCapacity) + "\nok\n")
	_, status = clm.ConsoleReadLineFrom(input)
	result.Assert(status == clm.ConsoleFailure, "reject overlong line")
	line, status = clm.ConsoleReadLineFrom(input)
	result.Assert(status == 0 && line == "ok", "drain overlong input to next line")
	character, status := clm.ConsoleReadCharacterFrom(strings.NewReader("Q"))
	result.Assert(status == 0 && character == 'Q', "read character")
	for _, test := range []struct {
		input    string
		expected bool
		status   clm.ConsoleStatus
	}{{" yes \n", true, 0}, {"no\n", false, 0}, {"yesterday\n", false, clm.ConsoleInvalidArgument}} {
		output.Reset()
		confirmed, status := clm.ConsoleConfirmFrom(strings.NewReader(test.input), &output, "Continue?")
		result.Assert(status == test.status && confirmed == test.expected && output.String() == "Continue? [y/n]: ", "confirmation parsing")
	}
	result.Assert(clm.ConsoleWriteTo(failedWriter{}, "text") == clm.ConsoleFailure && clm.ConsoleWriteTo(shortWriter{}, "text") == clm.ConsoleFailure, "propagate write failures and zero progress")
	_, status = clm.ConsoleReadLineFrom(nil)
	result.Assert(status == clm.ConsoleInvalidArgument, "reject nil reader")
	result.Assert(clm.ConsoleWriteTo(io.Discard, "") == 0, "empty output succeeds")
	CLMTestSummary("Console Tests Summary", result)
	return result
}
