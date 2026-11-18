# clm_widget_toast

> Language-neutral module overview and behavioral contract.

## Purpose

Noninteractive, temporary notification overlay for application and game surfaces.

## Behavior and constraints

A toast shows a short message and optionally an icon above the normal widget layer. It does not take focus or intercept pointer input. Supported placement is top-center, center, or bottom-center of the surface.

Toasts begin hidden and become visible when shown. Showing a new message replaces the previous message and restarts the lifetime. A configurable expiration interval hides the toast automatically; a zero-duration toast remains visible until explicitly hidden. Timed expiration is driven by the surface's event/clock updates, so applications must keep processing events.

The root controls background, border, and padding; child image and text widgets allow additional presentation customization.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_renderer`](renderer.md)
- [`clm_widget`](widget.md)
- [`clm_widget_image`](widget_image.md)
- [`clm_widget_span`](widget_span.md)

## Integration
- Capability availability can vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_toast. It explains
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

Use clm_widget_toast when an application needs: Noninteractive, temporary notification overlay for application and game surfaces.

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
