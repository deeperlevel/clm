# CLM String Env

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Reads and writes environment-file-style key/value text into CLM structured data.

## Capabilities and operations

- Parse line-based key/value assignments and common quoting/comment conventions.
- Serialize the supported subset back to environment-style text.
- Expose file and text-based entry points.
- File parsing is not equivalent to automatically changing the host process environment.

## Detailed module contract

## Purpose

`clm_string_env` reads line-oriented environment assignments into a `clm_tree`
object. It does not read or modify the process environment.

## Supported input

- Blank lines and full-line `#` comments.
- `NAME=VALUE` assignments, with an optional leading `export` keyword.
- Names beginning with an ASCII letter or underscore, followed by ASCII
  letters, digits, or underscores.
- Unquoted values and single- or double-quoted values. Double quotes use CLM
  string escape decoding. Values, including text that resembles numbers or
  booleans, are stored as strings.
- UTF-8 input.

Names must already be registered in `clm_id`. Duplicate names, invalid names,
malformed quoted values, and lines without `=` fail. A failed parse removes its
partial tree.

## Serialization

The serializer emits one assignment per root string leaf and quotes/escapes
values. It does not preserve comments, source whitespace, or whether `export`
was present.

## Operations

`loads` parses an existing CLM string ID, and `load` reads a file named by a
CLM string ID. `dumps` writes to an existing CLM string ID, and `dump` writes
to a file named by a CLM string ID. There are no raw C-string, caller-buffer,
or raw-path codec entry points. Text is stored internally as UTF-8; CLM string
blocks carry their preferred external encoding. File codecs currently read
and write UTF-8. Operations return CLM success/failure statuses.

## Language-neutral contract

The result is a root object of UTF-8 string values keyed by registered numeric
IDs. Keep environment values as text; this codec never performs shell
expansion, interpolation, type inference, or process-environment updates.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_file](clm_file.md), [clm_string_format](clm_string_format.md).

## Example use cases

Parse line-based key/value assignments and common quoting/comment conventions.
