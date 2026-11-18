# clm_gen

> Language-neutral module overview and behavioral contract.

## Purpose

Cryptographically randomized identifiers, ranges, text and passwords. Random data comes from platform cryptographic entropy, not rand()/srand(). Integer ranges include BOTH endpoints and use rejection sampling to avoid modulo bias. Output-writing _to variants use caller buffers; ID variants update existing BPA blocks. GENERATION_FAILURE reports generation/validation failure.

## Behavior and constraints

Cryptographically randomized identifiers, ranges, text and passwords.
Random data comes from platform cryptographic entropy, not rand()/srand().
Integer ranges include BOTH endpoints and use rejection sampling to avoid
modulo bias. Output-writing _to variants use caller buffers; ID variants update
existing BPA blocks. GENERATION_FAILURE reports generation/validation failure.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bpa`](bpa.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_gen. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.
- [clm_bpa](bpa.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_gen when an application needs: Cryptographically randomized identifiers, ranges, text and passwords. Random data comes from platform cryptographic entropy, not rand()/srand(). Integer ranges include BOTH endpoints and use rejection sampling to avoid modulo bias. Output-writing _to variants use caller buffers; ID variants update existing BPA blocks. GENERATION_FAILURE reports generation/validation failure.

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
