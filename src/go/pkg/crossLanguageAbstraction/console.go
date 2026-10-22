package crossLanguageAbstraction

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

var (
	ErrUnsupported     = errors.New("console capability is unavailable")
	ErrInvalidArgument = errors.New("invalid console argument")
)

type consoleState struct {
	mu            sync.Mutex
	initialized   bool
	originalInput platformInputState
	cursorVisible *bool // nil means unknown; changes by third parties cannot be detected.
	cursorChanged bool
}

var state consoleState

type consoleFacade struct{}

var Console = consoleFacade{}

var inputMu sync.Mutex
var inputReader = bufio.NewReader(os.Stdin)

func claConsoleInit() error {
	state.mu.Lock()
	defer state.mu.Unlock()
	if state.initialized {
		return nil
	}
	original, err := platformCaptureInput()
	if err != nil && !errors.Is(err, ErrUnsupported) {
		return err
	}
	state.originalInput = original
	state.initialized = true
	return nil
}
func claConsoleRestore() error {
	state.mu.Lock()
	defer state.mu.Unlock()
	if !state.initialized {
		return nil
	}
	var err error
	if state.cursorChanged && terminalOutput() {
		_, err = io.WriteString(os.Stdout, "\x1b[?25h") /* best-effort: original visibility unknown */
	}
	err = errors.Join(err, platformRestoreInput(state.originalInput))
	state.initialized = false
	state.cursorChanged = false
	state.cursorVisible = nil
	return err
}
func claConsoleSession(action func() error) (err error) {
	if action == nil {
		return fmt.Errorf("%w: nil action", ErrInvalidArgument)
	}
	if err = claConsoleInit(); err != nil {
		return err
	}
	defer func() { err = errors.Join(err, claConsoleRestore()) }()
	return action()
}
func claConsoleWrite(text string) error { _, err := io.WriteString(os.Stdout, text); return err }
func claConsoleWriteLine(text string) error {
	_, err := io.WriteString(os.Stdout, text+"\n")
	return err
}
func claConsoleFlush() error { return nil }
func terminalOutput() bool   { return platformIsTerminal(os.Stdout) }
func terminalInput() bool    { return platformIsTerminal(os.Stdin) }
func ansi(sequence string) error {
	if !terminalOutput() {
		return ErrUnsupported
	}
	if err := platformEnableANSI(); err != nil {
		return err
	}
	return claConsoleWrite(sequence)
}
func claConsoleClear() error { return ansi("\x1b[2J\x1b[H") }
func claConsoleSetTitle(title string) error {
	if strings.ContainsAny(title, "\x00\x07\x1b\r\n") {
		return fmt.Errorf("%w: title contains terminal controls", ErrInvalidArgument)
	}
	return platformSetTitle(title)
}
func claConsoleGetWindowSize() (Size, error) { return platformWindowSize() }
func claConsoleSetWindowSize(size Size) error {
	if size.Width < 1 || size.Height < 1 {
		return fmt.Errorf("%w: window size must be positive", ErrInvalidArgument)
	}
	return platformSetWindowSize(size)
}
func claConsoleGetCursorPosition() (Position, error) { return platformCursorPosition() }
func claConsoleSetCursorPosition(position Position) error {
	if position.X < 0 || position.Y < 0 {
		return fmt.Errorf("%w: cursor position is negative", ErrInvalidArgument)
	}
	return ansi(fmt.Sprintf("\x1b[%d;%dH", position.Y+1, position.X+1))
}
func claConsoleShowCursor() error { return changeCursor(true) }
func claConsoleHideCursor() error { return changeCursor(false) }
func changeCursor(visible bool) error {
	sequence := "\x1b[?25l"
	if visible {
		sequence = "\x1b[?25h"
	}
	if err := ansi(sequence); err != nil {
		return err
	}
	state.mu.Lock()
	defer state.mu.Unlock()
	value := visible
	state.cursorVisible = &value
	state.cursorChanged = true
	return nil
}
func claConsoleIsCursorVisible() (bool, error) {
	state.mu.Lock()
	defer state.mu.Unlock()
	if state.cursorVisible == nil {
		return false, ErrUnsupported
	}
	return *state.cursorVisible, nil
}
func claConsoleIsAttached() (bool, error) { return terminalOutput(), nil }
func claConsoleSetEchoOn() error          { return platformSetInputFlag(true, true) }
func claConsoleSetEchoOff() error         { return platformSetInputFlag(true, false) }
func claConsoleSetLineOn() error          { return platformSetInputFlag(false, true) }
func claConsoleSetLineOff() error         { return platformSetInputFlag(false, false) }
func claConsoleRead(prompt string) (string, error) {
	if err := claConsoleWrite(prompt); err != nil {
		return "", err
	}
	return claConsoleReadLine()
}
func claConsoleReadLine() (string, error) {
	inputMu.Lock()
	defer inputMu.Unlock()
	text, err := inputReader.ReadString('\n')
	if err != nil && !(errors.Is(err, io.EOF) && len(text) > 0) {
		return "", err
	}
	return strings.TrimSuffix(strings.TrimSuffix(text, "\n"), "\r"), nil
}
func claConsoleReadCharacter() (string, error) {
	inputMu.Lock()
	defer inputMu.Unlock()
	r, _, err := inputReader.ReadRune()
	if err != nil {
		return "", err
	}
	return string(r), nil
}
func claConsoleReadKey() (string, error) {
	if !terminalInput() {
		return "", ErrUnsupported
	}
	inputMu.Lock()
	defer inputMu.Unlock()
	restore, err := platformRawInput()
	if err != nil {
		return "", err
	}
	defer restore()
	r, _, err := inputReader.ReadRune()
	if err != nil {
		return "", err
	}
	return string(r), nil
}
func claConsoleWriteCharacter(char string) error {
	if utf8.RuneCountInString(char) != 1 || !utf8.ValidString(char) {
		return fmt.Errorf("%w: exactly one Unicode code point required", ErrInvalidArgument)
	}
	return claConsoleWrite(char)
}
func claConsoleConfirm(prompt string) (bool, error) {
	for {
		answer, err := claConsoleRead(prompt)
		if err != nil {
			return false, err
		}
		switch strings.ToLower(strings.TrimSpace(answer)) {
		case "y", "yes":
			return true, nil
		case "n", "no":
			return false, nil
		}
	}
}

var colors = map[string]int{"black": 30, "red": 31, "green": 32, "yellow": 33, "blue": 34, "magenta": 35, "cyan": 36, "white": 37}

func claConsoleWriteColor(text, color string) error {
	code, ok := colors[strings.ToLower(color)]
	if !ok {
		return fmt.Errorf("%w: unknown color %q", ErrInvalidArgument, color)
	}
	if !terminalOutput() {
		return claConsoleWrite(text)
	}
	if err := platformEnableANSI(); err != nil {
		return err
	}
	return claConsoleWrite(fmt.Sprintf("\x1b[%dm%s\x1b[0m", code, text))
}
func claConsoleWriteEffect(text string, delay float32) error {
	if delay < 0 {
		return fmt.Errorf("%w: negative delay", ErrInvalidArgument)
	}
	for _, char := range text {
		if err := claConsoleWrite(string(char)); err != nil {
			return err
		}
		if delay > 0 {
			time.Sleep(time.Duration(float64(delay) * float64(time.Second)))
		}
	}
	return claConsoleWriteLine("")
}

func (consoleFacade) Init() error                          { return claConsoleInit() }
func (consoleFacade) Restore() error                       { return claConsoleRestore() }
func (consoleFacade) Session(action func() error) error    { return claConsoleSession(action) }
func (consoleFacade) Clear() error                         { return claConsoleClear() }
func (consoleFacade) Flush() error                         { return claConsoleFlush() }
func (consoleFacade) SetTitle(title string) error          { return claConsoleSetTitle(title) }
func (consoleFacade) GetWindowSize() (Size, error)         { return claConsoleGetWindowSize() }
func (consoleFacade) SetWindowSize(size Size) error        { return claConsoleSetWindowSize(size) }
func (consoleFacade) GetCursorPosition() (Position, error) { return claConsoleGetCursorPosition() }
func (consoleFacade) SetCursorPosition(position Position) error {
	return claConsoleSetCursorPosition(position)
}
func (consoleFacade) ShowCursor() error                   { return claConsoleShowCursor() }
func (consoleFacade) HideCursor() error                   { return claConsoleHideCursor() }
func (consoleFacade) IsCursorVisible() (bool, error)      { return claConsoleIsCursorVisible() }
func (consoleFacade) IsAttached() (bool, error)           { return claConsoleIsAttached() }
func (consoleFacade) SetEchoOn() error                    { return claConsoleSetEchoOn() }
func (consoleFacade) SetEchoOff() error                   { return claConsoleSetEchoOff() }
func (consoleFacade) SetLineOn() error                    { return claConsoleSetLineOn() }
func (consoleFacade) SetLineOff() error                   { return claConsoleSetLineOff() }
func (consoleFacade) Write(text string) error             { return claConsoleWrite(text) }
func (consoleFacade) WriteLine(text string) error         { return claConsoleWriteLine(text) }
func (consoleFacade) Read(prompt string) (string, error)  { return claConsoleRead(prompt) }
func (consoleFacade) ReadLine() (string, error)           { return claConsoleReadLine() }
func (consoleFacade) ReadKey() (string, error)            { return claConsoleReadKey() }
func (consoleFacade) ReadCharacter() (string, error)      { return claConsoleReadCharacter() }
func (consoleFacade) WriteCharacter(char string) error    { return claConsoleWriteCharacter(char) }
func (consoleFacade) Confirm(prompt string) (bool, error) { return claConsoleConfirm(prompt) }
func (consoleFacade) WriteColor(text, color string) error { return claConsoleWriteColor(text, color) }
func (consoleFacade) WriteEffect(text string, delay float32) error {
	return claConsoleWriteEffect(text, delay)
}
