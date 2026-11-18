# clm_tree

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Nested object, array, and scalar values backed by CLM storage.

## Behavior and constraints

Nested object, array, and scalar values backed by CLM storage.

Parent containers own references to child values. Structural updates must preserve type
information and release replaced child references.

## Integration

- C reference: [**clm_tree**](../c/tree.md).
- Related CLM declarations in this archive: `clm_dictionary`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
