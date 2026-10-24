# CLM Container

## 1. Purpose and scope

`clm_container` is a procedural, **typed key/value collection registry**. A numeric **container ID** identifies each collection. Within it, caller-defined numeric **entry keys** identify values. Container keys are **not** positional array indices; gaps in the numeric key space are permitted.

The module supplies:

- Typed keyed storage and explicit create/clear/destroy operations.
- One per-container **positional navigation cursor**, independent of entry keys.
- A detached metadata snapshot (`ContainerInfo`).
- Copy-based, key-preserving **slicing** of entries by navigation position.
- **Manual reference counts** and explicit, deferred disposal of zero-reference containers.

`clm_container` **does not** implement unkeyed arrays, caller-selected initial capacity, fixed-width arena allocation, multidimensional blocks/atlases, data shuffling, or collection sampling. Those belong to **`clm_storage`**, as discussed for the CLM roadmap. `clm_container` does not allocate entry IDs or map text names to keys; the separate `clm_id` module supports name-to-ID mappings.

This document specifies externally observable behavior. Individual language ports choose their own native collection representations and result conventions, but should preserve the same operations, type codes, status codes, and lifetime rules.

## 2. Supported types

| Constant | Code | Value category |
|---|---:|---|
| `CONTAINER_TYPE_STRING` | `0x01` | Text string |
| `CONTAINER_TYPE_BYTE` | `0x02` | Signed 8-bit integer |
| `CONTAINER_TYPE_SHORT` | `0x03` | Signed 16-bit integer |
| `CONTAINER_TYPE_INTEGER` | `0x04` | Signed 32-bit integer |
| `CONTAINER_TYPE_LONG_INTEGER` | `0x05` | Signed 64-bit integer |
| `CONTAINER_TYPE_FLOAT` | `0x06` | Floating-point value, nominally 32-bit |
| `CONTAINER_TYPE_DOUBLE` | `0x07` | Floating-point value, nominally 64-bit |
| `CONTAINER_TYPE_DYNAMIC` | `0xFE` | Reserved for binary storage; **not supported by current Container** |
| `CONTAINER_TYPE_OBJECT` | `0xFF` | Language-native object/reference/opaque value |

The declared type is immutable for the lifetime of a container. All entries in a container share the declared value category. Object value ownership is **not automatically transferred** to Container; the language port must specify how object references are held and released. Python's implementation holds ordinary references, without invoking object-specific destruction.

Floating-point representation, precision, and behavior of non-finite values may be language-specific. The supplied Python implementation accepts floating-point values (including non-finite values) and checks finite `FLOAT` values against the representable single-precision magnitude, but does not coerce stored values to 32-bit precision.

## 3. Numeric result codes

| Status | Code | Meaning |
|---|---:|---|
| `CONTAINER_SUCCESS` | `0x00` | Operation succeeded |
| `CONTAINER_ERROR_DUPLICATE_ID` | `0x01` | Container ID or entry key already in use |
| `CONTAINER_ERROR_ID_NOT_FOUND` | `0x02` | Requested entry/cursor position is absent or navigation cannot proceed |
| `CONTAINER_ERROR_TYPE_NOT_SUPPORTED` | `0x03` | Unknown or unsupported container type |
| `CONTAINER_ERROR_TYPE_NOT_MATCHED` | `0x04` | Operation requires another declared container type |
| `CONTAINER_ERROR_NOT_FOUND` | `0x05` | Container ID does not exist |
| `CONTAINER_ERROR_INVALID_VALUE` | `0x06` | Invalid typed value, index, range, or reference-count operation |

Operations that return values conceptually return **status plus value**. A language may implement this using a result record, out parameter, tuple, or equivalent. Every failure must remain distinguishable from a legitimate stored null/object value.

## 4. Container metadata

`clm_container_get_info(container_id)` returns a **read-only snapshot** of:

| Field | Meaning |
|---|---|
| `id` | Container ID |
| `type` | Declared element type code |
| `committed` | Current number of stored key/value entries |
| `reserved` | Internally reportable reserved capacity, or `-1` when unavailable |
| `references` | Explicit, manually managed owner/reference count |
| `index` | Current **zero-based positional cursor**, or `-1` when unpositioned |

An entry's numeric key and its positional index are different. For example:

| Insertion position | Key | Value |
|---:|---:|---|
| 0 | 100 | Alpha |
| 1 | 900 | Beta |
| 2 | 17 | Gamma |

At the second position, `index = 1` while the key is `900`. Key iteration follows **insertion order**, not sorted key order. Updating an existing value leaves its position unchanged; deleting and reinserting a key places it at the end.

The Python implementation's native dictionary does not expose a meaningful reserved element capacity through this API, so its `reserved` snapshot is `-1`. The caller cannot set `reserved` on Container creation. No particular growth/allocation strategy is prescribed.

Metadata snapshots do **not** grant mutation authority. Cursor movement and reference count changes must use their dedicated functions.

## 5. Key/value lifecycle and operations

### 5.1 Registry lifecycle

| Function | Contract |
|---|---|
| `clm_container_create(id, type)` | Create one empty container, `references = 1`, unpositioned cursor |
| `clm_container_clear(id)` | Remove every entry and unposition cursor; preserve ID, type, and reference count |
| `clm_container_destroy(id)` | Explicitly destroy the named container and release its registry storage; ID becomes reusable |
| `clm_container_count(id)` | Get the number of stored entries |
| `clm_container_get_count(id)` | Alias of `count` with the same return behavior |
| `clm_container_contains(id, key)` | Determine whether an entry key exists |
| `clm_container_get_info(id)` | Obtain metadata snapshot |

**Current explicit-destroy policy:** `destroy(id)` is unconditional when the ID exists, even if `references` is positive. This preserves compatibility with earlier versions, which had no reference tracking. By contrast, `dispose()` destroys only zero-reference containers. Applications must not use `destroy()` on resources still held by other users.

### 5.2 Typed entry operations

For each suffix listed below, the module offers:

```text
clm_container_add_SUFFIX(container_id, entry_key, value)
clm_container_edit_SUFFIX(container_id, entry_key, value)
clm_container_get_SUFFIX(container_id, entry_key) -> status, value
clm_container_delete_SUFFIX(container_id, entry_key)
```

| Suffix | Type |
|---|---|
| `string` | `CONTAINER_TYPE_STRING` |
| `byte` | `CONTAINER_TYPE_BYTE` |
| `short` | `CONTAINER_TYPE_SHORT` |
| `integer` | `CONTAINER_TYPE_INTEGER` |
| `long` | `CONTAINER_TYPE_LONG_INTEGER` |
| `float` | `CONTAINER_TYPE_FLOAT` |
| `double` | `CONTAINER_TYPE_DOUBLE` |
| `object` | `CONTAINER_TYPE_OBJECT` |

`add` rejects duplicate keys; `edit` rejects keys not yet present; `delete` rejects missing keys. A typed operation applied to a different declared type returns `CONTAINER_ERROR_TYPE_NOT_MATCHED`. Value validation is performed at the insertion/update boundary. `OBJECT` may contain native objects or null values, as supported by the implementation.

## 6. Navigation and current-item access

Container owns one mutable navigation cursor per container ID. No independent iterator handles are exposed in the current contract. Do not interleave two independent traversals of the same container without coordinating the shared cursor.

### 6.1 Legacy key-returning operations, preserved

```text
clm_container_first(id)      -> status, entry_key
clm_container_next(id)       -> status, entry_key
clm_container_previous(id)   -> status, entry_key
clm_container_last(id)       -> status, entry_key
```

### 6.2 Positional navigation operations

```text
clm_container_iterate(id)             -> status  # move first
clm_container_iterate_backward(id)    -> status  # move last
clm_container_move_first(id)          -> status
clm_container_move_last(id)           -> status
clm_container_move_next(id)           -> status
clm_container_move_previous(id)       -> status
clm_container_get(id)                 -> status, current_value
clm_container_get_key(id)             -> status, current_entry_key
clm_container_get_index(id)           -> status, zero_based_index_or_minus_one
clm_container_set_index(id, index)    -> status
clm_container_is_first(id)            -> status, boolean
clm_container_is_last(id)             -> status, boolean
```

- `iterate` and `move_first` are equivalent; `iterate_backward` and `move_last` are equivalent.
- `set_index` uses a **zero-based position**, never the numeric entry key.
- `get` returns the current entry **value** without advancing, whereas `get_key` returns its key.
- An empty container or unpositioned cursor has index `-1`. `get`/`get_key` return `ID_NOT_FOUND` if unpositioned.
- Moving beyond either end returns `ID_NOT_FOUND` and **leaves the cursor on the last valid entry**.
- Out-of-bounds `set_index` returns `INVALID_VALUE` without modifying the cursor.
- `is_first`/`is_last` return false when the container exists but has no positioned cursor.
- Deleting the currently selected entry unpositions the cursor; other deletions leave the cursor attached to the same entry key (its positional index may change).
- `clear`, successful `slice` replacement, and `destroy` invalidate the corresponding cursor.

### Example (language-neutral pseudocode)

```text
create(MY_CONTAINER, INTEGER)
add_integer(MY_CONTAINER, 100, 10)
add_integer(MY_CONTAINER, 400, 40)

if iterate(MY_CONTAINER) == SUCCESS:
    repeat:
        status, key = get_key(MY_CONTAINER)
        status, value = get(MY_CONTAINER)
        process(key, value)
    until move_next(MY_CONTAINER) != SUCCESS
```

## 7. Copy-based slicing

```text
clm_container_slice(source_id, destination_id, start, count) -> status
```

A slice selects `count` entries beginning at **zero-based navigation position** `start` and copies the **original keys and values** to an already existing destination container.

- Source and destination must exist, have the **same declared type**, and be distinct IDs.
- Source is unchanged, including its cursor and reference count.
- Destination contents are **replaced**, never appended; its cursor becomes unpositioned.
- Destination ID, type, and reference count remain unchanged.
- `start >= 0`, `count >= 0`, and `start + count <= committed` are required.
- `start == committed` with `count == 0` is permitted and clears the destination.
- Invalid arguments or a type mismatch leave the destination unchanged.
- Container storage is independent after copying. For `OBJECT` values, values are **shallow references**, not deep-cloned objects.
- Key insertion order is preserved in the destination.

Example:

```text
Source:      { 100: "A", 200: "B", 300: "C", 400: "D" }
slice(source, destination, start=1, count=2)
Destination: { 200: "B", 300: "C" }
```

Current Python implementation constructs a temporary dictionary and replaces the destination only after it is successfully populated. For low-level ports, validate ranges and prepare destination storage before committing changes to avoid partially overwritten destinations on allocation failure.

## 8. Explicit reference counting and deferred disposal

The container registry tracks **manual** reference counts. It never attempts to determine reachability from language variables, stack frames, pointers, or closures.

```text
clm_container_reference(container_id)    -> status
clm_container_unreference(container_id)  -> status
clm_container_dispose()                  -> status, disposed_count
```

Lifecycle:

1. `create` starts with `references = 1` for the creating owner.
2. `reference` increments the count when another owner explicitly retains it.
3. `unreference` decrements the count; it **does not destroy** the container.
4. At `references = 0`, `dispose()` can reclaim the container.
5. `dispose()` scans all registered containers and destroys only those with zero references.

An `unreference` on an existing zero-reference container returns `INVALID_VALUE` and never makes the count negative. `reference` can reacquire a zero-reference container while it still exists, before disposal. After disposal, attempts to access its ID return `NOT_FOUND` until a new container is created under that ID.

**Important:** a forgotten `unreference` prevents automatic disposal. This is intentional: CLM respects explicit owner tracking rather than guessing. A container with zero references remains registered and accessible until disposal, but callers must retain it before relying on it across a potential collection cycle.

`dispose()` is suitable for an explicit call or for invocation from future `clm_managed`. It is **not** a tracing or cyclic garbage collector. It does not free arbitrary allocations reachable through stored object pointers; object ownership remains a separate concern.

## 9. Portability, ownership, concurrency

- **Storage:** Use the language's native keyed map/hash/dictionary, or an equivalent. Do not mandate an allocator or object model.
- **Type checking:** Keep signed integer width rules. Reject invalid types rather than silently coerce them.
- **Cursor:** Insertion order must be observable. If the native map does not preserve order, maintain an ordered key index.
- **Object ownership:** Container owns its registry/storage bookkeeping. It does not automatically own externally allocated objects. Define destructor callbacks separately if a port needs them.
- **Capacity:** `reserved` is informative where meaningful; `-1` indicates capacity is unspecified/unavailable. Container does not accept a `reserved` creation argument.
- **Reference safety:** Manual reference counts are advisory ownership records, not automatic pointer/lifetime detection.
- **Concurrency:** The current Python module is **not synchronized** for concurrent mutation, iteration, or disposal. Callers must coordinate concurrent access. A future port may add internal synchronization, provided semantics remain consistent.
- **Stale IDs:** An ID may be reused after destroy. A stale numeric ID could then identify a different container; generation handles are not part of this release.
- **Status handling:** All normal operation failures use the documented status codes. Do not substitute silent failures.

## 10. Implementation checklist / regression tests

- [ ] Unique container IDs across declared types; unsupported type rejected.
- [ ] Typed add/edit/get/delete operations retain their original contracts.
- [ ] Sparse integer keys supported; traversal uses insertion order.
- [ ] Creation sets references to one and index to `-1`.
- [ ] Metadata snapshots are detached/read-only.
- [ ] `committed` follows entry count; `reserved` uses `-1` if not observable.
- [ ] Both legacy and new cursor APIs traverse the same position.
- [ ] Position is independent of entry key, including nonconsecutive keys.
- [ ] Invalid position changes do not move the cursor.
- [ ] Out-of-range next/previous calls leave cursor on boundary.
- [ ] Slicing preserves keys/order and shallow object values, replaces destination, leaves source intact.
- [ ] Failed slicing does not modify destination.
- [ ] Reference/unreference do not destroy; unreference cannot underflow.
- [ ] Dispose reclaims only zero-reference containers; nonzero-reference containers remain.
- [ ] Explicit destroy works independently of reference count (current compatibility policy).
- [ ] Cleared/destroyed IDs invalidate cursors; destroyed IDs may be reused.
- [ ] Container never requires `clm_storage`, `clm_depend`, or `clm_managed` for basic operations.

## 11. Python reference implementation and tests

The Python port provides the procedural operations plus the optional `Container` static-method facade. The facade is **not required** by the language-neutral contract.

Run the reference tests:

```shell
python -m unittest -v test_clm_container test_clm_container_extended
```

The corresponding `clm_storage` and `clm_depend` revisions are separate projects and are **not** implemented by this Container update.

---

**CLM — Cross Language Modules**  
Public Domain — CC0 1.0 Universal  
*Don't start from scratch, bro.*


## Pending-disposal metadata (extension)

`clm_container_pending_count()` returns the number of containers with zero
manual references. It does not modify or collect any resource. This counter
is used by `clm_managed` for threshold/hybrid scheduling.


### Python return contract

`clm_container_pending_count() -> integer` returns a plain nonnegative integer (not a `(status, count)` tuple). It counts registered resources with `references == 0`, including empty resources; it does not collect, mutate or change their reference counts. New resources start with one reference and are not pending until unreferenced. Explicit destruction removes a resource from the registry immediately. The count is a point-in-time snapshot and requires external synchronization if registries are shared across threads.
