# clm_net_web

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_async_pool`](async_pool.md)
- [`clm_bytes`](bytes.md)
- [`clm_compress`](compress.md)
- [`clm_http_utils`](http_utils.md)
- [`clm_multipart`](multipart.md)
- [`clm_stream`](stream.md)
- [`clm_template`](template.md)
- [`clm_tls`](tls.md)
- [`clm_websocket`](websocket.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_net_web. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_async_pool](async_pool.md) - required CLM capability.
- [clm_bytes](bytes.md) - required CLM capability.
- [clm_compress](compress.md) - required CLM capability.
- [clm_multipart](multipart.md) - required CLM capability.
- [clm_stream](stream.md) - required CLM capability.
- [clm_websocket](websocket.md) - required CLM capability.
- [clm_http_utils](http_utils.md) - required CLM capability.
- [clm_template](template.md) - required CLM capability.
- [clm_tls](tls.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_net_web when an application needs: Shared web-server routing, request state, and response delivery.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
