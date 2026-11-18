# clm_image

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Image pixel storage and reference-counted image resources.

## Behavior and constraints

Image pixel storage and reference-counted image resources.

Image IDs refer to owned pixel allocations. Pixel layout and row stride describe how
consumers access the backing bytes.

## Integration

- C reference: [**clm_image**](../c/image.md).
- Related CLM declarations in this archive: `clm_resource`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
