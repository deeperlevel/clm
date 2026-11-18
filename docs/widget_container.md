# clm_widget_container

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

A virtual grouping widget; panels remain flat. The container owns its descendants through the widget tree. It starts transparent, nonfocusable, and accepts pointer input so empty areas can receive wheel scrolling.

## Behavior and constraints

A virtual grouping widget; panels remain flat. The container owns its
descendants through the widget tree. It starts transparent, nonfocusable,
and accepts pointer input so empty areas can receive wheel scrolling.

Root placement uses widget.layout: ABSOLUTE, DOCK, FILL, or POPUP
(CENTER for a dialog, or an aligned side of an anchor for a popup).
A parent container controls the placement of each direct child, ignoring
that child's own layout; the child container still arranges its children.

NONE uses each child's preferred rectangle relative to this content box.
FLOW wraps preferred sizes left-to-right, using style gutters and margins.
FLEX uses the existing non-wrapping row/column shrink algorithm; GRID uses
children_layout's explicit rows and columns; LIST stacks preferred heights
vertically and stretches width. Overflow is clipped if clip_children is set.
Hidden children do not occupy space. Layout changes take effect on polling.

Automatic scrolling is enabled on both axes. Bars reserve space at the right
and bottom only when direct-child allocations overflow the content viewport.
Flex shrinking, flow wrapping and grid sizing are preserved; their preferred
sizes do not force overflow. Extents include visible direct-child allocations
and trailing margins, not transforms or descendants outside those allocations.
Negative preferred coordinates remain clipped at the zero scroll origin.
Scrolling forces viewport clipping, even if clip_children is false. Disable
both scroll axes to use only the widget's normal clip_children setting.
Pointer passthrough remains an independent panel setting.
Scroll positions clamp on content/viewport changes. Owned scrollbar IDs are
exposed for styling only, not destruction/reparenting/geometry/state changes.
Wheel input over descendants scrolls the nearest container that can move;
otherwise it chains to an outer container. Public widget events do not bubble.

Specified background and border are rendered with per-corner radii through a
sprite-capable renderer. With no visible background or border no renderer is
needed. Styles are caller-owned, and errors are reported through the surface.

## Integration

- C reference: [**clm_widget_container**](../c/widget_container.md).
- Related CLM declarations in this archive: `clm_renderer`, `clm_widget_manager`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
