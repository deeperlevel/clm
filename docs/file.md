# clm_file

> Language-neutral module overview and behavioral contract.

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
Filesystem integration is implemented by the module rather than exposed as a separate public contract.
Registry teardown is explicit; it is not automatically tied to BPA shutdown.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bpa`](bpa.md)
- [`clm_gen`](gen.md)
- [`clm_path`](path.md)
- [`clm_string`](string.md)
- [`clm_stringtify`](stringtify.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_file. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.
- [clm_path](path.md) - required CLM capability.
- [clm_bpa](bpa.md) - required CLM capability.
- [clm_string](string.md) - required CLM capability.
- [clm_gen](gen.md) - required CLM capability.
- [clm_stringtify](stringtify.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_file when an application needs: Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native resources. _as accepts raw UTF-8 paths, _to exports caller-owned data, and _from consumes caller-owned data. Native handle representation remains private. Existing BPA binary blocks have fixed capacity; text outputs are existing CLM strings or string arrays. Whole-file outputs are prepared before publication. Size/position queries use 64-bit values; callers synchronize registry access. Status functions return SUCCESS/FAILURE/UNSUPPORTED; exists and is_open are direct predicates returning 1, 0, or -1. Streaming…

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
