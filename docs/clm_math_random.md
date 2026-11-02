# CLM Math Random

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_math_core](clm_math_core.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Deterministic seeded pseudorandom operations for simulations and reproducible calculations.

## Capabilities and operations

- Initialize per-ID generator state from a seed.
- Produce integer, floating-point and bounded random values.
- Preserve the algorithm and state transitions for cross-language reproducibility.
- Not a substitute for the cryptographic randomness in clm_crypto.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Reproduce identical pseudorandom simulation sequences from an agreed seed and algorithm.
