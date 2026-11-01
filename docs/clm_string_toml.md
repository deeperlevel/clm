# CLM String Toml

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

TOML text parsing and serialization into CLM trees.

## Capabilities and operations

- Parse the documented subset of TOML tables, fields and values.
- Serialize supported tree structures into TOML.
- Provide file-based and text-based helpers.
- Avoid claiming complete TOML specification support where the parser supports a subset.

## Detailed module contract

## Purpose

`clm_string_toml` parses a documented TOML subset into a typed `clm_tree`
object. It is not a full TOML implementation.

## Supported input

- Blank lines and `#` comments outside quoted text.
- Root assignments and simple one-level `[table]` headers.
- Quoted strings, booleans, supported integer and floating-point literals, and
  inline arrays containing supported values. Arrays may be nested.
- UTF-8 input. Numeric and boolean leaves retain their BPA types; strings are
  stored as string leaves.

Table/key names must resolve through `clm_id`. Duplicate table/key entries,
null, dotted keys/table paths, inline tables, date/time values, multiline
strings, and unsupported numeric spellings fail. A failed parse removes its
partial tree.

## Serialization

Objects are emitted as simple tables and arrays as inline arrays. String leaves
are quoted and escaped; numbers and booleans are emitted as literals. Null
leaves and object-valued assignments that cannot be represented by this subset
fail serialization. Comments and source formatting are not retained.

## Operations

`loads` parses an existing CLM string ID, and `load` reads a file named by a
CLM string ID. `dumps` writes to an existing CLM string ID, and `dump` writes
to a file named by a CLM string ID. There are no raw C-string, caller-buffer,
or raw-path codec entry points. Text is stored internally as UTF-8; CLM string
blocks carry their preferred external encoding. File codecs currently read
and write UTF-8. A parse creates a new owned tree; operations return CLM
success/failure statuses.

## Language-neutral contract

Represent each table as an object, arrays as ordered array nodes, strings as
strings, and supported numeric/boolean literals as typed scalar nodes. Resolve
each table/key name using the application's field-ID registry. Do not infer
support for TOML constructs outside the subset above.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_file](clm_file.md), [clm_string_format](clm_string_format.md).

## Example use cases

Parse the documented subset of TOML tables, fields and values.
