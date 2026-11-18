# clm_math_image

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Allocation-free color, reconstruction and image-coordinate mathematics. Colors use normalized, straight-alpha sRGB components. Filtering accumulates premultiplied components to avoid transparent-edge color fringes. All checked functions preserve outputs on failure and use MATH_ERROR_VALUE for invalid pointers/nonfinite values, DOMAIN for unsupported selectors/ranges.

## Behavior and constraints

Allocation-free color, reconstruction and image-coordinate mathematics.
Colors use normalized, straight-alpha sRGB components. Filtering accumulates
premultiplied components to avoid transparent-edge color fringes. All checked
functions preserve outputs on failure and use MATH_ERROR_VALUE for invalid
pointers/nonfinite values, DOMAIN for unsupported selectors/ranges.

## Integration

- C reference: [**clm_math_image**](../c/math_image.md).
- Related CLM declarations in this archive: `clm_math`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
