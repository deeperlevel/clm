# CLM Math Quaternion

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Quaternion-based 3D orientation and vector rotation helpers.

## Capabilities and operations

- Construct identity and axis/angle quaternions.
- Normalize, conjugate and multiply quaternion values.
- Rotate vectors and interpolate orientations using spherical interpolation.
- Distinguish quaternion dimensions from ordinary 2D/3D vectors.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_math_core](clm_math_core.md).

## Example use cases

Construct identity and axis/angle quaternions.
