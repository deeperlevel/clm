# CLM Net Web

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

A small HTTP/1.1 server with routes, middleware and response helpers, built on CLM networking and concurrency foundations.

## Capabilities and operations

- Configure and create a server; register routes and middleware, start and stop the listener.
- Inspect requests, headers, route parameters and multipart data; attach request-scoped context values.
- Set status and response headers; write strings/bytes, redirect and render templates.
- Offer TLS and compression through supporting modules.
- Current server runs synchronous handler work on worker-pool threads; idle active connections can occupy workers. HTTP/2 and HTTP/3 are not provided by this module snapshot.

## Server lifecycle

The application initializes server configuration, creates a server, adds routes and middleware, starts serving, and then stops and disposes the server. The listener can report its actual local address. Middleware runs in the request-handling chain and can reject or annotate a request before it reaches a handler.

## Requests and responses

- Request helpers expose header lookup, route parameters and multipart body parsing.
- Request-scoped context can carry typed application values associated with a single request.
- Response helpers set status and headers, write byte/text bodies, redirect clients and render templates.
- Related modules supply HTTP message parsing, optional compression, multipart handling and TLS sessions.

## Present protocol limits

The 55-module snapshot provides an **HTTP/1.1 server** with synchronous handler dispatch to a worker pool. Active idle connections can occupy worker capacity. HTTP/2, QUIC/HTTP/3, complete asynchronous request/response streaming and production-grade reverse proxy behavior are future work, not promises of the current API.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Protocol scope in this snapshot: HTTP/1.1 server. HTTP/2 and HTTP/3 require additional modules or future work.

## Related CLM modules

**Supporting modules:** [clm_async_pool](clm_async_pool.md), [clm_bytes](clm_bytes.md), [clm_compress](clm_compress.md), [clm_multipart](clm_multipart.md), [clm_stream](clm_stream.md), [clm_string_httpp](clm_string_httpp.md), [clm_template](clm_template.md), [clm_tls](clm_tls.md).

## Example use cases

Serve a static index page over HTTP/1.1 and add a middleware to reject unsupported request methods.
