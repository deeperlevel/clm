# clm_file

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native resources. _as accepts raw UTF-8 paths, _to exports caller-owned data, and _from consumes caller-owned data. Native handle representation remains private. Existing BPA binary blocks have fixed capacity; text outputs are existing CLM strings or string arrays. Whole-file outputs are prepared before publication. Size/position queries use 64-bit values; callers synchronize registry access. Status functions return SUCCESS/FAILURE/UNSUPPORTED; exists and is_open are direct predicates returning 1, 0, or -1. Streaming…

## Behavior and constraints

Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native
resources. _as accepts raw UTF-8 paths, _to exports caller-owned data, and
_from consumes caller-owned data. Native handle representation remains private.
Existing BPA binary blocks have fixed capacity; text outputs are existing CLM
strings or string arrays. Whole-file outputs are prepared before publication.
Size/position queries use 64-bit values; callers synchronize registry access.
Status functions return SUCCESS/FAILURE/UNSUPPORTED; exists and is_open are
direct predicates returning 1, 0, or -1. Streaming transfers expose partial
progress and writes are not transactional. Closed IDs remain retired until
clm_file_shutdown; shutdown attempts every close before releasing registry state.
Text input/output in memory is UTF-8; encoding selects the file representation.
BOM variants require/emit the matching BOM; append never inserts a BOM into
existing content. UTF16 compatibility names mean explicit little-endian UTF-16.
Text conversion rejects malformed input and embedded NUL. Line helpers split
CRLF, LF, and CR, omit delimiters, and do not invent a final empty line; append
line helpers append exactly one LF, encoded for the selected file encoding.
Whole-file calls open and close internally; they do not register open-file IDs.
Native opens reject final-component links/reparse points rather than follow
them implicitly. Windows creation inherits parent ACLs; POSIX uses mode 0600.
Platform .inc backends are compiled through file.c, not as standalone sources.
Registry teardown is explicit; it is not automatically tied to BPA shutdown.

## Integration

- C reference: [**clm_file**](../c/file.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_gen`, `clm_path`, `clm_string`, `clm_stringtify`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
