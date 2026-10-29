# CLM - Dictionary

## 1. Purpose and scope

CLM provides two dictionaries keyed by `clm_id` values. Keys are not array
indices and may be sparse, including zero and the full unsigned `clm_id` range.

- `clm_typed_dictionary` stores one configured BPA value type in an array-backed
    dictionary. Values are copied into private storage.
- `clm_dictionary` is heterogeneous: each key maps to a separately allocated
    BPA block, so different entries may have different BPA types. It can borrow
    an existing block or own a copy.

Both APIs provide lookup, set/add/update, removal, clearing, and deterministic
insertion-order key iteration. Sorting is not part of the dictionary contract.

## 2. Lifecycle and value types

Create a heterogeneous dictionary with no initial entries:

```text
clm_dictionary_new(dictionary_id)
dictionary.delete(dictionary_id)
```

Create a homogeneous typed dictionary separately:

```text
clm_typed_dictionary_new(dictionary_id, value_type, struct_size)
clm_typed_dictionary_delete(dictionary_id)
```

For a typed dictionary, `value_type` may be a BPA built-in fixed-width type,
`CLM_BPA_TYPE_STRUCT`, or `CLM_BPA_TYPE_STRING`. For `STRUCT`, provide a
nonzero `struct_size`; otherwise pass zero. Every entry has that same type and
width. String values are copied as bounded, NUL-terminated strings.

Creation rejects duplicate IDs across BPA blocks and BPAA arrays, unsupported
types, and invalid sizes. Both dictionary kinds own private metadata and index
resources; do not address or delete those resources directly.

## 3. Typed dictionary operations

| Operation | Behavior |
|---|---|
| `clm_typed_dictionary_add_from(id, key, value, size)` | Inserts a copied value only when the key is absent. |
| `clm_typed_dictionary_update_from(id, key, value, size)` | Replaces a copied value only when the key exists. |
| `clm_typed_dictionary_set_from(id, key, value, size)` | Copies caller bytes; fixed-width types require the exact width, and strings require a NUL within the bound. |
| `clm_typed_dictionary_set(id, key, source_block_id)` | Copies a compatible BPA block's value. |
| `clm_typed_dictionary_get_to(id, key, buffer, capacity)` | Copies a value to caller memory; strings include their terminator. |
| `clm_typed_dictionary_get(id, key, destination_block_id)` | Copies into a compatible, pre-created BPA block. |
| `clm_typed_dictionary_contains/count(id, key?)` | Checks for a key or returns the entry count. |
| `clm_typed_dictionary_remove(id, key)` | Removes a key and its copied value. |
| `clm_typed_dictionary_clear(id)` | Removes all entries while preserving the type and dictionary ID. |
| `clm_typed_dictionary_keys_to(id, buffer, capacity)` | Copies keys in insertion order. Capacity counts `clm_id` elements. |

All values in a typed dictionary must match its configured type. The typed
dictionary owns copied values and does not retain source BPA blocks.

## 4. Heterogeneous dictionary operations

| Operation | Behavior |
|---|---|
| `clm_dictionary_set/add/update(id, key, source_block_id)` | Stores a reference to an existing BPA block. `add` is insert-only; `update` requires an existing key; `set` inserts or replaces. |
| `clm_dictionary_set_from(id, key, type, value, size)` | Creates a BPA block of the supplied type, copies the value, and makes the dictionary own that block. |
| `clm_dictionary_get_id(id, key, &value_id)` | Returns the mapped BPA block ID without copying or transferring ownership. |
| `clm_dictionary_get_type(id, key, &type)` | Returns the mapped BPA block's type. |
| `clm_dictionary_get_to(id, key, buffer, capacity)` | Copies the mapped block bytes to caller memory; strings include NUL. |
| `clm_dictionary_get(id, key, destination_block_id)` | Copies into a compatible pre-created BPA block. |
| `clm_dictionary_contains/count(id, key?)` | Checks for a key or returns the entry count. |
| `clm_dictionary_remove(id, key)` | Removes an entry and deletes its block only if the dictionary owns it. |
| `clm_dictionary_clear(id)` | Removes all entries and deletes all dictionary-owned blocks. |
| `clm_dictionary_keys_to`, `iterate_reset`, `iterate_next` | Enumerates keys in insertion order. |

`set`, `add`, and `update` borrow source block IDs; they do not retain those
blocks. The caller must keep borrowed blocks alive until they are removed or
the dictionary is deleted. `set_from` creates an owned copy. Replacing,
removing, clearing, or deleting a dictionary releases an old value only when
that entry is marked as owned. Do not independently delete dictionary-owned
block IDs. `CLM_BPA_TYPE_ANY` itself is not a storable value type; each stored
block has a concrete BPA type.

## 5. Iteration and key order

```text
clm_typed_dictionary_iterate_reset(dictionary_id)
clm_typed_dictionary_iterate_next(dictionary_id, &key)
clm_dictionary_iterate_reset(dictionary_id)
clm_dictionary_iterate_next(dictionary_id, &key)
```

Both dictionary kinds visit keys in **insertion order**. Replacing a value
preserves its position; removing and reinserting a key moves it to the end.
Adding, removing, and clearing reset the dictionary's shared iteration cursor.
Mutation during iteration is not supported as a continuing traversal. The
iterator yields keys; use the corresponding typed or heterogeneous getter.

Each `keys_to` operation copies a snapshot of insertion-ordered keys. The
destination must have room for the current count; a zero-length dictionary
permits a null buffer with zero capacity. The snapshot is independent of later
mutations.

## 6. Keyed-map behavior and limits

Both APIs use unsigned `clm_id` keys, not arbitrary language-level hashable
keys. A typed dictionary has one BPA type and copies values. A heterogeneous
dictionary maps keys to concrete BPA block IDs and can own copies or borrow
existing blocks. Neither API stores arbitrary language objects. A string BPA
value is NUL-terminated UTF-8 bytes; Unicode string operations belong to
`clm_string`.

## 7. Complexity and memory

Hash lookup is expected amortized O(1), subject to table load and collisions.
Iteration and `keys_to` are O(n). The typed dictionary stores keys and values
in parallel BPAA arrays; removal compacts them and adjusts indexed hash slots,
so removal is O(n + table capacity). The heterogeneous dictionary uses a typed
dictionary index to reference BPA blocks; owned value blocks are also released
when their entries are removed.

## 8. Results and operational notes

| Result | Meaning |
|---|---|
| `CLM_BPA_SUCCESS` (`0`) | General operation succeeded |
| `CLM_BPA_GET_SUCCESS` (`1`) | Value or key was retrieved |
| `CLM_BPA_SET_SUCCESS` (`2`) | A value was set or replaced |
| `CLM_BPA_FAILURE` (`-1`) | Invalid/missing resource or key, incompatible type/size, or allocation failure |

`contains` returns `false` both for an absent key and for an invalid dictionary.
Count functions return `CLM_BPA_FAILURE` for an invalid dictionary.

The C typed dictionary uses open addressing with linear probing and grows its
hash table as entries are added. A separate key array preserves insertion
order. The dynamic dictionary stores its key-to-block-reference index in a
typed dictionary. Like BPA/BPAA registries, dictionary operations are not
internally synchronized; concurrent access must be serialized by the caller.

## 9. Ownership and compatibility notes

A typed dictionary's metadata is a BPA block; its slots, keys, and copied values
are private BPAA resources. A heterogeneous dictionary's index is itself a
typed dictionary, alongside its metadata block and any owned value blocks.
These backing resource IDs are private; do not address, reuse, or destroy them
directly. `clm_bpa_shutdown()` forcibly cleans underlying resources and
invalidates every dictionary. Dictionary IDs belong to BPA's block ID
namespace; avoid using a dictionary ID as an unrelated BPA block.

The C metadata layouts are private and may change; do not persist or share them
across versions. The API does not serialize dictionaries or manage persistent
files.

### Example (C)

```c
clm_typed_dictionary_new(100, CLM_BPA_TYPE_INT, 0);
clm_int score = 42;
clm_typed_dictionary_add_from(100, 900, &score, sizeof(score));
score = 99;
clm_typed_dictionary_set_from(100, 900, &score, sizeof(score));
clm_id key;
clm_typed_dictionary_iterate_reset(100);
while (clm_typed_dictionary_iterate_next(100, &key) == CLM_BPA_GET_SUCCESS) {
    clm_int current;
    if (clm_typed_dictionary_get_to(100, key, &current, sizeof(current)) ==
        CLM_BPA_GET_SUCCESS) {
        /* use key and current */
    }
}
clm_typed_dictionary_clear(100);
clm_typed_dictionary_delete(100);
```

```c
clm_dictionary_new(200);
clm_dictionary_set_from(200, 1, CLM_BPA_TYPE_STRING,
                        "host", sizeof("host"));
clm_dictionary_set_from(200, 2, CLM_BPA_TYPE_INT,
                        &score, sizeof(score));
clm_byte value_type;
clm_dictionary_get_type(200, 1, &value_type); /* STRING */
clm_dictionary_get_type(200, 2, &value_type); /* INT */
clm_dictionary_delete(200); /* also releases copied values */
```

### Additional conformance tests

- Typed dictionary: insert-only duplicate rejection, update-only missing-key rejection, and homogeneous type/size enforcement.
- Both kinds: stable insertion order, delete/reinsert ordering, collisions, key zero, count, clear, reuse, and cleanup.
- Typed values: exact-size `STRUCT`, empty strings, and copy independence.
- Heterogeneous values: mixed BPA types, type/ID lookup, compatible destination checks, borrowed-block lifetime, owned-copy cleanup on replacement/removal/clear/delete.
