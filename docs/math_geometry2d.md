# clm_math_geometry2d

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Two-dimensional geometric shapes, measurements, bounds, and transforms.

## Behavior and constraints

Supports geometric calculations involving rectangles, oriented boxes, lines, segments, triangles, polygons, ellipses, clipping regions, and transformed geometry. Operations include distance and intersection helpers, point containment, polygon area, and coordinate/layout calculations.

Several operations are pure calculations on values rather than stored resources. Their input contracts include finite coordinates and defined coordinate and boundary conventions. Some 2D array-based helpers integrate with numeric storage adapters. This module provides geometry calculations; it is not a physics simulation engine.

## Integration

- Related capabilities: math core, matrices, 2D numeric representations.
- [C reference](../c/math_geometry2d.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
