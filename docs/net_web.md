# clm_net_web

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Shared web-server routing, request state, and response delivery.

## Behavior and constraints

Shared web-server routing, request state, and response delivery.

Network polling advances connections while handlers produce responses. Partial I/O and
asynchronous handler completion require persistent connection state.

HTTP/2-first HTTPS server with HTTP/1.1 fallback, dispatched through clm_async_pool.
Plain HTTP defaults to HTTP/1.1; require_http2 selects cleartext prior knowledge.
The h2 facade requires HTTP/2; the h1 facade requires HTTP/1.1 exclusively.
The separate net_web_h3 module shares application APIs over QUIC/UDP only.
Connection tasks park while waiting for socket readiness, so idle keep-alive
sockets do not occupy worker execution. Synchronous handlers run on pool
workers; HTTP/1 body reads and response writes can wait under backpressure.
HTTP/2 uses nghttp2 memory I/O on one connection task at a time and executes
handlers on a separate bounded pool. Only the connection task accesses TLS
until shutdown joins that task and takes over final transport closure.

## Integration

- C reference: [**clm_net_web**](../c/net_web.md).
- Related CLM declarations in this archive: `clm_async_pool`, `clm_bytes`, `clm_compress`, `clm_http_utils`, `clm_multipart`, `clm_stream`, `clm_template`, `clm_tls`, `clm_websocket`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
