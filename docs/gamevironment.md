# clm_gamevironment

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

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

## Integration

- C reference: [**clm_gamevironment**](../c/gamevironment.md).
- Related CLM declarations in this archive: `clm_audio`, `clm_renderer`, `clm_widget_manager`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
