# clm_stream

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Byte sources, sinks, adapters, and incremental copying.

## Behavior and constraints

Byte sources, sinks, adapters, and incremental copying.

A pump retains bytes not yet accepted by its sink. END, AGAIN, and successful partial
transfer represent different progress states.

Incremental, bounded data transfer between a source and a sink.

A source produces bytes and a sink consumes them. A pump moves bytes from
one to the other through a fixed-size staging buffer, one bounded step at a
time, so it works with blocking endpoints (files, memory) and nonblocking
ones (sockets) alike: a step that cannot progress returns WOULD_BLOCK and
reports which side to wait for. Memory use is bounded by the buffer size,
never by the amount transferred.

Endpoint contract:
read  - SUCCESS transfers at least one byte; END means no more data;
WOULD_BLOCK means none is available yet; anything else is an error.
write - SUCCESS accepts at least one byte and may accept fewer than given;
WOULD_BLOCK accepts none now; anything else is an error.
finish - signals end of data to a sink; may return WOULD_BLOCK and is then
called again. It is optional.
Endpoints do not own what they point at (ids, buffers, other endpoints); do
not move or free those while a pump uses the endpoint. Built-in endpoints
keep their state inside the struct, so initialize one per transfer.

## Integration

- C reference: [**clm_stream**](../c/stream.md).
- Related CLM declarations in this archive: `clm_bytes`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
