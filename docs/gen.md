# clm_gen

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Cryptographically randomized identifiers, ranges, text and passwords. Random data comes from platform cryptographic entropy, not rand()/srand(). Integer ranges include BOTH endpoints and use rejection sampling to avoid modulo bias. Output-writing _to variants use caller buffers; ID variants update existing BPA blocks. GENERATION_FAILURE reports generation/validation failure.

## Behavior and constraints

Cryptographically randomized identifiers, ranges, text and passwords.
Random data comes from platform cryptographic entropy, not rand()/srand().
Integer ranges include BOTH endpoints and use rejection sampling to avoid
modulo bias. Output-writing _to variants use caller buffers; ID variants update
existing BPA blocks. GENERATION_FAILURE reports generation/validation failure.

## Integration

- C reference: [**clm_gen**](../c/gen.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
