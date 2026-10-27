package clm

import (
	"fmt"
	"io"
	"math"
	"os"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"
)

type ConsoleStatus int

const (
	ConsoleSuccess          ConsoleStatus = 0
	ConsoleFailure          ConsoleStatus = -1
	ConsoleInvalidArgument  ConsoleStatus = -2
	ConsoleInputCapacity                  = 4096
	ConsoleColorReset                     = "\x1b[0m"
	ConsoleColorBlack                     = "\x1b[30m"
	ConsoleColorRed                       = "\x1b[31m"
	ConsoleColorGreen                     = "\x1b[32m"
	ConsoleColorYellow                    = "\x1b[33m"
	ConsoleColorBlue                      = "\x1b[34m"
	ConsoleColorMagenta                   = "\x1b[35m"
	ConsoleColorCyan                      = "\x1b[36m"
	ConsoleColorWhite                     = "\x1b[37m"
	ConsoleColorBlackBold                 = "\x1b[1;30m"
	ConsoleColorRedBold                   = "\x1b[1;31m"
	ConsoleColorGreenBold                 = "\x1b[1;32m"
	ConsoleColorYellowBold                = "\x1b[1;33m"
	ConsoleColorBlueBold                  = "\x1b[1;34m"
	ConsoleColorMagentaBold               = "\x1b[1;35m"
	ConsoleColorCyanBold                  = "\x1b[1;36m"
	ConsoleColorWhiteBold                 = "\x1b[1;37m"
	ConsoleColorBlackBG                   = "\x1b[40m"
	ConsoleColorRedBG                     = "\x1b[41m"
	ConsoleColorGreenBG                   = "\x1b[42m"
	ConsoleColorYellowBG                  = "\x1b[43m"
	ConsoleColorBlueBG                    = "\x1b[44m"
	ConsoleColorMagentaBG                 = "\x1b[45m"
	ConsoleColorCyanBG                    = "\x1b[46m"
	ConsoleColorWhiteBG                   = "\x1b[47m"
)

func ConsoleWriteTo(writer io.Writer, text string) ConsoleStatus {
	if writer == nil {
		return ConsoleInvalidArgument
	}
	return consoleWriteAll(writer, []byte(text))
}

func ConsoleInit() ConsoleStatus {
	return ConsoleSuccess
}

func ConsoleRestore() ConsoleStatus {
	return ConsoleSuccess
}

func ConsoleClearTo(writer io.Writer) ConsoleStatus {
	return ConsoleWriteTo(writer, "\x1b[2J\x1b[H")
}

func ConsoleClear() ConsoleStatus {
	return consoleWriteAndFlush(os.Stdout, "\x1b[2J\x1b[H")
}

func ConsoleFlushTo(writer io.Writer) ConsoleStatus {
	if writer == nil {
		return ConsoleInvalidArgument
	}
	if flusher, ok := writer.(interface{ Flush() error }); ok {
		if flusher.Flush() != nil {
			return ConsoleFailure
		}
	}
	return ConsoleSuccess
}

func ConsoleFlush() ConsoleStatus {
	return ConsoleFlushTo(os.Stdout)
}

func ConsoleSetCursorPositionTo(writer io.Writer, position Position) ConsoleStatus {
	if writer == nil || position.X < 0 || position.Y < 0 {
		return ConsoleInvalidArgument
	}
	sequence := "\x1b[" + strconv.FormatInt(int64(position.Y)+1, 10) + ";" + strconv.FormatInt(int64(position.X)+1, 10) + "H"
	return ConsoleWriteTo(writer, sequence)
}

func ConsoleSetCursorPosition(position Position) ConsoleStatus {
	return consoleWriteAndFlushCursor(position)
}

func ConsoleSetCursorVisibleTo(writer io.Writer, visible bool) ConsoleStatus {
	if visible {
		return ConsoleWriteTo(writer, "\x1b[?25h")
	}
	return ConsoleWriteTo(writer, "\x1b[?25l")
}

func ConsoleShowCursor() ConsoleStatus {
	return consoleWriteAndFlush(os.Stdout, "\x1b[?25h")
}

func ConsoleHideCursor() ConsoleStatus {
	return consoleWriteAndFlush(os.Stdout, "\x1b[?25l")
}

func ConsoleWrite(text string) ConsoleStatus {
	return ConsoleWriteTo(os.Stdout, text)
}

func ConsoleWriteFormatTo(writer io.Writer, format string, arguments ...any) ConsoleStatus {
	if writer == nil {
		return ConsoleInvalidArgument
	}
	return consoleWriteAll(writer, fmt.Appendf(nil, format, arguments...))
}

func ConsoleWriteLineFormatTo(writer io.Writer, format string, arguments ...any) ConsoleStatus {
	if status := ConsoleWriteFormatTo(writer, format, arguments...); status != ConsoleSuccess {
		return status
	}
	return ConsoleWriteTo(writer, "\n")
}

func ConsoleWriteFormat(format string, arguments ...any) ConsoleStatus {
	return ConsoleWriteFormatTo(os.Stdout, format, arguments...)
}

func ConsoleWriteLineFormat(format string, arguments ...any) ConsoleStatus {
	status := ConsoleWriteLineFormatTo(os.Stdout, format, arguments...)
	if status == ConsoleSuccess {
		return ConsoleFlush()
	}
	return status
}

func ConsoleWriteLineTo(writer io.Writer, text string) ConsoleStatus {
	if status := ConsoleWriteTo(writer, text); status != ConsoleSuccess {
		return status
	}
	return ConsoleWriteTo(writer, "\n")
}

func ConsoleWriteLine(text string) ConsoleStatus {
	status := ConsoleWriteLineTo(os.Stdout, text)
	if status == ConsoleSuccess {
		return ConsoleFlush()
	}
	return status
}

func ConsoleReadLineFrom(reader io.Reader) (string, ConsoleStatus) {
	if reader == nil {
		return "", ConsoleInvalidArgument
	}

	line := make([]byte, 0, ConsoleInputCapacity-1)
	overflow := false
	var pendingError error
	for {
		character, err := consoleReadByte(reader, &pendingError)
		if err != nil {
			if err != io.EOF || len(line) == 0 {
				return "", ConsoleFailure
			}
			break
		}
		if character == '\n' {
			break
		}
		if len(line) < ConsoleInputCapacity-1 {
			line = append(line, character)
		} else {
			overflow = true
		}
	}
	if overflow {
		return "", ConsoleFailure
	}
	if len(line) > 0 && line[len(line)-1] == '\r' {
		line = line[:len(line)-1]
	}
	return string(line), ConsoleSuccess
}

func ConsoleReadLine() (string, ConsoleStatus) {
	return ConsoleReadLineFrom(os.Stdin)
}

func ConsoleRead(prompt string) (string, ConsoleStatus) {
	if prompt != "" && ConsoleWriteTo(os.Stdout, prompt) != ConsoleSuccess {
		return "", ConsoleFailure
	}
	if ConsoleFlush() != ConsoleSuccess {
		return "", ConsoleFailure
	}
	return ConsoleReadLine()
}

func ConsoleReadCharacterFrom(reader io.Reader) (byte, ConsoleStatus) {
	if reader == nil {
		return 0, ConsoleInvalidArgument
	}
	var character [1]byte
	if _, err := io.ReadFull(reader, character[:]); err != nil {
		return 0, ConsoleFailure
	}
	return character[0], ConsoleSuccess
}

func ConsoleReadCharacter() (byte, ConsoleStatus) {
	return ConsoleReadCharacterFrom(os.Stdin)
}

func ConsoleReadKey() (byte, ConsoleStatus) {
	return ConsoleReadCharacter()
}

func ConsoleWriteCharacterTo(writer io.Writer, character string) ConsoleStatus {
	if writer == nil || character == "" || !utf8.ValidString(character) || utf8.RuneCountInString(character) != 1 || strings.IndexByte(character, 0) >= 0 {
		return ConsoleInvalidArgument
	}
	return ConsoleWriteTo(writer, character)
}

func ConsoleWriteCharacter(character string) ConsoleStatus {
	status := ConsoleWriteCharacterTo(os.Stdout, character)
	if status == ConsoleSuccess {
		return ConsoleFlush()
	}
	return status
}

func ConsoleConfirmFrom(input io.Reader, output io.Writer, prompt string) (bool, ConsoleStatus) {
	if input == nil || output == nil {
		return false, ConsoleInvalidArgument
	}
	if prompt != "" && ConsoleWriteTo(output, prompt) != ConsoleSuccess {
		return false, ConsoleFailure
	}
	if ConsoleWriteTo(output, " [y/n]: ") != ConsoleSuccess || ConsoleFlushTo(output) != ConsoleSuccess {
		return false, ConsoleFailure
	}
	response, status := ConsoleReadLineFrom(input)
	if status != ConsoleSuccess {
		return false, status
	}
	switch strings.ToLower(strings.TrimSpace(response)) {
	case "y", "yes":
		return true, ConsoleSuccess
	case "n", "no":
		return false, ConsoleSuccess
	default:
		return false, ConsoleInvalidArgument
	}
}

func ConsoleConfirm(prompt string) (bool, ConsoleStatus) {
	return ConsoleConfirmFrom(os.Stdin, os.Stdout, prompt)
}

func ConsoleWriteColorTo(writer io.Writer, text string, color string) ConsoleStatus {
	if writer == nil {
		return ConsoleInvalidArgument
	}
	if status := ConsoleWriteTo(writer, color); status != ConsoleSuccess {
		return status
	}
	if status := ConsoleWriteTo(writer, text); status != ConsoleSuccess {
		return status
	}
	return ConsoleWriteTo(writer, ConsoleColorReset)
}

func ConsoleWriteColor(text string, color string) ConsoleStatus {
	status := ConsoleWriteColorTo(os.Stdout, text, color)
	if status == ConsoleSuccess {
		return ConsoleFlush()
	}
	return status
}

func ConsoleWait(seconds float64) ConsoleStatus {
	if math.IsNaN(seconds) || math.IsInf(seconds, 0) || seconds < 0 {
		return ConsoleInvalidArgument
	}
	duration := seconds * float64(time.Second)
	if duration >= float64(uint64(1)<<63) {
		return ConsoleInvalidArgument
	}
	time.Sleep(time.Duration(duration))
	return ConsoleSuccess
}

func ConsoleWriteEffectTo(writer io.Writer, text string, delaySeconds float64) ConsoleStatus {
	if writer == nil || math.IsNaN(delaySeconds) || math.IsInf(delaySeconds, 0) || delaySeconds < 0 {
		return ConsoleInvalidArgument
	}
	for index := 0; index < len(text); index++ {
		if status := consoleWriteAll(writer, []byte{text[index]}); status != ConsoleSuccess {
			return status
		}
		if status := ConsoleFlushTo(writer); status != ConsoleSuccess {
			return status
		}
		if index+1 < len(text) && ConsoleWait(delaySeconds) != ConsoleSuccess {
			return ConsoleFailure
		}
	}
	return ConsoleSuccess
}

func ConsoleWriteEffect(text string, delaySeconds float64) ConsoleStatus {
	return ConsoleWriteEffectTo(os.Stdout, text, delaySeconds)
}

func consoleWriteAll(writer io.Writer, data []byte) ConsoleStatus {
	for len(data) > 0 {
		written, err := writer.Write(data)
		if written < 0 || written > len(data) {
			return ConsoleFailure
		}
		data = data[written:]
		if err != nil {
			return ConsoleFailure
		}
		if written == 0 {
			return ConsoleFailure
		}
	}
	return ConsoleSuccess
}

func consoleReadByte(reader io.Reader, pendingError *error) (byte, error) {
	if *pendingError != nil {
		err := *pendingError
		*pendingError = nil
		return 0, err
	}
	var character [1]byte
	for {
		count, err := reader.Read(character[:])
		if count > 0 {
			*pendingError = err
			return character[0], nil
		}
		if err != nil {
			return 0, err
		}
	}
}

func consoleWriteAndFlush(writer io.Writer, text string) ConsoleStatus {
	if status := ConsoleWriteTo(writer, text); status != ConsoleSuccess {
		return status
	}
	return ConsoleFlushTo(writer)
}

func consoleWriteAndFlushCursor(position Position) ConsoleStatus {
	if position.X < 0 || position.Y < 0 {
		return ConsoleInvalidArgument
	}
	sequence := "\x1b[" + strconv.FormatInt(int64(position.Y)+1, 10) + ";" + strconv.FormatInt(int64(position.X)+1, 10) + "H"
	return consoleWriteAndFlush(os.Stdout, sequence)
}
