# clm_widget_scroller

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Scrollbar with a proportional, minimum-sized thumb. Values and dimensions are finite logical units; range is 0..max(0, content_size - viewport_size). Values clamp on assignment. Defaults: vertical, 100 content, 10 viewport, value 0, step 16, minimum thumb 12. Focusable, with no arrow buttons.

## Behavior and constraints

Scrollbar with a proportional, minimum-sized thumb. Values and dimensions
are finite logical units; range is 0..max(0, content_size - viewport_size).
Values clamp on assignment. Defaults: vertical, 100 content, 10 viewport,
value 0, step 16, minimum thumb 12. Focusable, with no arrow buttons.

Drag the thumb (pointer capture is supplied by panels), click the track to
page, use wheel/arrow/PageUp/PageDown/Home/End keys. Readonly and effective
disabled/hidden state prevent interaction. color styles the thumb, backcolor
the track; border, padding and corner styles apply. Platform templates supply
unspecified colors. Requires a sprite-capable renderer.

Change callbacks are input-only, run under widget callback restrictions,
and observe the updated value. Programmatic setters do not notify.
Container-owned bars may be styled but not destroyed, reparented, hidden or
resized, nor have their orientation/range changed by the application.

## Integration

- C reference: [**clm_widget_scroller**](../c/widget_scroller.md).
- Related CLM declarations in this archive: `clm_renderer`, `clm_widget`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
