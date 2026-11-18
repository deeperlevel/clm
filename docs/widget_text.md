# clm_widget_text

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Editable UTF-8 text with color tags, word wrapping and optional logical line numbers. Reuses input caret/selection/clipboard and Enter/Ctrl+Enter behavior. Multiline and wrapping default on; line numbers default off. Offsets refer to plain UTF-8 text at codepoint boundaries, not markup or grapheme clusters.

## Behavior and constraints

Editable UTF-8 text with color tags, word wrapping and optional logical line
numbers. Reuses input caret/selection/clipboard and Enter/Ctrl+Enter behavior.
Multiline and wrapping default on; line numbers default off. Offsets refer to
plain UTF-8 text at codepoint boundaries, not markup or grapheme clusters.

config.text and set_text parse registered ``<style=ID>`...`</style>`` color tags.
Tags may nest; unmatched/malformed tags or unregistered IDs fail atomically.
&lt;, &gt; and &amp; escape literal text. Other text is literal. Typing, paste,
insert_text and set_plain_text NEVER parse tags. Clipboard contains plain text.
Insertions inherit the preceding character's style (the following character
at offset zero); selection replacement inherits the first replaced style.

Body style controls typography and foreground/background. Tags override only
explicitly specified color, otherwise inheriting body color. Number style
controls the gutter's typography/foreground/background. Their box properties
do not alter the outer widget box; use config.widget.style_id for that.
The style_ids array is copied. Styles are borrowed, observed by revision
without replacing application callbacks, and must remain alive until
unregistered/destruction.

Wrapping retains all whitespace and falls back to codepoint breaks for long
words. Continuations have no repeated number. With numbers enabled, every
wrapped row of the caret's logical line is highlighted. No syntax analysis,
bidi/shaping or rich-text clipboard formats are provided.
Undo defaults to 100 steps (history_limit), including programmatic text and
inline-format edits, without typing coalescing. Restores plain text, inline
styles and selection. Creation starts with no history; new edits discard redo.
Ctrl/Cmd+Z undoes, Ctrl/Cmd+Shift+Z or Ctrl+Y redoes, respecting readonly.

## Integration

- C reference: [**clm_widget_text**](../c/widget_text.md).
- Related CLM declarations in this archive: `clm_platform`, `clm_renderer`, `clm_widget_manager`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
