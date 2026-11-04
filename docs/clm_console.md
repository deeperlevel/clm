# CLM Console

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_stringtify](clm_stringtify.md), [clm_uuid](clm_uuid.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Console text input/output, formatted object display and optional terminal styling.

## Capabilities and operations

- Write text and CLM objects; object rendering uses clm_stringtify.
- Read console input and provide typical terminal controls, colors and presentation helpers.
- Treat ANSI support and terminal features as capabilities that may vary by platform.
- Do not assume arbitrary console output is the same as structured serialization.

## Detailed module contract

## 1. Purpose and scope

CLM Console provides a small shared interface for standard-stream text input/output and common ANSI terminal controls. It supplements language standard libraries with consistent, optional helpers; it does not replace them.

The public operations describe observable behavior, while implementations may use their language's standard I/O facilities and terminal-specific output sequences. A reference implementation may use ordinary standard-stream facilities and ANSI escape sequences.

The API does not promise that terminal-control sequences are supported by the active host. Redirected output may contain the literal escape sequences.

## 2. Result model

Representative results include `clm_console_status`:

| Result | Value | Meaning |
|---|---:|---|
| `CLM_CONSOLE_SUCCESS` | `0` | Operation completed successfully |
| `CLM_CONSOLE_FAILURE` | `-1` | I/O, formatting, allocation, or clock failure |
| `CLM_CONSOLE_INVALID_ARGUMENT` | `-2` | Null pointer, invalid value, malformed character, or invalid confirmation response |

Other language bindings may use their native result type, but should preserve the distinction between invalid arguments and operational failures where possible.

## 3. Text output

| Operation | Behavior |
|---|---|
| `clm_console_write(text)` | Writes text to standard output without a newline. |
| `clm_console_write_line(text)` | Writes text and appends exactly one newline. |
| `clm_console_write_format(format, ...)` | Writes formatted text to standard output without a newline. |
| `clm_console_write_line_format(format, ...)` | Writes formatted text and appends exactly one newline. |
| `clm_console_write_character(character)` | Writes exactly one valid UTF-8 encoded Unicode scalar value. |
| `clm_console_write_color(text, color)` | Writes color sequence, text, then reset sequence. |
| `clm_console_write_effect(text, delay_seconds)` | Writes text one byte at a time, waiting between bytes; does not append a newline. |

Formatted output may follow a binding-specific format grammar. They format into dynamically allocated storage before writing, so a format/allocation failure writes no formatted text. A later stream write failure may still leave partial output. Other language ports should use their native formatting conventions and document them.

The color argument is an ANSI SGR sequence, commonly one of the `CONSOLE_COLOR_*` constants. The reference constants include regular and bold foreground colors, background colors, and reset. Color output always writes the reset sequence after the text when preceding writes succeed. It does not detect terminal attachment or suppress ANSI codes when redirected.

`write_character` validates a single UTF-8 encoded scalar, rejecting malformed or multi-character inputs. `write_effect` iterates bytes, so a multi-byte UTF-8 character may be emitted in multiple delayed steps. The reference timing is presentation-oriented; this is for presentation only, not precise timing.

## 4. Text input

| Operation | Behavior |
|---|---|
| `clm_console_read_line(out_text)` | Reads a line from standard input and returns a pointer to shared internal storage. |
| `clm_console_read(prompt, out_text)` | Writes an optional prompt, flushes standard output, then reads a line. |
| `clm_console_read_character(out_text)` | Reads one byte from standard input and returns a NUL-terminated two-byte buffer. |
| `clm_console_read_key(out_text)` | Alias of `read_character`; it does not provide raw, nonblocking, or special-key input. |
| `clm_console_confirm(prompt, out_value)` | Writes an optional prompt followed by ` [y/n]: ` and parses one response. |

The reference binding may impose a fixed line-input limit. CRLF and LF terminators are removed. Overlong lines fail, are drained through the next newline, and clear the output buffer. The returned line and character pointers refer to static shared storage and are overwritten by the next corresponding read.

Character/key input uses standard buffered input; in an interactive terminal the user may need to press Enter. It reads a byte, not a decoded Unicode character or terminal key event.

Confirmation trims leading/trailing whitespace and accepts `y`, `yes`, `n`, or `no` case-insensitively. Any other input returns `CLM_CONSOLE_INVALID_ARGUMENT`; the function does not repeatedly prompt.

## 5. ANSI terminal controls

| Operation | Behavior |
|---|---|
| `clm_console_clear()` | Emits the ANSI clear-screen and home-position sequence. |
| `clm_console_set_cursor_position(position)` | Moves cursor to a zero-based `{x, y}` position. |
| `clm_console_show_cursor()` | Emits the ANSI show-cursor sequence. |
| `clm_console_hide_cursor()` | Emits the ANSI hide-cursor sequence. |
| `clm_console_flush()` | Flushes standard output. |

The reference implementation also provides stream-targeted forms ending in `_to` for output/control operations (and input functions ending in `_from`). They accept C `FILE*` streams and are implementation conveniences, not required pointer-based signatures for other languages.

`clm_console_init()` and `clm_console_restore()` are successful no-ops in the standard-reference implementation. There is no state to initialize or restore because it does not change terminal modes. Implementations with stateful native features should describe and restore that state explicitly.

ANSI control support depends on the terminal. The API does not query whether output is attached to a terminal and cannot confirm that a terminal acted on a request.

## 6. Portability limits and unsupported features

The shared reference implementation intentionally omits operations that standard C I/O and ANSI output cannot reliably provide:

- Setting a window or terminal title.
- Querying or changing window dimensions.
- Querying the cursor position or actual cursor visibility.
- Detecting whether a standard stream is attached to a terminal.
- Changing input echo or canonical/raw mode.
- Reading an individual key without standard input buffering.

Platform-specific implementations may provide such features separately, but should not claim they are universally supported by the shared API. ANSI output may be displayed as literal escape characters in environments that do not interpret it.

## 7. Examples

```text
clm_console_write_line("CLM Console")
clm_console_write_format("Count: %d", count)
clm_console_write_line_format("Result: %s", result)

if clm_console_read("Name: ", name) == SUCCESS:
    clm_console_write_line_format("Hello, %s", name)

if clm_console_confirm("Continue?", answer) == SUCCESS and answer:
    run_next_step()
```

Formatting syntax varies by language binding; and all status-returning operations should be checked when output or input failure matters.

## 8. Suggested conformance checks

- Write versus write-line newline behavior.
- Formatted integer/string output and exactly one line terminator.
- Invalid null format, text, stream, and output pointers.
- Valid and malformed UTF-8 character writes.
- LF/CRLF input, EOF, and overlong-line handling.
- Confirmation parsing with whitespace and mixed case.
- ANSI output sequences and cursor-coordinate conversion.
- Invalid coordinates, delays, and output-stream failures.
- Behavior under redirected streams and terminals without ANSI support.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Write text and CLM objects; object rendering uses clm_stringtify.
