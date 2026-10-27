# clm_math_geometry — 2D geometry

CLM — Cross Language Modules · CC0 1.0 Universal · https://github.com/deeperlevel/clm

## Scope

2D geometry. Python reference implementation; a language-agnostic functional contract, not a requirement to match the Python implementation strategy.

## Data representation

Points are two-element DOUBLE Storage resources. Polygons are linear DOUBLE Storage containing interleaved x,y coordinates, minimum three vertices.

**No public math operation receives an array, buffer, raw pointer, dictionary, or mutable vector object.** Structured inputs and outputs are identified by developer-assigned CLM Storage IDs. A function can return a single scalar, a `(status, scalar)` pair for resource-backed queries, or an integer status for mutations. Python scalar tuples (such as extended-GCD coefficients) are returned by value, not passed as buffers.

## Behavioral contract

Polygon area is nonnegative regardless of winding. Points on polygon edges count as inside. Point-to-segment distance treats degenerate segments as points. A numerical epsilon of 1e-12 is used in Python boundary checks; ports should follow equivalent tolerance.

For all Storage operations, the destination must be previously created as `STORAGE_TYPE_DOUBLE` (or `STORAGE_TYPE_LONG_INTEGER` where explicitly stated), writable, and of linear kind. Result length may be resized automatically. Operations read all source values before writing, so source/destination ID aliasing is supported. Errors arising before a write leave the destination unchanged. All Storage resources are owned by the caller; math functions neither create nor destroy them.

## Functions

- `clm_math_geometry_distance_point_segment(point, start, end)`
- `clm_math_geometry_orientation(first, second, third)`
- `clm_math_geometry_point_in_polygon(point, polygon)`
- `clm_math_geometry_polygon_area(polygon)`
- `clm_math_geometry_closest_point_segment(point, start, end, destination)`

## Error handling

Storage-based mutation functions return `MATH_SUCCESS=0`, `MATH_ERROR_RESOURCE=1`, `MATH_ERROR_DIMENSION=2`, `MATH_ERROR_DOMAIN=3`, `MATH_ERROR_VALUE=4`, `MATH_ERROR_SINGULAR=5`, or `MATH_ERROR_NOT_SUPPORTED=6`. Resource-based scalar queries return `(status, value)` and `None` as the value on failure. Scalar-only functions in the Python reference generally return their result directly and raise native exceptions for invalid values. A cross-language scalar error ABI must be explicitly designed if exception-free C compatibility is required; do not assume exceptions cross a C ABI.

## Numerical portability

DOUBLE is binary64; floating-point results should be compared using documented tolerances, not bitwise identity. Trigonometric/elementary math follows native IEEE-754-capable runtime behavior and may vary in last bits. Dimensions, coordinate conventions, and output ordering MUST remain identical across ports.

## Security

This is mathematical utility code, not an audited security implementation. Do not use general-purpose algorithms here as drop-in constant-time cryptographic primitives.
