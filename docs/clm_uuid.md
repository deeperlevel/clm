# CLM - UUID

## Dependencies

The current CLM reference implementation composes these modules:

- [clm_base](./clm_base.md) — shared CLM identifiers, byte types, and conventions.
- [clm_bpa](./clm_bpa.md) — ID-addressed, typed storage and lifecycle for UUID values.
- [clm_gen](./clm_gen.md) — cryptographically random UUID version-4 generation.
- [clm_string](./clm_string.md) — parsing from and formatting into CLM string resources.

**External dependencies:** None required directly. The generator obtains randomness through the platform capabilities used by `clm_gen`; no separate UUID library is required.

## 1. Purpose and scope

`clm_uuid` manages **128-bit universally unique identifier values** using CLM's ID-based resource model. It provides binary-value storage, string conversion, comparison, structural validation, and **UUID version-4 generation**. Generation is deliberately limited to version 4; the module is not a general-purpose implementation of every UUID generation scheme.

A **CLM resource ID** identifies a managed UUID value. The **UUID value** is a separate 128-bit identifier stored *under* that resource ID. They are not interchangeable: changing or deleting a managed block does not change the meaning of the UUID's 16 bytes.

The behavioral contract can be implemented with language-native UUID values and storage mechanisms where appropriate. Cross-language consistency concerns the value, supported operations, formatting, and failure semantics—not identical native structure layouts.

## 2. UUID representation and text formats

A UUID value consists of **16 bytes in canonical textual order** (network byte order): the first byte corresponds to the first two hexadecimal digits of the standard representation. This ordering is independent of host endianness and of platform-specific GUID structure layouts.

| Representation | Example | Length |
|---|---|---:|
| Standard, dashed | `550e8400-e29b-41d4-a716-446655440000` | 36 characters |
| Compact, no dashes | `550e8400e29b41d4a716446655440000` | 32 characters |
| Binary value | 16 canonical bytes | 16 bytes |

- Input accepts either the standard **8-4-4-4-12** hexadecimal grouping or exactly 32 hexadecimal digits, using uppercase or lowercase letters.
- Input rejects surrounding whitespace, braces, `urn:uuid:` prefixes, misplaced separators, nonhexadecimal characters, and trailing content.
- Default output uses **lowercase hexadecimal with dashes**. Formatting options allow uppercase letters and omission of dashes.
- String output is complete and terminated in implementations requiring a terminator; insufficient output capacity is reported rather than producing truncated text.
- A UUID's binary value should be compared or transferred using canonical bytes, not by copying a platform-specific GUID structure.

**Validation is structural.** A correctly formatted UUID may contain any version or variant, including values that are not version 4. Only the *generation* operation promises an RFC-variant version-4 UUID.

## 3. Resource identity and lifecycle

The reference implementation stores UUIDs as BPA-managed blocks tagged `UUID`, each holding exactly 16 bytes. The caller supplies the CLM resource ID; the module creates a missing UUID block or updates an existing UUID block for operations that assign a value.

| Operation | Behavior |
|---|---|
| `clm_uuid_set(id, value)` | Creates or replaces a UUID block using canonical bytes. |
| `clm_uuid_get(id, out)` | Copies the full UUID value to caller-owned output. |
| `clm_uuid_copy(destination_id, source_id)` | Copies one stored UUID into another ID; self-copy is valid. |
| `clm_uuid_delete(id)` | Removes a UUID resource without deleting a different BPA resource type. |
| `clm_uuid_is_valid(id, out)` | Reports whether an ID contains a well-formed, 16-byte UUID resource. |

A missing destination may be created by a setter, parser, or generator. An existing block of a different type is **never silently replaced**. Reading an absent ID fails; reading an ID of the wrong type reports a type mismatch. The module does not automatically allocate numeric IDs or infer an ID from the UUID value.

`is_valid` checks the **managed resource's type and shape**, not its RFC version or whether its value is nil. Every 128-bit binary value is representable.

## 4. Generation, parsing, and formatting

| Operation | Behavior |
|---|---|
| `clm_uuid_generate(id)` | Generates and stores a random UUID version 4 with the RFC variant. |
| `clm_uuid_validate(text)` | Checks the structure of a standard or compact UUID string without storing it. |
| `clm_uuid_validate_string(string_id, out)` | Performs the same structural check on a CLM string resource. |
| `clm_uuid_parse(id, text)` | Parses a valid UUID string into a UUID resource. |
| `clm_uuid_parse_string(id, string_id)` | Parses a UUID from a CLM string resource. |
| `clm_uuid_format_value_to(value, format, uppercase, out, capacity)` | Formats a directly supplied UUID value. |
| `clm_uuid_format_to(id, out, capacity)` | Formats a stored UUID using the default representation. |
| `clm_uuid_format_to_with_options(id, format, uppercase, out, capacity)` | Formats a stored UUID using explicit options. |
| `clm_uuid_format_string(id, string_id)` | Copies formatted UUID text into a CLM string resource. |
| `clm_uuid_format_string_with_options(id, string_id, format, uppercase)` | Copies formatted text using explicit options. |

Generation uses a cryptographically suitable system randomness source through [clm_gen](./clm_gen.md). If randomness cannot be obtained, generation fails rather than substituting predictable identifiers. Generated values have **version nibble 4** and **RFC variant bits `10`**.

The reference implementation offers output buffer capacities of **37 bytes including the terminator** for dashed text and **33 bytes including the terminator** for compact text. Other language implementations may return native strings, but should preserve the same formatting contract.

## 5. Comparison and special values

| Operation | Behavior |
|---|---|
| `clm_uuid_compare(left_id, right_id, out)` | Returns negative, zero, or positive ordering by **unsigned canonical byte sequence**. |
| `clm_uuid_equal(left_id, right_id, out)` | Tests full 128-bit value equality. |
| `clm_uuid_set_nil(id)` / `clm_uuid_is_nil(id, out)` | Stores or tests the all-zero UUID. |
| `clm_uuid_set_max(id)` / `clm_uuid_is_max(id, out)` | Stores or tests the all-one UUID. |
| `clm_uuid_version(id, out)` | Reads the raw high nibble of canonical byte 6. |
| `clm_uuid_get_variant(id, out)` | Classifies the variant using the leading bits of canonical byte 8. |
| `clm_uuid_get_segments(id, out)` / `clm_uuid_set_segments(id, value)` | Converts to or from the reference implementation's legacy field-based representation. |

The **nil** and **max** UUIDs are legitimate values of the 128-bit type, even though they do not indicate generated UUID version 4. The version accessor returns the stored nibble (for example, nil is `0`, max is `15`) without claiming that arbitrary values have a meaningful generation version.

Variant categories distinguished by the reference are **NCS**, **RFC**, **Microsoft**, and **reserved**. Legacy segment conversion is a convenience for compatibility with field-based UUID APIs. Native numeric field layout is not a portable serialization format; the canonical 16-byte sequence is authoritative.

## 6. Results, failure behavior, and ownership

The reference implementation defines the following semantic results. Language ports may express them as status values or idiomatic errors while preserving the distinctions.

| Result | Meaning |
|---|---|
| `SUCCESS` | Requested operation completed. |
| `FAILURE` | Allocation, randomness, or another operation failed. |
| `INVALID_ARGUMENT` | Required input, UUID text, or stored UUID shape is invalid. |
| `NOT_FOUND` | Source resource ID does not exist. |
| `TYPE_MISMATCH` | Resource ID refers to a different BPA type. |
| `BUFFER_TOO_SMALL` | Caller-owned text buffer cannot hold the complete result. |

Invalid parsing, incompatible destination types, failed generation, and insufficient output capacity must not silently replace valid existing values or publish partial output. The caller owns supplied buffers; CLM owns the storage of registered BPA resources until deletion or shutdown.

Managed UUID IDs share the BPA registry and its synchronization requirements. **An ID is not itself a thread-safe reference**: callers must coordinate concurrent access according to the storage implementation's rules.

## 7. Illustrative usage

Language-neutral pseudocode; specific syntax varies by language:

```text
uuid.generate(USER_UUID_ID)                    # creates an RFC-variant UUID v4
uuid.format(USER_UUID_ID)                      # standard lowercase dashed text
uuid.parse(COPIED_UUID_ID, "550e8400-e29b-41d4-a716-446655440000")
uuid.equal(USER_UUID_ID, COPIED_UUID_ID)
uuid.version(USER_UUID_ID)                     # 4 for generated values
uuid.set_nil(OPTIONAL_UUID_ID)
uuid.delete(OPTIONAL_UUID_ID)
```

The parsed example is illustrative, not a claim about the random UUID returned by the generation operation.

## 8. Boundaries and interoperability

- **Generation:** version 4 only; no version 1, 3, 5, 6, 7, or 8 generators.
- **Parsing:** structural UUID text parsing, not version-4-only validation.
- **Ordering:** lexicographic unsigned comparison of canonical bytes; not chronological order.
- **Storage:** ID-addressed typed value, not a naming registry or persistence database.
- **Serialization:** canonical 16-byte order or the specified textual forms; platform-native GUID layouts are not interchangeable.
- **Dependencies:** `clm_console` and other modules may consume UUID values, but they are **not dependencies** of `clm_uuid`.

These boundaries keep UUID support narrow and reusable across higher-level CLM modules and language implementations.
