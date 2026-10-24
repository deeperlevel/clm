# CLM Console

## Overview

CLM Console provides a small, procedural interface for interactive terminal input, output, screen control, and console lifecycle management. It defines *what an operation does*, not which operating-system calls, terminal sequences, libraries, or language features must implement it.

CLM Console is **not** a replacement for every language's standard input/output library. Its value is collecting useful terminal capabilities behind recognizable operations across language implementations, especially those that are less convenient than basic text output.

Implementations may use native terminal APIs, ANSI/VT control sequences, or standard-language I/O as appropriate. Features that a particular environment cannot reliably support must be identified as unavailable rather than silently claimed to work.

### Design principles

- **Procedural first.** Independent `clm_console_*` operations are the shared API. An optional `Console` facade may forward to them without adding another implementation.
- **Native first.** Operating-system and language-native facilities may differ while shared observable behavior remains consistent.
- **Capability aware.** Redirected streams, IDE consoles, terminals, remote shells, and headless environments offer different capabilities.
- **Explicit lifecycle.** Console state changes should be restored when no longer needed, including after failures.
- **No mandatory OOP, reflection, or runtime type inspection.** Language-specific conveniences remain optional.

## Naming and current implementation

This document specifies the **CLM naming convention** `clm_console_*`. The supplied reference implementation still exposes the historical **CLA names** `cla_console_*`, such as `cla_console_init()` and `cla_console_write_line()`. Renaming or aliasing those functions is an implementation migration, **not something that has already happened**.

The source also exposes a `Console` convenience facade. This document describes the intended procedure-level behavior; see [Implementation observations](#implementation-observations) for known differences in the supplied reference.

## Shared concepts

| Concept | Meaning |
|---|---|
| **Console session** | A period during which console settings may be changed and subsequently restored. |
| **Attached terminal** | An interactive terminal connected to standard output. This does not necessarily imply interactive input. |
| **Position** | An `{x, y}` cursor location, expressed as zero-based column and row. |
| **Size** | A `{width, height}` terminal size in columns and rows. |
| **Capability** | A console operation supported by the current terminal, host, and implementation. |
| **Input mode** | Console configuration controlling input echo and line-buffered (canonical) input. |

`Position` and `Size` are conceptual value records. Languages can represent them with structs, records, tuples, or comparable native types.

## Quick start

The following is **language-neutral pseudocode**, not syntax required by every implementation:

```text
clm_console_init()
try:
    clm_console_set_title("CLM Example")
    clm_console_write_line("Hello from CLM Console")
    clm_console_write_color("Ready", "green")
    clm_console_write_line("")

    if clm_console_is_attached():
        clm_console_write_line("Interactive terminal detected")
finally:
    clm_console_restore()
```

An implementation may also expose a session helper that guarantees restoration around a block or scope. In languages without scope guards or automatic cleanup, callers must explicitly arrange cleanup paths.

## API reference

Names below are the **target CLM names**. The provided Python source currently uses the corresponding `cla_console_*` names unless otherwise noted.

### Lifecycle and status

| Operation | Purpose | Expected behavior |
|---|---|---|
| `clm_console_init()` | Initialize console-session management. | Capture relevant original settings where possible. Repeated calls should not repeatedly overwrite captured originals. |
| `clm_console_restore()` | Restore settings changed during the session. | Best-effort restore of saved input settings and cursor visibility; safe to call if not initialized. |
| `clm_console_session()` | Scoped initialization and restoration. | Convenience facility where the language supports it; restoration must occur even if the body fails. |
| `clm_console_is_attached()` | Check interactive standard output. | Returns whether stdout is attached to an interactive terminal. It does not guarantee support for all control sequences. |
| `clm_console_flush()` | Flush buffered output. | Flush pending standard-output and standard-error data where supported. |

**Restoration limit:** Some terminals provide no reliable way to query their original cursor visibility. An implementation may restore a conventional visible cursor rather than the exact previous state; it should document this limitation.

### Text input and output

| Operation | Inputs | Result / behavior |
|---|---|---|
| `clm_console_write(text)` | Text | Write text without automatically appending a newline. |
| `clm_console_write_line(text)` | Text, optionally empty | Write text followed by a newline. |
| `clm_console_write_character(character)` | One character | Write exactly one character; reject invalid multi-character inputs. |
| `clm_console_read(prompt)` | Optional prompt | Display prompt as applicable and read an input line; return the entered text according to the implementation's newline convention. |
| `clm_console_read_line()` | None | Read the next line from standard input. |
| `clm_console_read_key()` | None | Read one key interaction, with terminal-specific representation for special keys. |
| `clm_console_read_character()` | None | Convenience alias of key reading in the reference implementation. |
| `clm_console_confirm(prompt)` | Optional prompt | Repeatedly request a yes/no answer and return a boolean result. |

**Key input is not universally normalized.** Terminal-specific key sequences and Unicode input can differ across Windows consoles, POSIX terminals, remote terminals, and language runtimes. Implementations must document whether a returned value represents a Unicode character, a raw byte sequence, or a special-key sequence.

**Confirmation input:** The reference accepts `y`, `yes`, `n`, and `no`, case-insensitively, and repeats the prompt for other answers. Translations or localized alternatives are optional implementation extensions.

### Screen and window operations

| Operation | Inputs | Result / behavior |
|---|---|---|
| `clm_console_clear()` | None | Request screen clear and move to the home position where supported. |
| `clm_console_set_title(title)` | Title text | Request a terminal/window title change. Reject unsafe control characters. |
| `clm_console_get_window_size()` | None | Return a `Size` representing columns and rows. |
| `clm_console_set_window_size(size)` | `Size` | Request a resize; terminals may ignore it. |
| `clm_console_get_cursor_position(timeout)` | Optional nonnegative timeout | Return a `Position` if a reliable query succeeds; otherwise indicate position unavailable. |
| `clm_console_set_cursor_position(position)` | `Position` | Request cursor movement to the given zero-based coordinates. |
| `clm_console_show_cursor()` | None | Request that the cursor become visible. |
| `clm_console_hide_cursor()` | None | Request that the cursor become hidden. |
| `clm_console_is_cursor_visible()` | None | Return last visibility state requested through CLM, or unknown. Does **not** independently inspect the terminal. |

**Coordinate convention:** `Position(0, 0)` denotes the top-left character cell. A backend using one-based terminal escape-sequence coordinates converts internally. Negative positions are invalid.

**Window size:** Some systems can determine the actual size, while others use a fallback. The supplied reference uses a fallback of **80 columns × 24 rows**. This is a reference behavior rather than a guaranteed terminal dimension on every platform.

**Cursor position queries:** Querying terminal cursor position may consume input bytes. Implementations should coordinate with other input readers and respect timeouts.

### Input modes

| Operation | Effect |
|---|---|
| `clm_console_set_echo_on()` | Enable echoing typed input. |
| `clm_console_set_echo_off()` | Disable echoing typed input. |
| `clm_console_set_line_on()` | Enable line-oriented/canonical input. |
| `clm_console_set_line_off()` | Disable line-oriented/canonical input. |

Input-mode operations may be unsupported in redirected input, non-interactive environments, or backend implementations that do not expose native terminal controls. Restore original input settings after mode changes.

**Security note:** Disabling echo is not, by itself, a complete secure-password-input API.

### Formatting and effects

| Operation | Inputs | Effect |
|---|---|---|
| `clm_console_write_color(text, color)` | Text, color name | Output colored text when supported; otherwise output plain text where the backend provides a fallback. |
| `clm_console_write_effect(text, delay)` | Text, nonnegative delay per character | Output characters incrementally and append a newline. |

The reference color vocabulary is:

`black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `reset`.

Color support depends on terminal capabilities. A backend must not inject terminal control sequences into redirected files merely because colored output was requested.

`write_effect` is a presentation convenience, not a precise real-time scheduler; delays can vary with operating-system scheduling and output buffering.

## Unsupported operations and errors

Console capabilities vary much more than ordinary string or container operations. Implementations should distinguish at least:

- **Unsupported capability:** The operation cannot reliably be provided by the active terminal or platform.
- **Invalid argument:** A caller supplies an invalid size, coordinate, character, delay, title, or color.
- **Operational failure:** An otherwise supported operation fails because of input/output or operating-system errors.
- **Unavailable result:** A query (especially cursor position or cursor visibility) cannot determine a value; where appropriate, return an explicit unknown/unavailable result instead of inventing one.

The reference uses exceptions for unsupported capabilities and invalid input, and a nullable/optional value for unavailable cursor-position queries. Other language implementations may use status codes, result records, or native error conventions, provided those conditions remain distinguishable.

## Portability and environment behavior

| Environment | Typical considerations |
|---|---|
| Interactive terminal | Text operations generally work; advanced capabilities depend on terminal support. |
| Redirected stdout | Preserve plain output; avoid emitting unwanted control sequences. |
| Redirected stdin | Interactive key reading and input-mode changes may be unavailable. |
| IDE-integrated console | May not implement clearing, resize, title, or cursor queries. |
| Windows console | May need native console APIs or supported VT sequences. |
| POSIX terminal | Often uses terminal settings and ANSI/VT sequences, subject to terminal capabilities. |
| Remote/headless execution | Some operations may be unavailable or have no visible effect. |

A CLM Console implementation should not assume that `is_attached()` alone proves support for a specific control sequence. Capabilities may need to be probed, configured, or conservatively declined.

## Usage patterns

### Interactive text prompt

```text
clm_console_init()
try:
    clm_console_write_line("Employee management")
    name = clm_console_read("Employee name: ")
    if clm_console_confirm("Save employee? (y/n): "):
        save_employee(name)
finally:
    clm_console_restore()
```

### Cursor-aware rendering

```text
if clm_console_is_attached():
    clm_console_set_cursor_position(Position(x=0, y=0))
    clm_console_write_line("STATUS: READY")
else:
    clm_console_write_line("STATUS: READY")
```

An attached terminal might still reject cursor movement. The caller should handle unsupported-capability errors or check a backend-specific capability interface before attempting advanced operations.

### Temporary input mode

```text
clm_console_init()
try:
    clm_console_set_echo_off()
    secret_input = clm_console_read_line()
finally:
    clm_console_restore()
```

This example illustrates restoration discipline only; it does not define a full secure secret-input workflow.

## Optional facade

Implementations may expose a `Console` convenience interface offering short names such as `Console.write_line(...)` or `Console.clear()`. The facade must forward to the same procedural functions and state: it is not a separate console subsystem. Languages that do not support classes can omit the facade entirely.

## Implementation observations

The supplied reference source predates the CLM name change. These are implementation-specific observations, **not additional shared API requirements**:

1. **Legacy naming:** Functions are named `cla_console_*`, not `clm_console_*`. The examples in this document use the intended CLM naming convention and require matching functions or aliases when used verbatim.
2. **Hidden-cursor state issue:** `cla_console_hide_cursor()` writes `_cursor_visible` and `_cursor_changed` instead of updating the `ClaConsoleState` fields. As written, the visibility query and session restoration do not correctly track a hide request.
3. **Facade input-mode issue:** `Console.set_echo_on`, `set_echo_off`, `set_line_on`, and `set_line_off` currently point to the lower-level `_cla_console_set_input_mode` helper, which expects **two parameters**. They should point to the corresponding no-argument public functions.
4. **Facade read naming:** The facade exposes `Console.cla_console_read` rather than the otherwise consistent `Console.read` name.
5. **Session helper:** `cla_console_session()` exists in the source but is not exposed through the `Console` facade.
6. **Platform scope:** The reference performs some input-mode operations only on POSIX terminals and explicitly reports those operations unsupported on Windows without a dedicated native backend.
7. **Best-effort controls:** Title changes, resizing, screen clearing, and cursor queries are requests to the terminal, not guaranteed effects.

These observations can be addressed while migrating from CLA to CLM without changing the module's cross-language contract.

## Suggested compatibility checklist

An implementation intended to comply with this specification should verify that:

- [ ] Public console operations have the documented intent and argument semantics.
- [ ] Position coordinates are zero-based and dimensions are width × height.
- [ ] A console session can be initialized and restored safely.
- [ ] Interactive-only operations report lack of capability appropriately.
- [ ] Redirected output does not receive unintended control codes.
- [ ] Key input and special keys have documented representations.
- [ ] Cursor-visibility tracking reports requested state, not an unverified actual state.
- [ ] Input modes can be restored after exceptions or early exits.
- [ ] Invalid input is distinguished from unsupported capability and operating-system failure.
- [ ] Language-specific facades do not diverge from the procedural implementation.

## Relationship to CLM

CLM Console is an **independent, source-first module**. Applications may include just this module instead of adopting an entire CLM runtime. As with other CLM modules, developers may bundle its source for interpreted-language distribution or include separate source files for compiled languages.

**Don't start from scratch, bro.**
