# clm_image_png

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

PNG parsing, image conversion, and PNG serialization.

## Behavior and constraints

PNG parsing, image conversion, and PNG serialization.

Chunk lengths and image dimensions are validated before allocating pixel storage.
Filtering and compression belong to the PNG representation, not the image registry.

## Integration

- C reference: [**clm_image_png**](../c/image_png.md).
- Related CLM declarations in this archive: `clm_bytes`, `clm_image`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
