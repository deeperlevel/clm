# CLM Tls

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

TLS-encrypted client/server sessions layered over connected TCP sockets.

## Capabilities and operations

- Establish a verified client handshake with server-name identity checks.
- Accept a server-side handshake with configured certificate credentials.
- Read decrypted bytes and write plaintext through the authenticated session.
- Distinguish authenticated close from truncated TCP EOF; the original socket remains owned by its caller.

## TLS session lifecycle

A TLS client opens a session on an already connected TCP socket and verifies the peer identity using the requested server name. A server opens a session with configured certificate material. After handshaking, reads return decrypted data and writes accept plaintext for encryption.

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

## Related CLM modules

**Supporting modules:** [clm_net](clm_net.md).

## Example use cases

Verify a server certificate when opening an encrypted TCP connection.
