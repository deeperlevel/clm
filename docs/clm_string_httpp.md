# CLM String Httpp

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

HTTP/1.x wire-message parsing and serialization independent of a particular HTTP server.

## Capabilities and operations

- Parse request and response start lines, headers and body framing information.
- Serialize messages and encode/decode chunked transfer bodies.
- Represent data as borrowed spans or caller-provided storage as documented.
- Do not confuse HTTP message syntax with full server lifecycle, routing or HTTP/2/3 protocol handling.

## HTTP message framing

`clm_string_httpp` handles the textual HTTP/1.x message representation, not the server socket lifecycle. Requests and responses have distinct start lines and header collections. The parser identifies framing such as no body, a declared body length, chunked transfer or connection-close-delimited content where applicable.

The module also provides chunked body decoding and encoding and case-insensitive header comparisons. Parsing and serialization errors must be distinguishable from incomplete incoming data.

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
