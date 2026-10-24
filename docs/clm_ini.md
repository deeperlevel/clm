# CLM INI

## Public API

- `clm_ini_parse(text) -> Document`
- `clm_ini_serialize(document) -> text`

**Supported subset:** `[section]` headers, `key=value` or `key:value` assignments, blank lines, full-line `#` or `;` comments. All values remain strings; no interpolation, nested sections, global/default-section keys, escaping, or multiline values. Duplicate sections and keys are errors. The serializer uses `key = value` syntax.

```python
clm_ini_parse('[display]\nwidth=800\n')
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

`clm_ini.py` requires `clm_format.py`.
