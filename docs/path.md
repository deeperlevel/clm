# clm_path

> Language-neutral module overview and behavioral contract.

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

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bpa`](bpa.md)
- [`clm_platform`](platform.md)
- [`clm_string`](string.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_path. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.
- [clm_bpa](bpa.md) - required CLM capability.
- [clm_string](string.md) - required CLM capability.
- [clm_platform](platform.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_path when an application needs: Filesystem-path API using UTF-8 strings referenced by CLM IDs. Output and in-place parameters refer to string contents, not the ID value. Lexical operations do not query the filesystem unless explicitly documented. Operations return 0 on success and 1 on error; predicates return 0/1 or -1 on error. Output IDs must name existing string blocks. Aliasing is supported; failed operations leave their outputs unchanged. Lexical grammar is native, while explicit UNC helpers recognize Windows UNC syntax on every platform.

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
