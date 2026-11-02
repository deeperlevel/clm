# CLM - HTTP Utilities

## Dependencies

The current CLM C reference implementation composes:

- [clm_base](./clm_base.md) — scalar/status types and CLM resource-ID conventions.
- [clm_bpa](./clm_bpa.md) — access to registered resource blocks for string-backed operations.
- [clm_string](./clm_string.md) — reading and writing CLM string resources.

**External dependencies:** None. HTTP text parsing, formatting, and validation use standard language/runtime capabilities. HTTP/2 framing and HTTP/3/QPACK framing belong to their respective [web-server modules](./clm_net_web.md), not to the HTTP/1 message parser.

## 1. Purpose and scope

`clm_http_utils` provides **strict, reusable HTTP/1.x message utilities**: request/response parsing, header handling, framing validation, chunked coding, serialization, status phrases, dates, and common connection/request helpers. It is independent of sockets, TLS, routing, and application handlers.

The parser prioritizes **unambiguous framing**. It deliberately rejects several permissive historical HTTP/1 constructs associated with request-smuggling and parser disagreement.

## 2. Message model and ownership

| Kind | Meaning |
|---|---|
| `REQUEST` / `RESPONSE` | Whether the message contains a request line or status line. |
| `span` | Borrowed byte span; not necessarily terminated by NUL. |
| `header` | Original-case name and borrowed value. |
| `BODY_NONE` | Message has no body. |
| `BODY_LENGTH` | Body has an explicitly determined byte count. |
| `BODY_CHUNKED` | Body holds **raw chunked wire encoding**, including terminator/trailers. |
| `BODY_UNTIL_CLOSE` | Response body ends when transport EOF is reported. |

A message also records protocol version, method/target or status/reason, header count/capacity, body, and **consumed input length**.

Parsing is **allocation-free and zero-copy**: spans borrow the input, while header entries are written to caller-provided storage. Both source input and header storage must remain valid and unchanged while the parsed message is used. The `consumed` field lets the caller locate the next pipelined message.

`message_init(kind)` prepares a message for serialization. It defaults to HTTP/1.1; response initialization defaults to status 200 with an empty length-delimited body, causing `Content-Length: 0` to be generated when needed.

## 3. Parsing requests and responses

| Operation | Behavior |
|---|---|
| `parse_request(data, headers)` | Parse one complete HTTP request, including framed body. |
| `parse_request_headers(data, headers)` | Parse only the complete request-line/header section without waiting for the body. |
| `parse_response(data, head_request, headers)` | Parse one response with HEAD-response awareness. |
| `parse_response_ex(data, head_request, connect_request, input_closed, headers)` | Parse with CONNECT and transport-EOF context. |
| `loads_request(string_id, headers)` | Parse a CLM string resource as a request. |
| `loads_response(string_id, head_request, headers)` | Parse a CLM string resource as a response. |
| `loads_response_ex(string_id, head_request, connect_request, input_closed, headers)` | Parse with CONNECT/EOF context from a string resource. |

Request parsing skips leading empty lines, requires **exactly one nonempty `Host` header for HTTP/1.1**, and accepts only supported, consistent framing. Response parsing accounts for **HEAD**, informational statuses, **204**, and **304**. Successful **CONNECT** responses end after headers so that subsequent bytes can be interpreted as tunnel data.

For close-delimited response bodies, completion requires an explicit transport EOF signal supplied to `parse_response_ex`; until then the parser returns `INCOMPLETE`. A truncated length-delimited or chunked message at EOF is a **failure**, not a valid partial response.

## 4. Strict syntax and framing

Parsing requires:

- **CRLF** line endings (not bare LF).
- No obsolete folded headers and no whitespace before header-name colons.
- Valid header names and safe field values.
- No conflicting `Content-Length` and `Transfer-Encoding` framing.
- No duplicate/conflicting Content-Length values.
- A single supported `Transfer-Encoding: chunked` field; unsupported transfer codings are rejected.

These requirements are deliberate interoperability and security boundaries. The module does not infer or repair ambiguous message framing.

## 5. Serialization and chunked encoding

| Operation | Behavior |
|---|---|
| `serialize(message, destination)` | Serialize to HTTP/1.x bytes with CRLF lines. |
| `dumps(message, string_id)` | Serialize to an existing CLM string resource when representable as UTF-8 without embedded NUL. |
| `decode_chunked(data, destination)` | Validate and decode raw chunked transfer encoding. |
| `encode_chunked(data, destination)` | Encode supplied bytes as **one chunk plus terminating zero chunk**. |
| `parse_chunk_size(line)` | Parse a hexadecimal chunk size and validate extensions. |
| `validate_trailer(line)` | Validate one trailer field, rejecting prohibited framing-related fields. |

Serialization checks method/header tokens, status, version, field values, and consistent framing headers. It adds `Content-Length` or `Transfer-Encoding: chunked` when appropriate and missing. For `BODY_CHUNKED`, the body must **already contain valid chunked encoding**.

Size-measurement calls permit a null destination with zero capacity. In the C reference API, serialized text reports the required byte count **including NUL**; encoded chunked output reports the exact **wire-byte count without NUL**. Insufficient destination capacity leaves the destination unchanged.

## 6. General HTTP helpers

| Operation | Behavior |
|---|---|
| `find_header(message, name)` | Find the first header by ASCII case-insensitive name. |
| `span_equals` / `span_equals_ignore_case` | Compare borrowed spans to text. |
| `is_token(name)` | Validate an RFC-style HTTP method or header-name token. |
| `header_has_token(value, token)` | Search a comma-separated header token list; `q=0` marks refusal. |
| `content_length(message)` | Parse a valid decimal length; absent value yields zero. |
| `expects_continue(message)` | Recognize `Expect: 100-continue` on applicable HTTP/1.1 requests. |
| `keep_alive(message)` | Determine connection reuse intent from version and `Connection`. |
| `path_from_target(target)` | Return target portion preceding the first `?` or `#`. |
| `reason_phrase(status)` | Return standard status text or a generic fallback. |
| `format_date(output)` | Format the current clock time as an HTTP IMF-fixdate in GMT. |

`format_date` requires at least **30 bytes** of output capacity in the current C implementation. The current date comes from the host clock; HTTP date formatting is distinct from the module's parsing of HTTP message timestamps.

## 7. Results and errors

| Result | Meaning |
|---|---|
| `SUCCESS` | Complete valid operation. |
| `INCOMPLETE` | Additional input or EOF information is required. |
| `FAILURE` | Malformed/unsafe input or invalid message for serialization. |
| `INVALID_ARGUMENT` | Invalid caller arguments or unsupported resource use. |
| `BUFFER_TOO_SMALL` | Caller output cannot hold the complete result. |
| `TOO_MANY_HEADERS` | Caller-provided header storage exhausted. |
| `LIMIT_EXCEEDED` | A numeric value exceeds the representable size limit. |

Failed parses do not publish a valid result message. Implementations should preserve the distinctions between **not yet complete**, **malformed**, and **capacity exceeded**.

## 8. Illustrative usage

```text
headers = allocate_header_storage(128)
parsed = http_utils.parse_request(incoming_bytes, headers)
if parsed == INCOMPLETE:
    receive_more_bytes()
else if parsed == SUCCESS:
    host = http_utils.find_header(parsed.message, "Host")
    route_path = http_utils.path_from_target(parsed.message.target)
    remaining = incoming_bytes[parsed.message.consumed :]

response = http_utils.message_init(RESPONSE)
response.body = "Hello"
wire_bytes = http_utils.serialize(response)
```

This is language-neutral pseudocode, not a literal API binding.

## 9. Boundaries and interoperability

- **No transport:** parsing and serialization do not open, poll, or close sockets.
- **HTTP/1.x wire format:** HTTP/2 uses binary frames/HPACK and HTTP/3 uses QUIC frames/QPACK; neither is serialized as an HTTP/1 start-line.
- **Strictness:** support for lenient legacy messages is intentionally limited.
- **No streaming parser state:** callers assemble enough bytes for one operation; header-only parsing helps servers begin request processing before full bodies arrive.
