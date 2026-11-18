# clm_db_pg_native_scram

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.

> **Scope:** This appears to be a support/internal or shared header; do not assume it is independently usable without its owning modules.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_db_pg_native_scram. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_db_pg_native_scram when an application needs: PostgreSQL SCRAM-SHA-256 authentication messages.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
