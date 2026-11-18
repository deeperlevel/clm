# clm_math_random

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Deterministic random-number state and sampling.

## Behavior and constraints

Deterministic random-number state and sampling.

The caller controls generator state. Reusing a seed reproduces the sequence; this module
is distinct from cryptographic random generation.

## Integration

- C reference: [**clm_math_random**](../c/math_random.md).
- Related CLM declarations in this archive: `clm_math_core`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
