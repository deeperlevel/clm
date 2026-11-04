# CLM Arena

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

`clm_arena` manages **ID-addressed, growable bump arenas**: contiguous regions of memory in which allocations are identified by **stable byte offsets** rather than individually owned addresses. Its optional **borrow/return lease** mechanism grants temporary direct access to the backing storage while preventing normal arena operations from invalidating that storage.

The module provides a foundation for efficient temporary allocations, bulk data construction, and internal storage services such as the slab pools used by `clm_bpa`.

## Capabilities and operations

- Create and destroy arenas under caller-supplied CLM identifiers.
- Allocate zero-initialized, aligned byte ranges with stable offsets.
- Grow reserved capacity without changing the amount of allocated data.
- Reset an arena or rewind it to a previous byte offset without releasing capacity.
- Inspect current used size and reserved capacity.
- Borrow the contiguous backing storage for repeated direct access without repeated ID lookups.
- Track live leases and reject operations that would invalidate or repurpose borrowed memory.
- Allow implementations to use native memory-management techniques while preserving the observable arena and lease contract.

## Detailed module contract

## 1. Purpose and scope

An **arena** is a region of storage managed as one resource. Each successful allocation advances an internal cursor; unlike a general-purpose allocator, this module does **not** individually free or resize allocations. Storage can instead be reclaimed in bulk by `rewind`, `reset`, or `destroy`.

Two access styles are available:

1. **ID-based management:** creation, allocation, reservation, inspection, reset, rewind, and destruction take an arena ID.
2. **Explicit borrowing:** an application temporarily receives a view of the arena's backing storage. During a live borrow, the arena is pinned against operations that could invalidate or repurpose that view.

The lease mechanism is a **cooperative runtime safeguard**, not a language-level proof of memory safety or an exclusive/mutable borrow checker. In particular, it does not automatically detect aliasing or concurrent writes through borrowed views.

This document specifies **observable behavior**. The C implementation's pointer representation, registry structure, locks, allocation functions, and token representation are not mandatory implementation choices for other languages.

### Conceptual relationship

```text
clm_base
    |
    v
clm_arena  (ID-addressed bump storage + temporary leases)
    |
    +----> clm_bpa  (C implementation: backing storage for small-object slabs)
    |
    +----> other modules/applications that benefit from arena allocation
```

`clm_arena` is an independently usable module. An application does not have to use BPA in order to use arenas.

## 2. Resource and storage model

| Term | Meaning |
|---|---|
| **Arena ID** | Caller-supplied `clm_id` identifying one live arena in the arena registry. |
| **Used size** | Byte position immediately after the most recently allocated range, including any alignment padding. |
| **Capacity** | Number of bytes currently reserved in the arena's contiguous backing storage. |
| **Allocation offset** | Byte displacement relative to the arena base at which one successful allocation begins. |
| **Alignment** | Required address boundary for the beginning of an allocation. |
| **Borrowed view / lease** | Temporary handle containing access to the arena's backing storage and information required to return the borrow. |
| **Generation** | Lifetime discriminator used by the current C implementation when validating a returned lease. |
| **Borrow token** | Unique identifier for an individual outstanding borrow in the current C implementation. |

An arena begins with `used_size = 0` and a positive reserved capacity. The reference C implementation chooses **4,096 bytes** when the caller requests an initial capacity of zero; that default is a reference value, not a required allocator strategy for every language.

The arena's ID remains stable across internal growth. An **allocation offset** remains meaningful relative to its arena as long as that allocation has not been discarded by a rewind, reset, or destruction. Growth may move the physical backing buffer, so an offset is **not** itself a permanent raw pointer.

The registry rejects duplicate live arena IDs. Destroying an arena removes its registration and releases its storage. A later explicit creation may reuse the same numerical ID; old offsets must not be treated as belonging to the newly created arena.

### Offset versus address

```text
Arena backing storage
[padding][allocation A][padding][allocation B][unused capacity...]
         ^                      ^
       offset A               offset B

Physical address of allocation = current arena base + allocation offset
```

Clients should store offsets for long-lived references to logical allocations and obtain a fresh borrowed base address when direct access is needed. A caller must still validate that the desired byte range belongs to an allocation that remains live.

## 3. Public operations

The following are **language-neutral conceptual signatures**. A specific binding may use output arguments, results/tuples, exceptions, or other idiomatic constructs while preserving the described outcomes.

| Operation | Intended behavior |
|---|---|
| `arena.create(id, initial_capacity)` | Register a new, empty arena with requested initial capacity. |
| `arena.destroy(id)` | Destroy a live arena if no borrow is outstanding. |
| `arena.allocate(id, byte_size, alignment)` | Allocate a zero-initialized range and return its starting byte offset. |
| `arena.reserve(id, requested_capacity)` | Ensure capacity is at least the requested number of bytes. |
| `arena.reset(id)` | Set used size to zero while retaining capacity. |
| `arena.rewind(id, offset)` | Set used size to an earlier valid offset, discarding later allocations. |
| `arena.get_info(id)` | Return current used size and capacity. |
| `arena.borrow(id)` | Acquire a tracked lease providing direct access to the backing storage. |
| `arena.return(lease)` | Release one live lease and invalidate/clear its handle. |

### 3.1 Create and destroy

```text
arena.create(ARENA_ID, initial_capacity=8192)
arena.destroy(ARENA_ID)
```

Creation allocates an empty arena; it does **not** create a first user allocation. Creation of an already registered ID fails without replacing the existing arena.

Destroying an unknown ID fails. Destroying an arena with **any live lease** fails with a busy outcome, leaving the arena intact. A successful destruction releases arena-owned storage and invalidates all of its offsets and any pointers previously obtained from it.

### 3.2 Bump allocation

```text
(status, offset) = arena.allocate(ARENA_ID, byte_size=64, alignment=8)
```

The allocator computes the necessary padding from the current used size, returns the aligned start offset, and advances the used size by padding plus payload length. Successful allocations are initialized to zero, including newly traversed padding bytes in the current C implementation.

- `byte_size` must be positive.
- Alignment must be valid for the binding; **zero requests the default alignment**.
- The C implementation accepts **power-of-two alignments up to `alignof(max_align_t)`**. Larger/over-aligned requests are not supported by this source snapshot.
- The operation may grow capacity when necessary; growth does not change earlier allocation offsets.
- Allocation fails if any lease is active **even if the new allocation would fit within already reserved capacity**. This is the current reference contract.
- Invalid parameters, arithmetic overflow, or allocation failure must not advance the used-size cursor or publish a partial successful allocation.

An arena is a bump allocator: it does not provide individual `free(offset)` or per-allocation reference counting.

### 3.3 Reserve

```text
arena.reserve(ARENA_ID, requested_capacity=16384)
```

`reserve` ensures that capacity is **at least** the requested size. It does not alter used size or invalidate logical allocation offsets. The reference implementation grows capacity geometrically where practical.

If the request requires backing-storage growth while leases are active, it fails with `BUSY`. **A no-op reserve that is already satisfied may succeed while borrowed**, because it does not move or change the storage. Capacity is not reduced by `reserve`.

### 3.4 Reset and rewind

```text
arena.rewind(ARENA_ID, saved_offset)
arena.reset(ARENA_ID)
```

- `rewind` changes the used size to an offset from `0` through the current used size, inclusive.
- `reset` is equivalent to discarding all arena allocations and setting used size to zero.
- Neither operation reduces reserved capacity.
- Both reject modification while **any lease** is outstanding.
- `rewind` rejects an offset greater than the current used size.

The offset supplied to `rewind` is a **byte cursor**, not an allocation ID or guarantee of allocation-boundary validity. Callers are responsible for remembering meaningful checkpoints. Rewinding into an allocation can invalidate part of that allocation.

**Discarded memory is not promised to be erased immediately.** In the C reference implementation, reset/rewind modify the used-size cursor; bytes are zeroed when a later successful allocation reuses the region. Do not rely on reset as secure data erasure.

### 3.5 Inspect state

```text
(size, capacity) = arena.get_info(ARENA_ID)
```

Reports the current used size and capacity without changing either. In the C interface both output locations are required, and their values are unchanged when the operation fails. Inspection is permitted while leases are active.

## 4. Borrowing and returning leases

Borrowing is the key optimization and lifetime-safeguard mechanism of this module.

```text
lease = arena.borrow(ARENA_ID)

// Repeated access uses the borrowed base, not another arena-ID lookup.
write_bytes(lease.data, offset, bytes)

arena.return(lease)
```

A successful borrow provides a view containing, conceptually:

| Lease field | Meaning |
|---|---|
| `id` | Borrowed arena's resource ID. |
| `data` | Direct base address/view of the backing storage. |
| `size` | Used-size snapshot at the moment of borrowing. |
| `capacity` | Capacity snapshot at the moment of borrowing. |
| `generation` | Arena lifetime discriminator in the C reference implementation. |
| Private owner/token | Internal lease identity and validation data; not application payload. |

The direct view avoids repeated registry lookups during repeated access. The caller must use only valid allocation ranges; a capacity snapshot does **not** grant permission to treat every reserved byte as a committed allocation.

### 4.1 Borrow rules

- Borrowing an unknown arena fails.
- Multiple leases on the same arena are permitted and independently tracked.
- Acquiring a new lease does **not** request exclusive writer ownership and does not make existing views read-only.
- Every successful borrow must be matched with **exactly one** successful return.
- Lease objects must **not be copied** as independently owned leases; passing a view or copying an underlying pointer does not register an additional borrow.
- Once returned, the direct view must no longer be accessed. In the C implementation, a successful return zeroes the lease structure.
- Invalid or stale return attempts fail rather than releasing some other lease.

### 4.2 Operations while borrowed

| Operation | With one or more active leases |
|---|---|
| `borrow` again | Allowed; new independent lease. |
| `get_info` | Allowed. |
| `allocate` | **BUSY**, including allocations that would fit. |
| `reset` / `rewind` | **BUSY**. |
| `destroy` | **BUSY**. |
| `reserve` requiring growth | **BUSY**. |
| `reserve` with no capacity change | Allowed in the C reference implementation. |
| Read/write through a valid borrowed view | Allowed; synchronization and bounds are the caller's responsibility. |
| `return` one valid lease | Allowed; other leases remain active. |

An arena becomes eligible for normal mutation only after **its final outstanding lease** has been returned. An implementation should not forcibly destroy or move borrowed storage through the checked arena API.

### 4.3 What the safeguards do not prove

This is **not** a strict borrow checker. The checked API cannot, by itself, prevent:

- Writing out of bounds or into unallocated portions of the backing buffer.
- Multiple borrowed views writing to the same bytes without synchronization.
- Keeping a copied raw pointer after the lease has been returned.
- Forging or modifying memory in unsafe environments such as C.
- Using an old allocation offset after reset/rewind and a subsequent allocation reuses it.

The main guarantee is **storage pinning for the duration of registered leases**, not universal pointer or data-race safety.

## 5. Results and error behavior

The current C reference exposes the following result categories and numeric constants. Other languages may use native error representations, but should preserve the distinctions that application code can observe.

| Result | C value | Meaning |
|---|---:|---|
| `SUCCESS` | `0` | Completed normally. |
| `FAILURE` | `-1` | Unclassified failure (including exhausted internal generation/token sequence). |
| `INVALID_ARGUMENT` | `-2` | Invalid size, alignment, output location, or lease argument. |
| `NOT_FOUND` | `-3` | Arena does not exist, or returned lease no longer matches a live arena lifetime. |
| `ALREADY_EXISTS` | `-4` | Arena creation used an existing ID. |
| `NO_MEMORY` | `-5` | Backing storage, arena metadata, or lease-tracking allocation failed. |
| `BUSY` | `-6` | A live lease prevents an operation that would modify, move, or destroy storage. |
| `OUT_OF_RANGE` | `-7` | Rewind offset outside used size, or allocation-size arithmetic overflow. |

`allocate` returns a starting offset on success. `borrow` returns a live lease on success. `get_info` returns size/capacity; `return` releases the handle. In the C reference implementation, an unsuccessful `allocate` leaves its output offset unchanged and an unsuccessful `get_info` leaves its outputs unchanged.

## 6. Lifetime, ownership, and synchronization

**Arena ownership:** creation establishes one arena-owned backing allocation. The arena manager retains ownership until successful destruction. Borrowing does not transfer ownership to the caller.

**Lease lifetime:** a lease starts when `borrow` succeeds and ends when its matching `return` succeeds. The caller must arrange to return all leases before destroying an arena or invoking an operation that requires mutation.

**Registry synchronization:** the C reference implementation protects its process-global arena registry and arena metadata with a single atomic-flag spinlock. This serializes its checked operations, including borrow/return and allocation. The current lookup uses a linked list and is linear in the number of registered arenas.

**Borrowed-data synchronization:** the registry lock is **not held while application code uses a borrowed pointer**. The module does not serialize application reads/writes through leases. Callers must provide appropriate synchronization when borrowed memory is shared between threads.

Other language ports may use a different registry, locks, native slices, reference objects, or allocator, but they must preserve the observable busy/pinning behavior and the documented concurrency limitations.

## 7. Relationship to `clm_bpa`

The current C `clm_bpa` implementation uses `clm_arena` internally **only to back its small-object slab pools**:

1. BPA creates a private arena for a new size-class slab.
2. It allocates the slab's complete storage from that arena.
3. It borrows the arena once and retains the lease while the slab exists.
4. It performs normal free-list allocations and releases directly within the borrowed memory, without repeating arena ID lookups.
5. At pool cleanup, it returns the lease and destroys the private arena.

In the current C snapshot, BPA uses 10 pool classes from **8 to 4,096 bytes**, and each slab contains **32 slots**. Larger blocks, growable BPAA array backing stores, and various metadata still use general-purpose heap allocations.

This is an **internal storage strategy**, not a requirement that every language port of `clm_bpa` allocate physical slabs or use `clm_arena` under the hood. See [clm_bpa](clm_bpa.md) for BPA's public typed-block and array semantics.

**Important distinction:** `arena.borrow` creates a tracked, pinning lease. BPA's `bpa.borrow` and advanced BPAA pointer/view operations have **different, weaker** invalidation rules; do not assume they acquire arena leases for user-visible BPA allocations.

## 8. Language adaptation guidance

| Language family | Possible internal representation | Contract requirement |
|---|---|---|
| C / C++ | Contiguous allocator buffer, offset arithmetic, checked lease registry | Preserve alignment, offsets, pinning, checked failures, and return semantics. |
| D / Odin / Zig / Rust / C3 | Native allocator and views/borrows, with explicit lease tracking where necessary | Preserve stable offsets and observable busy behavior rather than relying solely on language-local lifetimes. |
| Pascal | Contiguous storage and explicit registry/lease records | Preserve the same allocation and return rules. |
| Python / Ruby / Go | Byte buffers and managed handles; pin/copy or disallow resize during borrow as appropriate | Expose equivalent logical views, offset access, and mutation restrictions. |

A port can add idiomatic RAII, scoped guards, context managers, or automatically released leases as conveniences, but it should still support the documented ownership outcomes. A garbage collector does not by itself fulfill the **active lease prevents mutation/destruction** rule.

**Cross-language guarantee:** equivalent lifetime, offset, alignment, and error behavior, not identical raw pointer types or allocator performance. Host-language ownership safety may be stronger than the C reference implementation.

## 9. Example workflow (language-neutral pseudocode)

```text
ARENA = 0xA001

check(arena.create(ARENA, initial_capacity=256))

(status, header_offset) = arena.allocate(ARENA, byte_size=16, alignment=8)
check(status)

(status, payload_offset) = arena.allocate(ARENA, byte_size=64, alignment=8)
check(status)

// Arena offsets are stable; the physical base address need not be.
lease = check(arena.borrow(ARENA))

write_bytes(lease.data, header_offset, header_bytes)   // 16 bytes maximum
write_bytes(lease.data, payload_offset, payload_bytes) // 64 bytes maximum

assert(arena.reset(ARENA) == BUSY)
check(arena.return(lease))

// The old allocations are now discarded together.
check(arena.reset(ARENA))
check(arena.destroy(ARENA))
```

`write_bytes` and `check` are explanatory pseudocode helpers, not CLM APIs. In particular, `clm_arena` does **not** expose a separate `arena.get(id, offset)` operation in this snapshot. Direct access is provided through a borrowed view.

## 10. Suggested portable conformance tests

| Group | Minimum checks |
|---|---|
| Identity | Create, duplicate ID rejection, unknown-ID rejection, destroy and explicit ID reuse. |
| Initial state | Used size zero; capacity positive; zero initial-capacity default behavior as documented by the binding. |
| Allocation | Positive length, zero-initialization, alignment, cursor advancement, stable offsets across growth. |
| Invalid allocation | Zero size, invalid alignment, overflow, output/result unchanged on failure. |
| Reserve | No-op reserve, growth, used size unchanged; no capacity reduction. |
| Reset/rewind | Cursor reset, valid/invalid rewind, retained capacity, zeroed bytes when a discarded range is allocated again. |
| Borrow/return | Obtain base view; multiple leases; independent returns; zero/invalidated returned handle. |
| Pinning | Reject allocate, reset, rewind, destroy, and growth while borrowed; succeed after final return. |
| Return validation | Invalid return, second return, stale or mismatched handle rejected. |
| Synchronization | Checked registry operations under concurrent usage; caller-coordinated accesses to shared borrowed storage. |
| BPA integration | Pooled slab memory is valid until cleanup; lease returned before its arena is destroyed. |

Native implementations should additionally run memory instrumentation and stress tests around create/borrow/return/destroy cycles. Performance benchmarks should distinguish **one-time lease acquisition** from repeated direct accesses made under an already-acquired lease.

## 11. Development notes and open design questions

The following are not promises of the current module and should be considered separately if the contract evolves:

1. Over-aligned allocations (for example, alignments greater than the host's fundamental maximum alignment).
2. A hash-based or otherwise scalable registry in place of the C reference's linked list.
3. Separate shared/read-only and exclusive/writable lease modes.
4. Scoped or automatic lease-return conveniences in languages that support them.
5. Arena-local object destructors, individual deallocation, or independently resizable blocks (none currently supplied).
6. Secure clearing of discarded data, which `reset` and `rewind` do not guarantee.
7. Namespace/context isolation versus the current process-global C registry.

**Design principle:** use CLM IDs and stable offsets for resource management; borrow a view only when direct access is justified; return it before attempting to alter the arena's allocation lifetime.

---

*This is a language-neutral development reference based on the supplied 2026-11-01 `arena.h` and `arena.c`, not a guarantee of finalized CLM 1.0 APIs.*

## Shared behavioral contract

- CLM resource IDs identify managed resources; IDs alone do not keep objects alive.
- Preserve observable create/allocate/borrow/return/reset/rewind/destroy behavior across language ports.
- Use native storage, error, and lifetime mechanisms where appropriate without silently weakening the checked lease contract.
- Binding-specific signatures, memory representations, packaging, and source layout belong to each language implementation.

## Example use cases

Fast scratch allocation with bulk reset; offset-stable storage during buffer growth; temporary direct access for native processing; backing storage for reusable slab pools.
