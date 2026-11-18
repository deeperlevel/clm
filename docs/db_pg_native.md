# clm_db_pg_native

> Language-neutral module overview and behavioral contract.

## Purpose

Native PostgreSQL framing, authentication, and query state.

## Behavior and constraints

Native PostgreSQL framing, authentication, and query state.

Messages are length-prefixed and connection state must remain synchronized through
ReadyForQuery. Transport failures can make further connection reuse unsafe.

Experimental protocol 3.0 implementation: NO libpq dependency.
Requires CLM net/tls/crypto/string_base64. Default sslmode=prefer tries
certificate-unverified TLS, then reconnects without TLS only if the server
refuses SSLRequest ('N'). sslmode=require enforces TLS but does not verify
its certificate; sslmode=verify-full verifies the certificate and hostname;
sslmode=disable selects plaintext. Use verify-full for remote/production.
No plaintext fallback occurs after TLS handshake or network failures.
Uses a restricted keyword/value DSN (see README), SCRAM-SHA-256 for
printable ASCII passwords, and PostgreSQL's extended query protocol.
Result sets are buffered; not yet a streaming driver.
Buffered result allocations are capped at 64 MiB, excluding the current
wire frame (at most 16 MiB) and allocator overhead.
TCP connect attempts and the pre-TLS SSL response use 30-second deadlines.
DNS resolution is synchronous; TLS uses CLM's default readiness timeout.
Transport failures or unsupported COPY invalidate the connection. Finalize
remaining statements, then close and reconnect; no query is retried silently.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_db`](db.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_db_pg_native. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_db](db.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_db_pg_native when an application needs: Native PostgreSQL framing, authentication, and query state.

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
