# CLM Math Geometry

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

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

## Related CLM modules

**Supporting modules:** [clm_math_core](clm_math_core.md).

## Example use cases

Measure point-to-segment distance and closest points on segments.
