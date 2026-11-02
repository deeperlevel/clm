# CLM - HTTP/1.1-Only Web Server

## Dependencies

The reference implementation composes:

- [clm_net_web](./clm_net_web.md) — shared configuration, routing, middleware, request/response APIs, workers, and server lifecycle.
- [clm_tls](./clm_tls.md) — **optional at runtime** for HTTPS over TCP.

**External dependencies:** In the current C build, the shared `clm_net_web` engine still links **nghttp2** even when the HTTP/1-only facade is selected. No distinct HTTP/1 codec dependency is introduced by this facade. Other language ports may arrange optional dependencies differently.

## 1. Purpose and scope

`clm_net_web_h1` is a **thin HTTP/1.1-only facade** over [clm_net_web](./clm_net_web.md). It exists for applications that wish to **require HTTP/1.1 explicitly**, rather than rely on HTTPS ALPN negotiation or the shared server's HTTP/2-first default.

It supports **plain HTTP and HTTPS**, uses the common request/response and middleware contracts, and rejects HTTP/1.0 or HTTP/2 requests **before application dispatch**. It is not a separate routing implementation or a way to remove the HTTP/2 dependency from the current shared C build.

## 2. Configuration and protocol enforcement

The facade reuses the common web-server configuration and server types, with HTTP/1.1 exclusivity enforced at creation:

- `config_init` establishes the shared defaults and enables `require_http1`.
- `server_create` forces `require_http1 = true` and clears `require_http2`, using a **copy** of the supplied configuration.
- TLS is **optional**. With TLS, only the `http/1.1` ALPN identifier is advertised, regardless of a caller-supplied ALPN list.
- A client that omits ALPN may connect but must speak HTTP/1.1. Other request versions are not dispatched.

HTTP/1.1 permits persistent connections and chunked transfer coding. Keep-alive counts, limits, timeouts, routing, compression, and handler behavior are inherited from the common web-server contract.

## 3. Facade operations

| Operation | Behavior |
|---|---|
| `h1_config_init(config)` | Set common defaults and require HTTP/1.1. |
| `h1_server_create(config)` | Create a shared server with exclusive HTTP/1.1 enforcement. |
| `h1_server_start(server, address)` | Start an HTTP or HTTPS TCP listener. |
| `h1_server_stop(server)` | Stop acceptance and wait for active callbacks to return. |
| `h1_server_destroy(server)` | Release shared server state and owned resources. |
| `h1_server_local_address(server)` | Return the bound TCP address/port, including an ephemeral port. |

Route registration, middleware, request inspection, response writes, streaming, context management, and cancellation all use the **normal `clm_net_web_*` application operations** described in [clm_net_web](./clm_net_web.md). There are no separate H1-only handler types.

## 4. Request and response behavior

The HTTP/1.1 facade uses the HTTP/1.x implementation of the shared engine:

- Request parsing and framing use strict HTTP/1.x rules from [clm_http_utils](./clm_http_utils.md).
- Small request bodies may be buffered; larger uncompressed bodies may be read incrementally by the handler, with capacity limits on worker-occupying streams.
- Buffered responses can be flushed into HTTP/1.1 chunked output where appropriate.
- Async routes can return `AGAIN` and resume when socket readiness permits.
- HTTP/2-/HTTP/3-only features, such as multiplexed request streams and response trailers, are not available on this facade.

A connection is a sequential HTTP/1.1 request/response channel, not a multiplexed HTTP/2 session. HTTP/1.1 cancellation closes the affected connection; optional HTTP/2/HTTP/3 graceful GOAWAY handling does not apply.

## 5. Illustrative usage

```text
config = web_h1.config_init()
config.tls = null                  # or HTTPS credentials
server = web_h1.server_create(config)
web.route_add(server, "GET", "/", handle_home)
web_h1.server_start(server, address)
# ...
web_h1.server_stop(server)
web_h1.server_destroy(server)
```

Language-neutral pseudocode; the API is intentionally a **narrow facade over the shared server**.

## 6. Results and boundaries

The facade returns the normal [clm_net_web](./clm_net_web.md) result categories. Missing configuration or output pointers, conflicting protocol requirements, and invalid listener use fail before serving traffic.

**Scope boundaries:** HTTP/1.1 only; plain or TLS TCP; no HTTP/2 fallback; no UDP/QUIC; no independent route/middleware implementation. In the current C backend, the facade does not make nghttp2 a build-time optional dependency.
