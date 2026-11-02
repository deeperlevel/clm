# CLM Math Vector

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_math_core](clm_math_core.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

ID-based vector arithmetic and geometric operations.

## Capabilities and operations

- Create/set vectors and perform addition, subtraction and scalar scaling.
- Compute dot product, cross product, length and distance.
- Normalize, reflect and linearly interpolate vectors.
- Enforce supported dimensions and preserve behavior for zero-length vectors.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Create/set vectors and perform addition, subtraction and scalar scaling.
