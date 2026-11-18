# clm_widget_slider

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

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

## Integration

- C reference: [**clm_widget_slider**](../c/widget_slider.md).
- Related CLM declarations in this archive: `clm_renderer`, `clm_widget`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
