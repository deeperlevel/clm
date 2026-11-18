# CLM http_utils

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_http_utils](http_utils.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

## Purpose

HTTP message parsing, framing, and serialization.

## Behavior and constraints

HTTP message parsing, framing, and serialization.

Header syntax and message framing are validated independently of application routing.
Parsed spans can borrow the input buffer.

HTTP utilities: HTTP/1.x parsing/serialization/chunked coding (RFC 9112),
QUIC variable-length integers and decoded HTTP/3 field normalization.

Parsing is zero-copy and allocation-free: a parsed message holds spans that
point into the input and header entries stored in caller-owned memory. The
spans stay valid only while the input is unchanged. Parsing is strict:
CRLF line endings only, no obsolete line folding, no whitespace before the
header colon, and conflicting framing (Content-Length with Transfer-Encoding,
or repeated Content-Length values) is rejected to prevent request smuggling.
For simplicity and predictable framing, Transfer-Encoding supports only a
single 'chunked' field; other transfer codings are rejected. HTTP/1.1
requests require exactly one nonempty Host field.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_http_utils. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_http_utils when an application needs: HTTP message parsing, framing, and serialization.

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
