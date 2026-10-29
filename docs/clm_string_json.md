# CLM String JSON

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

Output is compact and contains no comments. Object names and all scalar leaves
are JSON-quoted; typed numbers, booleans, and null markers are converted to
their textual spellings and emitted as JSON strings. This quote-all behavior is
valid JSON but intentionally does not preserve scalar types across a round
trip. Arrays retain order.

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
target tree supports it. Quote-all output is a text representation choice, not
a change to the types held by the parsed tree.