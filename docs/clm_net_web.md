# CLM - Web Server

## Dependencies

The current CLM C reference implementation composes:

- [clm_base](./clm_base.md) — common scalar values and IDs, transitively through the modules below.
- [clm_net](./clm_net.md) — TCP listeners, UDP listeners for HTTP/3, socket readiness, and addresses.
- [clm_tls](./clm_tls.md) — TLS for HTTP/1.1 and HTTP/2 over TCP.
- [clm_async_pool](./clm_async_pool.md) — connection execution and bounded handler scheduling.
- [clm_bytes](./clm_bytes.md) — buffered request and response data.
- [clm_stream](./clm_stream.md) — byte-stream copying for response output.
- [clm_http_utils](./clm_http_utils.md) — HTTP/1.x parsing and shared HTTP field utilities.
- [clm_multipart](./clm_multipart.md) — multipart request-body parsing.
- [clm_compress](./clm_compress.md) — optional HTTP content compression/decompression.
- [clm_template](./clm_template.md) — optional response template rendering.
- [clm_thread](./clm_thread.md) — reference-implementation synchronization and worker lifecycle.
- [clm_net_quic](./clm_net_quic.md) — **optional**, when the HTTP/3 adapter is enabled.

**External dependencies:** The C implementation uses **nghttp2** for HTTP/2, even when accessed through the HTTP/1-only facade, because the current shared engine includes the HTTP/2 adapter. HTTPS uses the platform TLS backend described by `clm_tls`. Optional HTTP/3 builds additionally require **nghttp3, ngtcp2, and a QUIC-capable TLS backend** (currently OpenSSL 3.5+). External libraries are implementation choices, not prescribed for every language port.

## 1. Purpose and scope

`clm_net_web` is a **server-side HTTP application layer** with a common routing, middleware, request, response, and cancellation API across supported HTTP protocols. The C reference implementation provides:

- **HTTP/1.1** over TCP, including plain HTTP and HTTPS.
- **HTTP/2** over TLS/TCP, negotiated with ALPN; default HTTPS prefers `h2` and permits HTTP/1.1 fallback.
- **HTTP/3** through the optional [clm_net_web_h3](./clm_net_web_h3.md) adapter over QUIC/UDP.

Separate thin facades [clm_net_web_h1](./clm_net_web_h1.md) and [clm_net_web_h2](./clm_net_web_h2.md) restrict the shared TCP implementation to one HTTP version. The HTTP/3 facade is a separately configured UDP listener, not an ALPN mode of an existing TCP connection.

This is an **HTTP server**, not an HTTP client, reverse proxy, or general-purpose QUIC implementation.

## 2. Shared configuration and defaults

The server accepts a configuration defining worker counts, listener backlog, connection and request limits, body buffering, timeouts, gzip, TLS, and HTTP/2-specific resource limits.

| Setting | C reference default / behavior |
|---|---|
| Worker count | **4** |
| Pending connections | **128** |
| Maximum live connections | **1,024** |
| Request byte limit | **8 MiB** |
| Response byte limit | **16 MiB** |
| Maximum header count | **128** |
| Maximum keep-alive requests | **100** |
| Maximum body buffered before handler | **64 KiB** |
| Streaming HTTP/1 connection slots | Worker count minus one; minimum **1** |
| Idle/read/write timeouts | **30 seconds** each |
| TCP backlog | **128** |
| Gzip support | Enabled by initialized defaults |
| Shutdown grace | **0**, meaning immediate cancellation by default |
| HTTP/2 concurrent streams per connection | **100** |
| HTTP/2 decoded header budget | **64 KiB** |
| HTTP/2 connection memory reservation budget | **1 GiB** |
| HTTP/2 initial SETTINGS ACK timeout | **10 seconds** |

The C reference provides additional HTTP/2 protection limits for output queues, SETTINGS entries, pending ACKs, CONTINUATION frames, and per-second stream/control-frame rates. Zero values select defined defaults for many size and timeout options; fields that intentionally accept zero as a setting are documented separately.

## 3. Server and route lifecycle

| Operation | Behavior |
|---|---|
| `config_init(config)` | Fill the common configuration with defaults. |
| `server_create(config)` | Create a server and retain its copied configuration/credentials. |
| `route_add(server, method, pattern, handler, context)` | Register a synchronous route. |
| `route_add_async(server, method, pattern, handler, context)` | Register a **resumable**, nonblocking application route. |
| `middleware_add(server, callback, context)` | Append middleware in registration order. |
| `server_start(server, address)` | Start a listener and return while it runs asynchronously. |
| `server_local_address(server)` | Query the actual bound endpoint, including an ephemeral port. |
| `server_stop(server)` | Stop acceptance; cancel or drain according to protocol/configuration. |
| `server_destroy(server)` | Stop if necessary and release server-owned resources. |

Routes and middleware are configured **before start** and are immutable while the server is serving traffic. A route uses an HTTP method token or `*` (match any method), and a leading-slash path pattern. Supported patterns include literal segments, `:parameter` segments, and a final `*catchall` segment. Specific literal matches are preferred over generic parameters when competing routes match.

Handlers receive a **normalized request and response** regardless of the framing protocol. Application callback contexts are borrowed until server destruction.

## 4. Requests, body access, and middleware

A request exposes the method, target, path, headers, body or body reader, remote address, path parameters, request context, optional scheme/authority, and optional trailing fields.

| Operation | Behavior |
|---|---|
| `request_header(request, name)` | Find an initial request header by case-insensitive name. |
| `request_trailer(request, name)` | Look up a separate HTTP/2 or HTTP/3 trailer. |
| `request_param(request, name)` | Find a matched route path parameter. |
| `request_read_body(request, buffer)` | Read the next body bytes or signal `END` / `AGAIN`. |
| `request_multipart(request, storage)` | Parse a buffered multipart request body using its `Content-Type` boundary. |
| `context_request_id(context)` | Read the unique request-local identifier. |
| `context_set` / `context_get` | Store/retrieve per-request user data with optional disposal callbacks. |
| `context_is_cancelled` / `context_cancel` | Observe or request cancellation. |

Middleware executes in registration order and either continues, finishes dispatch, or reports an error. Its callback receives the same normalized request/response types as a route handler.

**Input buffering depends on protocol and handler type:**

- **HTTP/1.1:** Small bodies are buffered before dispatch; larger bodies may stream through a body reader. Chunked transfer coding is decoded as bytes are read. Compressed or multipart requests use bounded buffering.
- **HTTP/2:** Async routes receiving uncompressed/identity data may consume the body incrementally **per stream**, with flow-control credit returned as data is consumed. Synchronous and compressed requests buffer to configured limits.
- **HTTP/3:** The current implementation **buffers the bounded request body completely** before invoking a handler, including async routes. Incremental request-body streaming is not yet provided in the HTTP/3 adapter.

Request header spans and borrowed body buffers remain valid only for their request's defined lifetime. An async route uses its **request-local context** to retain progress across resumptions; local stack variables do not persist.

## 5. Responses and streaming

| Operation | Behavior |
|---|---|
| `response_set_status(response, status)` | Set HTTP status before commitment. |
| `response_set_header(response, name, value)` | Add or replace an initial response header. |
| `response_set_trailer(response, name, value)` | Add or replace an HTTP/2 or HTTP/3 trailing field. |
| `response_write(response, bytes)` / `response_write_string(response, text)` | Append response body bytes. |
| `response_flush(response)` | Commit headers and advance streamed body delivery. |
| `response_redirect(response, location, status)` | Configure an HTTP redirect. |
| `response_render_template(response, template, values)` | Render a CLM template into the response. |

Before the first flush, a response is buffered. **The first flush commits status and initial headers**, after which they cannot be changed. HTTP/1.1 uses chunked streaming where appropriate; HTTP/2 and HTTP/3 use protocol DATA streams. HTTP/2 and HTTP/3 omit hop-by-hop HTTP/1-only headers from encoded output.

Response trailers are **HTTP/2 and HTTP/3 only**. Framing, routing, authentication, and other prohibited trailer fields are rejected, and HEAD / 204 / 304 no-body responses cannot carry trailers.

HTTP/2 and HTTP/3 multiplex many request streams per connection. Under backpressure, an async route may receive `AGAIN` on a response write or flush; it must return and resume later, retrying writes that were not accepted. Synchronous handlers should **not** wait for a peer to react to a flush; their output becomes available to the connection owner when the handler returns.

## 6. Worker and connection execution

The implementation separates protocol work from application work:

- The **TCP reactor** polls listeners and existing connections, parks idle or blocked connections, and wakes them on readiness or a deadline.
- The **HTTP/2 connection owner** controls the nghttp2 session and stream state; bounded worker tasks execute application handlers without concurrently touching codec state.
- The **HTTP/3 UDP reactor** owns QUIC and nghttp3 processing; bounded handler tasks operate on request-local exchanges.
- Connection and stream-level resource caps limit buffered input/output and the work admitted during load.

For HTTP/2 and HTTP/3, cancellation and errors normally affect an individual request stream without necessarily aborting healthy sibling streams. HTTP/1.1 cancellation affects its connection.

## 7. TLS, protocol selection, and shutdown

The default TCP HTTPS server advertises **`h2` and `http/1.1`**, preferring `h2` when offered; a client offering only HTTP/1.1 or omitting ALPN can be served as HTTP/1.1. Plain TCP remains HTTP/1.1. Cleartext HTTP/2 (`h2c`) is not implemented.

- [clm_net_web_h1](./clm_net_web_h1.md) forces HTTP/1.1 and optionally uses TLS.
- [clm_net_web_h2](./clm_net_web_h2.md) requires TLS and negotiated HTTP/2, with **no HTTP/1 fallback**.
- [clm_net_web_h3](./clm_net_web_h3.md) uses its own QUIC/UDP listener and QUIC TLS integration.

Shutdown stops new request acceptance. With a configured grace period, established HTTP/2 and HTTP/3 sessions initiate **GOAWAY/drain** and allow admitted requests to finish until the deadline; the default is immediate cancellation. HTTP/1.1 retains immediate stop behavior. A grace deadline cannot forcibly interrupt an application callback that does not return cooperatively.

## 8. Results and illustrative usage

| Result | Meaning |
|---|---|
| `SUCCESS` | Operation completed. |
| `NOT_FOUND` | No matching resource or route. |
| `INVALID_ARGUMENT` | Invalid configuration, request, or API argument. |
| `SYSTEM_ERROR` / `TLS_ERROR` / `NO_MEMORY` | Transport, crypto, or allocation failure. |
| `BUSY` | Server or stream cannot admit work. |
| `LIMIT_EXCEEDED` | A request, response, header, or queue limit was reached. |
| `HANDLER_ERROR` | Application callback failed. |
| `END` | Request body reader reached the end. |
| `AGAIN` | Async body/response operation should resume later. |

```text
config = web.config_init()
config.tls = server_credentials            # optional: enables TCP HTTPS
server = web.server_create(config)

web.route_add(server, "GET", "/hello/:name", handle_hello)
web.middleware_add(server, audit_request)
web.server_start(server, listen_address)

# Callback receives a normalized request and response regardless of HTTP version.
handle_hello(request, response):
    name = web.request_param(request, "name")
    web.response_set_header(response, "Content-Type", "text/plain")
    web.response_write_string(response, "Hello " + name)

web.server_stop(server)
web.server_destroy(server)
```

The example is language-neutral pseudocode. Actual function signatures and ownership conventions depend on each language binding.

## 9. Boundaries and interoperability

- **Server-side only:** client connection management, redirects on the client, cookie jars, and reverse proxying are not furnished by this module.
- **No automatic HTTP/3 upgrade:** protocol discovery and choosing QUIC versus TCP belong to the client. A UDP HTTP/3 listener may coexist with a TCP HTTPS listener on the same numeric port.
- **Security and limits:** the exposed defaults are starting points, not an assurance of safe production limits for every workload.
- **Cross-language promise:** handlers, routing, and response semantics are portable; implementation internals such as nghttp2/nghttp3, sockets, and TLS credential storage may differ.
- **Application termination:** shutdown cannot preempt callbacks that fail to return or observe cancellation.
