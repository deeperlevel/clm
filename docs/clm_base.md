# CLM base

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_types](types.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

## Purpose

Fixed-width CLM scalar aliases and plain data geometry types. The scalar aliases intentionally fix serialized/FFI-visible widths independently of the native integer type sizes. Geometry types are lightweight value records: position = origin, size = extents, block = origin plus extents. The unqualified variants use signed 32-bit fields; explicit suffixes select other widths. No dynamic storage or ownership is associated with these records.

## Behavior and constraints

Fixed-width CLM scalar aliases and plain data geometry types.
The scalar aliases intentionally fix serialized/FFI-visible widths independently
of the native integer type sizes. Geometry types are lightweight value records:
position = origin, size = extents, block = origin plus extents. The unqualified
variants use signed 32-bit fields; explicit suffixes select other widths.
No dynamic storage or ownership is associated with these records.

## CLM dependencies and related modules

No direct dependency on another documented CLM module is identified in the current reference snapshot.

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_types. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- No direct CLM module dependency is declared by this module interface.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_types when an application needs: Fixed-width CLM scalar aliases and plain data geometry types. The scalar aliases intentionally fix serialized/FFI-visible widths independently of the native integer type sizes. Geometry types are lightweight value records: position = origin, size = extents, block = origin plus extents. The unqualified variants use signed 32-bit fields; explicit suffixes select other widths. No dynamic storage or ownership is associated with these records.

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
