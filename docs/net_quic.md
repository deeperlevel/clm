# clm_net_quic

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

QUIC connection state and caller-driven UDP transport.

## Behavior and constraints

QUIC connection state and caller-driven UDP transport.

Timers, outgoing datagrams, and stream callbacks advance the protocol. UDP packet
ownership is distinct from retained stream data.

## Integration

- C reference: [**clm_net_quic**](../c/net_quic.md).
- Related CLM declarations in this archive: `clm_net`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
