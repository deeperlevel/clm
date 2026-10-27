# CLM - Dictionary

## 1. Purpose and scope

`clm_dictionary` stores typed values under integer keys. Each dictionary has a
`clm_id`; each entry key is also a `clm_id`. Keys are not array indices and may
be sparse, including zero and the full unsigned `clm_id` range.

The C implementation uses BPA for dictionary metadata and BPAA arrays for its
hash slots, insertion-ordered keys, and values. Values are copied into the dictionary; callers do not
transfer ownership of their source buffers or BPA blocks. The API provides
lookup, set/replace, removal, clearing, and deterministic insertion-order
key iteration. Keys remain distinct from array positions; sorting is intentionally
not part of the dictionary contract.

## 2. Lifecycle and value types

Create a dictionary with a fixed value type:

```text
dictionary.new(dictionary_id, value_type, struct_size)
dictionary.delete(dictionary_id)
```

`value_type` may be a BPA built-in fixed-width type, `CLM_BPA_TYPE_STRUCT`, or
`CLM_BPA_TYPE_STRING`. For `STRUCT`, provide a nonzero `struct_size`; otherwise
pass zero. Each key in one dictionary has the same value type and, for
`STRUCT`, the same width. String values are stored as BPAA strings. Their bytes
are copied as bounded, NUL-terminated strings; encoding validation is not part
of this module.

Creation rejects duplicate IDs across BPA blocks and BPAA arrays, unsupported
types, and invalid sizes. Deleting a dictionary also deletes its private arrays
and metadata. Do not separately delete or modify the dictionary's internal BPA
or BPAA resources.

## 3. Operations

| Operation | Behavior |
|---|---|
| `clm_dictionary_add_from(id, key, value, value_size)` | Inserts only when the key does not exist; duplicate keys fail. |
| `clm_dictionary_update_from(id, key, value, value_size)` | Replaces only when the key exists; missing keys fail. |
| `clm_dictionary_set_from(id, key, value, value_size)` | Copies bytes from caller-owned memory; fixed-width values require the exact configured width, and strings require a NUL terminator within the supplied bound. |
| `clm_dictionary_set(id, key, source_block_id)` | Copies a compatible BPA block's value, including a BPA string. |
| `clm_dictionary_get_to(id, key, buffer, buffer_size)` | Copies a value to caller-owned memory; the buffer must fit the full value, including a string's NUL terminator. |
| `clm_dictionary_get(id, key, destination_block_id)` | Copies a value to a compatible, pre-created BPA block. |
| `clm_dictionary_contains(id, key)` | Returns whether the key exists. |
| `clm_dictionary_count(id)` | Returns the number of entries as `clm_int`. |
| `clm_dictionary_remove(id, key)` | Removes a key and its value. |
| `clm_dictionary_clear(id)` | Removes every entry but retains the dictionary ID and type. |
| `clm_dictionary_keys_to(id, buffer, capacity)` | Copies keys into a caller-provided array in insertion order. Capacity counts `clm_id` items. |

Setting an existing key replaces its value without changing the entry count.
Setting a missing key inserts it. Missing-key retrieval and removal return
failure. `get` destinations must match the value type; fixed-width destinations
must also have the configured byte width. String retrieval preserves the
destination block's own metadata while replacing its string contents.

## 4. Insertion order and iteration

```text
clm_dictionary_iterate_reset(dictionary_id)
clm_dictionary_iterate_next(dictionary_id, &key)
```

Keys are visited in **insertion order**, like modern Python dictionaries.
Setting a key already present changes its value but not its position. Removing
and subsequently adding a key puts it at the end. Rehashing does not reorder
keys. Adding, removing, and clearing reset the dictionary's shared iteration
position; mutation during iteration is not supported as a continuing traversal.
The iterator yields keys; call `get` or `get_to` to retrieve their values.

`keys_to` copies a snapshot of insertion-ordered keys. The destination must
have space for `count()` elements; a zero-length dictionary permits a null
buffer with zero capacity. Copies are independent of later dictionary mutations.

## 4.1 Python-like behavior, deliberately limited

CLM preserves **integer-key mapping** semantics, not general Python `dict`:
keys are unsigned `clm_id`, and every value in one dictionary shares a BPA type.
`set_from` is an upsert, `add_from` is insert-only, and `update_from` is
replace-only. Values are copied. `STRING` stores NUL-terminated bytes; UTF-8
normalization, Unicode code-point indexing, arbitrary Python objects, generic
hashable keys, and object-reference ownership are not implemented here.

## 4.2 Complexity and memory

Hash lookup is expected amortized O(1), subject to table load and collisions.
Iteration and `keys_to` are O(n). Values and keys are held in parallel BPAA
arrays; removing an entry compacts them and adjusts indexed hash slots, so
**removal is O(n + table capacity)**. The current design favors straightforward
ownership and iteration order over constant-time deletion.

## 5. Results and operational notes

| Result | Meaning |
|---|---|
| `CLM_BPA_SUCCESS` (`0`) | General operation succeeded |
| `CLM_BPA_GET_SUCCESS` (`1`) | Value or key was retrieved |
| `CLM_BPA_SET_SUCCESS` (`2`) | A value was set or replaced |
| `CLM_BPA_FAILURE` (`-1`) | Invalid/missing resource or key, incompatible type/size, or allocation failure |

`contains` returns `false` both for an absent key and for an invalid dictionary.
The count function returns `CLM_BPA_FAILURE` for an invalid dictionary.

The C implementation uses open addressing with linear probing and grows the
hash table as entries are added. A separate key array guarantees
insertion-order iteration, independent of hash-bucket arrangement. Like BPA/BPAA registries, dictionary operations are
not internally synchronized; concurrent access to shared dictionaries must be
serialized by the caller.

## 6. Ownership and compatibility notes

A dictionary's metadata is a BPA block whose internal slots, keys, and values
are BPAA resources. These backing resource IDs are private; do not address,
reuse, or destroy them directly. `clm_bpa_shutdown()` forcibly cleans the
underlying resources, invalidating every dictionary. The module is not
thread-safe. Dictionary IDs belong to BPA's block ID namespace; avoid using a
dictionary ID as an unrelated BPA block.

The public metadata structure changed to add `keys_id`. Rebuild all sources
against the updated header; do not persist or share its in-memory layout across
versions. The API does not serialize dictionaries or manage persistent files.

### Example (C)

```c
clm_dictionary_new(100, CLM_BPA_TYPE_INT, 0);
clm_int score = 42;
clm_dictionary_add_from(100, 900, &score, sizeof(score));
score = 99;
clm_dictionary_set_from(100, 900, &score, sizeof(score));
clm_id key;
clm_dictionary_iterate_reset(100);
while (clm_dictionary_iterate_next(100, &key) == CLM_BPA_GET_SUCCESS) {
    clm_int current;
    if (clm_dictionary_get_to(100, key, &current, sizeof(current)) ==
        CLM_BPA_GET_SUCCESS) {
        /* use key and current */
    }
}
clm_dictionary_clear(100);
clm_dictionary_delete(100);
```

### Additional conformance tests

- Insert-only duplicate rejection and update-only missing-key rejection.
- Stable insertion order across hash-table growth, replacement, and rehashing.
- Delete and reinsert moves key to end; no stale slot mappings.
- Clear and reuse; count and existence checks after clear.
- Exact-size `STRUCT` values, empty strings, key zero, and collision/tombstones.
- Ownership independence and cleanup on deletion or shutdown.
