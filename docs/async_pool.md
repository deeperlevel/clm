# clm_async_pool

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Worker pools layered over cooperative schedulers.

## Behavior and constraints

Worker pools layered over cooperative schedulers.

Worker command queues transfer scheduling work across threads. Pool shutdown coordinates
pending tasks, cancellation, and worker lifetime.

## Integration

- C reference: [**clm_async_pool**](../c/async_pool.md).
- Related CLM declarations in this archive: `clm_async`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
