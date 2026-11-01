# CLM Math Interpolation

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Interpolation and easing for scalars and stored numeric sequences.

## Capabilities and operations

- Perform linear and inverse-linear interpolation and smoothstep.
- Apply ease-in, ease-out and ease-in-out curves.
- Evaluate cubic interpolation and interpolate stored vector data.
- Keep extrapolation, clamping and invalid-range behavior explicit.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_math](clm_math.md), [clm_math_vector](clm_math_vector.md).

## Example use cases

Perform linear and inverse-linear interpolation and smoothstep.
