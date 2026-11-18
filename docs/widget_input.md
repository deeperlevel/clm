# clm_widget_input

> Language-neutral module overview and behavioral contract.

## Purpose

UTF-8 textbox with codepoint caret/selection (not grapheme, bidi or shaping). Lines break on LF, without automatic wrapping; caret navigation scrolls the text viewport on either axis. CRLF/CR normalize to LF. Single-line inputs reject pasted/programmatic line breaks. Tabs display as four spaces.

## Behavior and constraints

UTF-8 textbox with codepoint caret/selection (not grapheme, bidi or shaping).
Lines break on LF, without automatic wrapping; caret navigation scrolls the
text viewport on either axis. CRLF/CR normalize to LF. Single-line inputs
reject pasted/programmatic line breaks. Tabs display as four spaces.

Left/Right, Up/Down, Home/End, Ctrl+Home/End, Shift selection, Backspace/Delete,
Ctrl/Cmd+A/C/X/V and mouse drag selection are supported. TAB cycles focus.
The existing on_enter widget handler is submission ONLY for inputs: Enter
submits a single line; Ctrl+Enter submits multiline, otherwise Enter inserts LF.
Physical keys handle Enter; committed TEXT newlines are ignored to avoid
double insertion. Submissions and changes run under widget callback guards.
Undo defaults to 100 steps (history_limit), including programmatic edits.
Each content-changing edit is one step, without typing coalescing. New edits
discard redo; no-ops and rejected edits do not. Creation starts with no history.
Ctrl/Cmd+Z undoes, Ctrl/Cmd+Shift+Z or Ctrl+Y redoes, respecting readonly.
Snapshots include real password text and are cleared before release.

Password 0 displays text, 1 displays '*' per codepoint, 2 displays no text or
selection and keeps the caret at the origin. Both password modes block copy
and cut, but permit paste. Info/change callbacks expose real text to the
trusted application. Buffers are cleared before release; this is not secure
storage against process inspection.

Numeric filters validate the complete replacement atomically. Decimal and
integer permit a leading minus; empty and intermediate '-', '.', '-.' forms
are valid while editing. Decimal uses style.decimal_separator ('.' or ',').
Hex permits ASCII 0-9/a-f/A-F only, no prefix/sign. Restricted filters reject
all whitespace, including newlines. Invalid user edits are ignored and set
last_edit_status, not a surface error. Font/renderer/clipboard errors report.

Accessories must be distinct existing same-surface widgets, not ancestors.
Creation reparents and owns them, reserving their preferred widths beside
text. Their internal children remain usable (e.g. a Send button label).
The internal viewport is reserved; do not destroy/reparent/resize it.
A font resolver and sprite/text-capable renderer are needed for visible
editing. Empty unfocused inputs can draw their box without a font.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_platform`](platform.md)
- [`clm_renderer`](renderer.md)
- [`clm_widget_manager`](widget_manager.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_input. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_platform](platform.md) - required CLM capability.
- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget_manager](widget_manager.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_input when an application needs: UTF-8 textbox with codepoint caret/selection (not grapheme, bidi or shaping). Lines break on LF, without automatic wrapping; caret navigation scrolls the text viewport on either axis. CRLF/CR normalize to LF. Single-line inputs reject pasted/programmatic line breaks. Tabs display as four spaces.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
