# CLM compress

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_compress](compress.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

## Purpose

Compression codecs, incremental transforms, and ZIP helpers.

## Behavior and constraints

Compression codecs, incremental transforms, and ZIP helpers.

Codebinding-specific state is adapted to common buffer and stream interfaces. Callers must
distinguish output exhaustion from completed input.

Compression for bytes, strings, streams and ZIP archives.

Formats (vendored libraries under ext/):
RAW_DEFLATE, ZLIB, GZIP   miniz     HTTP "deflate" is ZLIB, "gzip" is GZIP
LZ4                       lz4       LZ4 frame format (with content checksum)
BROTLI                    brotli    HTTP "br"
ZIP archives              miniz     in memory, see clm_compress_zip_*
Output is the standard format of each library, so it interoperates with other
tools. memlz and uzlib are not used: memlz is marked beta and uzlib duplicates
what miniz already provides.

Decoding treats its input as hostile: output is capped (max_output) so a tiny
input cannot expand without bound, corrupt, truncated and trailing data are
reported, and nothing is written past the cap. The incremental codec and the
stream adapters work in bounded steps, so they suit nonblocking transfers.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bytes`](bytes.md)
- [`clm_stream`](stream.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_compress. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_bytes](bytes.md) - required CLM capability.
- [clm_stream](stream.md) - required CLM capability.
- [clm_types](types.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_compress when an application needs: Compression codecs, incremental transforms, and ZIP helpers.

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
