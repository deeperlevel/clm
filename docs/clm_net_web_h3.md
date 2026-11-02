# CLM - HTTP/3-Only Web Server

## Dependencies

The current CLM C reference implementation composes:

- [clm_net_web](./clm_net_web.md) — shared application routing, middleware, request/response exchange, cancellation, and bounded handler execution.
- [clm_net_quic](./clm_net_quic.md) — QUIC transport, streams, TLS 1.3 handshake, acknowledgment, and timers.
- [clm_net](./clm_net.md) — UDP socket, datagram delivery, and event polling through the shared web infrastructure.
- [clm_http_utils](./clm_http_utils.md) — normalized HTTP message and field validation used by shared dispatch and HTTP/3 translation.
- [clm_async_pool](./clm_async_pool.md) — bounded asynchronous application-handler execution.
- [clm_bytes](./clm_bytes.md) — request and response buffering and owned payloads.

**External dependencies:** The optional C backend requires **nghttp3** (HTTP/3 and QPACK) and the external dependencies of **`clm_net_quic`**: ngtcp2 and a QUIC-capable TLS 1.3 backend (currently OpenSSL 3.5+). The current shared C web engine also compiles nghttp2; implementations in other languages may use alternate codecs. HTTP/3 is included only when the QUIC build feature is enabled (`CLM_HAS_QUIC` / `CLM_ENABLE_QUIC`).

## 1. Purpose and scope

`clm_net_web_h3` is an **HTTP/3-only server facade and protocol adapter**. It translates HTTP/3 requests carried over encrypted QUIC streams into the **same route, middleware, request context, and response APIs** used by [clm_net_web](./clm_net_web.md).

Unlike the HTTP/1.1 and HTTP/2 facades, it uses a **separate UDP listener** and QUIC-specific configuration. HTTP/3 is not a protocol selected on a TCP/TLS socket; it has no HTTP/1.1 or HTTP/2 transport fallback within this server.

It provides HTTP/3 framing and QPACK through a suitable codec, not its own generic QUIC transport implementation. QUIC connection and stream operations come from [clm_net_quic](./clm_net_quic.md).

## 2. Configuration and defaults

HTTP/3 configuration wraps the common web-server settings and adds QUIC-specific values:

| Setting | C reference default / behavior |
|---|---|
| Shared web configuration | Initialized using the common `clm_net_web` defaults. |
| Certificate chain | Required **PEM** certificate path in the C reference. |
| Private key | Required **PEM** key path in the C reference. |
| Concurrent request streams | **100** per QUIC connection. |
| Decoded HTTP/3 header budget | **64 KiB**, with per-field accounting overhead. |
| Connection memory reservation budget | **1 GiB**, excluding TLS allocations. |
| Buffered QUIC output budget | **1 MiB** per connection. |
| Idle/read/write timeout, request and response limits | Inherited from the shared web configuration. |
| Graceful shutdown deadline | Inherited from `web.shutdown_grace_ms`. |

The C reference requires `web.tls`, `web.require_http1`, and `web.require_http2` to remain **unset**: HTTP/3 uses a separate QUIC/TLS integration. Server creation copies credential path strings, while credential validity is checked at listener startup.

The current UDP implementation requires a **concrete local IP address**, not an unspecified/wildcard bind address. The port may be chosen by the caller, including an ephemeral port.

## 3. Server lifecycle and application registration

| Operation | Behavior |
|---|---|
| `h3_config_init(config)` | Initialize common web defaults and HTTP/3-specific limits. |
| `h3_server_create(config)` | Create a shared server configured for the HTTP/3 adapter. |
| `h3_server_start(server, address)` | Bind UDP, start the QUIC reactor and bounded application-worker pool. |
| `h3_server_stop(server)` | Stop acceptance and initiate HTTP/3 draining/cancellation. |
| `h3_server_destroy(server)` | Stop if necessary and release shared resources. |
| `h3_server_local_address(server)` | Return the bound **UDP** endpoint. |

Routes and middleware are registered using the ordinary **`clm_net_web_route_add`, `clm_net_web_route_add_async`, and `clm_net_web_middleware_add`** operations, before the server starts. Handlers do not need an HTTP/3-specific signature.

The current C implementation creates a QUIC server connection from valid Initial datagrams, matches subsequent datagrams to connection IDs, feeds ordered streams to nghttp3, and maps decoded HTTP messages to request-local exchanges.

## 4. HTTP/3 request handling

HTTP/3 carries an initial HEADERS section, optional DATA, and optional trailing HEADERS over a client-initiated QUIC request stream.

- HTTP/3 **pseudo-fields** such as method, target, scheme, and authority are normalized into the common request model, rather than exposed as ordinary request headers.
- Decoded header fields are subject to field-count and byte budgets, with malformed or forbidden fields rejected.
- Trailing fields remain **separate** from initial headers; they can be retrieved through the shared request trailer API.
- **Request bodies are completely buffered** up to `max_request_bytes` before handler dispatch. The current HTTP/3 adapter does **not** provide progressive, async request-body streaming.
- The initial implementation does not support HTTP/3 extended CONNECT or WebTransport. Ordinary `CONNECT` does not establish a supported application tunnel and is treated as unimplemented.

The HTTP/3 codec handles QPACK and HTTP/3 frame sequencing; the application handler works with normalized HTTP request data instead of QPACK representations.

## 5. HTTP/3 responses and backpressure

The shared response API writes status, initial headers, body data, and optional trailers. The HTTP/3 adapter translates them into HTTP/3 HEADERS/DATA sections and QUIC stream output.

- Initial response status/headers are committed on flush or completion.
- Hop-by-hop fields meaningful only in HTTP/1.x are omitted from HTTP/3 output.
- Request-local outgoing buffers have configured size limits, and QUIC output is tracked until acknowledgment.
- Under pressure, a resumable async handler can receive **`AGAIN`** without accepting new bytes; it must resume and retry that write.
- HTTP/3 response trailers may be emitted after DATA on supported responses.
- No-body responses, including **HEAD**, **204**, and **304**, cannot send response trailers.

As with HTTP/2, synchronous handlers should return control to the protocol owner after preparing their response rather than block waiting for a remote peer to consume a flush.

## 6. Ownership, concurrency, and shutdown

The **UDP reactor exclusively owns** the active QUIC and HTTP/3 codec objects and drives transport expiry, packet reception, encoded frames, and datagram writes. Application handlers run in a **bounded worker pool** with request-local exchanges.

A handler's request/response exchange remains valid while its scheduled task owns it. The reactor collects completed task state before reclaiming or publishing the exchange. Request cancellation is reported to application code; a cancelled stream can be reset without necessarily aborting other streams in the same QUIC connection.

On graceful stop, the adapter initiates HTTP/3 **GOAWAY** and refuses new work, allowing admitted exchanges to complete until the configured grace deadline. The default zero grace period cancels promptly. The server cannot forcibly interrupt a noncooperative running application callback.

## 7. Results, security, and boundaries

The facade uses the common [clm_net_web](./clm_net_web.md) result categories: `SUCCESS`, `INVALID_ARGUMENT`, `TLS_ERROR`, `SYSTEM_ERROR`, `BUSY`, `LIMIT_EXCEEDED`, `NO_MEMORY`, and request-level statuses including `AGAIN` and `END` where applicable.

The first server-side implementation deliberately omits:

- Plain HTTP/3, TCP fallback, HTTP/1.1/HTTP/2 listeners, or automatic client protocol switching.
- QUIC **0-RTT**, **Retry**, and connection migration.
- HTTP/3 **server push**, extended CONNECT, and WebTransport.
- Incremental request-body delivery before the entire bounded body is received.
- Built-in QUIC client connection support.

Because the current QUIC transport does not implement Retry, the UDP dispatcher needs appropriate **connection admission and abuse controls** for public deployment.

## 8. Illustrative usage

```text
config = web_h3.config_init()
config.certificate_file = "server-cert.pem"
config.private_key_file = "server-key.pem"
config.web.max_request_bytes = 8 * MiB
config.web.shutdown_grace_ms = 5000

server = web_h3.server_create(config)
web.route_add(server, "GET", "/health", handle_health)
web.route_add_async(server, "POST", "/data", handle_data)
web_h3.server_start(server, concrete_udp_address)

# Ordinary web.request_*, web.response_*, and web.context_* APIs apply.

web_h3.server_stop(server)
web_h3.server_destroy(server)
```

This is language-neutral pseudocode. Production applications must also plan how clients discover and select HTTP/3; the server itself does not supply an HTTP/1.x fallback connection.

## 9. Interoperability guidance

An HTTP/3-only UDP listener may coexist with an HTTPS TCP listener at the **same numerical port**, since they use different transports. A client can discover HTTP/3 by mechanisms such as Alt-Svc or explicit prior knowledge, but **fallback and connection selection are client responsibilities**.

Across CLM languages, preserve the same HTTP handler semantics and supported limits even if the chosen QUIC/TLS or HTTP/3 codec differs. Binary framing, QPACK state, TLS provider structures, and reactor scheduling remain **internal** rather than part of the language-neutral application contract.
