# CLM Math Collision

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

2D shape-intersection tests for typical collision primitives.

## Capabilities and operations

- Test axis-aligned boxes, circles and point containment.
- Check circle–box intersections and ray–box intersections.
- Report intersection results separately from invalid geometry/storage failures.
- Treat touching-boundary semantics as part of the shared behavior contract.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_math_core](clm_math_core.md).

## Example use cases

Test axis-aligned boxes, circles and point containment.
