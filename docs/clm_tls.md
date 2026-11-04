# CLM Tls

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net](clm_net.md)
- **External libraries / platform facilities (C reference):** OpenSSL on supported non-Windows paths; Schannel/SSPI on Windows.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

**Current protocol use:** `clm_net_web` uses TLS ALPN for HTTP/2 selection, and `clm_net_web_h3` uses QUIC’s own cryptographic integration rather than a TCP TLS session.

## Purpose

TLS-encrypted client/server sessions layered over connected TCP sockets.

## Capabilities and operations

- Establish a verified client handshake with server-name identity checks.
- Accept a server-side handshake with configured certificate credentials.
- Read decrypted bytes and write plaintext through the authenticated session.
- Distinguish authenticated close from truncated TCP EOF; the original socket remains owned by its caller.

## TLS session lifecycle

A TLS client opens a session on an already connected TCP socket and verifies the peer identity using the requested server name. A server opens a session with configured certificate material. After handshaking, reads return decrypted data and writes accept plaintext for encryption.

## Nonblocking mode

`clm_tls_client_new`/`clm_tls_server_new` create a session without handshaking; the `*_open` functions are the same plus a blocking handshake. After `clm_tls_set_nonblocking(session, true)`, `clm_tls_handshake`, `clm_tls_read` and `clm_tls_write` never wait: they return `CLM_TLS_WANT_READ` or `CLM_TLS_WANT_WRITE` and the caller retries once the socket is ready for that event (the handshake can need either, and so can reads and writes). A write that returns success only means the bytes were accepted; encrypted bytes the socket has not taken yet stay queued in the session until `clm_tls_flush` returns success. On WANT_* nothing was consumed or accepted. Closing a nonblocking session sends `close_notify` once, best effort, and never waits.

`insecure_skip_verify` (client only) disables certificate and name checks. It exists for tests against throwaway certificates and must not be used for real traffic.

## Closure and ownership

- An authenticated TLS `close_notify` is distinct from an unclean transport EOF, which is reported as truncated.
- Session closure releases TLS resources but does not itself close the caller-owned registered socket.
- Key/certificate configuration and credential storage may differ across platforms, while certificate verification and closure semantics remain part of the cross-language contract.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- The TLS layer does not transfer ownership of the underlying registered network socket.

## Example use cases

Verify a server certificate when opening an encrypted TCP connection.
