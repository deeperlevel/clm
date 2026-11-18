# clm_net_web_h2

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

HTTP/2 stream processing and server integration.

## Behavior and constraints

HTTP/2 stream processing and server integration.

Connection-level HTTP/2 framing is mapped to per-stream requests. Flow control and
asynchronous response state outlive individual frame callbacks.

## Integration

- C reference: [**clm_net_web_h2**](../c/net_web_h2.md).
- Related CLM declarations in this archive: `clm_net_web`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
