# CLM Renderer

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_drawing](clm_drawing.md), [clm_panel](clm_panel.md)
- **External libraries / platform facilities (C reference):** The selected platform graphics/runtime backend (e.g., DirectX, Metal, OpenGL/ES, Vulkan, WebGPU, terminal). Many are not yet implemented.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides a unified rendering command interface for surfaces and panels while allowing platform-specific graphics and terminal backends.

## Capabilities and operations

- Select a renderer backend and query its implemented capabilities.
- Begin/end drawing frames, clear targets and draw geometric primitives.
- Draw lines, shapes, sprites, sprite-atlas regions and text.
- Offer polyline, vertex, mesh and scene-oriented draw commands where implemented.
- Render either to an entire surface or to a panel with its own transform and clip.

## Resource, data and ownership model

A renderer consumes surfaces/panels as drawing targets and resource IDs as inputs. A surface target uses surface coordinates; a panel target uses panel-local coordinates and needs correct clipping and transforms. Resource data and backend native GPU handles are distinct.

## Behavioral contract and boundaries

Not all enumerated backends or draw commands are currently implemented. Backend selection must not silently claim universal support. The reference includes working subsets (such as terminal panel drawing and selected GPU paths) and returns explicit not-implemented or not-supported states for absent paths. Drawing is intended to occur during appropriate redraw callbacks; operations should preserve target-local clipping.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Render a sprite and text overlay to a panel using a selected supported backend, while keeping application logic independent of backend APIs.

## Related CLM modules

[clm_drawing](clm_drawing.md), [clm_panel](clm_panel.md).
