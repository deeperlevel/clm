# CLM — Cross Language Modules

Public domain (CC0 1.0 Universal). https://github.com/deeperlevel/clm

> Don't start from scratch, bro.

# clm_format

**One-line description:** Reusable lexical rules, tokenization, and ID-mapped owned document trees shared by independent data-format codecs.

## Functions and types

| API | Purpose |
|---|---|
| `LexRules(delimiters, inline_comment, indent_matters)` | Lexer configuration |
| `Token(kind, text, line, column)` | Source token (one-based location) |
| `clm_lexer(text, rules)` | Tokenize text; not a format validator |
| `Node(kind, value?, container_id?)` | Scalar, array, or object representation |
| `Field(name, value)` | Object entry with original field name |
| `Document(root, owned, destroyed)` | Owned CLM tree and resource registry |
| `clm_tree_build(value)` | Build from scalar or internal `_Object`/`_Array` representation; does **not** accept Python dict |
| `clm_tree_get(node, key)` | Retrieve a child by field ID / array index |
| `clm_tree_destroy(document)` | Explicit, idempotent resource release |
| `clm_format_parse(name, text)` | Optional compatibility dispatcher to installed codec |
| `clm_format_serialize(name, doc)` | Optional dispatcher |

`clm_format` provides reusable lexer/inline-literal helpers; complete JSON/YAML/INI/TOML/CSV grammars belong in their respective modules. The compatibility dispatcher lazily imports a codec; the individual codecs do not import each other.

`clm_id` and `clm_container` are required. In Python, container traversal uses a shared cursor, so avoid simultaneously iterating/mutating one container from multiple threads.

## Cross-language contract

- Procedural public operations; a language may optionally expose a facade.
- A parsed document has a root node of kind `scalar`, `object`, or `array`.
- Object children are stored in a CLM OBJECT container keyed by pre-registered field IDs (`clm_id`). The original field spelling is preserved for serialization. An array's children use zero-based integer keys.
- Object keys must be registered **before** parsing/building. Duplicate field names, duplicate IDs in the same object, and unsupported structures are errors. Different names mapped to the same ID cannot coexist in one object.
- Containers belong to a document. `clm_tree_destroy(document)` releases all owned containers; using a destroyed document for serialization is an error. Do not retain node handles after destruction.
- Implementations may use native parsing/storage internals but must preserve documented observable behavior. Text is Unicode; field IDs and array indices are integers.
- Failure must be reported (Python uses `FormatError`; other languages may use status/result types). A parser must clean up partially created document containers on failure.
- Round trips preserve supported *values* and object field names/order, not exact source formatting, comments, or whitespace.

## Dependencies

`clm_format.py` requires `clm_container.py` and `clm_id.py`.
