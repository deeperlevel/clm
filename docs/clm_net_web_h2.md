# CLM - HTTP/2-Only Web Server

## Dependencies

The reference implementation composes:

- [clm_net_web](./clm_net_web.md) — shared routing, middleware, request/response exchange, worker and connection management.
- [clm_tls](./clm_tls.md) — **required** TLS over TCP and ALPN negotiation.
- [clm_http_utils](./clm_http_utils.md) — HTTP semantics and field utilities through the shared engine.

**External dependencies:** The current C reference uses **nghttp2** for HTTP/2 framing, HPACK, flow control, and stream state, alongside the platform TLS backend exposed by `clm_tls`. Other languages may use their native HTTP/2 implementations.

## 1. Purpose and scope

`clm_net_web_h2` is a **thin HTTP/2-only HTTPS facade** over [clm_net_web](./clm_net_web.md). It shares the web server's route/middleware definitions and normalized request/response APIs, while making **negotiated HTTP/2 mandatory**.

The current reference serves HTTP/2 **only over TLS with ALPN `h2`**. It does not support cleartext `h2c`, prior-knowledge HTTP/2 on plain TCP, or fallback to HTTP/1.1 after a failed negotiation.

## 2. Configuration and exclusive negotiation

The facade reuses the shared web configuration and:

- Initializes shared defaults with `require_http2 = true`.
- Requires non-null TLS server credentials before creating a server.
- Forces `require_http2 = true` and `require_http1 = false` on a **configuration copy**.
- Advertises only **`h2`** via TLS ALPN, overriding caller-supplied protocol offers.
- Rejects connections where `h2` was not negotiated, including peers that omit ALPN, **before dispatching requests**.

Configured worker counts, request limits, header budgets, stream limits, compression behavior, and grace periods are inherited from `clm_net_web`.

## 3. Facade operations

| Operation | Behavior |
|---|---|
| `h2_config_init(config)` | Set shared defaults and HTTP/2 exclusivity. |
| `h2_server_create(config)` | Create shared server, requiring TLS and only `h2`. |
| `h2_server_start(server, address)` | Start an HTTPS/TCP listener. |
| `h2_server_stop(server)` | Stop acceptance; drain admitted streams if configured. |
| `h2_server_destroy(server)` | Release server and connection resources. |
| `h2_server_local_address(server)` | Return the actual TCP endpoint. |

Use the **same** `clm_net_web_route_add`, `clm_net_web_route_add_async`, middleware registration, response operations, and context accessors as the general web-server module. No duplicate H2-only application API is introduced.

## 4. HTTP/2 request and response semantics

A single TCP/TLS connection may carry multiple independent HTTP/2 request streams. The HTTP/2 protocol owner handles the codec while bounded application worker tasks handle request exchanges.

- Initial request headers are normalized for shared handlers. HTTP/2 pseudo-fields are exposed as request scheme/authority rather than as regular headers.
- Buffered bodies are bounded. **Async uncompressed request bodies** may stream incrementally, returning flow-control credit as consumed.
- Response writes and flushes may encounter **per-stream backpressure**, returning `AGAIN` to resumable handlers without accepting the blocked write.
- HTTP/2 response trailers are supported and kept separate from initial headers.
- Invalid or cancelled individual streams may be reset while sibling streams remain active.
- Client HTTP/2 `CONNECT` is not implemented as a tunnel by the current shared handler engine; unsupported requests receive an appropriate failure response.

The reference defaults to **100** concurrent streams and a **64 KiB** decoded-header budget per connection, plus separate settings, continuation, rate, output, and memory-budget protections.

## 5. Graceful shutdown

With a nonzero shared `shutdown_grace_ms`, the server initiates HTTP/2 **GOAWAY**, refuses new work, and allows already admitted streams to finish until the grace deadline. A zero grace period cancels immediately. The server waits for application callbacks to return cooperatively before releasing their request-owned data.

## 6. Illustrative usage

```text
config = web_h2.config_init()
config.tls = server_credentials  # mandatory for this facade
config.shutdown_grace_ms = 5000
server = web_h2.server_create(config)
web.route_add(server, "GET", "/status", handle_status)
web_h2.server_start(server, address)
# ...
web_h2.server_stop(server)
web_h2.server_destroy(server)
```

Language-neutral pseudocode; the facade delegates the application API to `clm_net_web`.

## 7. Results and boundaries

Results follow the shared [clm_net_web](./clm_net_web.md) contract. Creating a server without TLS is invalid; failing to negotiate `h2` does **not** trigger fallback to HTTP/1.x.

**Scope boundaries:** TLS/TCP HTTP/2 only; no h2c, no HTTP/1.1, no QUIC/HTTP/3; the facade itself adds no independent routing, codec, or worker engine.
