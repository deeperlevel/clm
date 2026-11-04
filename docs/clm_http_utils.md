# CLM HTTP Utils

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Transport-independent HTTP message and framing utilities, including HTTP/1.x syntax, chunked coding, QUIC variable-length integers and normalized decoded HTTP/3 headers.

## Capabilities and operations

- Parse and serialize HTTP/1.x request/response lines and headers.
- Determine body framing, connection reuse and Expect/100-continue handling.
- Encode/decode chunked transfer bodies; validate chunk extensions and trailers.
- Validate method/header tokens and content-length values.
- Decode and encode QUIC variable-length integer representations and parse HTTP/3 frame headers.
- Validate normalized HTTP/3 field names/values and pseudo-header ordering after QPACK decoding.

## Resource, data and ownership model

Inputs and header spans may be borrowed from application-owned buffers. The utility module owns no network socket, TLS session, HTTP/2 encoder or QPACK state; it interprets already available bytes or decoded HTTP/3 header fields.

## Behavioral contract and protocol boundaries

HTTP/1 parser input must follow strict CRLF and header framing rules. Distinguish incomplete input from malformed syntax and size limits. For HTTP/3, the API validates already decoded field sequences; it does not implement QPACK. QUIC stream FIN is not TCP connection close. HTTP/3 requests are normalized into the shared HTTP message model with the correct scheme/authority and pseudo-field requirements.

## Cross-language interoperability

Preserve protocol semantics, resource ownership, error distinctions and callback lifetime. Native concurrency, socket, cryptographic and allocation choices may differ by language. Supported protocols are implementation capabilities rather than a guarantee that every backend is present.

## Example use case

Parse an HTTP/1.1 request, validate a chunked trailer, or normalize QPACK-decoded HTTP/3 fields before dispatching an application route.

## Related CLM modules

[clm_base](clm_base.md).
