# clm_db

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Database driver dispatch, prepared statements, and transactions.

## Behavior and constraints

Database driver dispatch, prepared statements, and transactions.

The common layer validates calls and forwards backend work through driver callbacks.
Result values and driver-owned handles follow explicit lifetimes.

## Integration

- C reference: [**clm_db**](../c/db.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
