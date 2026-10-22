# `clm_container` — ID-Addressed Typed Containers

**Project:** CLM — Cross Language Modules  
**Repository:** https://github.com/deeperlevel/clm  
**Module:** `clm_container`  
**Interface style:** Procedural-first; optional language-native convenience facade

## Overview

`clm_container` provides **typed, ID-addressed storage** for applications, libraries, and other CLM modules. A container is identified by a numeric **container ID**; entries within that container are identified by numeric **entry keys**. A container's type is chosen when it is created and remains fixed until the container is destroyed.

CLM is designed for multiple programming languages from the outset. Implementations may use native maps, arrays, structures, pointer storage, or other appropriate mechanisms. **The public operations, numeric constants, and observable behavior are the shared contract—not the underlying data structure.**

Common uses include configuration storage, route-handler tables, dependency registrations, message-handler registries, and other ID-indexed data. `clm_container` itself is a storage primitive: it does not perform dependency injection, dispatch messages, or translate names to IDs.

> **Portability note:** This document describes the cross-language contract derived from the current module's public API. Certain implementation-dependent details (especially object ownership, floating-point representation, error return syntax, and concurrency) must be specified by each language implementation. A language-specific implementation should document any intentional deviation.

## Fundamental model

```text
Container Registry
  ├─ Container ID 0x100  [STRING]
  │    ├─ Key 0x01 → "localhost"
  │    └─ Key 0x02 → "example.org"
  │
  ├─ Container ID 0x200  [INTEGER]
  │    ├─ Key 0x01 → 8080
  │    └─ Key 0x02 → 9090
  │
  └─ Container ID 0x300  [OBJECT]
       ├─ Key 0xA101 → handler reference
       └─ Key 0xA102 → handler reference
```

- **Container IDs are unique within one container registry**, regardless of type.
- **Entry keys are unique only within their own container.** Different containers may use the same key.
- **Container types are fixed at creation.** Typed entry operations reject a mismatched container type.
- **Insertion and replacement are separate operations.** `add` does not overwrite; `edit` does not insert.
- IDs and keys are numeric. Human-readable names can be managed separately through `clm_id`.

## Quick start

The examples below use **language-neutral pseudocode**. Adapt return handling and variable declarations to the target language.

```text
CONTAINER_SETTINGS = 0x100
FIELD_HOST         = 0x01

status = clm_container_create(CONTAINER_SETTINGS, CONTAINER_TYPE_STRING)
require(status == CONTAINER_SUCCESS)

status = clm_container_add_string(CONTAINER_SETTINGS, FIELD_HOST, "localhost")
require(status == CONTAINER_SUCCESS)

(status, host) = clm_container_get_string(CONTAINER_SETTINGS, FIELD_HOST)
require(status == CONTAINER_SUCCESS)
print(host)   // localhost

status = clm_container_destroy(CONTAINER_SETTINGS)
require(status == CONTAINER_SUCCESS)
```

Operations that modify state return a **status code**. Retrieval and inspection operations conceptually return both a **status** and an **output value**. The representation can vary by language: a tuple, an output parameter, or another documented native equivalent.

## Container types

| Constant | Value | Logical value type | Accessor suffix |
|---|---:|---|---|
| `CONTAINER_TYPE_STRING` | `0x01` | Text/string | `string` |
| `CONTAINER_TYPE_BYTE` | `0x02` | Signed 8-bit integer (−128 to 127) | `byte` |
| `CONTAINER_TYPE_SHORT` | `0x03` | Signed 16-bit integer (−32,768 to 32,767) | `short` |
| `CONTAINER_TYPE_INTEGER` | `0x04` | Signed 32-bit integer (−2³¹ to 2³¹−1) | `integer` |
| `CONTAINER_TYPE_LONG_INTEGER` | `0x05` | Signed 64-bit integer (−2⁶³ to 2⁶³−1) | `long` |
| `CONTAINER_TYPE_FLOAT` | `0x06` | Floating-point value, nominally single precision | `float` |
| `CONTAINER_TYPE_DOUBLE` | `0x07` | Floating-point value, nominally double precision | `double` |
| `CONTAINER_TYPE_OBJECT` | `0xFF` | Language-native object, opaque value, reference, or handle | `object` |

A typed `add` or `edit` must validate that a value is acceptable for the container's declared type. An invalid value returns `CONTAINER_ERROR_INVALID_VALUE`.

**Language-dependent considerations:**

- Some languages do not have separate runtime numeric types for every entry above; they must still enforce the intended integer ranges.
- The current Python implementation validates finite `FLOAT` values against approximately the maximum binary32 magnitude but does not quantize them to binary32 precision. Cross-language bit-for-bit floating-point equivalence is therefore **not yet guaranteed**.
- Rules for non-finite floating-point values (`NaN`, infinities), implicit numeric conversions, and string encoding should be documented by each port until a stricter common contract is established.
- An `OBJECT` entry may contain a callable, service instance, structure handle, opaque pointer, or equivalent. Its representation and ownership are language-specific.

## Status codes

These numeric codes are part of the shared API:

| Constant | Value | Meaning |
|---|---:|---|
| `CONTAINER_SUCCESS` | `0x00` | Operation succeeded. |
| `CONTAINER_ERROR_DUPLICATE_ID` | `0x01` | Container ID already exists, or an entry key already exists during `add`. |
| `CONTAINER_ERROR_ID_NOT_FOUND` | `0x02` | Entry key is absent, or the iterator has no valid requested position. |
| `CONTAINER_ERROR_TYPE_NOT_SUPPORTED` | `0x03` | Unsupported container type during creation. |
| `CONTAINER_ERROR_TYPE_NOT_MATCHED` | `0x04` | Typed operation does not match container's declared type. |
| `CONTAINER_ERROR_NOT_FOUND` | `0x05` | Container ID does not exist. |
| `CONTAINER_ERROR_INVALID_VALUE` | `0x06` | Supplied value fails type/range validation. |

**Distinguish the two not-found statuses:** `ERROR_NOT_FOUND` refers to the **container**; `ERROR_ID_NOT_FOUND` refers to an **entry or cursor position**.

## API reference

### Container lifecycle

| Operation | Parameters | Output | Behavior |
|---|---|---|---|
| `clm_container_create` | `(container_id, container_type)` | `status` | Creates an empty typed container; rejects an existing ID. |
| `clm_container_clear` | `(container_id)` | `status` | Removes all entries, retains ID/type, resets cursor. |
| `clm_container_destroy` | `(container_id)` | `status` | Removes the container and cursor; ID becomes available for reuse. |
| `clm_container_count` | `(container_id)` | `status, count` | Counts stored entries. |
| `clm_container_contains` | `(container_id, key)` | `status, exists` | Reports whether an entry key is present. |

### Typed entry operations

Each type exposes four operations. Replace `<type>` with `string`, `byte`, `short`, `integer`, `long`, `float`, `double`, or `object`:

| Operation | Signature | Output | Behavior |
|---|---|---|---|
| Add | `clm_container_add_<type>(container_id, key, value)` | `status` | Inserts a new entry; duplicate keys fail. |
| Edit | `clm_container_edit_<type>(container_id, key, value)` | `status` | Replaces an existing value; absent keys fail. |
| Delete | `clm_container_delete_<type>(container_id, key)` | `status` | Removes an existing entry. |
| Get | `clm_container_get_<type>(container_id, key)` | `status, value` | Retrieves an entry of the declared type. |

Examples of real operation names:

```text
clm_container_add_string(container_id, key, value)
clm_container_edit_integer(container_id, key, value)
clm_container_delete_double(container_id, key)
clm_container_get_object(container_id, key)
```

**Validation order and expected behavior:**

- Missing container: `CONTAINER_ERROR_NOT_FOUND`.
- Existing container with wrong declared type: `CONTAINER_ERROR_TYPE_NOT_MATCHED`.
- `add` with existing key: `CONTAINER_ERROR_DUPLICATE_ID`.
- `edit`, `delete`, or `get` with missing key: `CONTAINER_ERROR_ID_NOT_FOUND`.
- `add` or `edit` with invalid value: `CONTAINER_ERROR_INVALID_VALUE`.

When an operation fails, callers must not use an unspecified output value. Languages without multi-value returns should expose the result through an output parameter or other documented mechanism.

### Navigation and iteration

| Operation | Parameters | Output | Behavior |
|---|---|---|---|
| `clm_container_first` | `(container_id)` | `status, key` | Positions cursor at first inserted entry. |
| `clm_container_previous` | `(container_id)` | `status, key` | Moves cursor one entry backward. |
| `clm_container_next` | `(container_id)` | `status, key` | Moves cursor one entry forward. |
| `clm_container_last` | `(container_id)` | `status, key` | Positions cursor at last inserted entry. |

**Iteration follows insertion order, not numeric key order.** Each container has one shared cursor.

```text
clm_container_create(0x400, CONTAINER_TYPE_STRING)
clm_container_add_string(0x400, 30, "A")
clm_container_add_string(0x400, 10, "B")
clm_container_add_string(0x400, 20, "C")

(status, key) = clm_container_first(0x400)
while status == CONTAINER_SUCCESS:
    (value_status, value) = clm_container_get_string(0x400, key)
    require(value_status == CONTAINER_SUCCESS)
    print(key, value)
    (status, key) = clm_container_next(0x400)

clm_container_destroy(0x400)

// Iteration order: 30/A, 10/B, 20/C
```

Cursor rules:

- `first` or `last` establishes a valid starting position.
- An empty container or an unpositioned cursor produces `CONTAINER_ERROR_ID_NOT_FOUND`.
- Moving beyond either end returns `CONTAINER_ERROR_ID_NOT_FOUND` **without advancing from the last valid position**.
- Deleting the selected entry resets its cursor; deleting another entry does not.
- `clear` and `destroy` reset/remove the cursor.
- Concurrent or interleaved traversals of a shared container do **not** have independent cursors. Use external coordination when necessary.

## Object containers

`CONTAINER_TYPE_OBJECT` supports application-defined values. This permits CLM modules such as `clm_depend`, `clm_dispatch`, and SPIder route registries to store callable references, state handles, or other resources using numeric keys.

```text
ROUTE_HANDLERS = 0x500
METHOD_GET     = 0xA101

clm_container_create(ROUTE_HANDLERS, CONTAINER_TYPE_OBJECT)
clm_container_add_object(ROUTE_HANDLERS, METHOD_GET, get_employees)

(status, handler) = clm_container_get_object(ROUTE_HANDLERS, METHOD_GET)
if status == CONTAINER_SUCCESS:
    invoke(handler, request_context)   // Invocation syntax is language-specific.

clm_container_destroy(ROUTE_HANDLERS)
```

**Object lifecycle is not defined by the container API alone.** The module does not assign dependency-injection lifetimes, automatically call domain-specific destructors, or ensure a retrieved reference remains valid after removal. Each port must explain how ownership, references, and cleanup work. `clm_depend` can build a higher-level lifetime-management policy on top of storage.

## Relationship to other CLM modules

| Module | Responsibility |
|---|---|
| `clm_container` | Stores typed, ID-keyed entries. |
| `clm_id` | Resolves meaningful names or labels to numeric IDs. |
| `clm_depend` | Registers constructors, resolves dependencies, and manages service lifetimes. |
| `clm_dispatch` | Registers and invokes message handlers. |
| SPIder | Uses CLM facilities for routes, handlers, and application infrastructure. |

No name-based lookup is required by `clm_container`. If an application uses names such as `"employees"` or `"host"`, it should convert them into appropriate IDs separately.

## Optional convenience facade

CLM is **procedural-first**, not anti-OOP. A language implementation may offer a convenience facade (for example, a static class or namespace) that forwards to the procedural operations:

```text
Container.create(ID, CONTAINER_TYPE_INTEGER)
Container.add_integer(ID, KEY, 42)
(status, value) = Container.get_integer(ID, KEY)
Container.destroy(ID)
```

This facade is optional and must not create a separate storage model or alter the semantics of the underlying functions. Languages without classes can simply expose the procedural API.

## Portability and implementation guidance

The following are **implementation choices**, not mandates of the public API:

- **Storage:** native dictionary/hash table, dense vector with an ID index, dynamic array, or another suitable structure. Sparse numeric IDs should not be treated as dense array offsets without considering memory use.
- **Memory management:** garbage collection, reference counting, smart pointers, explicit allocation/free, or language-native ownership. Define object cleanup and borrowed-reference validity clearly.
- **Return convention:** tuple, multiple return values, status plus output pointer, or another native form while preserving status meanings.
- **Type representation:** native integer widths, appropriate range checks, floating-point representation, and an opaque object/reference type.
- **Thread safety:** the current API does not promise synchronization. Implementations must document whether concurrent access is safe; an application should not assume it is.
- **Iteration cost:** internal representation may change iteration complexity, but insertion-order behavior and cursor semantics should remain consistent.

The shared contract does **not** require reflection, generics/templates, inheritance, decorators, or a custom memory allocator. A language may use these internally where they are natural, provided consumers are not required to adopt them.

## Recommended usage practices

1. Allocate distinct container IDs for logically separate registries.
2. Use stable numeric constants for well-known entry keys.
3. Check status codes before interpreting output values.
4. Prefer typed containers over object containers when the stored value has a uniform primitive type.
5. Treat object ownership and service lifetimes as separate design responsibilities.
6. Destroy containers when they are no longer needed; coordinate cleanup with their consumers.
7. Avoid simultaneous traversals of the same container unless cursor access is coordinated.

## Compatibility checklist for language ports

A conforming implementation should be tested for:

- Duplicate container IDs and duplicate entry keys.
- Identical entry keys in different containers.
- Type mismatch and invalid-value handling.
- Signed integer range boundaries.
- Add/edit/delete/get semantics for every supported type.
- Clear and destroy behavior, including container-ID reuse.
- Entry count and membership queries.
- Insertion-order cursor navigation and boundary behavior.
- Cursor invalidation after selected-entry deletion or clearing.
- Object/reference storage and documented lifetime behavior.

Where language implementations differ (for example, binary32 conversion or object cleanup), document the differences explicitly rather than silently assuming identical behavior.

---

**CLM — Cross Language Modules**  
*Different languages. Shared technologies. No borders.*  
*Don't start from scratch, bro.*
