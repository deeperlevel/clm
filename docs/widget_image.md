# clm_widget_image

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Displays an existing clm_image resource, retaining one reference while assigned. The application may release its own reference after creation/assignment. Fits the whole image inside the style-derived content box, centered and preserving its pixel aspect ratio. Images may scale up or down; DPI metadata does not affect fitting. Original colors/alpha are preserved, not tinted by the style's foreground. Backgrounds and borders are not painted.

## Behavior and constraints

Displays an existing clm_image resource, retaining one reference while assigned.
The application may release its own reference after creation/assignment.
Fits the whole image inside the style-derived content box, centered and
preserving its pixel aspect ratio. Images may scale up or down; DPI metadata
does not affect fitting. Original colors/alpha are preserved, not tinted by
the style's foreground. Backgrounds and borders are not painted.

Drawing respects panel transforms and panel/ancestor clipping, once per DRAW
before application draw callbacks. Missing resources/renderers and unsupported
backends report a surface error and are recorded in image info.
Generic widget, panel, ancestor and surface destruction release the reference.

## Integration

- C reference: [**clm_widget_image**](../c/widget_image.md).
- Related CLM declarations in this archive: `clm_image`, `clm_renderer`, `clm_widget`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
