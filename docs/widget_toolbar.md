# clm_widget_toolbar

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Toolbar with 1-8 equal-width, owned container columns. One column (the default) has no divider. Column count is fixed at creation; column indices are zero-based. Placement uses the ordinary widget layout and rectangle.

## Behavior and constraints

Toolbar with 1-8 equal-width, owned container columns. One column (the
default) has no divider. Column count is fixed at creation; column indices
are zero-based. Placement uses the ordinary widget layout and rectangle.

Each column starts with GRID layout using column_rows/column_columns (1x1
by default). Retrieve a column ID, then create/reparent buttons or any other
widgets into it. Existing container/manager APIs can change its grid, child
layout, style, clipping and scrolling. Grid capacity must fit its children;
existing grid overflow rules still apply. Normal container scrolling is on.

The toolbar owns column geometry/lifetime/visibility and its direct child
list. Do not destroy/reparent/hide columns or add other direct children.
Destroying the toolbar destroys columns and their descendants.

Toolbar style controls background, border, padding and horizontal gutter.
Vertical dividers use its border color (platform button-border color if
unspecified) and divider_width (default 1). The gutter adds space around
each divider. Columns' outer slots are equal; their style margins inset
those slots. On narrow toolbars divider/gutter widths clamp to available
space, keeping all allocations nonnegative. Width zero hides dividers.
Styles are borrowed and observed through normal style revision handling.
Backgrounds/dividers require a sprite-capable renderer; errors are reported
through get_info and the surface. Layout changes commit at the next poll.

## Integration

- C reference: [**clm_widget_toolbar**](../c/widget_toolbar.md).
- Related CLM declarations in this archive: `clm_widget_container`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
