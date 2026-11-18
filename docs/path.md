# clm_path

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Filesystem-path API using UTF-8 strings referenced by CLM IDs. Output and in-place parameters refer to string contents, not the ID value. Lexical operations do not query the filesystem unless explicitly documented. Operations return 0 on success and 1 on error; predicates return 0/1 or -1 on error. Output IDs must name existing string blocks. Aliasing is supported; failed operations leave their outputs unchanged. Lexical grammar is native, while explicit UNC helpers recognize Windows UNC syntax on every platform.

## Behavior and constraints

Filesystem-path API using UTF-8 strings referenced by CLM IDs.
Output and in-place parameters refer to string contents, not the ID value.
Lexical operations do not query the filesystem unless explicitly documented.
Operations return 0 on success and 1 on error; predicates return 0/1 or -1
on error. Output IDs must name existing string blocks. Aliasing is supported;
failed operations leave their outputs unchanged. Lexical grammar is native,
while explicit UNC helpers recognize Windows UNC syntax on every platform.

## Integration

- C reference: [**clm_path**](../c/path.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_platform`, `clm_string`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
