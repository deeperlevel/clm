# CLM - TLS

## Dependencies

The current CLM C reference implementation composes:

- [clm_net](./clm_net.md) — registered TCP sockets, socket readiness, and transport ownership.
- [clm_base](./clm_base.md) — shared status, integer, and resource-ID conventions, transitively through `clm_net`.

**External dependencies:** The C implementation uses **Schannel / Windows cryptographic APIs** on Windows and **OpenSSL** on POSIX platforms. These are implementation backends rather than requirements for every language port. `clm_tls` does **not** implement QUIC TLS; see [clm_net_quic](./clm_net_quic.md) for the separate QUIC-specific TLS integration.

## 1. Purpose and scope

`clm_tls` provides authenticated **TLS 1.2 or newer** client and server sessions over connected [clm_net](./clm_net.md) TCP sockets. It supports certificate and hostname verification, TLS handshakes, negotiated application protocols (ALPN), encrypted reads and writes, and orderly closure.

The TLS session is a **transport wrapper**, not an HTTP client or server. TLS records and backend-specific cryptographic structures are private to the implementation; applications exchange plaintext bytes using the module's I/O operations.

## 2. Sessions and configuration

A TLS session is associated with an existing connected TCP socket and configured with options for:

| Option | Meaning |
|---|---|
| Handshake / I/O timeout | Zero selects the **30-second** default in the C reference implementation. |
| Server credentials | Certificate chain and private key, in a platform-supported format. |
| Certificate password | Password for a certificate container when required. |
| Client verification override | `insecure_skip_verify` disables peer identity verification **for testing only**. |
| ALPN protocol list | Optional length-prefixed protocol identifiers, in preference order. |

The current C implementation expects a **PKCS#12 / PFX** certificate on Windows and a **PEM certificate chain plus PEM private key** on POSIX for server credentials. This packaging distinction is not part of the cross-language TLS wire contract.

Clients verify peer certificates and the supplied DNS name or IP identity by default. The client-provided server name participates in identity verification; the implementation may also send SNI when appropriate. Disabling verification removes protection against impersonation and must not be used for production traffic.

## 3. Opening and handshaking

| Operation | Behavior |
|---|---|
| `client_open(socket, server_name, options)` | Create a client TLS session and finish its blocking handshake. |
| `server_open(socket, options)` | Create a server TLS session and finish its blocking handshake. |
| `client_new(socket, server_name, options)` | Create client state **without performing handshake I/O**. |
| `server_new(socket, options)` | Create server state **without performing handshake I/O**. |
| `set_nonblocking(session, enabled)` | Select blocking or nonblocking TLS behavior. |
| `handshake(session)` | Advance the handshake; repeat as needed until successful or failed. |
| `get_alpn(session)` | Obtain the negotiated application protocol after handshake completion. |

The two `*_new` operations permit applications to configure nonblocking behavior before the handshake starts. The two `*_open` operations are convenience paths that perform the handshake before returning.

The caller retains ownership of the underlying registered TCP socket. **Close the TLS session before closing the socket.** Failure to create or handshake a session must not silently transfer ownership of the socket.

## 4. Blocking and nonblocking I/O

**Blocking mode** is the default. Operations wait for socket readiness, with the configured timeout applied to each readiness wait.

**Nonblocking mode** does not wait. When an operation cannot progress it returns one of two distinct results:

- `WANT_READ`: wait until the underlying socket is readable, then retry the operation.
- `WANT_WRITE`: wait until the underlying socket is writable, then retry the operation.

A TLS **read may require write readiness**, and a TLS **write may require read readiness**. Callers must honor the actual requested event, not the name of the original operation. In nonblocking mode, the application's reactor is responsible for timeouts.

| Operation | Behavior |
|---|---|
| `read(session, buffer, capacity)` | Read decrypted plaintext into caller storage. |
| `write(session, bytes)` | Accept up to the supplied plaintext byte count; report how many were accepted. |
| `flush(session)` | Drain ciphertext retained by the session after a write or handshake. |
| `close(session)` | Send TLS `close_notify` where possible and release session resources. |

Writes may be **partial**. A successful nonblocking write can mean plaintext was accepted while ciphertext remains queued internally; an application should flush before considering its output drained or closing the session. A `WANT_WRITE` result from `write` means no new plaintext was accepted by that call.

An authenticated peer `close_notify` is distinguishable from an unexpected transport EOF. The latter is reported as **truncation**, not as a clean TLS close.

## 5. Application-Layer Protocol Negotiation (ALPN)

ALPN allows the parties to agree on a protocol such as `h2` or `http/1.1` during the TLS handshake. Protocol identifiers are **opaque byte strings**, not necessarily NUL-terminated strings.

- A configured ALPN offer is a sequence of **one-byte length + 1–255 protocol bytes**, with a total length of at most 65,535 bytes.
- The client advertises supported protocols. The server selects the first shared protocol **in its configured preference order**.
- If both sides offer ALPN but have no protocol in common, negotiation fails. A peer that does not offer ALPN is permitted by this module.
- Querying ALPN before handshake completion returns `NOT_READY`; successful negotiation may still report no protocol selected.

An HTTP layer must advertise **only protocols it can actually serve**. The TCP HTTP web server manages its own ALPN selection when using this module; HTTP/3 negotiates `h3` through QUIC's separate TLS integration.

## 6. Results and failure behavior

| Result | Meaning |
|---|---|
| `SUCCESS` | Requested operation completed. |
| `CLOSED` | Peer authenticated the end of the TLS session. |
| `WANT_READ` / `WANT_WRITE` | Nonblocking operation must be retried after the indicated readiness event. |
| `INVALID_ARGUMENT` / `NOT_FOUND` | Invalid input or missing registered socket. |
| `UNSUPPORTED` | Requested TLS capability is unavailable in the backend. |
| `SYSTEM_ERROR` / `NO_MEMORY` | Operating-system, backend, or resource failure. |
| `HANDSHAKE_FAILED` / `CERTIFICATE_ERROR` | Failed negotiation or peer identity checks. |
| `TRUNCATED` | TCP transport ended without authenticated TLS closure. |
| `TIMEOUT` | Blocking readiness wait expired. |
| `NOT_READY` | Negotiated ALPN requested before the handshake completed. |

Language implementations may use idiomatic return types, but should preserve the distinctions between clean closure, truncation, certificate failure, and transient nonblocking readiness.

## 7. Illustrative usage

Language-neutral pseudocode:

```text
socket = net.connect_tcp(server_address)
options = tls.options(verify_peer = true, alpn = ["h2", "http/1.1"])
session = tls.client_new(socket, "example.org", options)
tls.set_nonblocking(session, true)

repeat:
    result = tls.handshake(session)
    if result == WANT_READ:  wait_for_socket_readable(socket)
    if result == WANT_WRITE: wait_for_socket_writable(socket)
until result == SUCCESS or result is fatal

protocol = tls.get_alpn(session)     # borrowed bytes; possibly absent
# Read/write plaintext, handling partial output and readiness events.
tls.flush(session)
tls.close(session)
net.close(socket)
```

This pseudocode illustrates the behavioral contract, not the exact syntax of any CLM language implementation.

## 8. Boundaries and interoperability

- **TCP only:** this module does not expose UDP sockets, QUIC packet processing, or HTTP/3 handshakes.
- **No implicit HTTP behavior:** ALPN selection identifies a protocol but does not parse its messages.
- **Session synchronization:** access to one session must be serialized; an ID or pointer alone does not make concurrent reads and writes safe.
- **Backend differences:** certificate file formats and native TLS providers may vary by platform; verification and plaintext I/O semantics should remain consistent.
- **Socket lifetime:** closing a TLS session does not close the underlying CLM socket.
