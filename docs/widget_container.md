# clm_widget_container

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_renderer`](renderer.md)
- [`clm_widget_manager`](widget_manager.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_container. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget_manager](widget_manager.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_container when an application needs: A virtual grouping widget; panels remain flat. The container owns its descendants through the widget tree. It starts transparent, nonfocusable, and accepts pointer input so empty areas can receive wheel scrolling.

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
