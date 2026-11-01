# CLM HTTP Utils

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

HTTP/1.x utilities for message parsing, serialization, chunked transfer coding and header helpers, independent of a particular HTTP server. [clm_net_web](clm_net_web.md) uses it for request and response wire handling.

## Capabilities and operations

- Parse request and response start lines, headers and body framing information.
- Serialize messages and encode/decode chunked transfer bodies.
- Validate header/method tokens, match tokens in list-valued headers (such as `Connection` or `Accept-Encoding`, honouring `q=0`), and read `Content-Length`.
- Decide connection reuse (`keep_alive`) and `Expect: 100-continue` handling, parse chunk-size lines, and split a request target into path and query.
- Provide reason phrases for status codes and format the HTTP `Date` header value.
- Represent data as borrowed spans or caller-provided storage as documented.
- Do not confuse HTTP message syntax with full server lifecycle, routing or HTTP/2/3 protocol handling.

## HTTP message framing

`clm_http_utils` handles the textual HTTP/1.x message representation, not the server socket lifecycle. Requests and responses have distinct start lines and header collections. Its parsing functions identify framing such as no body, a declared body length, chunked transfer or connection-close-delimited content where applicable.

The module also provides chunked body decoding and encoding and case-insensitive header comparisons. Parsing and serialization errors must be distinguishable from incomplete incoming data. Chunk framing is strict: lines end in CRLF (bare LF and stray CR are rejected), chunk extensions must be well formed, and trailer fields must be valid header lines that are not framing or routing fields (Content-Length, Transfer-Encoding, Host and similar). `clm_http_utils_parse_chunk_size` and `clm_http_utils_validate_trailer` expose these checks so incremental decoders such as the `clm_net_web` request-body reader apply the identical rules as the buffered decoder.

Use [clm_net_web](clm_net_web.md) for serving routes and [clm_net](clm_net.md) for raw transport.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_string](clm_string.md).

## Example use cases

Parse incoming HTTP request headers or serialize an outgoing HTTP response.
