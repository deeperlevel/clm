# clm_widget_text

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_platform`](platform.md)
- [`clm_renderer`](renderer.md)
- [`clm_widget_manager`](widget_manager.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_text. It explains
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

Use clm_widget_text when an application needs: Editable UTF-8 text with color tags, word wrapping and optional logical line numbers. Reuses input caret/selection/clipboard and Enter/Ctrl+Enter behavior. Multiline and wrapping default on; line numbers default off. Offsets refer to plain UTF-8 text at codepoint boundaries, not markup or grapheme clusters.

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
