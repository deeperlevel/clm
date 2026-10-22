package crossLanguageAbstraction

import (
	"bufio"
	"errors"
	"io"
	"os"
	"strings"
	"testing"
)

func withInput(text string, action func()) {
	inputMu.Lock()
	previous := inputReader
	inputReader = bufio.NewReader(strings.NewReader(text))
	inputMu.Unlock()
	defer func() { inputMu.Lock(); inputReader = previous; inputMu.Unlock() }()
	action()
}
func captureOutput(t *testing.T, action func()) string {
	t.Helper()
	read, write, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	previous := os.Stdout
	os.Stdout = write
	defer func() { os.Stdout = previous }()
	action()
	write.Close()
	bytes, err := io.ReadAll(read)
	read.Close()
	if err != nil {
		t.Fatal(err)
	}
	return string(bytes)
}
func TestWriteAndFacade(t *testing.T) {
	got := captureOutput(t, func() {
		if err := Console.Write("A"); err != nil {
			t.Fatal(err)
		}
		if err := Console.WriteLine("B"); err != nil {
			t.Fatal(err)
		}
		if err := Console.WriteCharacter("é"); err != nil {
			t.Fatal(err)
		}
		if err := Console.Flush(); err != nil {
			t.Fatal(err)
		}
	})
	if got != "AB\né" {
		t.Fatalf("got %q", got)
	}
}
func TestInvalidCharacter(t *testing.T) {
	for _, s := range []string{"", "ab", "\xff"} {
		if err := Console.WriteCharacter(s); !errors.Is(err, ErrInvalidArgument) {
			t.Fatalf("%q: %v", s, err)
		}
	}
}
func TestReadLineAndCharacter(t *testing.T) {
	withInput("Chris\r\nñ", func() {
		s, err := Console.ReadLine()
		if err != nil || s != "Chris" {
			t.Fatalf("readline: %q, %v", s, err)
		}
		s, err = Console.ReadCharacter()
		if err != nil || s != "ñ" {
			t.Fatalf("readchar: %q, %v", s, err)
		}
	})
}
func TestConfirm(t *testing.T) {
	withInput("maybe\nyEs\n", func() {
		got := captureOutput(t, func() {
			answer, err := Console.Confirm("Continue? ")
			if err != nil || !answer {
				t.Fatalf("confirm: %v, %v", answer, err)
			}
		})
		if got != "Continue? Continue? " {
			t.Fatalf("prompt output %q", got)
		}
	})
}
func TestRedirectedColor(t *testing.T) {
	got := captureOutput(t, func() {
		if err := Console.WriteColor("hello", "green"); err != nil {
			t.Fatal(err)
		}
	})
	if got != "hello" {
		t.Fatalf("got %q", got)
	}
	if err := Console.WriteColor("x", "ultraviolet"); !errors.Is(err, ErrInvalidArgument) {
		t.Fatal(err)
	}
}
func TestEffect(t *testing.T) {
	got := captureOutput(t, func() {
		if err := Console.WriteEffect("abc", 0); err != nil {
			t.Fatal(err)
		}
	})
	if got != "abc\n" {
		t.Fatalf("got %q", got)
	}
	if err := Console.WriteEffect("x", -1); !errors.Is(err, ErrInvalidArgument) {
		t.Fatal(err)
	}
}
func TestInputValidation(t *testing.T) {
	if err := Console.SetCursorPosition(Position{-1, 0}); !errors.Is(err, ErrInvalidArgument) {
		t.Fatal(err)
	}
	if err := Console.SetWindowSize(Size{Width: 0, Height: 20}); !errors.Is(err, ErrInvalidArgument) {
		t.Fatal(err)
	}
	if err := Console.SetTitle("hi\x1b"); !errors.Is(err, ErrInvalidArgument) {
		t.Fatal(err)
	}
}
func TestRedirectedClear(t *testing.T) {
	captureOutput(t, func() {
		if err := Console.Clear(); !errors.Is(err, ErrUnsupported) {
			t.Fatalf("expected unsupported; got %v", err)
		}
		if _, err := Console.IsAttached(); err != nil {
			t.Fatal(err)
		}
	})
}
func TestSessionRestoresAfterError(t *testing.T) {
	sentinel := errors.New("action error")
	err := Console.Session(func() error { return sentinel })
	if !errors.Is(err, sentinel) {
		t.Fatal(err)
	}
	state.mu.Lock()
	initialized := state.initialized
	state.mu.Unlock()
	if initialized {
		t.Fatal("session left console initialized")
	}
}
func TestUnknownVisibility(t *testing.T) {
	state.mu.Lock()
	state.cursorVisible = nil
	state.mu.Unlock()
	_, err := Console.IsCursorVisible()
	if !errors.Is(err, ErrUnsupported) {
		t.Fatal(err)
	}
}
func TestReadEOF(t *testing.T) {
	withInput("", func() {
		_, err := Console.ReadLine()
		if !errors.Is(err, io.EOF) {
			t.Fatalf("got %v", err)
		}
	})
}
