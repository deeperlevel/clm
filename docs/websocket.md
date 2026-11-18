# clm_websocket

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Incremental WebSocket frame parsing and serialization.

## Behavior and constraints

Incremental WebSocket frame parsing and serialization.

Frame boundaries differ from message boundaries when fragmentation is used. Masking,
control frames, and close state are validated separately.

## Integration

- C reference: [**clm_websocket**](../c/websocket.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
