# clm_uuid

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

UUID generation, parsing, formatting, and field inspection.

## Behavior and constraints

UUID generation, parsing, formatting, and field inspection.

UUID byte layout and textual formatting are separate concerns. Version-specific
generation has distinct time and randomness requirements.

## Integration

- C reference: [**clm_uuid**](../c/uuid.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
