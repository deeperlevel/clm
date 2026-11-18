# clm_net_websocket

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

WebSocket connection lifecycle and network transport.

## Behavior and constraints

WebSocket connection lifecycle and network transport.

HTTP upgrade, frame processing, and close-handshake state are separate phases. Outgoing
queues preserve partial writes across socket readiness events.

## Integration

- C reference: [**clm_net_websocket**](../c/net_websocket.md).
- Related CLM declarations in this archive: `clm_net`, `clm_tls`, `clm_types`, `clm_websocket`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
