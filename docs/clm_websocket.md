# CLM Websocket

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Parses and constructs WebSocket frames and messages independent of TCP, HTTP upgrades or particular TLS transports.

## Capabilities and operations

- Create a configurable WebSocket parser/serializer state machine.
- Feed received bytes and iterate decoded events.
- Send messages and fragments; initiate graceful close.
- Retrieve bounded encoded output for an external transport to send.
- Validate server-accept keys and handshake fields where provided.

## Resource, data and ownership model

The codec owns message/framing state, not the underlying network socket or HTTP transport. A caller supplies data, consumes output and handles backpressure. Callbacks/events expose borrowed data as documented by the binding.

## Behavioral contract and boundaries

Masking roles, fragmentation ordering, control-frame constraints, UTF-8 text validation and close-handshake transitions must be honored. Transport-specific HTTP/1.1 or extended CONNECT negotiation is handled by higher-level adapters.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Receive bytes from an established WebSocket connection, decode a text event, queue a reply and send the produced frames.

## Related CLM modules

[clm_base](clm_base.md).
