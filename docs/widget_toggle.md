# clm_widget_toggle

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_image`](image.md)
- [`clm_renderer`](renderer.md)
- [`clm_widget_span`](widget_span.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_toggle. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_image](image.md) - required CLM capability.
- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget_span](widget_span.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_toggle when an application needs: Checkbox with an optional owned span label. A label ID is assigned internally; callers may edit its text/style, but must not destroy, reparent or reposition it. The label is pointer-passthrough, so clicking it activates the toggle. The checkbox uses up to 16 logical units, vertically centered in the content box, with a gap of up to 4 units before the label. The label gets the remaining width and follows layout changes. Panel transforms are not inherited by child widgets, as with other widget compositions.

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
