# clm_widget_toolbar

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_container`](widget_container.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_toolbar. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget_container](widget_container.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_toolbar when an application needs: Toolbar with 1-8 equal-width, owned container columns. One column (the default) has no divider. Column count is fixed at creation; column indices are zero-based. Placement uses the ordinary widget layout and rectangle.

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
