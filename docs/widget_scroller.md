# clm_widget_scroller

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_renderer`](renderer.md)
- [`clm_widget`](widget.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_scroller. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget](widget.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_scroller when an application needs: Scrollbar with a proportional, minimum-sized thumb. Values and dimensions are finite logical units; range is 0..max(0, content_size - viewport_size). Values clamp on assignment. Defaults: vertical, 100 content, 10 viewport, value 0, step 16, minimum thumb 12. Focusable, with no arrow buttons.

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
