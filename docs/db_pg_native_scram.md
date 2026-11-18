# clm_db_pg_native_scram

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

PostgreSQL SCRAM-SHA-256 authentication messages.

## Behavior and constraints

PostgreSQL SCRAM-SHA-256 authentication messages.

Client proofs and server signatures bind the exchanged transcript to derived keys. The
server signature must be verified before accepting authentication.

Returns false on invalid server attributes, unsupported password normalization,
iteration count, or authentication failure. Caller owns result buffers.
Passwords are deliberately restricted to printable ASCII in this first pass:
PostgreSQL's SASLprep normalization for Unicode passwords is not implemented.

Verify the server-final SCRAM authentication message.

## Integration

- C reference: [**clm_db_pg_native_scram**](../c/db_pg_native_scram.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.

> **Scope:** This appears to be a support/internal or shared header; do not assume it is independently usable without its owning modules.
