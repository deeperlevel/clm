# clm_imagelib

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Dependency-free, out-of-place RGBA8 image manipulation by image IDs.

## Behavior and constraints

Dependency-free, out-of-place RGBA8 image manipulation by image IDs.

Every operation requires nonzero input IDs and a destination ID distinct from
every input. Inputs must exist, but may share an ID with each other.
A missing destination is created; an existing image is replaced
atomically (including dimensions), preserving its ID and reference count.
Foreign BPA objects are never overwritten. Failure preserves all images.
Outputs are tightly packed, top-down straight-alpha RGBA8. Callers serialize
access as required by clm_image/BPA. No filesystem, codec or renderer is used.

Statuses are CLM_RESOURCE_*: INVALID_ARGUMENT for invalid selectors, ranges,
same IDs, or nonfinite parameters; NOT_FOUND for a missing source;
TYPE_MISMATCH for nonimage IDs; NO_MEMORY for allocation failure.
All adjustments preserve alpha except explicit invert/invert_alpha.

## Integration

- C reference: [**clm_imagelib**](../c/imagelib.md).
- Related CLM declarations in this archive: `clm_image`, `clm_math_image`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
