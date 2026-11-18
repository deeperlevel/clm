# clm_async

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Cooperative task scheduling and task lifecycle notifications.

## Behavior and constraints

Cooperative task scheduling and task lifecycle notifications.

Ready queues hold runnable tasks; waiting tasks must be woken explicitly. Generation
numbers distinguish reused task identifiers.

## Integration

- C reference: [**clm_async**](../c/async.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
