# clm_crypto

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Authenticated encryption, OS randomness, key derivation and key exchange. XChaCha20-Poly1305 is authenticated encryption, NOT a generic hashing API. Keys, nonces, tags and associated data must obey exact byte lengths; a nonce must never repeat for the same key. Encryption is not interchangeable with hashing, and decrypt must not expose plaintext before tag verification. Pointer and BPA-ID variants share the same underlying cryptographic operations.

## Behavior and constraints

Authenticated encryption, OS randomness, key derivation and key exchange.
XChaCha20-Poly1305 is authenticated encryption, NOT a generic hashing API.
Keys, nonces, tags and associated data must obey exact byte lengths; a nonce
must never repeat for the same key. Encryption is not interchangeable with
hashing, and decrypt must not expose plaintext before tag verification.
Pointer and BPA-ID variants share the same underlying cryptographic operations.

## Integration

- C reference: [**clm_crypto**](../c/crypto.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_string`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
