# clm_hash

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Deterministic message digests, HMAC and digest-to-text utilities. A digest does not encrypt data and cannot be reversed to recover its input. Legacy digest algorithms remain available for compatibility, NOT for creating new security-sensitive authentication schemes. HMAC deliberately restricts algorithms to SHA-256/384/512; output buffers and BPA string destinations have separate ownership and size rules.

## Behavior and constraints

Deterministic message digests, HMAC and digest-to-text utilities.
A digest does not encrypt data and cannot be reversed to recover its input.
Legacy digest algorithms remain available for compatibility, NOT for creating
new security-sensitive authentication schemes. HMAC deliberately restricts
algorithms to SHA-256/384/512; output buffers and BPA string destinations have
separate ownership and size rules.

## Integration

- C reference: [**clm_hash**](../c/hash.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_string`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
