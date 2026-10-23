# CLM — Cross Language Modules

Public domain (CC0 1.0 Universal). https://github.com/deeperlevel/clm

> Don't start from scratch, bro.

# clm_csv

**One-line description:** Parse and serialize header-based CSV into CLM arrays of string-valued row objects.

## Public API

- `clm_csv_parse(text) -> Document`
- `clm_csv_serialize(document) -> text`

**Supported:** comma delimiter, first record as unique nonempty column names, quoted fields, doubled quotes, embedded commas and line breaks, LF/CRLF records. Every data row must have the same field count. All parsed cell values are strings. The tree root is an array of object rows; each object maps registered column names to scalar nodes. Empty input results in an empty array; serialization of that array produces an empty string (column names are not retained without rows). No type inference or custom delimiter.

```python
clm_csv_parse('name,role\nAda,Engineer\n')
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

`clm_csv.py` requires `clm_format.py`.
