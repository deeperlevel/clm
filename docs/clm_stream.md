# CLM stream

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_stream](stream.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bytes`](bytes.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_stream. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.
- [clm_bytes](bytes.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_stream when an application needs: Byte sources, sinks, adapters, and incremental copying.

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
