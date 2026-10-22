package crossLanguageAbstraction

import (
	"fmt"
	"os"
	"syscall"
	"unsafe"
)

type platformInputState struct {
	value syscall.Termios
	valid bool
}
type linuxWinsize struct{ Rows, Cols, Xpixel, Ypixel uint16 }

func ioctl(fd uintptr, command uintptr, arg unsafe.Pointer) error {
	_, _, errno := syscall.Syscall(syscall.SYS_IOCTL, fd, command, uintptr(arg))
	if errno != 0 {
		return errno
	}
	return nil
}
func platformIsTerminal(f *os.File) bool {
	if f == nil {
		return false
	}
	var t syscall.Termios
	return ioctl(f.Fd(), uintptr(syscall.TCGETS), unsafe.Pointer(&t)) == nil
}
func platformCaptureInput() (platformInputState, error) {
	var t syscall.Termios
	if !platformIsTerminal(os.Stdin) {
		return platformInputState{}, ErrUnsupported
	}
	if err := ioctl(os.Stdin.Fd(), uintptr(syscall.TCGETS), unsafe.Pointer(&t)); err != nil {
		return platformInputState{}, err
	}
	return platformInputState{value: t, valid: true}, nil
}
func platformRestoreInput(state platformInputState) error {
	if !state.valid {
		return nil
	}
	return ioctl(os.Stdin.Fd(), uintptr(syscall.TCSETS), unsafe.Pointer(&state.value))
}
func platformEnableANSI() error {
	if !terminalOutput() {
		return ErrUnsupported
	}
	return nil
}
func platformWindowSize() (Size, error) {
	if !terminalOutput() {
		return Size{}, ErrUnsupported
	}
	var ws linuxWinsize
	if err := ioctl(os.Stdout.Fd(), uintptr(syscall.TIOCGWINSZ), unsafe.Pointer(&ws)); err != nil {
		return Size{}, err
	}
	return Size{Width: int(ws.Cols), Height: int(ws.Rows)}, nil
}
func platformSetWindowSize(size Size) error {
	if !terminalOutput() {
		return ErrUnsupported
	}
	if size.Width > 65535 || size.Height > 65535 {
		return fmt.Errorf("%w: window dimensions too large", ErrInvalidArgument)
	}
	ws := linuxWinsize{Rows: uint16(size.Height), Cols: uint16(size.Width)}
	return ioctl(os.Stdout.Fd(), uintptr(syscall.TIOCSWINSZ), unsafe.Pointer(&ws))
}
func platformSetTitle(title string) error { return ansi("\x1b]2;" + title + "\x07") }
func platformCursorPosition() (Position, error) {
	return Position{}, ErrUnsupported /* Queries consume terminal input; requires coordinated reader */
}
func platformSetInputFlag(echo, boolEnabled bool) error {
	if !terminalInput() {
		return ErrUnsupported
	}
	var t syscall.Termios
	if err := ioctl(os.Stdin.Fd(), uintptr(syscall.TCGETS), unsafe.Pointer(&t)); err != nil {
		return err
	}
	flag := uint32(syscall.ICANON)
	if echo {
		flag = uint32(syscall.ECHO)
	}
	if boolEnabled {
		t.Lflag |= flag
	} else {
		t.Lflag &^= flag
	}
	return ioctl(os.Stdin.Fd(), uintptr(syscall.TCSETS), unsafe.Pointer(&t))
}
func platformRawInput() (func(), error) {
	original, err := platformCaptureInput()
	if err != nil {
		return nil, err
	}
	t := original.value
	t.Lflag &^= uint32(syscall.ICANON | syscall.ECHO)
	t.Cc[syscall.VMIN] = 1
	t.Cc[syscall.VTIME] = 0
	if err = ioctl(os.Stdin.Fd(), uintptr(syscall.TCSETS), unsafe.Pointer(&t)); err != nil {
		return nil, err
	}
	return func() { _ = platformRestoreInput(original) }, nil
}
