# clm_hash

> Language-neutral module overview and behavioral contract.

## Purpose

Deterministic message digests, HMAC and digest-to-text utilities. A digest does not encrypt data and cannot be reversed to recover its input. Legacy digest algorithms remain available for compatibility, NOT for creating new security-sensitive authentication schemes. HMAC deliberately restricts algorithms to SHA-256/384/512; output buffers and BPA string destinations have separate ownership and size rules.

## Behavior and constraints

Deterministic message digests, HMAC and digest-to-text utilities.
A digest does not encrypt data and cannot be reversed to recover its input.
Legacy digest algorithms remain available for compatibility, NOT for creating
new security-sensitive authentication schemes. HMAC deliberately restricts
algorithms to SHA-256/384/512; output buffers and BPA string destinations have
separate ownership and size rules.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bpa`](bpa.md)
- [`clm_string`](string.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_hash. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_bpa](bpa.md) - required CLM capability.
- [clm_string](string.md) - required CLM capability.
- [clm_types](types.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_hash when an application needs: Deterministic message digests, HMAC and digest-to-text utilities. A digest does not encrypt data and cannot be reversed to recover its input. Legacy digest algorithms remain available for compatibility, NOT for creating new security-sensitive authentication schemes. HMAC deliberately restricts algorithms to SHA-256/384/512; output buffers and BPA string destinations have separate ownership and size rules.

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
