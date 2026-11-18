# clm_renderer

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Renderer selection, drawing targets, and backend dispatch.

## Behavior and constraints

Renderer selection, drawing targets, and backend dispatch.

Public drawing operations resolve a surface or panel target before calling backend
functions. Availability and supported operations vary by backend.

Central drawing API with private native GPU backends and Terminal rendering.

## Integration

- C reference: [**clm_renderer**](../c/renderer.md).
- Related CLM declarations in this archive: `clm_panel`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
