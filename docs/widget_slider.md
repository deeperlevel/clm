# clm_widget_slider

> Language-neutral module overview and behavioral contract.

## Purpose

Horizontal integer slider: minimum 0, maximum 1..INT32_MAX (default 100). Values clamp on creation/assignment. Focusable by default; Left/Right move by step (default 1), including key repeat. Step does not quantize mouse input. Clicking the track jumps to the nearest integer and starts dragging. Grabbing the thumb preserves the pointer offset; capture allows dragging beyond bounds. Readonly and effective disabled/hidden state prevent interaction.

## Behavior and constraints

Horizontal integer slider: minimum 0, maximum 1..INT32_MAX (default 100).
Values clamp on creation/assignment. Focusable by default; Left/Right move
by step (default 1), including key repeat. Step does not quantize mouse input.
Clicking the track jumps to the nearest integer and starts dragging. Grabbing
the thumb preserves the pointer offset; capture allows dragging beyond bounds.
Readonly and effective disabled/hidden state prevent interaction.

Main style: color fills the selected rail and thumb; backcolor is the remaining
rail. Border/color/corners style the rail and thumb; padding/border inset the
control's content. The rail is vertically centered, with endpoints at the
thumb centers. Thumb height fills the content height. thumb_width (default 16)
and track_height (default 4) clamp to available space. At zero horizontal
travel, mouse movement does not change the value; arrows remain available.
Defaults use platform colors and a 160x24 preferred rectangle.

Requires a sprite-capable renderer. Styles are borrowed. Rendering/input errors
are reported through the surface and info record. Change callbacks are
input-only, see the committed value, and follow normal callback restrictions.
Programmatic setters never notify; changing maximum cancels a current drag.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_renderer`](renderer.md)
- [`clm_widget`](widget.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_slider. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget](widget.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_slider when an application needs: Horizontal integer slider: minimum 0, maximum 1..INT32_MAX (default 100). Values clamp on creation/assignment. Focusable by default; Left/Right move by step (default 1), including key repeat. Step does not quantize mouse input. Clicking the track jumps to the nearest integer and starts dragging. Grabbing the thumb preserves the pointer offset; capture allows dragging beyond bounds. Readonly and effective disabled/hidden state prevent interaction.

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
