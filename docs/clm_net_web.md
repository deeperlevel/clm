# CLM Net Web

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_async_pool](clm_async_pool.md), [clm_bytes](clm_bytes.md), [clm_compress](clm_compress.md), [clm_http_utils](clm_http_utils.md), [clm_multipart](clm_multipart.md), [clm_net](clm_net.md), [clm_stream](clm_stream.md), [clm_template](clm_template.md), [clm_tls](clm_tls.md), [clm_websocket](clm_websocket.md)
- **External libraries / platform facilities (C reference):** nghttp2 for the C HTTP/2 backend when available; TLS, compression and QUIC support are provided through supporting CLM modules.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Shared HTTP server application layer for HTTP/1.1 and HTTP/2, with a separate HTTP/3 transport facade that reuses the same routes, middleware and handler abstractions.

## Capabilities and operations

- Create a web server, register routes and middleware, and start/stop a listener.
- Route HTTP requests with headers, parameters, context values and parsed multipart input.
- Produce headers, status codes, redirects, templates, streams and incremental responses.
- Register resumable asynchronous handlers with explicit backpressure behavior.
- Support WebSocket endpoints using HTTP/1.1 Upgrade or HTTP/2/3 extended CONNECT where applicable.
- Use HTTP/1.1 over cleartext or TLS; negotiate HTTP/2 through TLS ALPN; run HTTP/3 through `clm_net_web_h3`.

## Resource, data and ownership model

A server owns listener/reactor and worker resources. Requests and responses are scoped to a connection or stream; middleware and routes share application semantics across supported HTTP protocol engines. Context storage belongs to the request lifecycle and must not be used after completion.

## Behavioral contract and protocol boundaries

HTTP/2 over HTTPS is negotiated with ALPN; plain HTTP remains HTTP/1.1. HTTP/1.1-only and HTTP/2-only facades reject other protocol versions before route dispatch. HTTP/3 uses a distinct UDP listener. Asynchronous body read/flush operations may return an explicit `AGAIN`/yield state; handlers must preserve resumable work in request context rather than stack-local state. Slow connections and HTTP/2 sibling streams must not monopolize handler workers. WebSocket routes have separate message-codec limits and cancellation rules. Backend support and production completeness must be assessed per implementation.

## Cross-language interoperability

Preserve protocol semantics, resource ownership, error distinctions and callback lifetime. Native concurrency, socket, cryptographic and allocation choices may differ by language. Supported protocols are implementation capabilities rather than a guarantee that every backend is present.

## Example use case

Implement a shared `/status` route, expose it on an HTTP/1.1 listener and an HTTP/2 HTTPS listener, and optionally launch HTTP/3 on a UDP endpoint without duplicating route logic.

## Related CLM modules

[clm_async_pool](clm_async_pool.md), [clm_bytes](clm_bytes.md), [clm_compress](clm_compress.md), [clm_http_utils](clm_http_utils.md), [clm_multipart](clm_multipart.md), [clm_net](clm_net.md), [clm_stream](clm_stream.md), [clm_template](clm_template.md), [clm_tls](clm_tls.md), [clm_websocket](clm_websocket.md).
