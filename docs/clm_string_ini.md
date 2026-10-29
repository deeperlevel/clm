# CLM String INI

## Purpose

`clm_string_ini` parses a practical INI subset into a `clm_tree` object and
serializes supported trees back to text. This is a deliberately small codec,
not a complete implementation of every INI dialect.

## Supported input

- Blank lines and full-line comments beginning with `#` or `;`.
- Root-level assignments and one-level `[section]` headers.
- `=` and `:` assignment separators.
- Unquoted, single-quoted, and double-quoted string values. Double-quoted
  values use CLM string escape decoding; single-quoted values support doubled
  quote characters.
- UTF-8 input. Values are always stored as string leaves.

Duplicate keys, duplicate sections, nested sections, malformed quotes, and
unregistered names fail parsing. Section names and assignment names resolve
through the application's `clm_id` registry. A failed parse removes its partial
tree.

## Serialization

Root-level keys are emitted before sections. Values are quoted and escaped;
section names and keys use their registered names. Comments and original
whitespace are not retained. The output is normalized and uses LF line endings.

## Operations

`loads` parses an existing CLM string ID, and `load` reads a file named by a
CLM string ID. `dumps` writes to an existing CLM string ID, and `dump` writes
to a file named by a CLM string ID. There are no raw C-string, caller-buffer,
or raw-path codec entry points. Text is stored internally as UTF-8; CLM string
blocks carry their preferred external encoding. File codecs currently read
and write UTF-8. Operations return CLM success/failure statuses.

## Language-neutral contract

Treat the parsed root as an object. Each source key must already map to a
unique numeric field ID within its containing object. Preserve string values,
section structure, and insertion order. Do not promise source comments or
formatting on round trip.