# clm_imagelib

> Language-neutral module overview and behavioral contract.

## Purpose

Dependency-free, out-of-place RGBA8 image manipulation by image IDs.

## Behavior and constraints

Dependency-free, out-of-place RGBA8 image manipulation by image IDs.

Every operation requires nonzero input IDs and a destination ID distinct from
every input. Inputs must exist, but may share an ID with each other.
A missing destination is created; an existing image is replaced
atomically (including dimensions), preserving its ID and reference count.
Foreign BPA objects are never overwritten. Failure preserves all images.
Outputs are tightly packed, top-down straight-alpha RGBA8. Callers serialize
access as required by clm_image/BPA. No filesystem, codec or renderer is used.

Statuses are CLM_RESOURCE_*: INVALID_ARGUMENT for invalid selectors, ranges,
same IDs, or nonfinite parameters; NOT_FOUND for a missing source;
TYPE_MISMATCH for nonimage IDs; NO_MEMORY for allocation failure.
All adjustments preserve alpha except explicit invert/invert_alpha.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_image`](image.md)
- [`clm_math_image`](math_image.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_imagelib. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_image](image.md) - required CLM capability.
- [clm_math_image](math_image.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_imagelib when an application needs: Dependency-free, out-of-place RGBA8 image manipulation by image IDs.

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
