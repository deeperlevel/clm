# CLM Texture

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_image](clm_image.md), [clm_image_png](clm_image_png.md), [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Manages texture-oriented pixel data under resource IDs, which a renderer may later upload to GPU-native objects.

## Capabilities and operations

- Create a texture source from PNG-encoded file/memory data.
- Copy pixel data from an existing CPU image.
- Read texture dimensions, format and pixel payload.
- Retain/release texture sources under a common resource lifecycle.

## Resource, data and ownership model

The **current C reference stores texture source pixels on the CPU**, even though a renderer may use them to initialize GPU texture handles. The CLM resource ID is not itself a portable GPU handle.

## Behavioral contract and boundaries

The current format is tightly packed RGBA8. Image-to-texture conversion copies pixels, so the original image can be released independently. Texture creation does not guarantee an immediate GPU allocation or a live graphics context. Any future dedicated GPU texture contract must specify ownership separately.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Create a texture source from a PNG image, release the source image, then draw it through a renderer backend that supports sprite uploads.

## Related CLM modules

[clm_image](clm_image.md), [clm_image_png](clm_image_png.md), [clm_resource](clm_resource.md).
