# clm_widget_image

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_image`](image.md)
- [`clm_renderer`](renderer.md)
- [`clm_widget`](widget.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_image. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_image](image.md) - required CLM capability.
- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget](widget.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_image when an application needs: Displays an existing clm_image resource, retaining one reference while assigned. The application may release its own reference after creation/assignment. Fits the whole image inside the style-derived content box, centered and preserving its pixel aspect ratio. Images may scale up or down; DPI metadata does not affect fitting. Original colors/alpha are preserved, not tinted by the style's foreground. Backgrounds and borders are not painted.

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
