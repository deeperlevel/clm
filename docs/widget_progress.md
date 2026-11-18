# clm_widget_progress

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Integer progress/HP/mana bar. Minimum is 0, maximum is 1..INT32_MAX. Creation and value/maximum setters clamp the value to [0,maximum]. Value is not changed by mouse or keyboard input. Readonly and nonfocusable by default, but not pointer-passthrough: normal widget hover/mouse handlers can display an application-owned tooltip. Disabled/hidden behavior is standard.

## Behavior and constraints

Integer progress/HP/mana bar. Minimum is 0, maximum is 1..INT32_MAX.
Creation and value/maximum setters clamp the value to [0,maximum].
Value is not changed by mouse or keyboard input. Readonly and nonfocusable
by default, but not pointer-passthrough: normal widget hover/mouse handlers
can display an application-owned tooltip. Disabled/hidden behavior is standard.

Main style: color is the fill, backcolor the track, plus border, padding and
per-corner radii. Defaults use the platform accent, background and border.
The background/border enclose the entire control. Content space is reserved
for optional icon and label at the zero end, then the remaining track.
Both may be supplied: icon closest to the edge, followed by label and track.
Icon is at most 16 logical units square. label_extent reserves logical units
along the progress axis; labels remain horizontal even for vertical bars.
Style gutter separates accessories and track; overflow content is clipped.

Optional text/image children are owned and automatically positioned. IDs are
exposed for content/style edits only; do not destroy/reparent/reposition them.
percentage_id always exists, hidden unless show_percentage is enabled.
Its text is managed by this widget: floor(value*100/maximum), centered in the
remaining track (not the whole control), without wrapping. Text defaults to
the platform foreground, independently of fill. Override child styles to
customize it. Initial private child style is reserved; do not edit/destroy it.
Styles are borrowed. Images are retained by their owned image widget.

Requires a sprite-capable renderer. Nonempty labels and visible percentages
require a widget-manager font resolver. Rendering errors are reported through
the surface and widget info (child text/image errors through their own info).

## Integration

- C reference: [**clm_widget_progress**](../c/widget_progress.md).
- Related CLM declarations in this archive: `clm_widget_image`, `clm_widget_span`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
