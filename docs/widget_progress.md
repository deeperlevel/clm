# clm_widget_progress

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_image`](widget_image.md)
- [`clm_widget_span`](widget_span.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_progress. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget_image](widget_image.md) - required CLM capability.
- [clm_widget_span](widget_span.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_progress when an application needs: Integer progress/HP/mana bar. Minimum is 0, maximum is 1..INT32_MAX. Creation and value/maximum setters clamp the value to [0,maximum]. Value is not changed by mouse or keyboard input. Readonly and nonfocusable by default, but not pointer-passthrough: normal widget hover/mouse handlers can display an application-owned tooltip. Disabled/hidden behavior is standard.

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
