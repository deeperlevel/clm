# clm_arena

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

ID-addressed bump allocation with explicit, checked borrow leases. Design: allocation returns offsets rather than durable pointers because growth can relocate storage. A successful borrow pins that storage; while a lease exists, operations that would invalidate it return BUSY. Return the exact lease once. This is a temporary workspace, not a general-purpose allocator with per-block frees. Registry operations are synchronized; writes through a borrowed pointer are not.

## Behavior and constraints

ID-addressed bump allocation with explicit, checked borrow leases.
Design: allocation returns offsets rather than durable pointers because growth can
relocate storage. A successful borrow pins that storage; while a lease exists,
operations that would invalidate it return BUSY. Return the exact lease once.
This is a temporary workspace, not a general-purpose allocator with per-block frees.
Registry operations are synchronized; writes through a borrowed pointer are not.

## Integration

- C reference: [**clm_arena**](../c/arena.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
