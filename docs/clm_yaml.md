# CLM — Cross Language Modules

Public domain (CC0 1.0 Universal). https://github.com/deeperlevel/clm

> Don't start from scratch, bro.

# clm_yaml

**One-line description:** Parse and serialize a deliberately limited block-oriented YAML subset.

## Public API

- `clm_yaml_parse(text) -> Document`
- `clm_yaml_serialize(document) -> text`

**Supported subset:** indented mappings and sequences, scalar strings/numbers/booleans/null, JSON-style flow arrays/objects, `#` comments outside quotes, blank lines and simple `---`/`...` markers, empty collections. No anchors, aliases, tags, merge keys, block literal/folded scalars, complex keys, or general YAML 1.2 compliance. Indentation must use spaces. Quoted scalar strings follow the supported JSON-style escapes.

```python
clm_yaml_parse('person:\n  age: 22\n')
```

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

`clm_yaml.py` requires `clm_format.py`.
