# clm_math_geometry3d

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Three-dimensional geometric shapes, measurements, bounds, and transforms.

## Behavior and constraints

Provides allocation-free operations for common 3D geometric primitives including axis-aligned boxes, oriented boxes, and sphere bounds. Examples include constructing an oriented box from a unit rotation, converting points into box-local coordinates, combining bounds, finding distances, and producing conservative bounds for spatial queries.

The C reference requires valid finite coordinates, positive dimensions where applicable, and orthonormal box axes. The module performs geometry calculations and can support higher-level collision or spatial systems; it does not itself implement a physics world.

## Integration

- Related capabilities: math core, matrices, common geometric comparisons.
- [C reference](../c/math_geometry3d.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
