# clm_gamevironment

> Language-neutral module overview and behavioral contract.

## Purpose

One global, UI-thread-owned game environment. Initialize creates a surface, its renderer, widget manager, full-surface game panel and optional audio engine. IDs are automatic; the renderer is addressed by the surface ID, not a separate registry ID. Do not destroy owned components or replace their handlers directly.

## Behavior and constraints

One global, UI-thread-owned game environment. Initialize creates a surface,
its renderer, widget manager, full-surface game panel and optional audio engine.
IDs are automatic; the renderer is addressed by the surface ID, not a separate
registry ID. Do not destroy owned components or replace their handlers directly.

step is nonblocking: poll input/chrono, update outside dispatch, then lay out
and draw. frame_rate zero is unlimited; positive rates cap update/render frames,
not a fixed physics timestep. A throttled step still pumps input/chrono and
layout, but retains pending redraw until a frame is due. Delta time is
measured monotonic elapsed time (first update: zero); there is no catch-up.
Rendering remains subject to visibility, backend readiness and presentation.

Initialization/update/shutdown hooks may mutate widgets and panels. Input,
window and draw hooks run under ordinary surface callback restrictions.
request_stop is callback-safe. initialize, run, step and shutdown are not
reentrant. run is desktop-only; browser/mobile hosts schedule step themselves.

run returns without destroying resources. Call shutdown afterwards, including
after errors. Initialization failure attempts rollback. Cleanup failure keeps
IDs and cleanup_pending so shutdown can retry; initialize cannot overwrite it.
The shutdown hook also unwinds partially failed initialization hooks. It may
be retried if it fails, and must release its audio playbacks before returning.
The environment never shuts down global CLM registries or unrelated resources.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_audio`](audio.md)
- [`clm_renderer`](renderer.md)
- [`clm_widget_manager`](widget_manager.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_gamevironment. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_audio](audio.md) - required CLM capability.
- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget_manager](widget_manager.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_gamevironment when an application needs: One global, UI-thread-owned game environment. Initialize creates a surface, its renderer, widget manager, full-surface game panel and optional audio engine. IDs are automatic; the renderer is addressed by the surface ID, not a separate registry ID. Do not destroy owned components or replace their handlers directly.

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
