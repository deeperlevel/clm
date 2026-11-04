# CLM Surface

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_drawing](clm_drawing.md), [clm_platform](clm_platform.md)
- **External libraries / platform facilities (C reference):** Native window/event backends on the target OS or platform (such as X11, Wayland, Win32, Cocoa, browser canvas, or terminal).

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Creates native or abstract rendering surfaces, manages their geometry and visibility, and routes operating-system input and lifecycle events.

## Capabilities and operations

- Create/destroy a surface; set title, size, visibility and state.
- Poll native events and request redraw.
- Listen to pointer, keyboard, timing and surface lifecycle callbacks.
- Pause/resume event categories and support specific native event requests.
- Expose platform integration hooks where required by an implementation.

## Resource, data and ownership model

A surface owns native window/view/terminal/canvas integration as appropriate; panels and renderers use its ID without owning the surface. Event payloads are borrowed for callback scope. A surface may exist on platforms without a conventional desktop window.

## Behavioral contract and boundaries

Backend support is platform-specific and may be incomplete. Failure to create a requested backend must be explicit. Callback entry and native UI-thread requirements are implementation concerns that ports must document. `clm_surface_events` is a supporting event-data header, not an independent subsystem.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Create a window or terminal surface, register mouse and resize callbacks, and request redraws for an associated renderer.

## Related CLM modules

[clm_base](clm_base.md), [clm_drawing](clm_drawing.md), [clm_platform](clm_platform.md).
