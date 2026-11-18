# clm_bytes

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Binary-safe, growable buffers and non-owning byte-range views. Buffer owns its storage; view borrows bytes and never frees them. A buffer may have a nonzero internal offset after consuming bytes, allowing inexpensive front removal before a later compaction or growth. The optional length limit constrains logical data, not necessarily previously reserved backing capacity. Do not retain a view across mutations that can move/reallocate storage.

## Behavior and constraints

Binary-safe, growable buffers and non-owning byte-range views.
Buffer owns its storage; view borrows bytes and never frees them. A buffer may
have a nonzero internal offset after consuming bytes, allowing inexpensive
front removal before a later compaction or growth. The optional length limit
constrains logical data, not necessarily previously reserved backing capacity.
Do not retain a view across mutations that can move/reallocate storage.

Binary-safe byte views and growable byte buffers.

A view borrows memory; a buffer owns it. A buffer holds the window
[offset, offset + length) of its allocation, so data can be appended at the
tail (for example from clm_net_receive) and consumed from the front (for
example after clm_net_send) without shifting bytes on every call.
Pointers and views taken from a buffer are invalidated by any call that
changes it. A zero-initialized clm_bytes_buffer is a valid empty buffer.
No function treats NUL specially unless it says so.

## Integration

- C reference: [**clm_bytes**](../c/bytes.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
