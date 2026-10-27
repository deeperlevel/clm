# CLM - Block Pool Allocation

## 1. Purpose and scope

`clm_bpa` is CLM's foundational managed-allocation module. It offers two facilities:

- **BPA — Block Pool Allocation:** creates and manages individually addressable blocks. The caller uses a **block ID**, not a permanent memory address.
- **BPAA — Block Pool Array Allocation:** creates and manages arrays of elements with one fixed byte width per array. Each array has an **array ID**, a count, a reserved capacity, and optional traversal state.

BPA/BPAA are *storage mechanisms*, not virtual objects or multidimensional data models. `clm_vos` may use BPA/BPAA for typed virtual objects and AVO (Arrays of Virtual Objects); `clm_storage` may use BPAA for rows, columns, and layers. Neither is part of `clm_bpa`, and neither is required to use it.

The module aims to reduce duplicated resource-management code in implementations of `clm_container`, `clm_storage`, `clm_depend`, and other CLM modules. **Use is optional:** applications and higher-level language ports can use native memory and collections when that is more appropriate, provided they preserve the public behavioral contract of the modules they expose.

The D port exposes `ClmBpaBlockNode`, `ClmBpaaArrayNode`, and `ClmBpaPoolClass`.
Their C-style names (`clm_bpa_block_node`, `clm_bpaa_array_node`, and
`clm_bpa_pool_class`) remain available as compatibility aliases.

### Conceptual dependency

```text
clm_base
   ↑
clm_bpa
   ├── BPA  — individually managed blocks
   └── BPAA — structure-agnostic managed arrays
   ↑
   ├── clm_vos      (optional virtual objects and AVO)
   ├── clm_storage  (2D / 3D organization)
   └── other modules, where useful
```

Arrows signify possible implementation dependencies, not compulsory dependencies for every language.

## 2. Identity and resource model

Every live block and every live array is registered under a caller-supplied `clm_id`. The caller must select an unused ID **within that resource registry** before creation. Block and array registries are distinct in the current C implementation; the same numeric ID can therefore name one block and one array simultaneously. Callers must know which resource kind an API expects.

- **Block ID:** identifies one BPA-managed allocation.
- **Array ID:** identifies an entire BPAA-managed array, not an individual element.
- **Array index:** zero-based position within an array; it is not a CLM object ID.
- **Physical address:** an internal implementation detail and may change independently of the ID.

Creating a resource fails if its ID is already registered in the applicable registry. Looking up an absent or deleted ID fails. Deleted IDs may be reused by a subsequent *explicit* creation; callers must not keep treating a deleted ID as a live resource. ID registration by itself does **not** provide generation-based protection against stale IDs that have been manually reused.

Resources created through BPA/BPAA are owned by the respective manager until released, explicitly deleted, or destroyed during module shutdown.

The registries are separate. `bpa.is_existing(block_id)` checks only the block registry, while `bpaa.is_existing(array_id)` checks only the array registry; the same numeric ID may exist in both and each query reports only its own resource kind.

## 3. Type and size model

The current C implementation recognizes the following type categories. These are **semantic type names** for cross-language documentation; implementations may use language-native enums or aliases.

| Type | Intended value | Fixed width |
|---|---|---:|
| `STRUCT` | Arbitrary fixed-width byte record | Supplied at creation |
| `BYTE` | 8-bit value | 1 byte |
| `SHORT` | 16-bit value | 2 bytes |
| `INT` | 32-bit value | 4 bytes |
| `LONG` | 64-bit value | 8 bytes |
| `FLOAT` | 32-bit floating-point | 4 bytes |
| `DOUBLE` | 64-bit floating-point | 8 bytes |
| `STRING` | Encoded text | Variable |
| `UUID` | UUID representation | 16 bytes |
| `DATETIME` | Datetime representation | 8 bytes in the current C implementation |

For named fixed-width types, the element size is derived from the type; `STRUCT` requires an explicit positive element size. The current BPA/BPAA C type tags **distinguish signed and unsigned integer values** of the same width. Exact signedness, UUID byte order, datetime units/timezone semantics, and permitted string encodings should be specified separately before those types are treated as portable serialized representations.

**Binary compatibility:** Two implementations may offer equivalent APIs without having identical native object layouts. Raw structure bytes should not be transferred between languages, machines, or ABIs without a separate serialization contract. For C++ and similar languages, arbitrary byte copying is appropriate only for representations for which bytewise copying is valid; complex objects may require construction/destruction-aware handling.

## 4. Operation results

The current C interface uses these result constants:

| Result | Numeric value | Meaning |
|---|---:|---|
| `FAILURE` | `-1` | Invalid operation or failed allocation/access |
| `SUCCESS` | `0` | Successful general operation |
| `GET_SUCCESS` | `1` | Successful data retrieval |
| `SET_SUCCESS` | `2` | Successful assignment |

Some text operations return a **nonnegative byte length** on success instead of the constants above. Implementations in other languages may represent a result as a tagged union, exception, tuple, result object, or native status type, provided success/failure and returned data remain unambiguous.

**Current limitation:** `FAILURE` groups several causes (missing ID, type mismatch, bounds error, allocation failure). Detailed cross-language error categories are not yet standardized. Do not claim that every failure can be distinguished until an error taxonomy is adopted.

## 5. BPA — managed individual blocks

### 5.1 Creation and allocation

**Conceptual operations:**

```text
bpa.create(block_id, type)
bpa.create_sized(block_id, STRUCT, element_size)
```

Creating a fixed-width block reserves space for one value/record, initially zero-filled in the C implementation. A string block initially has no text payload and acquires it through a string setter. Creating `STRUCT` without a positive size fails. A failed creation must not register a partially constructed block.

### 5.2 Reads, writes, and copies

```text
bpa.is_existing(block_id) → boolean
bpa.get_to(block_id, external_destination, byte_capacity)
bpa.set_from(block_id, external_source, byte_count)
bpa.get_string_to(block_id, external_destination, byte_capacity)
bpa.set_string_from(block_id, external_source, byte_count)
bpa.replace(block_id, external_source, byte_count)
bpa.copy_block(source_block_id, destination_block_id)
```

`is_existing` returns true only for a currently registered block. A matching array ID does not make it true.

For fixed-width blocks, reads copy the stored bytes; writes require the expected block size. `get_to` accepts a destination at least as large as the stored value. `set_from` requires an exact byte-count match in the current C implementation. Neither operation transfers ownership of the external buffer.

`replace` provides one operation for replacing either kind of BPA payload. For a fixed-width block it follows the exact-size assignment rules of `set_from`. For a string block, it copies the bounded source bytes into separately managed storage, adds a terminator when the source has none within the supplied byte count, and replaces the old value only after the new copy is ready. The current C function returns `SET_SUCCESS` for fixed-width blocks and the resulting string byte length for strings; failure is `FAILURE`. The old value remains intact if validation or allocation fails.

`copy_block` copies between two **already-created** blocks. Their type tags and fixed record widths must be compatible. For strings, the destination receives its own copy of the text. Managed-to-managed copying does not expose an internal pointer to application code.

The C header also retains `bpa.get`/`bpa.set` as native-buffer spellings; `get_to`/`set_from` are explicit aliases. This is an exception to the general BPAA convention where unsuffixed operations use managed IDs.

### 5.3 Reference counting and destruction

```text
bpa.ref(block_id)
bpa.unref(block_id)
bpa.delete(block_id)
```

- Creation starts with one reference in the current C implementation.
- `ref` increments the count unless it would overflow.
- `unref` decrements and automatically destroys the block when the count reaches zero; decrementing an absent or zero-reference block fails.
- **`delete` is explicit forced destruction.** It destroys the resource regardless of outstanding reference count. This is deliberate CLM policy, *not* an error in the reference-counting implementation.
- Every copied ID or borrowed address referring to a forcibly deleted resource becomes invalid.

References in this module are explicit counters, not automatically tracked language-level references. They do not detect reference cycles or prove that an external consumer has stopped using an ID.

### 5.4 Optional borrowed access

```text
bpa.borrow(block_id) → (status, temporary_address_or_view, size)
```

The C implementation offers a direct borrowed pointer for interoperability or performance-sensitive code. **Pooling does not make dangling pointers safe.** Deletion, replacement, shutdown, or any relevant relocation invalidates the borrow; a freed pool slot can be reused for another allocation.

Borrowing is an *optional, implementation-specific facility*, not a required operation for languages that do not expose native pointers. Safe-copy methods are the portable default.

## 6. Internal pool behavior

BPA may retain freed physical slots and reuse them for later allocations. An allocator is free to use slabs, size classes, free lists, native memory arenas, or equivalent language-runtime mechanisms. What must remain stable is **logical identity and ownership behavior**, not physical placement or allocator internals.

**Current C strategy (not a cross-language requirement):**

- Small individual payloads use reusable size-class pools with slots of 8, 16, 32, 64, 128, 256, 512, 1,024, 2,048, and 4,096 bytes (subject to native pointer-width alignment assumptions).
- A new slab contributes 32 slots to its size class.
- Released slots join a free list for reuse; slabs remain allocated until pool cleanup/shutdown.
- Payloads larger than the largest pooled class use the underlying general-purpose allocator.
- Block-node metadata and BPAA array backing allocations have their own allocation paths.
- Pool statistics and slab cleanup are implementation facilities; Python, Go, Ruby, and other managed runtimes need not emulate physical slabs.

Pooling can reduce repeated allocation costs but does **not** guarantee that every allocation is faster, that all blocks remain at permanent addresses, or that stale pointers remain readable.

## 7. BPAA — managed structure-agnostic arrays

### 7.1 Creation

```text
bpaa.create(array_id, type, reserved_capacity, encoding)
bpaa.create_sized(array_id, STRUCT, reserved_capacity, element_size, encoding)
bpaa.is_existing(array_id) → boolean
```

`is_existing` checks only the BPAA registry. It may return true for an ID that also exists in the separate BPA block registry; use `bpa.is_existing` to check a block.

Each BPAA array has:

| Property | Meaning |
|---|---|
| `id` | Logical array identity |
| `type` | Element representation/category |
| `block_size` | Fixed byte width for non-string elements |
| `count` | Number of committed elements |
| `capacity` | Number of elements accommodated without further capacity growth |
| `index` | Shared internal traversal cursor |
| `sealed` | Whether **capacity changes** are prohibited |
| `deck_size` | Number of entries remaining in the current deck draw cycle |
| `encoding` | String encoding metadata, where applicable |
| `refcount` | Explicit lifetime reference counter |

On creation, `count = 0` and `capacity` is the requested nonnegative reservation. In the C implementation, a positive capacity reserves actual array backing storage. A managed language may represent reserved capacity logically if its runtime does not expose physical reservation, but must preserve observable count/capacity behavior.

An array is **homogeneous in element width**, but the allocator does not need to understand the fields of an arbitrary structure. `STRUCT` arrays can hold plain fixed-width records such as CLM positions, sizes, blocks, or application-defined ECS components.

### 7.2 Managed-ID data transfer (preferred)

The **unsuffixed BPAA API** uses an already-existing BPA block ID as the source or destination:

```text
bpaa.get(array_id, index, destination_block_id, expected_size)
bpaa.set(array_id, index, source_block_id, expected_size)
bpaa.insert(array_id, index, source_block_id, expected_size)
bpaa.append(array_id, source_block_id, expected_size)

bpaa.get_first(array_id, destination_block_id, expected_size)
bpaa.get_last(array_id, destination_block_id, expected_size)
bpaa.get_current(array_id, destination_block_id, expected_size)
```

Rules:

1. The array ID and block ID must both resolve to live resources.
2. The array index must be within bounds. An insertion index may equal `count` to insert at the end.
3. For fixed-width elements, source/destination block size and `expected_size` must match the array element size.
4. Type categories must be compatible. The current C implementation permits matching non-`STRUCT` type tags, and uses a size-based rule when the array type is `STRUCT`.
5. The destination/source block is **not created automatically**. Allocate it explicitly before use.
6. Reads overwrite the destination block's value; writes copy data into the array. Neither operation reassigns the block ID or array ID.
7. `append` has no index argument; it adds after the last committed element.

### 7.3 External-buffer transfers

Functions ending in **`_to`** write to externally owned memory. Functions ending in **`_from`** read from externally owned memory:

```text
bpaa.get_to(array_id, index, external_destination, byte_capacity)
bpaa.set_from(array_id, index, external_source, byte_count)
bpaa.insert_from(array_id, index, external_source, byte_count)
bpaa.append_from(array_id, external_source, byte_count)

bpaa.get_first_to(array_id, external_destination, byte_capacity)
bpaa.get_last_to(array_id, external_destination, byte_capacity)
bpaa.get_current_to(array_id, external_destination, byte_capacity)
```

These permit ordinary native variables, buffers, and foreign-library APIs without requiring a BPA block for every local scalar. The implementation checks reported sizes; in native languages it cannot generally prove that an arbitrary non-null address is truly valid for that many bytes. A high-level language may expose ordinary values or result objects rather than literal memory pointers.

**Convention across CLM modules:**

- **No suffix on BPAA data transfers:** CLM-managed ID source or destination.
- **`_to`:** destination supplied by the caller.
- **`_from`:** source supplied by the caller.

This is a *direction convention*, not a demand that every language use identical pointer-based signatures.

### 7.4 BPAA slicing

The C API offers two independent-copy operations and a non-owning borrowed view (zero-based **element** indices):

```text
bpaa.slice(source_array_id, start, length, destination_array_id)
bpaa.slice_to(source_array_id, start, length, destination_buffer, byte_capacity)
bpaa.slice_borrow(source_array_id, start, length, &buffer, &byte_size)
```

- **`slice`** creates a **new** BPAA resource. Its destination ID must not already exist in the BPAA registry. The new array inherits the source type, fixed item width (for `STRUCT`), and encoding metadata, and starts with `count = capacity = length`, cursor `0`, and a freshly reset deck. For a zero-length slice, count and capacity are zero.
- Fixed-width elements are bulk-copied. For **string BPAA arrays**, `slice` deep-copies each NUL-terminated element; deleting or editing the source does not affect the destination. If a copy/allocation fails, the partial destination is deleted.
- **`slice_to`** copies only **fixed-width, non-string** element ranges to externally owned memory. `buffer_size` is the available number of **bytes**; `start` and `length` are measured in **elements**. Overlapping source/destination memory is supported using move semantics. The destination buffer is not owned or freed by BPAA.
- **`slice_borrow`** returns a direct pointer into a fixed-width, non-string array and reports the range size in bytes. It does not copy or retain the source array. Keep the source live and do not mutate it while using the view; delete, shutdown, capacity changes, or item writes/reordering can invalidate the pointer or change the viewed contents. A zero-length range succeeds with a null pointer and size zero.
- All variants reject negative positions/lengths, ranges outside `[0, count]`, or overflow. `start == count, length == 0` is valid. Slices do not alter source contents, source count, capacity, cursor, or deck state.
- The copied variants provide independent storage. `slice_borrow` is a risky, temporary view; use a copy variant when the result needs an independent lifetime. `slice_to` and `slice_borrow` support fixed-width arrays only, not string arrays.

Conceptual example:

```text
bpaa.slice(VALUES, start=1, length=3, destination_array_id=SUBSET)
bpaa.slice_to(VALUES, start=1, length=3, destination_buffer=native_buffer,
              byte_capacity=size_of(native_buffer))
```

### 7.5 Advanced Operations — RISKY

> **RISKY — expert use only.** These operations expose mutable internal
> metadata and memory. Incorrect use can corrupt array state, cause invalid
> memory access, or leave dangling pointers. Prefer the ordinary ID-based API
> unless you control the caller and can enforce the lifetime and invariants
> described below.

The C implementation provides this API for callers that need to avoid
repeated ID-registry lookups during bulk operations:

```text
clm_bpaa_advanced_borrow(array_id, &array)
clm_bpaa_advanced_release(&array)
clm_bpaa_advanced_data(array) → mutable_payload_or_NULL
clm_bpaa_advanced_get_raw(array, index, buffer, size)
clm_bpaa_advanced_set_raw(array, index, buffer, size)
clm_bpaa_advanced_insert_raw(array, index, buffer, size)
clm_bpaa_advanced_append_raw(array, buffer, size)
clm_bpaa_advanced_remove(array, index)
clm_bpaa_advanced_clear(array)
clm_bpaa_advanced_extend(array, additional_capacity)
clm_bpaa_advanced_shrink(array, new_capacity)
```

`advanced_borrow` returns the internal metadata pointer and increments the
array's reference count. An ordinary `unref` cannot destroy the array while a
borrow is held. Release each borrow exactly once with `advanced_release`; it
nulls the pointer passed by address and destroys the array if that was its last
reference.

The metadata pointer remains valid across backing-storage resizes, but is
invalidated immediately by explicit array deletion or module shutdown. Those
operations forcibly destroy arrays regardless of outstanding borrows; never
use or release a pointer after either operation. The payload pointer returned
by `advanced_data` has a shorter lifetime: any operation that changes capacity
may relocate it. Reacquire the payload pointer after `extend`, `shrink`, or an
insert/append that grows the array.

These direct data operations support fixed-width arrays only. For string
arrays, `advanced_data` returns `NULL` and the raw element helpers fail; string
storage is separately allocated and must be modified through the string-array
operations to preserve ownership.

The metadata pointer and fixed-width payload are mutable. Direct payload writes
do not update traversal metadata; callers that directly change `count`, cursor,
or deck fields must preserve every array invariant themselves. Prefer the
pointer-based helpers when possible because they update the associated
metadata. Do not edit registry identity, type, reference count, capacity, or
backing-storage fields directly. A raw pointer cannot be checked for staleness
or structural corruption, and these APIs do not make BPA thread-safe. They
are unsafe for untrusted callers and do not provide Pydantic-like runtime
validation; use the ordinary ID-based interface by default.

### 7.6 String elements

String arrays use string-specific operations:

```text
bpaa.get_string(array_id, index, destination_string_block_id)
bpaa.set_string(array_id, index, source_string_block_id)
bpaa.insert_string(array_id, index, source_string_block_id)
bpaa.append_string(array_id, source_string_block_id)

bpaa.get_string_to(array_id, index, external_destination, byte_capacity)
bpaa.set_string_from(array_id, index, external_source, byte_count)
```

The corresponding external insertion/append operations use the general `insert_from` and `append_from` functions in the current C API. String destinations must be registered as BPA string blocks. String transfers copy contents rather than sharing ownership of the same underlying character buffer.

**BPA/BPAA storage behavior:** strings are NUL-terminated byte sequences stored as separately allocated copies; the array holds a sequence of string pointers. A read needs space for the terminating NUL. The low-level BPA/BPAA operations do not impose an encoding or Unicode indexing rule. The higher-level `clm_string` module defines its own encoding and character-index semantics. Embedded NUL is the end of the string in the current C implementation.

### 7.7 Array metadata queries

| Managed-ID operation | External destination | Result |
|---|---|---|
| `get_count(array_id, block_id)` | `get_count_to(array_id, destination)` | Committed element count |
| `get_capacity(array_id, block_id)` | `get_capacity_to(array_id, destination)` | Reserved capacity |
| `get_length(array_id, block_id)` | `get_length_to(array_id, destination)` | Length of string at current cursor |
| `get_current_index(array_id, block_id)` | `get_current_index_to(array_id, destination)` | Current cursor index |
| `is_sealed(array_id, block_id)` | `is_sealed_to(array_id, destination)` | Capacity-seal flag |

Managed metadata results require a **preallocated BPA destination**. In the current C implementation, count/capacity/length require the 32-bit `UNSIGNED_INT` block category; the cursor index requires `INT`, and sealed requires `BYTE`. Metadata getters fail if their source array or destination block is invalid or incompatible.

`get_length` applies to string arrays and uses the current cursor rather than accepting an index. An invalid cursor or empty array causes failure.

### 7.8 Capacity, insertion, and removal

```text
bpaa.extend(array_id, additional_capacity)
bpaa.shrink(array_id, new_capacity)
bpaa.insert(...) / bpaa.append(...)
bpaa.remove(array_id, index)
bpaa.remove_first(array_id)
bpaa.remove_last(array_id)
bpaa.clear(array_id)
```

- `extend` increases **capacity**, not count.
- `shrink` lowers capacity but cannot discard committed elements; it rejects a capacity below `count`.
- `insert` shifts following elements to the right and increases `count`.
- `append` inserts at `count` and increases `count`.
- `remove` deletes the selected element, shifts trailing elements left, and decreases `count`.
- `clear` removes all committed elements while retaining reserved capacity.
- Automatic growth occurs when permitted and required for insertion/append.
- Internal reallocations may relocate the array payload without changing its array ID.

**Safety rule:** If source data may alias the array's backing allocation, insertion must snapshot the source before any reallocation or element shifting. On failed growth, existing elements and count must remain intact. In the current C implementation, the non-string external insert path takes a temporary snapshot to avoid alias invalidation.

### 7.9 Traversal and cursor

```text
bpaa.reset(array_id)
bpaa.move_first(array_id)
bpaa.move_previous(array_id)
bpaa.move_next(array_id)
bpaa.move_last(array_id)
bpaa.move_to(array_id, index)
bpaa.get_current(...)
```

Each array has **one shared mutable cursor** in the current design. This is useful for sequential traversal but **not** an independent iterator per consumer. `move_next`/`move_previous` fail at bounds; `move_to` requires an existing index. `reset` and `move_first` currently set the cursor to zero, even for an empty array; attempting to retrieve a current value from an empty array fails.

Inserting, removing, sorting, and other transformations can update cursor state. Cross-language consumers should not assume cursor stability across mutation. Concurrent or interleaved independent traversals of a single array require a future explicit iterator abstraction, or external coordination.

### 7.10 Sealing

```text
bpaa.seal(array_id)
bpaa.unseal(array_id)
bpaa.is_sealed(array_id, destination)
```

**Sealed means capacity-fixed, not immutable.** A sealed array rejects capacity growth, extension, and shrinking. Element updates, rearrangements, removals, clearing, and appending within existing reserved capacity can still be allowed. This is not an access lock or synchronization primitive.

### 7.11 Rearrangement and comparisons

```text
bpaa.sort_with(array_id, comparator)
bpaa.sort(array_id, predicate)
bpaa.reverse(array_id)
bpaa.shift_left(array_id, start_index, positions)
bpaa.shift_right(array_id, start_index, positions)
bpaa.swap(array_id, index_a, index_b)
bpaa.copy(array_id, source_index, destination_index)
bpaa.shuffle(array_id)
```

- `sort_with` sorts using a caller-provided comparator; the comparator must define the intended order and obey the hosting language's sort contract.
- `sort` is implemented as bubble sort. Its boolean predicate returns `true` when an adjacent pair should be exchanged; `sort_with` uses a three-way comparator with `qsort` in C.
- `reverse`, shift/rotate, swap, copy, and shuffle currently apply to non-string arrays.
- Shift operations rotate the **suffix starting at the supplied index**, rather than leaving holes or changing the count.
- `copy` overwrites another existing index; it does not insert or change count.
- `shuffle` rearranges values in place. The current C code uses its standard pseudorandom generator and does not promise unbiased cryptographic randomness.

### 7.12 Deck-style drawing

```text
bpaa.draw(array_id, destination_block_id, expected_size)
bpaa.draw_at(array_id, index, destination_block_id, expected_size)
bpaa.draw_to(array_id, external_destination, byte_capacity)
bpaa.draw_at_to(array_id, index, external_destination, byte_capacity)
```

BPAA supports a **deck state** using `deck_size`:

1. The current deck consists of a prefix of the array, initially of length `count`.
2. `draw_at` obtains the selected entry and swaps it with the last undrawn entry, then decrements `deck_size`.
3. A `draw` takes a value from the active deck. When depleted, the current C implementation shuffles the full array and starts another cycle.
4. Drawing **does not remove committed array elements** and does not reduce `count`.
5. Array mutations and rearrangements can reset the deck cycle.

The current implementation excludes string arrays from deck operations. For a single cycle, elements are drawn without replacement; subsequent cycles can return previously drawn elements again.

## 8. Lifecycle and cleanup

### 8.1 Arrays

```text
bpaa.ref(array_id)
bpaa.unref(array_id)
bpaa.delete(array_id)
```

Array reference counting mirrors individual BPA blocks. Arrays begin with one reference. `unref` destroys at zero. Explicit `delete` forcibly destroys even if the count is greater than zero. For string arrays, destruction releases owned string payloads and the array's backing storage.

### 8.2 Initialization

**No mandatory explicit initialize call exists in the current C API.** Registries and pool classes are initialized lazily on first use. Other languages can construct their internal registries lazily as well, unless a later module specification introduces an explicit context.

### 8.3 Shutdown

```text
bpa.shutdown()
```

`shutdown` force-releases outstanding BPA blocks and BPAA arrays, their ID registries, and internal pooled slabs, and resets the module's global state. It intentionally ignores outstanding reference counts. Repeated shutdown is supported in the current C implementation, and a subsequent allocation may lazily establish a fresh registry.

Call shutdown **after** higher-level modules that depend on BPA have stopped. Borrowed pointers and live IDs become invalid. The current implementation is not thread-safe; no other thread should access the module while shutdown runs.

### 8.4 Pool-only maintenance (implementation facility)

`pool_cleanup()` and `pool_stats()` are supported by the current C implementation. Pool cleanup requires that no pooled allocations remain live; full shutdown is the preferred whole-module cleanup operation. These low-level operations should not be required by portable application code unless the particular language binding exposes equivalent pool controls.

## 9. Safety and performance contract

CLM favors verified resource identity and predictable access over guaranteeing the absolute lowest runtime overhead.

**Expected guarantees for checked operations:**

- Unknown or deleted IDs fail rather than silently accessing an arbitrary resource.
- Array indices are checked before element access.
- Source and destination widths are validated before copying.
- A failed allocation should not register partial resources or corrupt existing values.
- Managed transfers do not expose the internal address of an array payload.
- Logical IDs survive internal array growth and relocation.
- Explicit deletion and shutdown invalidate logical resources even when references remain.

**Important limits:**

- A native external pointer plus declared size cannot be proven valid merely from the signature.
- Borrowed pointers can become dangling after deletion/reuse or relocation.
- BPA IDs are not automatically generation-checked and can be explicitly reused.
- Internal reference counters do not provide thread synchronization.
- Array cursors and global registries are currently single-threaded facilities.
- A contiguous byte array is not necessarily suitable for C++ objects with nontrivial ownership.
- IDs and safety checks introduce lookup/copying overhead; direct/native operations may be faster in specific workloads.

None of these limitations invalidate the module's purpose; they define the level of safety the current design can actually provide.

## 10. Language adaptation guidance

| Language category | Plausible implementation | Portability requirement |
|---|---|---|
| C | Hash registries, slab/free-list pools, resizable byte arrays | Enforce ID, width, bounds, lifetime and result rules |
| C++ | Templates and aliases over native containers or custom pools | Restrict byte-copy pathways to suitable types; preserve IDs |
| D / Odin / Zig | Native allocators and collections or explicit pool implementations | Preserve ownership and checked resource behavior |
| Go | Maps, slices, and runtime-managed objects | Implement logical capacity/ID/lifetime semantics |
| Python / Ruby | Dictionaries, lists, byte buffers and native runtime memory | Preserve API behavior without simulating native pointers or slabs |

CLM's cross-language promise is **consistent operations and observable behavior**, not matching internal allocation algorithms. Typed wrappers, templates, or generics are encouraged where idiomatic, with aliases available for readability. A managed language may return an ordinary value where C needs an output pointer, provided the specification's error and copying behavior remains recognizable.

## 11. Suggested portable conformance tests

| Group | Minimum checks |
|---|---|
| Identity | Duplicate IDs rejected; absent IDs rejected; IDs can be reused after explicit deletion |
| Existence checks | Block/array existence queries distinguish separate registries even when numeric IDs match |
| Block lifecycle | Create, read, write, copy, ref, unref, forced delete, post-delete rejection |
| Block replacement | Exact-size replacement for fixed-width blocks; string replacement copies content and preserves old value on allocation failure |
| Pool behavior | Repeated allocate/release; physical reuse where applicable; live data unaffected |
| Arrays | Empty creation; reserved capacity; append/insert/remove; growth/shrink; clear |
| Data transfer | Managed ID and external-buffer variants; exact sizes; incompatible types; missing destination IDs |
| Aliasing | Insert from overlapping source; growth preserves source value; failed growth preserves data |
| Strings | Empty/nonempty text, replacement, copying, length, removal, cleanup |
| Cursor | First/previous/next/last/to, empty and out-of-range behavior |
| Seal | Prevent capacity changes; permit existing-capacity operations according to contract |
| Rearrangement | Bubble-sort predicate, sort comparator, reverse, rotation, swap, copy, shuffle |
| Slicing | Copy numeric/struct ranges to a new array or external buffer; deep-copy string arrays; invalid/empty ranges; source independence |
| Deck | Draw without replacement within a cycle; automatic refill/re-shuffle; count unchanged |
| Shutdown | Live resources released, repeated shutdown, subsequent fresh allocation |
| Robustness | Overflow/large-size cases, invalid arguments, allocator failures, thread-safety constraints |

For native C implementations, use sanitizers or comparable memory instrumentation in addition to assertions. A conforming higher-level implementation should test **the same behavior**, not the same memory addresses or pool statistics.

## 12. Cross-module usage examples (pseudocode)

### 12.1 Managed position records

```text
bpa.create_sized(POSITION_BUFFER, STRUCT, sizeof(PositionI32))
bpaa.create_sized(POSITIONS, STRUCT, reserved=128, element_size=sizeof(PositionI32))

bpa.set_from(POSITION_BUFFER, PositionI32(x=10, y=20))
bpaa.append(POSITIONS, POSITION_BUFFER, expected_size=sizeof(PositionI32))

bpaa.get(POSITIONS, index=0, destination_block_id=POSITION_BUFFER,
         expected_size=sizeof(PositionI32))
position = bpa.get_to(POSITION_BUFFER)

bpaa.delete(POSITIONS)
bpa.delete(POSITION_BUFFER)
```

`sizeof` is conceptual here: higher-level languages can use a structured value codec or a type descriptor instead of an address and native byte count.

### 12.2 Get array count without a managed block

```text
count = bpaa.get_count_to(ARRAY_ID)
```

The C implementation uses an external destination argument; managed languages may return `(status, count)` or equivalent.

### 12.3 Get array count into a managed block

```text
bpa.create(COUNT_BUFFER, INT)
bpaa.get_count(ARRAY_ID, COUNT_BUFFER)
bpa.get_to(COUNT_BUFFER, count_destination)
```

Managed metadata operations do not create `COUNT_BUFFER` automatically.

## 13. Development notes and open design questions

The following are **not settled cross-language promises** and should be documented explicitly as they evolve:

1. Consistent cross-language representation of signed/unsigned integer types and serialized numeric values.
2. Unified error taxonomy and error reporting beyond the current C status constants.
3. Encoding identifiers and embedded-NUL behavior for low-level BPA strings. The higher-level string module defines UTF-8 as its default and code-point indexing; equivalent contracts across every language binding still need conformance coverage.
4. Generation-aware handles or namespace/context isolation to reduce stale-ID reuse hazards.
5. Concurrency policy, independent iterators, and the interaction between shared cursors and parallel callers.
6. Guaranteed alignment and lifecycle callbacks for complex user-defined records.
7. Whether string-array rearrangement/deck helpers should become part of the portable BPAA contract rather than a higher-level string module.
8. Random-generator injection and repeatability for shuffle/draw.
9. Distinguishing implementation-only pooling metrics and borrowed-pointer APIs from portable features.
10. Separate context instances versus the current C global registry architecture.

**Design principle:** `clm_base` defines basic types; `clm_bpa` manages physical blocks and arrays; optional `clm_vos` manages virtual objects; `clm_storage` manages dimensional organization; other modules reuse these capabilities where useful without mandatory VOS overhead.

---

*This is a language-agnostic development document, not a guarantee of finalized CLM 1.0 APIs. Updated for the October 2026 C slicing additions.*
