# clm_http_utils

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

HTTP message parsing, framing, and serialization.

## Behavior and constraints

HTTP message parsing, framing, and serialization.

Header syntax and message framing are validated independently of application routing.
Parsed spans can borrow the input buffer.

HTTP utilities: HTTP/1.x parsing/serialization/chunked coding (RFC 9112),
QUIC variable-length integers and decoded HTTP/3 field normalization.

Parsing is zero-copy and allocation-free: a parsed message holds spans that
point into the input and header entries stored in caller-owned memory. The
spans stay valid only while the input is unchanged. Parsing is strict:
CRLF line endings only, no obsolete line folding, no whitespace before the
header colon, and conflicting framing (Content-Length with Transfer-Encoding,
or repeated Content-Length values) is rejected to prevent request smuggling.
For simplicity and predictable framing, Transfer-Encoding supports only a
single 'chunked' field; other transfer codings are rejected. HTTP/1.1
requests require exactly one nonempty Host field.

## Integration

- C reference: [**clm_http_utils**](../c/http_utils.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
