# CLM Tree

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_bpa](clm_bpa.md), [clm_dictionary](clm_dictionary.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

ID-based hierarchical representation of objects, arrays and values.

## Capabilities and operations

- Create nodes, attach children and traverse nested structured data.
- Use dictionary-like object members and ordered array elements.
- Serialize or parse trees through JSON, YAML, TOML and other codecs.
- Make ownership, deletion and cycle constraints explicit when sharing node IDs.

## Detailed module contract

## 1. Purpose and scope

`clm_tree` provides a hierarchy of dictionary-backed objects and ordered
arrays with typed BPA leaf values. Objects map numeric `clm_id` keys to leaves
or child containers; arrays hold leaves or child containers in insertion order.

Use a tree for nested keyed data. Use [CLM Table](./clm_table.md) for ordered
rows and named columns, and [CLM Dictionary](./clm_dictionary.md) when a flat
key-to-value map or explicit control of borrowed/owned blocks is sufficient.

The tree supports object and array containers, plus explicit null and boolean
leaf markers. Scalar roots are not represented; a root is an object or array.

## 2. Representation

A tree object is a heterogeneous `clm_dictionary`. An array owns a private
dictionary whose keys are consecutive zero-based indexes. Child objects and
arrays are containers; other entries are BPA leaves of concrete types such as
`CLM_BPA_TYPE_STRING`, `CLM_BPA_TYPE_INT`, or `CLM_BPA_TYPE_DOUBLE`. Null and
boolean values use explicit `STRUCT` markers and are not ordinary struct leaves.

Object detection uses the dynamic dictionary's private metadata, not merely the
BPA block type. Therefore, a regular `STRUCT` leaf is not mistaken for an
object. `clm_tree_get_type` reports `CLM_BPA_TYPE_STRUCT` for child objects;
it reports the same type for child arrays; use the matching typed accessor to
retrieve a container.

Each object key is a `clm_id`, commonly resolved from a name with `clm_id_get`
or `clm_id_try_get`. The tree does not retain field names. Call
`clm_id_get_name` when serialization needs the earliest registered name for an
ID, or retain the desired spelling separately when aliases matter.

## 3. Ownership and graph constraints

- Root objects created by `clm_tree_new` are owned by the caller.
- Root arrays created by `clm_tree_new_array` are owned by the caller.
- `clm_tree_add_object` creates a fresh child dictionary and makes the parent
  responsible for deleting it and its descendants.
- `clm_tree_add_array`, `clm_tree_array_append_object`, and
  `clm_tree_array_append_array` create exclusively owned child containers.
- `clm_tree_array_append` borrows a BPA leaf; `clm_tree_array_append_from`
  copies a typed leaf.
- `clm_tree_set` stores a **borrowed** BPA leaf block ID. The caller must keep
  that block alive until it is replaced or removed from the tree.
- `clm_tree_set_from` copies bytes into a new dictionary-owned BPA leaf block.
- Replacing or removing a child object recursively deletes that object's
  Replacing or removing a child container recursively deletes its exclusively
  owned subtree. Replacing/removing leaves follows the underlying
  dynamic dictionary's ownership behavior.
- Child containers must not be shared between parents. Container links must be
  acyclic. Use tree container-creation functions; direct dictionary calls can
  construct invalid shared or cyclic graphs.

`clm_tree_delete` validates the hierarchy for cycles, shared child containers,
and excessive nesting before deleting any tree node. The current safety depth
limit is 256 container levels. A rejected graph remains intact. Borrowed BPA
leaves are never deleted by tree removal or destruction; owned leaves are
released by the dictionary that owns them.

## 4. Operations

| Operation | Behavior |
|---|---|
| `clm_tree_new(object_id)` | Creates an empty object node. |
| `clm_tree_new_array(array_id)` | Creates an empty ordered array node. |
| `clm_tree_is_object(id)` | Checks whether the ID is a tree/dynamic dictionary object. |
| `clm_tree_is_array(id)` / `clm_tree_is_null(id)` / `clm_tree_is_boolean(id, &value)` | Checks array, null, and boolean node markers. |
| `clm_tree_add_object(parent_id, key, child_id)` | Creates a fresh child object and inserts it at `key`; duplicate keys and used child IDs fail. |
| `clm_tree_add_array(parent_id, key, array_id)` | Creates an owned array child at `key`. |
| `clm_tree_set(object_id, key, source_block_id)` | Stores a borrowed BPA leaf block, replacing an existing value. Tree object blocks are rejected as leaves. |
| `clm_tree_set_from(object_id, key, type, value, size)` | Copies a typed value into a new owned BPA block at `key`. |
| `clm_tree_set_null(object_id, key)` / `clm_tree_set_boolean(object_id, key, value)` | Stores explicit null or boolean leaves. |
| `clm_tree_contains(object_id, key)` | Checks whether the key exists. |
| `clm_tree_count(object_id)` | Returns the number of entries. |
| `clm_tree_get_id(object_id, key, &value_id)` | Retrieves the underlying BPA block ID without copying or transferring ownership. |
| `clm_tree_get_type(object_id, key, &type)` | Returns the leaf type or `CLM_BPA_TYPE_STRUCT` for a child container. |
| `clm_tree_get_object(object_id, key, &child_id)` | Retrieves a child object ID and verifies it is an object. |
| `clm_tree_array_count(array_id)` | Returns the number of array elements. |
| `clm_tree_array_append_from(array_id, type, value, size)` | Appends a copied typed leaf; related functions borrow a leaf or append null, boolean, object, or array values. |
| `clm_tree_array_get_id(array_id, index, &value_id)` | Retrieves an element block ID without copying or transferring ownership. |
| `clm_tree_array_get_object` / `clm_tree_array_get_array` | Retrieves a child container ID of the requested kind. |
| `clm_tree_array_get_to` / `clm_tree_array_get` | Copies a leaf element; fails for containers and null markers. |
| `clm_tree_get_to(object_id, key, buffer, capacity)` | Copies a leaf into caller-owned memory; fails for containers and null markers. |
| `clm_tree_get(object_id, key, destination_id)` | Copies a leaf into a compatible existing BPA block. |
| `clm_tree_remove(object_id, key)` | Removes a leaf or recursively removes a child-container subtree. |
| `clm_tree_delete(root_id)` | Deletes an object or array root and its exclusively owned descendants. |

### Results

Creation and mutations use the usual CLM BPA statuses and return
`CLM_BPA_SET_SUCCESS` or `CLM_BPA_FAILURE`;
retrieval functions return `CLM_BPA_GET_SUCCESS` or `CLM_BPA_FAILURE`. Count
returns a nonnegative `clm_int` or `CLM_BPA_FAILURE`. Predicates return a
boolean; `clm_tree_contains` is false for both a missing key and an invalid
object ID.

A failed allocation or validation must not replace the current value. When a
subtree cleanup fails after a parent mutation, the operation reports failure;
callers should treat the tree as needing inspection or cleanup.

## 5. Example

```text
clm_tree_new(ROOT_ID)
clm_tree_add_object(ROOT_ID, SERVER_FIELD, SERVER_ID)
clm_tree_set_from(SERVER_ID, HOST_FIELD, CLM_BPA_TYPE_STRING,
                  "localhost", sizeof("localhost"))
port = 8080
clm_tree_set_from(SERVER_ID, PORT_FIELD, CLM_BPA_TYPE_INT,
                  &port, sizeof(port))
clm_tree_add_array(SERVER_ID, PORTS_FIELD, PORTS_ID)
clm_tree_array_append_from(PORTS_ID, CLM_BPA_TYPE_INT, &port, sizeof(port))

clm_tree_get_object(ROOT_ID, SERVER_FIELD, &server_id)
clm_tree_get_to(server_id, HOST_FIELD, host_buffer, host_capacity)
clm_tree_delete(ROOT_ID)
```

The `SERVER_ID` and `PORTS_ID` children are owned by `root`; do not separately
delete them. The example uses language-neutral pseudocode for BPA type
constants and buffer sizes.

## 6. Implementation status

`clm_dictionary` API. The implementation and tests cover nested objects and
arrays, mixed leaf types, borrowed and copied values, null/boolean markers,
recursive cleanup, and rejection of cycles or shared child containers.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Create nodes, attach children and traverse nested structured data.
