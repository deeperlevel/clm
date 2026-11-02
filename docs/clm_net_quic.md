# CLM - QUIC Network Transport

## Dependencies

The current CLM C reference implementation composes:

- [clm_net](./clm_net.md) — network addresses and UDP datagram transport used by the caller.
- [clm_base](./clm_base.md) — scalar/status types, transitively through `clm_net`.

**External dependencies:** The optional C backend requires **ngtcp2 1.25.0 or newer**, **ngtcp2 crypto OpenSSL integration** (`ngtcp2_crypto_ossl`), and **OpenSSL 3.5 or newer**, with QUIC-capable TLS 1.3. The C implementation is built only when QUIC support is enabled (`CLM_HAS_QUIC` / build option `CLM_ENABLE_QUIC`). No equivalent dependencies are prescribed for other language implementations.

## 1. Purpose and scope

`clm_net_quic` provides a **server-side QUIC transport connection** with encrypted multiplexed streams over UDP. QUIC handles packet sequencing, retransmission, acknowledgments, receive flow control, peer connection IDs, and transport timeouts. The module exposes stream events and opaque connection operations without imposing HTTP/3 framing.

The current reference implementation is a **memory-I/O transport component**: it does **not** own a UDP socket, spawn threads, or run an event loop. Its caller receives complete UDP datagrams, feeds them to a connection, transmits generated datagrams, and drives connection timers. The intended primary consumer is [clm_net_web_h3](./clm_net_web_h3.md), but other QUIC application protocols can use it with an appropriate ALPN value.

The current API **accepts server connections only**. There is no client-side QUIC connect operation yet.

## 2. Connection configuration and ownership

The reference configuration defines:

| Setting | C reference default / behavior |
|---|---|
| Certificate and private key | Required server PEM credential paths. |
| ALPN protocol | Required; defaults to `h3` as a convenience, but may be changed. |
| Maximum peer streams | **128 per stream type**. |
| Outbound buffered payload | **1 MiB** per connection. |
| Transport memory budget | **16 MiB**; excludes TLS/cryptographic allocations. |
| Idle timeout | **30 seconds**. |
| Callbacks / application context | Optional event handlers with caller-owned opaque context. |

An accepted connection is **owned by the caller** and destroyed explicitly. Credential paths and initial address/data arguments are borrowed during accept; subsequent connection handling operates on retained internal state. A connection must be driven by **one serialized owner**; concurrently invoking connection operations without serialization is unsupported.

## 3. Datagram processing and timer loop

| Operation | Behavior |
|---|---|
| `config_default(config)` | Establish reference limits and default ALPN. |
| `accept(config, local, remote, first_datagram, now)` | Create a server connection and process its first complete datagram. |
| `matches(connection, datagram)` | Test whether an incoming QUIC destination connection ID routes to this connection. |
| `receive(connection, local, remote, datagram, now)` | Process one complete UDP datagram. |
| `write(connection, destination_buffer, now)` | Produce **at most one complete datagram** and its source/destination addresses. |
| `expiry(connection)` | Return the next absolute transport deadline. |
| `handle_expiry(connection, now)` | Advance retransmission, timeout, and closing state. |
| `native_error(connection)` | Inspect the latest backend-specific transport error. |
| `destroy(connection)` | Release connection and transport resources. |

**Time values are monotonic nanoseconds**, all using the same clock domain; they are not wall-clock timestamps.

The first accepted UDP datagram must contain a valid QUIC Initial packet and meet the **1,200-byte minimum**. In the C reference, `write` requires an output capacity of at least **1,200 bytes**, returns one datagram on success, and signals `AGAIN` if no packet is currently ready. The caller must transmit emitted datagram bytes **unchanged** to the reported peer.

A connection-ID match is only **routing**, not authentication. Address/packet validation and admission limits are responsibilities of the UDP dispatcher and QUIC processing layer. The current reference explicitly ignores traffic arriving on a changed local or peer path.

## 4. Stream API and callback model

| Operation / event | Behavior |
|---|---|
| `open_uni(connection)` | Open a server-initiated unidirectional stream. |
| `send(connection, stream_id, data, fin)` | Copy application bytes into a bounded outbound queue; optionally append FIN. |
| `consume(connection, stream_id, byte_count)` | Return receive credit for delivered bytes actually consumed. |
| `shutdown_stream(connection, stream_id, application_error)` | Abort relevant stream directions. |
| `on_handshake` | QUIC/TLS handshake and ALPN negotiation completed. |
| `on_stream_data(id, bytes, fin)` | Ordered, nonduplicated received stream bytes; FIN may carry zero data. |
| `on_stream_acked(id, byte_count)` | Outbound stream bytes were acknowledged by the peer. |
| `on_stream_close(id, error)` | A stream closed. |
| `on_stream_reset(id, error)` | Peer reset a stream. |
| `on_stream_stop(id, error)` | Peer requested stopping writes. |

QUIC stream IDs are **protocol-level stream identifiers**, not CLM resource IDs. QUIC permits many independently flowing streams inside a single connection. The reference currently supports opening **server-initiated unidirectional** streams directly; peer-created bidirectional request streams arrive through callbacks.

`send` **copies all accepted bytes** into transport-owned buffers. If it returns `AGAIN`, **none of the supplied bytes are accepted**. The connection retains accepted send data until acknowledgment or confirmed stream closure. Sending after FIN or shutdown is invalid.

`on_stream_data` receives **borrowed bytes** that are valid only during that callback. The application must copy data it needs to retain and call `consume` only for bytes truly consumed. If no stream-data callback is installed, the reference discards and automatically consumes incoming data.

A callback may queue sends, consume input, open/shutdown streams, or request connection close. It must **not recursively call** receive, write, accept, expiry handling, or destruction on the same connection.

## 5. Flow control, acknowledgement, and resource limits

Receive-side QUIC flow control tracks the number of bytes delivered and consumed per stream. The caller returns credit through `consume`, which also advances connection-level receive credit. Repeated consumption or consumption exceeding delivered bytes is invalid.

Outbound memory is bounded by a **send-payload queue limit** and a separate **transport memory budget**. The latter covers transport-managed allocations and outbound queues but does **not** bound every allocation performed by the TLS provider. The reference releases queued payload storage after acknowledgments or stream closure, not merely because bytes have been handed to the UDP socket.

On stream reset or closure, previously delivered but unconsumed bytes still require accounting. The reference retains enough stream state to permit that accounting until those bytes are consumed.

## 6. TLS 1.3, security, and current limitations

QUIC integrates its own **TLS 1.3** handshake and transport secrets. The reference uses a QUIC-capable OpenSSL/ngtcp2 integration, **not** a `clm_tls` session wrapped around a UDP socket.

The present reference implementation deliberately excludes:

- QUIC **0-RTT / early data**.
- QUIC **Retry / address-validation tokens**.
- Connection **migration** and changed network paths.
- A **client-side** QUIC connection API.
- A built-in UDP listener, global connection registry, or event loop.

Applications exposing a public UDP listener must implement appropriate **connection-admission and handshake-flood protections** at the dispatcher. Disabling migration means peer address changes are ignored rather than transparently recovered.

## 7. Connection closure and results

| Result | Meaning |
|---|---|
| `SUCCESS` | Operation completed or one output datagram produced. |
| `AGAIN` | No output/progress available now, or a send cannot accept any bytes yet. |
| `CLOSED` | Connection is draining or has reached terminal state. |
| `INVALID_ARGUMENT` | Invalid connection, arguments, ordering, or stream use. |
| `NO_MEMORY` | An allocation failed. |
| `TLS_ERROR` | Cryptographic setup or negotiation failed. |
| `PROTOCOL_ERROR` | QUIC protocol or callback failure. |
| `LIMIT_EXCEEDED` | Configured resource budget exceeded. |

`close(connection, application_error)` requests an application-level CONNECTION_CLOSE; it does **not** immediately destroy the object. The caller continues driving output and timers until closure/draining finishes, then destroys the connection. Even after output stops, connection state may need to remain available for its closing deadline and for routing late packets.

## 8. Illustrative usage

```text
config = quic.config_default()
config.alpn = "h3"
config.certificate_file = "server-cert.pem"
config.private_key_file = "server-key.pem"
config.callbacks.on_stream_data = handle_stream_data

while udp_listener_is_running:
    if datagram_arrived:
        conn = find_matching_connection(datagram)
        if conn is absent:
            conn = quic.accept(config, local, peer, datagram, monotonic_nanoseconds())
        else:
            quic.receive(conn, local, peer, datagram, monotonic_nanoseconds())

    for each conn:
        if quic.expiry(conn) <= monotonic_nanoseconds():
            quic.handle_expiry(conn, monotonic_nanoseconds())
        repeat:
            result, packet, local, remote = quic.write(conn, monotonic_nanoseconds())
            if result == SUCCESS: udp.send_to(remote, packet)
        until result != SUCCESS
        if conn is terminal: quic.destroy(conn)
```

This is conceptual event-loop pseudocode, not a ready-made socket or TLS example.

## 9. Interoperability boundaries

`clm_net_quic` **does not parse HTTP headers or QPACK**. Its streams carry opaque application bytes. The HTTP/3 adapter provides higher-level request/response semantics on top of them. Cross-language ports should preserve event ordering, ownership, timer units, atomic send acceptance, receive-credit accounting, and closing behavior while allowing different native QUIC libraries or runtimes.
