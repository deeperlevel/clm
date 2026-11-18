# clm_math_collision3d

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Three-dimensional overlap, ray, segment, and sweep tests.

## Behavior and constraints

Three-dimensional overlap, ray, segment, and sweep tests.

Slab intervals and separating-axis tests reject disjoint shapes. Degenerate geometry and
initial overlap require explicit handling.

## Integration

- C reference: [**clm_math_collision3d**](../c/math_collision3d.md).
- Related CLM declarations in this archive: `clm_math_core`, `clm_math_geometry3d`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
