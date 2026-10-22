//go:build !linux && !windows

package crossLanguageAbstraction

import "os"

type platformInputState struct{}

func platformCaptureInput() (platformInputState, error) { return platformInputState{}, ErrUnsupported }
func platformRestoreInput(platformInputState) error     { return nil }
func platformIsTerminal(*os.File) bool                  { return false }
func platformEnableANSI() error                         { return ErrUnsupported }
func platformWindowSize() (Size, error)                 { return Size{}, ErrUnsupported }
func platformSetWindowSize(Size) error                  { return ErrUnsupported }
func platformSetTitle(string) error                     { return ErrUnsupported }
func platformCursorPosition() (Position, error)         { return Position{}, ErrUnsupported }
func platformSetInputFlag(bool, bool) error             { return ErrUnsupported }
func platformRawInput() (func(), error)                 { return nil, ErrUnsupported }
