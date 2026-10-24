# CLM Formats

## 1. Overview

`clm_formats` provides reusable lexical processing, a common document-tree representation, and parsing and serialization of several text-based data formats. It is designed for independent implementations in different programming languages, without requiring dynamic reflection, object inheritance, or third-party formatting engines.

**Supported format names:** `json`, `yaml` (alias `yml`), `toml`, `ini`, and `csv`.

The module has three conceptual responsibilities:

1. **Lexical analysis:** identify strings, words, separators, comments, and source locations under configurable lexical rules.
2. **Decoding and tree construction:** apply the chosen format's grammar and build object, array, or scalar nodes stored in CLM containers, resolving object field names through `clm_id`.
3. **Serialization:** walk the CLM tree and emit the requested target format, applying its encoding, quotation, and structural constraints.

The formats **share a tree representation**, but their grammars are not identical. A format-specific decoder or emitter may reuse lexical helpers while implementing its own parsing rules. The current reference implementation exposes a reusable `clm_lexer`, but its format codecs do **not** all call that public lexer; some use an additional internal tokenizer or scan text directly. This distinction is important for port authors.

## 2. Dependencies and design principles

| Module | Purpose |
|---|---|
| `clm_container` | Owns persistent object/array node storage in ID-addressed object containers; provides indexed retrieval and iteration. |
| `clm_id` | Maps source field names to application-defined integer field IDs. |

No external JSON, YAML, TOML, INI, or CSV parser is required by the reference design. Implementations may choose native internal data structures for temporary parsing and lexing, but **persistent parsed trees must be backed by `clm_container`** to follow this module's intended CLM contract.

General rules:

- **Procedural-first:** public operations are standalone functions; object-style facades are optional.
- **Explicit schema IDs:** applications register field names using `clm_id` before parsing documents containing those names.
- **Preserve shape and types:** distinguish objects, arrays, and scalar values; do not silently treat an array as an object.
- **Avoid language-specific requirements:** `Node`, `Document`, `Field`, `Token`, and `LexRules` are conceptual records, not mandatory classes.
- **Predictable failures:** reject invalid syntax, missing field names, duplicate fields/IDs, and unsupported representations explicitly.
- **Scope honesty:** supporting a practical subset of a format is not equivalent to full standards compliance.

## 3. Data model

### 3.1 Node

A **Node** is one of three kinds:

| Kind | Meaning | Stored content |
|---|---|---|
| `scalar` | One primitive value | Null, string, boolean, integer, or floating-point value. |
| `object` | Named members | Reference/ID of a CLM object container. |
| `array` | Ordered elements | Reference/ID of a CLM object container indexed by zero-based positions. |

Language-neutral representation:

```text
Node
    kind         : scalar | object | array
    value        : scalar value, where applicable
    container_id : integer container identifier, where applicable
```

### 3.2 Field

Object-container entries retain both the **source spelling** of a field and the node that represents its value.

```text
Field
    name  : original field name
    value : Node
```

The container entry's key is the numeric ID retrieved from `clm_id_get(name)`. The preserved name enables serialization without reverse-looking-up the integer ID.

### 3.3 Document

A **Document** represents one parsed tree and the CLM containers allocated for it:

```text
Document
    root      : Node
    owned     : collection of owned container IDs
    destroyed : lifecycle flag
```

The root can be **any** of the three node kinds. Parsing may succeed with a scalar root, an empty object, or an empty array, subject to the source format.

### 3.4 Example hierarchy

An input such as:

```json
{"server":{"host":"localhost","port":8080},"active":true}
```

is conceptually represented as:

```text
Document
  root (object) → container A
    FIELD_SERVER → Field("server", object → container B)
      FIELD_HOST → Field("host", scalar "localhost")
      FIELD_PORT → Field("port", scalar 8080)
    FIELD_ACTIVE → Field("active", scalar true)
```

The field IDs are supplied by the consuming application. Reusing `FIELD_HOST` in a *different object container* does not cause a collision. Arrays instead use consecutive `0, 1, 2, ...` indexes and do not require per-index name registration.

## 4. Registration and prerequisites

Before parsing any document containing object member names, register **each unique field name** with `clm_id`. Section names and CSV column headings count as field names. Numeric IDs should be managed consistently by the consuming application.

```text
clm_id_register("server", FIELD_SERVER)
clm_id_register("host",   FIELD_HOST)
clm_id_register("port",   FIELD_PORT)
clm_id_register("active", FIELD_ACTIVE)

try:
    document = clm_json_parse(source_text)
    server   = clm_tree_get(document.root, FIELD_SERVER)
    host     = clm_tree_get(server, FIELD_HOST)
    use(host.value)
finally:
    clm_tree_destroy(document)
```

*Examples are pseudocode, not a promise of identical call or exception syntax in every language.*

**Important:** Different source names that resolve to the same numeric ID can collide **inside one object**. Even though `clm_id` permits aliases, the tree builder cannot store both aliases under one numeric key in a single container. Such input fails rather than silently overwriting a field.

## 5. Public API reference

### 5.1 Common format operations

| Operation | Inputs | Result | Behavior |
|---|---|---|---|
| `clm_format_parse` | `format_name`, `text` | `Document` | Select decoder, parse source text, map names to IDs, build a CLM-owned tree. |
| `clm_format_serialize` | `format_name`, `document` | text | Serialize a live document into the requested format. |
| `clm_tree_build` | internal-format node or supported scalar | `Document` | Construct persistent CLM nodes; **does not accept a native dictionary/map as its input in the supplied reference**. |
| `clm_tree_get` | `node`, integer key | `Node` | Retrieve a child by field ID (object) or element index (array). |
| `clm_tree_destroy` | `document` | no value | Destroy all owned containers; repeated calls are harmless. |
| `clm_lexer` | `text`, lexical rules | ordered sequence of `Token` | Produce lexical tokens without applying format grammar. |

### 5.2 Format-specific operations

| Format | Parse | Serialize |
|---|---|---|
| JSON | `clm_json_parse(text)` | `clm_json_serialize(document)` |
| YAML | `clm_yaml_parse(text)` | `clm_yaml_serialize(document)` |
| TOML | `clm_toml_parse(text)` | `clm_toml_serialize(document)` |
| INI | `clm_ini_parse(text)` | `clm_ini_serialize(document)` |
| CSV | `clm_csv_parse(text)` | `clm_csv_serialize(document)` |

The generic parser recognizes `yml` as an alias for `yaml`. Format names are matched without regard to letter case in the supplied implementation.

### 5.3 Optional convenience facade

A language may expose a `Formats` convenience facade with operations such as `parse`, `serialize`, `tokenize`, `build`, `get`, and `destroy`. This is only a forwarding API; it does not introduce a second registry, object model, or parsing engine. The standalone functions remain the shared CLM specification.

## 6. Reusable lexical analysis

`clm_lexer(text, rules)` returns tokens with their original source positions. The lexical rules are represented by:

```text
LexRules
    delimiters     : collection of delimiter strings
    inline_comment : optional comment marker
    indent_matters : boolean
```

Each emitted token has:

```text
Token
    kind   : STRING | WORD | SYMBOL | INDENT | NEWLINE
    text   : original token text
    line   : 1-based source line
    column : 1-based source column
```

The reusable lexer recognizes delimiters outside quotes, preserves quoted token text, skips inline comments outside strings, and reports an unterminated quoted string. When indentation matters, it emits an `INDENT` token for each processed line. A `NEWLINE` token marks each input line.

The supplied lexical profiles are:

| Rules | Delimiters | Inline comments | Indentation |
|---|---|---|---|
| `JSON_RULES` | `: , { } [ ]` | None | Ignored |
| `YAML_RULES` | `: - [ ] { } ,` | `#` | Significant |
| `TOML_RULES` | `= [ ] , { }` | `#` | Ignored |
| `INI_RULES` | `= : [ ]` | `;` | Ignored |
| `CSV_RULES` | `,` | None | Ignored |

**Lexing is not parsing.** These settings do not, by themselves, validate a JSON document, resolve YAML nesting, interpret TOML tables, or preserve CSV multiline quoted cells. The current `clm_lexer` processes individual lines and is not suitable as a complete CSV decoder. The format-specific CSV decoder has its own multiline-aware state machine.

## 7. Shared tree-builder behavior

`clm_tree_build(value)` traverses decoder-created intermediate nodes and scalars to construct a persistent document:

1. Allocate a `CONTAINER_TYPE_OBJECT` container for each object or array node.
2. For an object, look up every field name through `clm_id_get` and insert a `Field(original_name, child_node)` under its integer ID.
3. For an array, insert each child node under its zero-based index.
4. Preserve scalar values as native scalar values within nodes.
5. Record each allocated container ID in the owning document.
6. If construction fails, destroy the containers allocated so far before reporting the error.

In the supplied version, supported scalar values are null, string, boolean, integer, and floating-point. There is no arbitrary object serialization contract.

### Access and lifetime

- `clm_tree_get(object_node, field_id)` retrieves that member's **value node**.
- `clm_tree_get(array_node, index)` retrieves the element node.
- Looking up a missing entry fails; looking up children of a scalar fails.
- `clm_tree_destroy` invalidates the document's container-backed nodes and releases its container registrations. It does not necessarily invalidate already copied independent scalar values.
- `clm_format_serialize` rejects a destroyed document. Callers should not attempt to traverse destroyed container nodes.

### Iteration

Array and object members are emitted in container iteration order; the supplied implementation uses **insertion order**. Its `clm_container` iterator maintains a **shared cursor per container**, so iteration of the same container must not be nested or interleaved with mutation. Port authors must preserve the observable output/order contract when required or document any intentional divergence.

## 8. Format contracts and supported syntax

This section documents the **current supplied implementation**, not an assertion of complete conformance to every formal format specification.

### 8.1 JSON

**Decoding:** objects, arrays, double-quoted strings, booleans, null, integer and floating-point literals, with nested values and whitespace. Object keys must be double-quoted. Duplicate field names and trailing commas are rejected. JSON string escapes include common escapes and Unicode `\uXXXX` sequences with surrogate-pair validation.

**Encoding:** produces objects, arrays, and scalar JSON values, escaping necessary string characters. Non-finite floating-point values are rejected by the serializer.

**Constraints:** comments are not supported. Array indexes are implicit. Field names must be registered with `clm_id` before decoding object members. This is a custom JSON implementation; formal RFC 8259 conformance must be established through a separate conformance test suite rather than assumed from supported features.

### 8.2 YAML

**Decoding:** a practical, indentation-sensitive subset supporting nested block mappings, block sequences, basic scalar values, simple inline collections, and single/double-quoted forms in the supported parser paths. `#` comments outside quotes are removed. Document separator markers `---` and `...` are skipped. Tabs are rejected when used for indentation.

**Encoding:** writes block-oriented YAML with two-space indentation, quoted mapping keys, and basic scalar/array/object values.

**Not provided as full YAML support:** anchors, aliases, tags, complex keys, block scalar styles (`|`, `>`), multi-document streams, and the full YAML scalar-resolution and quoting rules. In particular, several sequence-of-mapping and inline-flow constructs supported by general YAML parsers are not implemented here.

**Caution:** a YAML decoder may interpret an unquoted scalar as a boolean, number, or null. Quote text that must remain a string; test representative configuration inputs.

### 8.3 TOML

**Decoding:** section headers, nested/dotted table headers, basic arrays, strings, booleans, and number literals. Values are interpreted using shared inline-value parsing helpers.

**Encoding:** emits scalar/array assignments and nested table sections. Object nodes represent tables.

**Currently unsupported or incomplete:** arrays of tables (`[[table]]`), dotted assignment keys, date/time types, comprehensive TOML numeric forms, multiline strings, and various valid TOML constructs. TOML does not define a null scalar, and the serializer rejects it.

### 8.4 INI

**Decoding:** `[section]` headers containing key/value assignments separated by `=` or `:`. Values are stored as strings. Entire lines beginning with `#` or `;` are ignored. Duplicate members are rejected. The supplied decoder requires assignments to belong to a section.

**Encoding:** an object root whose children are section objects, each containing scalar properties. Section and key names are validated against the subset's structural restrictions. Multiline values are rejected.

**Constraints:** the current codec does not implement interpolation, inheritance, inline comments, configurable delimiters, or a global section. INI does not have one universal standardized grammar; these rules define the CLM-supported subset.

### 8.5 CSV

**Decoding:** the first row contains unique, nonempty column names; subsequent rows become object nodes, and the root is an array of rows. A character-level parser supports quoted fields, escaped quotes (`""`), commas, CR/LF line endings, and embedded newlines inside quoted values. Each data row must contain the same number of cells as the header.

**Encoding:** outputs the header from the first row object, followed by data rows. Column order and names must match in every row; each cell must be scalar. Values requiring quoting are quoted, and quotes are doubled. Generated output ends with a newline when nonempty.

**Constraints:** all decoded CSV cells remain strings; CSV does not carry numeric or boolean types automatically. The supported format assumes comma delimiters and header-based tables. Empty datasets do not preserve standalone header metadata in the existing document model.

### 8.6 Capability matrix

| Capability | JSON | YAML | TOML | INI | CSV |
|---|---|---|---|---|---|
| Nested objects | Yes | Limited | Tables | Sections only | No |
| Arrays | Yes | Limited | Basic arrays | No | Rows as root array |
| String scalars | Yes | Yes | Yes | Yes | Yes |
| Numeric/bool/null values | Yes | Limited | Numeric/bool; no null | Strings only | Strings only |
| Comments | No | `#` outside quotes | `#` outside quotes | Full-line `#` / `;` | No |
| Multiline cell/field | JSON escapes | Limited | Limited | No | Quoted CSV fields |
| Round-trip serialization | Supported subset | Supported subset | Supported subset | Supported subset | Supported subset |

A round trip means that a supported document can be decoded, encoded, and decoded again with equivalent supported values and structure. It does **not** promise preservation of original whitespace, comment placement, delimiter style, source spelling of numeric literals, or all other textual formatting.

## 9. Usage examples

### 9.1 Parse, read, and destroy

```text
register field name "name" → FIELD_NAME
register field name "age"  → FIELD_AGE

doc = clm_json_parse('{"name":"Ada","age":35}')
try:
    name = clm_tree_get(doc.root, FIELD_NAME)
    age  = clm_tree_get(doc.root, FIELD_AGE)
    assert name.value == "Ada"
    assert age.value == 35
finally:
    clm_tree_destroy(doc)
```

### 9.2 Convert between formats

Conversion works when the **tree shape and scalar types are supported by both source and destination formats**:

```text
doc = clm_yaml_parse(yaml_text)
try:
    json_text = clm_json_serialize(doc)
finally:
    clm_tree_destroy(doc)
```

Converting an arbitrary JSON root array to TOML or INI is not supported because those serializers require object roots with format-specific structures. A CSV table becomes an array of row objects and cannot automatically become an INI section hierarchy without transforming the tree.

### 9.3 CSV access

```text
register "name" → FIELD_NAME
register "city" → FIELD_CITY

doc = clm_csv_parse("name,city\nAda,London\nLin,Taipei\n")
try:
    first_row = clm_tree_get(doc.root, 0)
    first_name = clm_tree_get(first_row, FIELD_NAME)
    assert first_name.value == "Ada"
finally:
    clm_tree_destroy(doc)
```

## 10. Errors and validation

The supplied implementation reports parsing, unsupported-value, and CLM registration problems through `FormatError`, with some invalid calls producing type or key errors. Other language implementations may expose **status codes, tagged results, or exceptions**, but should preserve the meaning of these error conditions.

| Condition | Expected outcome |
|---|---|
| Unsupported format name | Reject with a format error. |
| Malformed source syntax | Reject; include source position where available. |
| Unregistered object member name | Reject during ID mapping/tree construction. |
| Duplicate source name or colliding numeric ID inside one object | Reject; never silently replace an entry. |
| Unsupported scalar or structural representation | Reject explicitly. |
| Invalid container creation or insertion | Propagate/translate a meaningful failure; roll back newly allocated containers. |
| Missing tree child | Indicate missing key/index. |
| Accessing children of a scalar | Reject. |
| Serializing a destroyed document | Reject. |
| Serializing data unsupported by target format | Reject rather than silently discard it. |

For cross-language portability, define a consistent error vocabulary even when a language has different error-reporting mechanisms. The uploaded source does **not** define a portable numeric `FormatStatus` enumeration, so this document does not invent one.

## 11. Ownership and concurrency

**Ownership:** Every successful parse or tree build returns a document that owns the containers created for its nodes. The caller is responsible for destroying the document. Destruction is idempotent in the reference implementation. Implementations in languages with explicit memory management must release any node payloads or buffers they own, according to their ownership contract.

**Document scope:** Keep the document alive while traversing or serializing its container-backed nodes. Do not reuse a destroyed document. Container IDs are internal implementation details and should not be treated as stable external identifiers.

**Concurrency:** The supplied `clm_container` maintains a cursor per container. Interleaved traversal of the same node can interfere with iteration. Concurrent mutation or destruction during traversal is unsafe without synchronization. Implementations must define and document their thread-safety and async-concurrency behavior; a globally available registry does not automatically imply safe concurrent access.

**Storage choices:** Port implementations may use native arrays, vectors, slices, structures, pointers, or other language-native representations for lexing and temporary nodes. They should retain CLM containers as the authoritative persistent tree storage if claiming compatibility with this specific module's storage contract.

## 12. Implementation guidance for contributors

A new language port should:

1. Preserve public function names and conceptual record fields where language conventions permit.
2. Implement field-name resolution through `clm_id`, not an independent name registry.
3. Back persistent object/array nodes with CLM object containers and integer keys.
4. Preserve distinctions between scalar, object, and array nodes.
5. Store source field spellings to permit re-serialization.
6. Support the specified subset of each format, or clearly declare any narrower support.
7. Implement safe string quoting/unquoting and malformed-input rejection.
8. Guarantee deterministic cleanup on parsing failure and explicit document destruction.
9. Avoid hidden dependency on third-party format parsers if implementing the standalone CLM codecs.
10. Add compatibility tests using the same representative documents and failure cases across languages.

The architecture should be idiomatic for the implementation language; it need not duplicate Python's internal classes, list types, or recursion strategy.

## 13. Compatibility test checklist

Recommended tests for every implementation include:

- Lexing quotes, delimiters, inline comments, indentation, and source line/column positions.
- Parsing and serializing nested JSON objects and arrays, booleans, null, numeric values, and Unicode escapes.
- Rejecting malformed JSON, unexpected tokens, duplicates, and trailing commas.
- YAML indentation, comments in and out of quotes, nested mappings, sequences, and empty objects/arrays.
- TOML headers, nested tables, arrays, and rejection of unsupported TOML constructs.
- INI section/key behavior, string-only values, and duplicate keys.
- CSV header mapping, quoted delimiters, escaped quotes, multiline cells, empty fields, and invalid row lengths.
- Rejection of an object field name missing from `clm_id`.
- Rejection of field-name aliases that map to the same ID inside one object.
- Correct zero-based indexing for arrays and preservation of insertion order.
- Rejection of target-format-incompatible structures or values.
- Correct cleanup after a successful parse, failed tree build, and repeated destroy.
- Concurrent access safety according to the implementation's documented guarantees.

## 14. Reference-version considerations

The current source has a few limitations that deserve explicit attention when extending the module:

- Its public `clm_lexer` and internal decoder tokenization are **not yet a single unified lexer pipeline**.
- The YAML and TOML implementations are **subset parsers**, not complete standards implementations.
- The CSV document model stores data rows but not independent header metadata, so an empty, header-only CSV cannot be faithfully reconstructed.
- Container traversal relies on shared cursor state, making interleaved access to the same container problematic.
- The current serializer validates the document lifecycle, but direct `clm_tree_get` access is based on a node rather than the owning document; users must enforce the lifetime externally.
- The public tree builder accepts internal node representations, not ordinary language-native dictionaries/maps.

These notes describe the **supplied reference implementation**; they are not additional requirements to emulate an avoidable bug or design limitation in other languages.

---

**CLM — Cross Language Modules**  
**Different languages. Shared technologies. No borders.**  
*Don't start from scratch, bro.*
