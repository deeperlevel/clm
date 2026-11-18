# clm_tls

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

TLS session state and encrypted socket I/O.

## Behavior and constraints

TLS session state and encrypted socket I/O.

Encrypted input, decrypted application data, and pending output are distinct buffers.
Partial native handshakes and socket writes retain state until the next readiness event.

TLS 1.2+ client and server sessions over registered CLM TCP sockets.
Client sessions verify the peer certificate and hostname by default.
Sessions keep a reference to their socket; dispose the TLS session before
closing the underlying clm_net socket. A session serializes access to itself.

Two I/O modes exist per session:
- Blocking (the default, and what the *_open functions use): calls wait on the
socket through clm_net, with timeout_ms applied to each readiness wait.
- Nonblocking (clm_tls_set_nonblocking): calls never wait. When the socket cannot
make progress they return CLM_TLS_WANT_READ or CLM_TLS_WANT_WRITE; the caller
waits for that socket event (for example by parking on a poller) and repeats
the same call. Timeouts are then the caller's responsibility.
TLS can need the opposite event from the operation: a read may need to write and a
write may need to read, so always honour the event that is requested.

## Integration

- C reference: [**clm_tls**](../c/tls.md).
- Related CLM declarations in this archive: `clm_net`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
