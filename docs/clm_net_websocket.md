# CLM Net WebSocket

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net](clm_net.md), [clm_tls](clm_tls.md), [clm_websocket](clm_websocket.md)
- **External libraries / platform facilities (C reference):** TLS and QUIC/HTTP2 libraries transitively through the selected transport.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Establishes and operates WebSocket sessions over HTTP/1.1 upgrades or HTTP/2/3 extended CONNECT with explicit protocol selection.

## Capabilities and operations

- Create a WebSocket client with host, path, optional subprotocol and headers.
- Complete the selected WebSocket handshake, then exchange messages and fragments.
- Read parsed WebSocket events and close the session cooperatively.
- Support plain or secure HTTP/1.1; require secure transport for HTTP/2/3.

## Resource, data and ownership model

The HTTP transport and WebSocket frame codec are separate layers. `clm_net_websocket` handles connection and handshake ownership; `clm_websocket` handles frame/message-level semantics.

## Behavioral contract and boundaries

Protocol selection is explicit: no automatic fallback. For HTTP/1.1 require a valid switching-protocols handshake; for HTTP/2/3 use successful extended CONNECT. TLS verification is enabled by default. Custom handshake headers cannot override reserved WebSocket/HTTP transport fields.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Connect to a secure WebSocket endpoint, send an application message and process incoming events.

## Related CLM modules

[clm_net](clm_net.md), [clm_tls](clm_tls.md), [clm_websocket](clm_websocket.md).
