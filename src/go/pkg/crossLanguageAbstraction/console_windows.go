package crossLanguageAbstraction

import (
	"fmt"
	"os"
	"syscall"
	"unsafe"
)

const (
	stdInputHandle  = uint32(0xFFFFFFF6)
	stdOutputHandle = uint32(0xFFFFFFF5)
	enableEchoInput = 0x0004
	enableLineInput = 0x0002
	enableVT        = 0x0004
)

var (
	k32              = syscall.NewLazyDLL("kernel32.dll")
	getStdHandle     = k32.NewProc("GetStdHandle")
	getConsoleMode   = k32.NewProc("GetConsoleMode")
	setConsoleMode   = k32.NewProc("SetConsoleMode")
	getCSBI          = k32.NewProc("GetConsoleScreenBufferInfo")
	setConsoleTitleW = k32.NewProc("SetConsoleTitleW")
)

type winCoord struct{ X, Y int16 }
type winRect struct{ Left, Top, Right, Bottom int16 }
type winCSBI struct {
	Size              winCoord
	CursorPosition    winCoord
	Attributes        uint16
	Window            winRect
	MaximumWindowSize winCoord
}
type platformInputState struct {
	input, output       uint32
	hasInput, hasOutput bool
}

func winHandle(kind uint32) (uintptr, error) {
	r, _, err := getStdHandle.Call(uintptr(kind))
	if r == 0 || r == ^uintptr(0) {
		return 0, fmt.Errorf("%w: GetStdHandle: %v", ErrUnsupported, err)
	}
	return r, nil
}
func winGetMode(handle uintptr) (uint32, error) {
	var mode uint32
	r, _, err := getConsoleMode.Call(handle, uintptr(unsafe.Pointer(&mode)))
	if r == 0 {
		return 0, fmt.Errorf("%w: GetConsoleMode: %v", ErrUnsupported, err)
	}
	return mode, nil
}
func winSetMode(handle uintptr, mode uint32) error {
	r, _, err := setConsoleMode.Call(handle, uintptr(mode))
	if r == 0 {
		return fmt.Errorf("SetConsoleMode: %w", err)
	}
	return nil
}
func platformIsTerminal(f *os.File) bool {
	if f == nil {
		return false
	}
	_, err := winGetMode(f.Fd())
	return err == nil
}
func platformCaptureInput() (platformInputState, error) {
	var original platformInputState
	if handle, err := winHandle(stdInputHandle); err == nil {
		if mode, err := winGetMode(handle); err == nil {
			original.input = mode
			original.hasInput = true
		}
	}
	if handle, err := winHandle(stdOutputHandle); err == nil {
		if mode, err := winGetMode(handle); err == nil {
			original.output = mode
			original.hasOutput = true
		}
	}
	if !original.hasInput && !original.hasOutput {
		return original, ErrUnsupported
	}
	return original, nil
}
func platformRestoreInput(original platformInputState) error {
	var result error
	if original.hasInput {
		if h, e := winHandle(stdInputHandle); e == nil {
			result = winSetMode(h, original.input)
		}
	}
	if original.hasOutput {
		if h, e := winHandle(stdOutputHandle); e == nil {
			if e = winSetMode(h, original.output); e != nil {
				result = e
			}
		}
	}
	return result
}
func platformEnableANSI() error {
	h, err := winHandle(stdOutputHandle)
	if err != nil {
		return err
	}
	mode, err := winGetMode(h)
	if err != nil {
		return err
	}
	if mode&enableVT != 0 {
		return nil
	}
	if err = winSetMode(h, mode|enableVT); err != nil {
		return fmt.Errorf("%w: VT output: %v", ErrUnsupported, err)
	}
	return nil
}
func platformWindowSize() (Size, error) {
	h, err := winHandle(stdOutputHandle)
	if err != nil {
		return Size{}, err
	}
	var info winCSBI
	r, _, e := getCSBI.Call(h, uintptr(unsafe.Pointer(&info)))
	if r == 0 {
		return Size{}, fmt.Errorf("%w: GetConsoleScreenBufferInfo: %v", ErrUnsupported, e)
	}
	return Size{Width: int(info.Window.Right-info.Window.Left) + 1, Height: int(info.Window.Bottom-info.Window.Top) + 1}, nil
}
func platformSetWindowSize(Size) error {
	return ErrUnsupported /* Windows window resizing requires separate buffer and window coordination. */
}
func platformSetTitle(title string) error {
	if !terminalOutput() {
		return ErrUnsupported
	}
	ptr, err := syscall.UTF16PtrFromString(title)
	if err != nil {
		return err
	}
	r, _, e := setConsoleTitleW.Call(uintptr(unsafe.Pointer(ptr)))
	if r == 0 {
		return fmt.Errorf("SetConsoleTitleW: %w", e)
	}
	return nil
}
func platformCursorPosition() (Position, error) {
	h, err := winHandle(stdOutputHandle)
	if err != nil {
		return Position{}, err
	}
	var info winCSBI
	r, _, e := getCSBI.Call(h, uintptr(unsafe.Pointer(&info)))
	if r == 0 {
		return Position{}, fmt.Errorf("%w: GetConsoleScreenBufferInfo: %v", ErrUnsupported, e)
	}
	return Position{X: int(info.CursorPosition.X), Y: int(info.CursorPosition.Y)}, nil
}
func platformSetInputFlag(echo, enabled bool) error {
	h, err := winHandle(stdInputHandle)
	if err != nil {
		return err
	}
	mode, err := winGetMode(h)
	if err != nil {
		return err
	}
	flag := uint32(enableLineInput)
	if echo {
		flag = enableEchoInput
	}
	if enabled {
		mode |= flag
		if echo {
			mode |= enableLineInput
		}
	} else {
		mode &^= flag
		if !echo {
			mode &^= enableEchoInput
		}
	}
	return winSetMode(h, mode)
}
func platformRawInput() (func(), error) {
	h, err := winHandle(stdInputHandle)
	if err != nil {
		return nil, err
	}
	original, err := winGetMode(h)
	if err != nil {
		return nil, err
	}
	if err = winSetMode(h, original&^(enableLineInput|enableEchoInput)); err != nil {
		return nil, err
	}
	return func() { _ = winSetMode(h, original) }, nil
}
