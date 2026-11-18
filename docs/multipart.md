# clm_multipart

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

MIME multipart boundary parsing and generation.

## Behavior and constraints

MIME multipart boundary parsing and generation.

Parsed parts borrow spans from the supplied message. Boundary recognition must
distinguish delimiter lines from matching bytes inside content.

MIME multipart body parsing and generation.

Parsing is zero-copy: part bodies and header spans borrow the input bytes.
The caller supplies storage for parts and headers. Only CRLF framing is
accepted; header folding and malformed delimiter lines are rejected.

## Integration

- C reference: [**clm_multipart**](../c/multipart.md).
- Related CLM declarations in this archive: `clm_bytes`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
