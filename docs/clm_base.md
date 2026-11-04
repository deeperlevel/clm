# CLM Base

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** None.
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

CLM’s foundational vocabulary of numeric IDs, numeric ranges, scalar types and small coordinate/size/rectangle-like value structures.

## Capabilities and operations

- Provides a shared fixed-width type model for byte, signed/unsigned integer and floating-point values.
- Defines 64-bit resource identifiers and basic position, dimension, size and rectangle structures in multiple numeric precisions.
- Contains declarations and value conventions, not a resource allocator or a name-to-ID service.
- Identifier registration and managed-resource lifetime belong to their respective modules.

## Value model

- **Identifiers:** CLM resources are identified by 64-bit unsigned integer values. An ID is not a pointer or an object lifetime.
- **Numbers:** shared terminology covers signed and unsigned integers of standard widths and single/double-precision floating-point values.
- **Spatial values:** position, dimensions and related value structures are available in multiple numeric precisions and should retain their field order and meaning across ports.
- **No runtime service:** name registration, allocation, dependency resolution and concurrency belong to other modules.

## Portability notes

The *meaning* and valid range of each type should remain stable; an implementation may express them through native equivalents. Avoid assuming that every host language has the same built-in integer width or default overflow behavior.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Provides a shared fixed-width type model for byte, signed/unsigned integer and floating-point values.
