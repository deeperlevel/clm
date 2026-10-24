# CLM JSON

## Public API

- `clm_json_parse(text) -> Document`
- `clm_json_serialize(document) -> text`

**Supported:** objects, arrays, strings with JSON escapes and surrogate pairs, integer and floating numeric literals, booleans, null, arbitrary scalar root. Rejects unquoted keys, duplicate keys, trailing commas, invalid escapes, trailing text, non-finite values for serialization. Strict `RFC 8259`-style subset. Python numbers use native integers/floats; serialization of non-finite floats is rejected.

```python
from clm_id import clm_id_register
from clm_json import clm_json_parse, clm_json_serialize
from clm_format import clm_tree_get, clm_tree_destroy
clm_id_register("name", 10)
doc = clm_json_parse('{"name":"UNDER"}')
assert clm_tree_get(doc.root, 10).value == "UNDER"
print(clm_json_serialize(doc))
clm_tree_destroy(doc)
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

`clm_json.py` requires `clm_format.py`.
