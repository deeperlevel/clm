# clm_math_geometry_common

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Shared geometric comparison and numeric boundary policy for multidimensional geometry.

## Behavior and constraints

Provides common numeric rules used to keep containment and overlap decisions consistent across geometry routines. The C reference includes an inclusive, relative-roundoff-aware comparison for finite floating-point values.

This is a **shared support capability** for geometry modules rather than a full independent set of 2D or 3D shape operations.

## Integration

- Related capabilities: math core, 2D and 3D geometry.
- [C reference](../c/math_geometry_common.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
