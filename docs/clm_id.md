# CLM Id

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_bpa](clm_bpa.md), [clm_dictionary](clm_dictionary.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

A named-ID registry associating human-readable names with numeric CLM identifiers.

## Capabilities and operations

- Register, look up, update and remove name-to-ID mappings.
- Use IDs to reference resources rather than embedding language-specific object references.
- Name registration does not allocate, resolve or delete the referenced BPA resource.
- Do not assume the registry is a dependency-injection container.

## Detailed module contract

## Overview

`clm_id` is a lightweight registry that associates **human-readable names** with **integer identifiers**. It provides a consistent, procedural interface for registering, retrieving, updating, and removing named identifiers across programming languages.

The registry is useful when readable names appear at system boundaries—such as configuration files, routes, or messages—but application components operate internally on numeric IDs. It does **not** allocate IDs automatically, own the objects those IDs may designate, or require object-oriented programming.

Each language implements the same observable behavior using appropriate native facilities. The specification does not mandate a particular map, string type, memory allocator, or error-handling mechanism.

## Core principles

- **Names map to integer IDs:** a registered name identifies one integer value.
- **Names are unique within the active matching mode:** registering the same effective name twice is an error.
- **IDs need not be unique:** multiple names may intentionally refer to the same integer ID.
- **Case-sensitive by default:** name comparisons are exact unless explicitly changed.
- **Procedural-first:** module-level functions are the primary API; optional facades may delegate to them.
- **Shared registry:** the baseline contract uses one process-local registry per module/runtime instance.
- **Explicit lifecycle:** applications register mappings and may update, remove, or clear them.

## Terminology

| Term | Meaning |
|---|---|
| **Name** | Non-empty text key supplied by the application. |
| **Identifier / ID** | Integer associated with a registered name. |
| **Registered spelling** | The original name spelling retained when the mapping was first registered. |
| **Normalized name** | Comparison key produced according to the active case-sensitivity setting. |
| **Alias** | A different name associated with the same numeric ID. |

## Quick start

The following examples are **language-neutral pseudocode**, not syntax required by any implementation.

```text
FIELD_HOST = 0x101
FIELD_PORT = 0x102

clm_id_register("host", FIELD_HOST)
clm_id_register("port", FIELD_PORT)

host_id = clm_id_get("host")       // 0x101
port_id = clm_id_get("port")       // 0x102

clm_id_set("port", 0x202)         // Update an existing mapping
clm_id_remove("host")             // Remove one mapping
clm_id_clear()                    // Remove all mappings
```

The precise representation of returned values and failures depends on the language binding; see **Error behavior**.

## Public API

### `clm_id_register(name, identifier)`

Creates a new name-to-ID mapping.

**Parameters**

- `name`: non-empty string/text value.
- `identifier`: integer value.

**Behavior**

1. Validate the name and identifier.
2. Compare the name under the current case-sensitivity policy.
3. Reject an already registered effective name without altering the registry.
4. Store the supplied integer ID, retaining the name's original spelling.

Different names may map to the same ID.

```text
clm_id_register("employees", 0x11)
clm_id_register("staff", 0x11)     // Valid alias
clm_id_register("employees", 0x12) // Duplicate-name error
```

### `clm_id_get(name)`

Returns the integer ID associated with a previously registered name.

**Parameters:** `name`, a non-empty text value.

**Behavior:** compares names according to the current case-sensitivity setting. An unknown name fails; it does not automatically register a new ID or return a default.

```text
clm_id_register("WebApi.host", 101)
assert clm_id_get("WebApi.host") == 101
```

### `clm_id_get_name(identifier, buffer, capacity, &byte_length)`

Copies the first registered spelling associated with an integer ID. Since
multiple names may be aliases for one ID, reverse lookup is not unique; this
operation deterministically selects the earliest remaining registration in
insertion order. Pass a null buffer with zero capacity to query the UTF-8 byte
length excluding the terminator. A real destination must have room for the
complete name and NUL terminator.

### `clm_id_set(name, identifier)`

Creates or updates a name-to-ID mapping.

If the effective name already exists, replace its ID. Otherwise, insert a new mapping. In case-insensitive mode, updating with an alternative capitalization retains the spelling from the original registration.

```text
clm_id_set("host", 101) // Inserts if absent
clm_id_set("host", 201) // Updates the existing mapping
```

Use `register` when accidental duplicates should be rejected; use `set` when replacement is intentional.

### `clm_id_remove(name)`

Deletes one registered name and its associated mapping. Removing an unknown name fails. Any *other* names mapped to the same integer ID remain registered.

```text
clm_id_register("employees", 17)
clm_id_register("staff", 17)
clm_id_remove("employees")
assert clm_id_get("staff") == 17
```

### `clm_id_set_case_sensitive(enabled)`

Selects the matching mode for subsequent registration, retrieval, updating, and removal.

| `enabled` | Matching policy |
|---|---|
| `true` | Case-sensitive comparison (default). |
| `false` | Unicode-aware caseless matching, equivalent to Unicode `casefold` semantics. |

Changing the mode **rebuilds the lookup index** using existing names, retaining their original spelling and integer values.

If switching to case-insensitive mode would cause two existing names to collide, the operation must fail **without changing** either the registry or the previous setting.

```text
clm_id_register("Host", 1)
clm_id_register("host", 2)
clm_id_set_case_sensitive(false) // Collision: operation fails atomically
```

Another example:

```text
clm_id_clear()
clm_id_set_case_sensitive(false)
clm_id_register("ServerName", 7)
assert clm_id_get("servername") == 7
clm_id_set("SERVERNAME", 8)
assert clm_id_get("ServerName") == 8
```

**Portability note:** For equivalent behavior, implementations should use Unicode full case folding (rather than ASCII-only lowercasing). An implementation without that capability should document its limitation instead of silently claiming equivalent Unicode support.

### `clm_id_is_case_sensitive()`

Returns a boolean indicating whether exact-case matching is active.

### `clm_id_clear()`

Removes **all** name-to-ID mappings, while preserving the current case-sensitivity setting.

```text
clm_id_set_case_sensitive(false)
clm_id_register("HOST", 1)
clm_id_clear()
assert clm_id_is_case_sensitive() == false
```

To restore baseline state, explicitly enable case sensitivity after clearing.

## Function reference

| Function | Input | Result / effect |
|---|---|---|
| `clm_id_register` | Name, integer ID | Inserts a new mapping; rejects duplicate names. |
| `clm_id_get` | Name | Retrieves the mapped integer ID. |
| `clm_id_get_name` | Integer ID, caller buffer/capacity, output byte length | Copies the earliest registered spelling for that ID. |
| `clm_id_set` | Name, integer ID | Inserts or replaces a mapping. |
| `clm_id_remove` | Name | Removes a mapping. |
| `clm_id_set_case_sensitive` | Boolean | Switches matching mode after collision validation. |
| `clm_id_is_case_sensitive` | None | Returns matching mode. |
| `clm_id_clear` | None | Removes all mappings, preserving matching mode. |

## Validation and error behavior

The shared behavior is defined by **error conditions**, not by language-specific exception classes. Language implementations may expose exceptions, error values, or status codes, provided they distinguish the important conditions and avoid unwanted mutations after failure.

| Condition | Affected operations | Required behavior |
|---|---|---|
| Name is empty or not valid text | Register, get, set, remove | Reject invalid name. |
| ID is not an integer | Register, set | Reject invalid ID. |
| Name is already registered | Register | Report duplicate-name error; retain prior mapping. |
| Name does not exist | Get, remove | Report unknown-name error. |
| Matching-mode argument is not boolean | Set case sensitivity | Reject invalid argument. |
| Changing matching mode creates collisions | Set case sensitivity | Report collision; preserve all prior state. |

The reference Python implementation uses `ValueError` for invalid names and collisions, `TypeError` for invalid ID and flag types, and `KeyError` for duplicate or unknown names. Other language implementations are **not required to adopt Python exception types**.

The current reference implementation accepts integer values without requiring them to be positive or unique. It rejects boolean values as IDs even in languages where booleans are represented as integers. Other implementations should document any narrower integer range imposed by their native types.

## Matching and alias semantics

**Name uniqueness is independent of ID uniqueness.** This distinction is essential when different configuration names intentionally refer to one field or service ID.

```text
clm_id_register("primary", 0x20)
clm_id_register("default", 0x20) // Both names are valid
```

In case-insensitive mode, capitalization variants are *not* separate names:

```text
clm_id_set_case_sensitive(false)
clm_id_register("Database", 0x30)
clm_id_get("DATABASE")            // 0x30
clm_id_register("database", 0x31)  // Duplicate-name error
```

When names are updated using `clm_id_set`, the original registered spelling is retained. It becomes significant when rebuilding the matching index after changing the matching mode.

The registry is **forward-only**: it guarantees lookup from name to ID, not lookup from ID to a unique name. Aliases make a unique reverse lookup inherently ambiguous.

## Using `clm_id` with other CLM modules

The ID registry can translate names from external configuration into numeric keys used by other modules.

```text
FIELD_HOST = 0x101
CONFIG_CONTAINER = 0x500

clm_id_register("host", FIELD_HOST)
clm_container_create(CONFIG_CONTAINER, CONTAINER_TYPE_STRING)

field_id = clm_id_get("host")
clm_container_add_string(CONFIG_CONTAINER, field_id, "localhost")
```

This illustrates a separation of responsibilities:

- `clm_id` resolves human-readable names into numeric identifiers.
- `clm_container` stores typed values at numeric keys.
- Modules such as `clm_container` and SPIder may establish their own numeric registration conventions.

`clm_id` does not manage object lifetimes, dependency graphs, route matching, or container values.

## Registry scope and concurrency

The baseline implementation maintains **one shared process-local registry**. Therefore:

- Registration in one part of a process is visible to other users of that same registry.
- `clm_id_clear()` affects all mappings in the shared registry.
- The module does not define separate namespaces or independent registry instances.
- Concurrent modifications are **not guaranteed to be synchronized** by the baseline contract. Applications should coordinate writes, or an implementation may explicitly document stronger guarantees.

Consumers should avoid changing matching modes while other components concurrently depend on lookup behavior.

## Implementation guidance

A language-specific implementation may use hash maps, balanced trees, associative arrays, vectors, or another suitable native data structure. To preserve the shared behavior, it should:

1. Store an integer ID for every registered name.
2. Preserve the original registered spelling.
3. Maintain or derive an index according to the current matching policy.
4. Reject duplicate effective names while permitting duplicate numeric IDs.
5. Rebuild its lookup index atomically when changing case-sensitivity settings.
6. Retain the matching mode when clearing entries.
7. Keep the procedural operations available even if an optional object-oriented facade is provided.

Memory allocation, string ownership, return conventions, and synchronization strategies remain implementation-specific. A reference binding might use explicit buffers and status codes; a Go binding may return an ID and an error; a C++ binding may expose exceptions or result types.

## Optional convenience facade

Implementations may offer a convenience `IdRegistry` interface equivalent to the procedural functions, where supported by the language. Such a facade should **delegate to the same registry and implementation** rather than create an independent set of mappings.

```text
IdRegistry.register("host", 101)
IdRegistry.get("host")
IdRegistry.clear()
```

The convenience interface is optional. Users are never required to create an object just to register or resolve names.

## Compatibility checklist

A conforming implementation should verify that:

- [ ] Names cannot be empty.
- [ ] IDs are integer values; boolean values are not accepted as IDs.
- [ ] `register` rejects duplicate effective names without overwriting them.
- [ ] `set` inserts absent names and updates existing names.
- [ ] Distinct names may share one numeric ID.
- [ ] `get` and `remove` report unknown names.
- [ ] Matching is case-sensitive by default.
- [ ] Case-insensitive matching supports the specified Unicode folding behavior, or documents a limitation.
- [ ] Switching to case-insensitive mode rejects collisions without changing existing state.
- [ ] Updating a name in case-insensitive mode preserves its registered spelling.
- [ ] `clear` removes mappings but preserves the matching mode.
- [ ] The procedural API remains usable without OOP or reflection.

## Portable operation families

These names describe CLM operations; each language binding may expose idiomatic signatures.

| Function | Behavior |
|---|---|
| `clm_id_register(name, id)` | Adds a mapping; rejects an existing effective name. |
| `clm_id_map(name, id)` | Convenience alias for `clm_id_register`. |
| `clm_id_set(name, id)` | Adds a mapping or updates its ID while preserving original spelling. |
| `clm_id_get(name)` | Returns the mapped ID or `CLM_ID_NOT_FOUND`. |
| `clm_id_try_get(name, &id)` | Status-returning alternative that can distinguish a missing name from any `clm_id` value. |
| `clm_id_remove(name)` / `clm_id_unmap_name(name)` | Removes one mapping by name. |
| `clm_id_unmap(id)` | Removes all names (aliases) associated with the ID. |
| `clm_id_set_case_sensitive(enabled)` | Rebuilds the name index, failing atomically if names collide. |
| `clm_id_is_case_sensitive()` | Returns the active matching mode. |
| `clm_id_clear()` | Removes mappings without changing the matching mode. |
| `clm_id_gen_reset()` / `clm_id_gen_next()` | Resets or advances the sequential generator, whose first result is zero. |

`clm_id_get` uses `CLM_ID_NOT_FOUND` (`UINT64_MAX`) as its missing-name
sentinel; use `clm_id_try_get` if that value itself must be returned without
ambiguity. The reference contract accepts nonempty UTF-8 names and
rejects malformed UTF-8. It does not perform Unicode normalization.
Case-insensitive mode folds ASCII letters only; non-ASCII bytes remain
case-sensitive. It therefore does not provide the Unicode case-folding
semantics described by the language-neutral contract.

The internal IDs `ID_STORAGE_ID`, `CLM_ID_INDEX_STORAGE_ID`,
`CLM_ID_NAMES_STORAGE_ID`, `CLM_ID_VALUES_STORAGE_ID`, and
`CLM_ID_NAME_BUFFER_ID` are reserved and must not be used by applications.
Generated IDs are independent of explicitly registered IDs; callers should
coordinate the two namespaces.

## Project and licensing

**CLM — Cross Language Modules** is an initiative by **Deeperlevel**, developed through community contributions.

- Official repository: https://github.com/deeperlevel/clm
- Contact: deeparchive@outlook.com
- Public-domain dedication: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)

CLM source is intended for reuse, modification, redistribution, and bundling. Refer to the repository's current license for its authoritative legal terms.

**Different languages. Shared technologies. No borders.**  
*Don't start from scratch, bro.*

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Register, look up, update and remove name-to-ID mappings.
