# clm_style

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Typed, renderer-independent partial styles. IDs use a private registry, not BPA. Calls require external serialization. Styles do not load fonts, draw, perform layout, or attach themselves to panels.

## Behavior and constraints

Typed, renderer-independent partial styles. IDs use a private registry, not
BPA. Calls require external serialization. Styles do not load fonts, draw,
perform layout, or attach themselves to panels.

## Integration

- C reference: [**clm_style**](../c/style.md).
- Related CLM declarations in this archive: `clm_drawing`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
