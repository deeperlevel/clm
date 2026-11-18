# clm_lighting

> Language-neutral module overview and behavioral contract.

## Purpose

Light descriptions and collections of scene lights.

## Behavior and constraints

Light descriptions and collections of scene lights.

Configuration initialization and validation are implemented; resource operations report
NOT_IMPLEMENTED where storage or backend support is not yet supplied.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_drawing`](drawing.md)
- [`clm_resource`](resource.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_lighting. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_drawing](drawing.md) - required CLM capability.
- [clm_resource](resource.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_lighting when an application needs: Light descriptions and collections of scene lights.

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
