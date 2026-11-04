# CLM Math Geometry

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math](clm_math.md), [clm_math_core](clm_math_core.md), [clm_math_matrix](clm_math_matrix.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

**Collision modules:** Geometric primitives feed the separate [2D](clm_math_collision2d.md) and [3D](clm_math_collision3d.md) collision-query interfaces.

## Purpose

Foundational 2D geometry and polygon queries.

## Capabilities and operations

- Measure point-to-segment distance and closest points on segments.
- Compute orientation of points and check point-in-polygon relations.
- Calculate polygon area and reject malformed coordinate shapes.
- Preserve sign and boundary conventions consistently across ports.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Measure point-to-segment distance and closest points on segments.
