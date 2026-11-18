# clm_string_base64

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Base64 encoding and decoding.

## Behavior and constraints

Base64 encoding and decoding.

Input lengths and padding determine output capacity. Binary decoding must not treat
embedded NUL bytes as string terminators.

## Integration

- C reference: [**clm_string_base64**](../c/string_base64.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
