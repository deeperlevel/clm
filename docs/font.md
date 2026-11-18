# clm_font

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Font loading, glyph metrics, and reference-counted font storage.

## Behavior and constraints

Font loading, glyph metrics, and reference-counted font storage.

Font data must remain available while glyph operations use it. Native font-library
handles are released when their CLM resource lifetime ends.

## Integration

- C reference: [**clm_font**](../c/font.md).
- Related CLM declarations in this archive: `clm_resource`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
