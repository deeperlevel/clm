# clm_db_pg_native

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

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

## Integration

- C reference: [**clm_db_pg_native**](../c/db_pg_native.md).
- Related CLM declarations in this archive: `clm_db`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
