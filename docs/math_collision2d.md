# clm_math_collision2d

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Two-dimensional overlap, ray, segment, and sweep tests.

## Behavior and constraints

Two-dimensional overlap, ray, segment, and sweep tests.

Oriented shapes are reduced to local coordinates where appropriate. Contact times and
normals describe query geometry rather than a collision-response solver.

## Integration

- C reference: [**clm_math_collision2d**](../c/math_collision2d.md).
- Related CLM declarations in this archive: `clm_math_core`, `clm_math_geometry2d`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
