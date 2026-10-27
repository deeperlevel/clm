# clm_math_vector — Vectors, 2D through 4D

CLM — Cross Language Modules · CC0 1.0 Universal · https://github.com/deeperlevel/clm

## Scope

Vectors, 2D through 4D. Python reference implementation; a language-agnostic functional contract, not a requirement to match the Python implementation strategy.

## Data representation

Vectors are caller-created, mutable, linear DOUBLE Storage resources with 2, 3 or 4 elements. Output storage must already exist.

**No public math operation receives an array, buffer, raw pointer, dictionary, or mutable vector object.** Structured inputs and outputs are identified by developer-assigned CLM Storage IDs. A function can return a single scalar, a `(status, scalar)` pair for resource-backed queries, or an integer status for mutations. Python scalar tuples (such as extended-GCD coefficients) are returned by value, not passed as buffers.

## Behavioral contract

Dot requires matching dimensions. Cross requires three components. Zero-vector normalization returns MATH_ERROR_DOMAIN. Reflect accepts a non-unit normal. Alias between any source and destination is supported.

For all Storage operations, the destination must be previously created as `STORAGE_TYPE_DOUBLE` (or `STORAGE_TYPE_LONG_INTEGER` where explicitly stated), writable, and of linear kind. Result length may be resized automatically. Operations read all source values before writing, so source/destination ID aliasing is supported. Errors arising before a write leave the destination unchanged. All Storage resources are owned by the caller; math functions neither create nor destroy them.

## Functions

- `clm_math_vector_set(destination, *components)`
- `clm_math_vector_add(first, second, destination)`
- `clm_math_vector_subtract(first, second, destination)`
- `clm_math_vector_scale(source, factor, destination)`
- `clm_math_vector_dot(first, second)`
- `clm_math_vector_length(source)`
- `clm_math_vector_distance(first, second)`
- `clm_math_vector_normalize(source, destination)`
- `clm_math_vector_cross(first, second, destination)`
- `clm_math_vector_reflect(direction, normal, destination)`
- `clm_math_vector_lerp(first, second, t, destination)`

## Error handling

Storage-based mutation functions return `MATH_SUCCESS=0`, `MATH_ERROR_RESOURCE=1`, `MATH_ERROR_DIMENSION=2`, `MATH_ERROR_DOMAIN=3`, `MATH_ERROR_VALUE=4`, `MATH_ERROR_SINGULAR=5`, or `MATH_ERROR_NOT_SUPPORTED=6`. Resource-based scalar queries return `(status, value)` and `None` as the value on failure. Scalar-only functions in the Python reference generally return their result directly and raise native exceptions for invalid values. A cross-language scalar error ABI must be explicitly designed if exception-free C compatibility is required; do not assume exceptions cross a C ABI.

## Numerical portability

DOUBLE is binary64; floating-point results should be compared using documented tolerances, not bitwise identity. Trigonometric/elementary math follows native IEEE-754-capable runtime behavior and may vary in last bits. Dimensions, coordinate conventions, and output ordering MUST remain identical across ports.

## Security

This is mathematical utility code, not an audited security implementation. Do not use general-purpose algorithms here as drop-in constant-time cryptographic primitives.
