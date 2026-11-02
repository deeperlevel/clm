# CLM Math Matrix

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_math_core](clm_math_core.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Matrix operations and practical 2D transforms using numeric storage IDs.

## Capabilities and operations

- Build identity, rotation, translation and scale matrices.
- Multiply and transpose matrices; transform vectors.
- Calculate determinants and inverses, reporting singular matrices.
- Specify element layout and dimensions consistently between language implementations.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Build identity, rotation, translation and scale matrices.
