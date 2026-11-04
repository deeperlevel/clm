# CLM Net QUIC

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net](clm_net.md)
- **External libraries / platform facilities (C reference):** ngtcp2 (>= 1.25) with its OpenSSL crypto integration and OpenSSL (>= 3.5), when QUIC is enabled.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides a bounded, event-driven QUIC transport connection that processes UDP datagrams supplied by a caller and produces datagrams to send.

## Capabilities and operations

- Configure and accept an incoming QUIC connection from the first UDP datagram.
- Feed received datagrams and monotonic time into the connection state machine.
- Produce outgoing UDP packets and track the next expiry deadline.
- Send and consume stream data, open server unidirectional streams, shut down streams and initiate connection close.
- Receive handshake, data, acknowledgement, reset and stream-closure callbacks.

## Resource, data and ownership model

The QUIC engine owns per-connection state but neither sockets nor threads. The calling event loop owns UDP I/O, clock input and packet dispatch. Connection state and callbacks are per peer, with a bounded send queue and receive credit accounting.

## Behavioral contract and boundaries

A write yields at most one complete datagram; packet bytes must be sent unchanged. `AGAIN` means no packet currently available, not necessarily an error. Stream data pointers are borrowed during callbacks; consumption grants flow-control credit. All calls for a connection are serialized. No 0-RTT, Retry or path migration is promised by this reference interface. The current C backend requires ngtcp2 and OpenSSL QUIC support; no unsupported-mode fallback is promised.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Accept a UDP Initial packet, drive receive/write/expiry from an event loop, and feed application HTTP/3 streams through the QUIC connection.

## Related CLM modules

[clm_net](clm_net.md).
