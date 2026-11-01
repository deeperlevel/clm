# CLM String Json

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

JSON parsing and writing through CLM’s common structured-tree model.

## Capabilities and operations

- Load JSON values, arrays and objects into typed tree nodes.
- Serialize compatible trees back to JSON.
- Preserve numeric, Boolean, null, string and nesting semantics supported by the implementation.
- Report malformed syntax instead of silently accepting invalid JSON.

## Detailed module contract

## Purpose

`clm_string_json` parses JSON-shaped documents into typed `clm_tree` nodes and
serializes supported objects or arrays as compact JSON. The parser accepts one
document root, which must be an object or array; scalar roots are unsupported.

## Supported input

- Recursive objects and arrays, whitespace, and double-quoted object keys.
- JSON strings with standard escapes and validated `\uXXXX` surrogate pairs.
- Integer and floating-point numbers, booleans, and `null`.
- `/* ... */` block comments outside strings as a CLM extension; comments are
  not part of standard JSON.
- UTF-8 input.

Object keys must already resolve through `clm_id`. Duplicate keys, trailing
commas, invalid escapes or surrogate pairs, malformed numbers, unclosed
comments, and trailing document text fail. Failed parsing removes the partial
tree.

## Serialization

Output is compact and contains no comments. Object names and string leaves are
JSON-quoted; numeric leaves, booleans, and null markers are emitted as native
JSON literals. Supported scalar types and array order are preserved on a
parse/serialize round-trip.

## Operations

`loads` parses an existing CLM string ID, and `load` reads a file named by a
CLM string ID. `dumps` writes to an existing CLM string ID, and `dump` writes
to a file named by a CLM string ID. There are no raw C-string, caller-buffer,
or raw-path codec entry points. Text is stored internally as UTF-8; CLM string
blocks carry their preferred external encoding. File codecs currently read
and write UTF-8. Parsing creates a new owned tree; operations return CLM
success/failure statuses.

## Language-neutral contract

Use object nodes keyed by registered numeric IDs, ordered array nodes, and
distinct string, numeric, boolean, and null scalar representations. Do not
serialize comments. Preserve array order and object insertion order where the
target tree supports it.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_file](clm_file.md), [clm_string_format](clm_string_format.md).

## Example use cases

Load JSON values, arrays and objects into typed tree nodes.
