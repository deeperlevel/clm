# clm_bpa

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Process-global ID-indexed storage for typed blocks and growable arrays. Architecture: callers hold numeric IDs rather than owning each heap allocation. Blocks and arrays have distinct open-addressed registries; small payloads use slab pools backed by arenas, while larger storage falls back to the heap. Ownership, reference counts, pointer invalidation, sealing and cursor/deck semantics must be understood before using advanced/borrowed APIs. IMPORTANT: Unlike arena's registry, BPA registries are not internally locked.

## Behavior and constraints

Process-global ID-indexed storage for typed blocks and growable arrays.
Architecture: callers hold numeric IDs rather than owning each heap allocation.
Blocks and arrays have distinct open-addressed registries; small payloads use
slab pools backed by arenas, while larger storage falls back to the heap.
Ownership, reference counts, pointer invalidation, sealing and cursor/deck
semantics must be understood before using advanced/borrowed APIs.
IMPORTANT: Unlike arena's registry, BPA registries are not internally locked.

## Integration

- C reference: [**clm_bpa**](../c/bpa.md).
- Related CLM declarations in this archive: `clm_arena`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
