# CLM - Multipart

## Dependencies

The current CLM reference implementation composes:

- [clm_base](./clm_base.md) — common value and status types.
- [clm_bytes](./clm_bytes.md) — destination byte buffer for multipart generation.

**External dependencies:** None. Multipart parsing and generation are implemented using ordinary byte operations; neither an HTTP server nor a network socket is required.

## 1. Purpose and scope

`clm_multipart` parses and generates complete **MIME multipart bodies**, including multipart/form-data payloads. It validates boundaries, extracts a boundary from `Content-Type`, exposes part headers and bodies, and writes correctly delimited messages.

It operates on **bytes rather than HTTP connections**. An HTTP server or client may use it, but the module itself neither sends requests nor receives streamed bodies.

## 2. Model and borrowed data

| Value | Meaning |
|---|---|
| `span` | A byte pointer/view plus length; need not be NUL-terminated. |
| `header` | Header name and value spans. |
| `part` | A range of headers and a body span. |
| `message` | Part/header arrays, counts, preamble, epilogue, and consumed byte count. |

**Parsing is zero-copy.** Part bodies, header names and values, preamble, and epilogue refer directly to the original input. They must not outlive or outlast modification of that input. The caller supplies arrays for parts and header fields; the parser does not allocate separate copies of those values.

Headers for all parts share a flat array. A part identifies its headers by a **header offset and count**. These ranges are independent of part body positions.

## 3. Boundary syntax and Content-Type

A multipart boundary is **1–70 bytes** long and must use the allowed RFC 2046 boundary character set. A delimiter line consists of a leading `--`, the boundary, and appropriate CRLF or closing-delimiter syntax.

| Operation | Behavior |
|---|---|
| `boundary_is_valid(boundary)` | Check length and allowed boundary characters. |
| `boundary_from_content_type(content_type, output)` | Extract and decode the `boundary` parameter from a multipart media type. |

Boundary extraction handles **quoted parameters and quoted-pair escapes**. It reports the required output size **including a terminating NUL** for C-style callers. A null output buffer with zero capacity may be used to measure the required capacity. A boundary is not generated automatically by this module; the caller selects a valid value.

## 4. Parsing complete multipart bodies

| Operation | Behavior |
|---|---|
| `parse(data, boundary, parts, headers)` | Parse a complete multipart body into caller-provided arrays and borrowed spans. |
| `find_header(message, part, name)` | Find the first part header by ASCII case-insensitive name. |
| `span_equals_ignore_case(span, text)` | Compare a borrowed span to text without ASCII case differences. |

The parser:

- Recognizes optional **preamble** before the first delimiter and optional **epilogue** after the final delimiter.
- Requires **CRLF** line endings and correct opening, interior, and closing delimiter syntax.
- Rejects malformed header fields and obsolete header folding.
- Distinguishes truncated/incomplete input from malformed input.
- Reports separate limits for too many parts and too many headers.
- Records the number of body bytes consumed through the closing delimiter.

The output part/header arrays are only valid after a **successful** parse. On incomplete or failed parsing, their contents are unspecified and must not be consumed as a partial result.

## 5. Generating multipart bodies

`generate(destination, boundary, parts, headers)` appends a **complete multipart body** to a CLM byte buffer. Each part uses a range of headers from the shared input header array and supplies a body byte span. Generation validates the boundary and part/header representation and emits the required delimiters and CRLF framing.

The destination buffer is **unchanged on failure**. A successful call appends output to any bytes already present; it does not implicitly clear the destination. The caller owns input header/body bytes and the destination resource and chooses a boundary to place in the associated HTTP `Content-Type` header.

## 6. Results and limits

| Result | Meaning |
|---|---|
| `SUCCESS` | Complete body parsed or generated. |
| `INCOMPLETE` | Additional bytes would be needed to finish parsing. |
| `FAILURE` | Multipart syntax or validation failed. |
| `INVALID_ARGUMENT` | Required input or storage description is invalid. |
| `BUFFER_TOO_SMALL` | Output buffer cannot hold a requested boundary value. |
| `TOO_MANY_PARTS` | Caller-supplied part capacity was exceeded. |
| `TOO_MANY_HEADERS` | Caller-supplied header capacity was exceeded. |
| `NO_MEMORY` / `LIMIT_EXCEEDED` | Memory or size restrictions prevented generation or parsing. |

Implementations may map these into exceptions or result types, provided that malformed, incomplete, and insufficient-capacity cases remain distinguishable.

## 7. Illustrative usage

```text
content_type = "multipart/form-data; boundary=example-boundary"
boundary = multipart.boundary_from_content_type(content_type)

parts = allocate_part_storage(limit = 32)
headers = allocate_header_storage(limit = 128)
message = multipart.parse(received_body, boundary, parts, headers)

for part in message.parts:
    disposition = multipart.find_header(message, part, "Content-Disposition")
    process(part.body, disposition)

# For generation, choose a valid boundary and construct parts/header ranges.
output = bytes.new_buffer()
multipart.generate(output, boundary, outgoing_parts, outgoing_headers)
```

Pseudocode illustrates operations; actual signatures vary across CLM languages.

## 8. Boundaries and interoperability

- **Complete-body parsing:** this is not a callback-based incremental streaming multipart parser; a caller can collect bytes first, then parse.
- **No HTTP coupling:** `Content-Type` extraction is a utility, not a request/response parser.
- **No automatic text conversion:** part bodies remain arbitrary bytes, including NULs and binary file contents.
- **Borrowed parse results:** retaining a part requires retaining or copying the original input bytes.
- **Strict framing:** tolerant MIME parsers may accept forms this module deliberately rejects.
