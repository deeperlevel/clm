# CLM Math

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

**Specialized modules:** Numerical calculus is described by [clm_math_calculus](clm_math_calculus.md), while collision and pathfinding have dimension-specific modules.

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

## Example use cases

Clamp, interpolate, apply smoothstep and wrap scalar values.
