# clm_math_matrix

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Matrix arithmetic, elimination, and affine projection.

## Behavior and constraints

Matrix arithmetic, elimination, and affine projection.

Matrix dimensions are validated before indexing. Pivot selection and singularity checks
control inversion and elimination failures.

## Integration

- C reference: [**clm_math_matrix**](../c/math_matrix.md).
- Related CLM declarations in this archive: `clm_math_core`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
