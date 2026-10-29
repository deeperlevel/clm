# CLM String Format

## 1. Purpose and scope

`clm_string_format` is the shared lexer/parser foundation for reading text-based
formats into CLM-owned structures. Non-tabular formats use `clm_tree`; delimited
row/column formats use `clm_table`.
The implemented format modules are:

| Module | Intended input |
|---|---|
| `string_ini` | Root assignments and one-level INI sections |
| `string_env` | Line-oriented environment assignments |
| `string_json` | JSON objects, arrays, and typed scalar values |
| `string_toml` | Tables, assignments, arrays, and typed scalar values |
| `string_yaml` | Indented mappings, block sequences, and typed scalar values |

Unlike the separate typed document model described in [CLM Formats](./clm_formats.md),
the C format layer uses `clm_tree` BPA leaves for typed values and `clm_table`
for string-valued cells. Format-specific codecs define how source literals map
to supported BPA leaf types.

This document describes the shared C implementation. Format-specific syntax
subsets are documented separately; these codecs are not full standards
implementations.

## 1.1 Simple default codecs

The built-in JSON and YAML codecs are intentionally simple. Their output
defaults are compact JSON and simple YAML with quoted scalar values. Parsed
tree leaves retain their BPA types, but quote-all output represents every leaf
as a string and is therefore lossy.

| Format | Parsing defaults | Dumping defaults |
|---|---|---|
| JSON | Objects/arrays, double-quoted strings, supported numbers, booleans, null, and `/* ... */` comments outside strings as a CLM extension. | Compact JSON without comments. Object keys and all scalar leaves are JSON-quoted; typed leaves are emitted as quoted text. |
| YAML | Indented block mappings/sequences, plain and quoted scalars, comments, and typed null/boolean/number leaves. | Simple block YAML without comments. Keys and all scalar leaves are quoted, so typed leaves are emitted as strings. |

The default YAML emitter should quote object keys as well for consistent,
unambiguous output. JSON and YAML parsers may remove supported comments, but
dumpers never preserve or emit comments. A more sophisticated codec may define
different parse/dump rules or a wider grammar while reusing the generic lexer,
token spans, parser cursor, and output helpers.

Quote-all output preserves readable scalar text, not scalar types. Reparse such
output to a tree and numeric/boolean/null-looking leaves become strings.

## 2. Current implementation status

The header-only C implementation is in `clm/string_format.h`. It provides:

- Token-kind, lexer-rule, token-span, lexer-state, parser-state, and error-location records.
- `clm_string_format_lexer_init` and `clm_string_format_parser_init`.
- Rule records and delimited-text helper stubs for `clm_table`.
- `clm_string_format_parse_tree` and `clm_string_format_serialize_tree` callback wrappers for non-tabular codecs.
- Working lexer, parser cursor, token-span, writer, and escaping helpers.

Tree wrappers create an object or array root as selected by the codec and delete
the entire partial tree if parsing fails. Serialization validates a supported
tree root and existing string destination before calling the format emitter.
Delimited table parsing/serialization remains outside the implemented codec
set. Other language implementations may use equivalent records and procedural
functions.

## 3. Text and scalar contract

Parser input is an exact-length UTF-8 byte sequence. Callers must decode any
external encoding before parsing, for example with the encoding conversion
operations described in [CLM String](./clm_string.md). Token spans are byte
offsets into the original input, while line and column locations are 1-based.
The input buffer and lexer rules are borrowed and must remain alive and unchanged
for the duration of parsing.

`clm_tree` stores leaf values as BPA blocks, so a format codec can preserve a
supported value type instead of converting every scalar to text. For example,
a JSON codec may store a string as `CLM_BPA_TYPE_STRING`, an integer as
`CLM_BPA_TYPE_INT` or `CLM_BPA_TYPE_LONG`, and a floating-point value as
`CLM_BPA_TYPE_DOUBLE`. Exact mappings, overflow behavior, and YAML scalar
resolution must be defined by each codec. String leaves use UTF-8.

A string-only table (for example a CSV row) continues to store every field as
text. In that case:

```yaml
port: 1234
```

with the field name `port` mapped to ID `123` produces the logical table cell
`(row 0, column 123) -> "1234"`. For tree-backed JSON, the same numeric literal
may instead be stored as an integer BPA value. Quoted strings have their
format-defined quotes and escapes decoded.

Tree objects use mapped numeric field IDs as keys; arrays use zero-based integer
indexes. The format codecs support object roots, and JSON/YAML additionally
support array roots. Scalar roots are not represented by `clm_tree`.

## 4. Field-name IDs

A parser must resolve each object member name to a numeric CLM ID. Applications
may pre-register names such as `port -> 123`, or a parser may offer an explicit
auto-map mode. Unknown-name behavior must be configured or documented; it must
not silently overwrite another field.

In C, `clm_id_get` only looks up an existing mapping; it does not register a
name. `clm_id_try_get` reports lookup success separately. `clm_id_gen_next`
returns sequential IDs but does not check for collisions with IDs assigned
explicitly elsewhere. Therefore, parser-generated mappings need a collision-aware
allocator and must register the resulting name/ID pair. If an API promises
`clm_int` IDs, it must also reject values outside that type's range; CLM
dictionary keys themselves are `clm_id` values.

Duplicate source names and distinct names resolving to the same ID within one
object must be reported as duplicate-key errors, not silently replaced.

## 5. Logical tree and ownership

The logical non-tabular result is a hierarchy of object dictionaries and typed
BPA leaf blocks:

```text
Object
  entries      : field-ID to BPA leaf, child object, or child array mapping
  owned        : child containers and copied leaf blocks created by tree APIs
Array
  items        : ordered BPA leaves, child objects, or child arrays
Scalar markers
  null         : explicit null node
  boolean      : explicit true/false node
```

`clm_tree` uses the heterogeneous `clm_dictionary` for object entries, so each
leaf can have its own BPA type. Child objects and arrays created through
`clm_tree` are exclusively owned and recursively deleted. Scalar leaves may be
typed BPA blocks, explicit null markers, or explicit boolean markers. A parse
callback failure deletes the newly created root hierarchy.

## 6. Shared lexer

Lexical rules configure scanning behavior, not a complete grammar. The current C
rule record contains:

| Rule | Meaning |
|---|---|
| `delimiters` | NUL-terminated set of single-byte ASCII delimiter characters |
| `line_comment_markers` | NUL-terminated set of single-byte line-comment starts, recognized outside quotes |
| `block_comment_start` / `block_comment_end` | Paired strings delimiting a block comment, such as `/*` and `*/`; both are absent or both are provided |
| `quote_characters` | NUL-terminated set of quote characters |
| `escape_character` | Escape character used within quoted text |
| `CLM_STRING_FORMAT_LEX_EMIT_NEWLINES` | Request newline tokens |
| `CLM_STRING_FORMAT_LEX_TRACK_INDENT` | Request indentation tokens for indentation-sensitive grammars |

A token stores its kind and its byte offset/length into the source, plus 1-based
line and column. Token text is borrowed rather than copied. An end token marks
end of input. The lexer must report malformed quoting and invalid input rather
than reading beyond the supplied length.

The current delimiter and line-comment fields are sets of single-byte ASCII
characters; block-comment delimiters are strings and can therefore be
multi-byte. Unterminated block comments must be reported as lexical errors.
These rules describe token boundaries, not complete grammar. A custom format
may supply different rules and its own parser/dumper, but multi-character
operators and context-sensitive tokenization may require a format-specific
scanner.

## 6.1 Delimited table parsing and serialization

`clm_string_format_parse_delimited` parses exact-length UTF-8 input into a new
`clm_table`. The delimiter and quote character are configurable. Quoted fields
may contain delimiters; doubled quote characters represent one literal quote.
Multiline quoted fields are accepted only when
`CLM_STRING_FORMAT_DELIMITED_ALLOW_MULTILINE_FIELDS` is enabled. Every field
value remains a string, and every record must have the same number of fields.

Column names come from one of two sources:

- With `CLM_STRING_FORMAT_DELIMITED_HAS_HEADER`, the first record supplies the
  names. Each name must already be registered in `clm_id` and must resolve to a
  unique column ID within the table.
- Without a header, `column_names` and `column_name_count` supply the schema.
  Every supplied name must already be registered in `clm_id`; the input records
  themselves remain headerless. Headerless input without an explicit mapped
  schema fails rather than inventing column IDs.

`clm_string_format_serialize_delimited` writes a table into an existing CLM
string block. `CLM_STRING_FORMAT_DELIMITED_WRITE_HEADER` controls whether the
retained column names are emitted. Fields are quoted when they contain the
delimiter, quote character, CR, or LF; embedded quote characters are doubled.
`CLM_STRING_FORMAT_DELIMITED_QUOTE_ALL_FIELDS` forces every field to be quoted.
Output records use LF line endings. Missing cells are emitted as empty fields,
so serialization does not preserve the distinction between a missing cell and
a present empty string when reparsed.

CSV is one delimited profile: comma delimiter, double-quote quoting, doubled
quotes, and multiline quoted fields. Other simple delimited formats can provide
different delimiters or quote characters using the same API. Comment syntax,
typed values, ragged records, and format-specific metadata are not handled by
this generic delimited profile.

The delimited-table functions remain stubs in this C module; CSV is not one of
the five implemented string codecs described here.

## 7. Parser helpers and format grammars

The generic parser owns a lexer cursor, one-token lookahead, and an error
location. `peek` observes the next token without consuming it; `take` consumes
a token; `expect` consumes only when the token kind and optional literal match.
These are cursor helpers, not a universal parser for every format.

Non-tabular codecs provide callbacks with the format grammar and emitter logic:

```text
clm_string_format_parse_tree(text, byte_length, lex_rules, root_object_id, parse_callback)
clm_string_format_serialize_tree(root_object_id, destination_string_id, serialize_callback)
```

`parse_tree` initializes parser state, creates the root object, and calls the
format parser. If the callback fails, the wrapper deletes the newly created
tree. `serialize_tree` verifies the tree root and destination string before
calling the format serializer. These wrappers do not implement a format's
grammar, scalar mapping, or emitter.

Each format module owns its grammar and uses shared tree, writer, and text
helpers where appropriate:

- INI and ENV: line-oriented assignments with string-valued leaves.
- JSON: recursive objects/arrays, escaped strings, typed scalar parsing, and
  CLM block-comment support.
- YAML: indentation-sensitive block mappings/sequences and scalar resolution;
  anchors, aliases, tags, merge keys, complex keys, flow collections, and block
  scalar styles are outside the supported subset.
- TOML: simple tables, assignments, scalar values, and inline arrays; dotted
  keys, inline tables, and date/time values are outside the supported subset.

The common lexer must not imply that a format is fully supported merely because
its punctuation can be tokenized. Each format guide must list its accepted
syntax and rejected features. Dumping is format-specific; the simple JSON and
YAML defaults are defined in [Simple default codecs](#11-simple-default-codecs).
Delimited table output follows [Delimited table parsing and serialization](#61-delimited-table-parsing-and-serialization).
See the per-format guides for exact grammar and serialization behavior.

## 8. Errors and results

Operations return the common CLM success/failure statuses. A parse failure
removes the partially constructed destination tree. The current codec APIs do
not expose structured diagnostics or source locations; failures include invalid
syntax, duplicate/unmapped names, unsupported constructs, and allocation errors.

## 9. Related documentation

- [CLM String](./clm_string.md) documents UTF-8 storage and external byte encodings.
- [CLM Dictionary](./clm_dictionary.md) documents typed dictionaries and their ownership.
- [CLM Formats](./clm_formats.md) documents the separate typed-scalar format tree contract.
- [CLM ID](./clm_id.md) documents name registration and numeric ID lookup.
