# clm_widget_toggle

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Checkbox with an optional owned span label. A label ID is assigned internally; callers may edit its text/style, but must not destroy, reparent or reposition it. The label is pointer-passthrough, so clicking it activates the toggle. The checkbox uses up to 16 logical units, vertically centered in the content box, with a gap of up to 4 units before the label. The label gets the remaining width and follows layout changes. Panel transforms are not inherited by child widgets, as with other widget compositions.

## Behavior and constraints

Checkbox with an optional owned span label. A label ID is assigned internally;
callers may edit its text/style, but must not destroy, reparent or reposition it.
The label is pointer-passthrough, so clicking it activates the toggle.
The checkbox uses up to 16 logical units, vertically centered in the content
box, with a gap of up to 4 units before the label. The label gets the
remaining width and follows layout changes. Panel transforms are not inherited
by child widgets, as with other widget compositions.

A click or a non-repeating Space press cycles unchecked -> checked, and when
tristate is enabled also -> semi-checked -> unchecked. Disabled or readonly
toggles ignore user activation. Programmatic state changes remain allowed.
Internal activation runs before application widget event handlers.

The default checkbox is a monochrome drawn glyph tinted with the resolved
foreground color. Per-state image IDs override it and retain their original
colors. The toggle retains each assigned image reference. Labels use the
normal span font resolver and inherited typography. No background is painted.
Default glyph rendering requires a sprite-capable renderer.

## Integration

- C reference: [**clm_widget_toggle**](../c/widget_toggle.md).
- Related CLM declarations in this archive: `clm_image`, `clm_renderer`, `clm_widget_span`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
