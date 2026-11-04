# CLM Image

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Stores mutable decoded raster pixels and metadata under CLM IDs, independent of any particular image file format or GPU.

## Capabilities and operations

- Create an image with a size and pixel format.
- Initialize pixels from an application-owned buffer.
- Read dimensions, format and stride information.
- Read or replace the complete pixel payload.
- Retain and release the image resource.

## Resource, data and ownership model

An image has dimensions, pixel format, row stride and storage size. Row padding, when present, belongs to its stored representation. Data is owned by the image after a successful create/copy operation.

## Behavioral contract and boundaries

The current reference supports straight-alpha RGBA8, top row first. A specified row stride must satisfy the stored-row layout, and the supplied byte count must equal row stride multiplied by height. Pixel read/write operations transfer complete stored buffers. This module neither uploads to the GPU nor decodes PNG by itself.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Decode a PNG into an image ID, inspect the image size, modify CPU pixels, then pass it as a source for a texture or renderer.

## Related CLM modules

[clm_resource](clm_resource.md).
