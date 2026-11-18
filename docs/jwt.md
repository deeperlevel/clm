# clm_jwt

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

JSON Web Token encoding, decoding, and signature verification.

## Behavior and constraints

JSON Web Token encoding, decoding, and signature verification.

Parsing a token does not establish authenticity. Verification must bind the selected
algorithm and key to the signed header and payload.

## Integration

- C reference: [**clm_jwt**](../c/jwt.md).
- Related CLM declarations in this archive: `clm_string_json`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
