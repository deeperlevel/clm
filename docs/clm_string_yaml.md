# CLM String Yaml

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

YAML text parsing and serialization into CLM trees.

## Capabilities and operations

- Parse an indentation-oriented subset of YAML collections and scalar values.
- Serialize compatible CLM trees to YAML.
- Provide text/file adapters and preserve nested relationships.
- This is a documented YAML subset, not an unrestricted YAML 1.2 engine with every advanced feature.

## Detailed module contract

## Purpose

`clm_string_yaml` supports a small indentation-based YAML subset backed by
`clm_tree`. It is not a general YAML 1.2 parser.

## Supported input

- Root and nested block mappings and block sequences.
- Space indentation; tabs used for indentation are rejected.
- `#` comments outside quoted values, plus optional `---` and `...` document
  markers.
- Plain, single-quoted, and double-quoted scalar values. Recognized `true`,
  `false`, `null`, and supported numeric forms become typed tree leaves;
  other plain values become strings.
- UTF-8 input.

Mapping names must already resolve through `clm_id`. Duplicate keys,
inconsistent indentation, malformed quoting, flow collections (`[]`/`{}`),
anchors, aliases, tags, merge keys, complex keys, and block scalar styles are
unsupported and fail. A failed parse removes the partial tree.

## Serialization

Mappings and sequences are emitted as block structures. Keys and all scalar
leaves are quoted; typed numbers, booleans, and null markers are represented as
quoted text. This is readable YAML but reparses those leaves as strings. Array
order is retained. Comments and original formatting are not retained.

## Operations

`loads` parses an existing CLM string ID, and `load` reads a file named by a
CLM string ID. `dumps` writes to an existing CLM string ID, and `dump` writes
to a file named by a CLM string ID. There are no raw C-string, caller-buffer,
or raw-path codec entry points. Text is stored internally as UTF-8; CLM string
blocks carry their preferred external encoding. File codecs currently read
and write UTF-8. Parsing creates a new owned object or array tree; operations
return CLM success/failure statuses.

## Language-neutral contract

Represent mappings with registered numeric field IDs, sequences with ordered
array nodes, and resolved scalar types with the corresponding tree leaf type.
Keep unsupported YAML features explicit; do not treat successful parsing of
this subset as general YAML conformance.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_file](clm_file.md), [clm_string_format](clm_string_format.md).

## Example use cases

Parse an indentation-oriented subset of YAML collections and scalar values.
