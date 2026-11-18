# CLM bytes

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_bytes](bytes.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_bytes. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_bytes when an application needs: Binary-safe, growable buffers and non-owning byte-range views. Buffer owns its storage; view borrows bytes and never frees them. A buffer may have a nonzero internal offset after consuming bytes, allowing inexpensive front removal before a later compaction or growth. The optional length limit constrains logical data, not necessarily previously reserved backing capacity. Do not retain a view across mutations that can move/reallocate storage.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
