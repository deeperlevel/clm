# clm_math_calculus

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Finite-difference derivatives, quadrature, and ODE steps.

## Behavior and constraints

Finite-difference derivatives, quadrature, and ODE steps.

Evaluation points must advance representably. Temporary results are staged so callback
and arithmetic failures do not partially overwrite caller outputs.

## Integration

- C reference: [**clm_math_calculus**](../c/math_calculus.md).
- Related CLM declarations in this archive: `clm_math_core`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
