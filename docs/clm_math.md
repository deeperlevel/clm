# CLM Math

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Scalar convenience mathematics forming the lightweight high-level layer above shared math conventions.

## Capabilities and operations

- Clamp, interpolate, apply smoothstep and wrap scalar values.
- Convert angles and compute common trigonometric, power, root, logarithmic and rounding functions.
- Surface domain errors and finite-value requirements where relevant.
- Vectors, matrices, geometry and other specialized mathematics belong to separate math modules.

## Math module family

The scalar operations in `clm_math` are deliberately separate from specialized modules:

- [clm_math_core](clm_math_core.md) — shared results and numeric storage adapters
- [clm_math_number](clm_math_number.md) — integer arithmetic and number theory
- [clm_math_vector](clm_math_vector.md) — vector arithmetic
- [clm_math_matrix](clm_math_matrix.md) — matrix transforms and inversion
- [clm_math_geometry](clm_math_geometry.md) — geometric queries
- [clm_math_collision](clm_math_collision.md) — 2D collision tests
- [clm_math_interpolation](clm_math_interpolation.md) — easing and interpolation
- [clm_math_quaternion](clm_math_quaternion.md) — quaternion rotation
- [clm_math_random](clm_math_random.md) — deterministic random numbers
- [clm_math_pathfinding](clm_math_pathfinding.md) — A* search
- [clm_math_physics2d](clm_math_physics2d.md) — elementary 2D dynamics

## Scalar behavior

Operations include clamp, linear interpolation, inverse interpolation, smoothstep, wrap, angle conversion, trigonometry, roots, powers, logarithms, exponential functions and rounding. Host-language math routines may implement them, but domain errors, precision and the contract for out-of-range values should be documented consistently.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_math_core](clm_math_core.md).

## Example use cases

Clamp, interpolate, apply smoothstep and wrap scalar values.
