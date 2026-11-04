# CLM Image PNG

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_bytes](clm_bytes.md), [clm_image](clm_image.md)
- **External libraries / platform facilities (C reference):** PNG lossless compression backend through the CLM codec implementation (library choice is implementation-specific).

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Converts between PNG-encoded data and CLM CPU-resident image resources.

## Capabilities and operations

- Decode PNG from an in-memory byte buffer or file into an image ID.
- Encode an existing image as PNG into memory or a file.
- Accept or emit Base64-encoded PNG text, including CLM string resources.
- Keep image identity and pixel storage separate from the encoded representation.

## Resource, data and ownership model

The PNG format adapter reads an encoded stream and creates a decoded `clm_image`, or takes a pre-existing image and serializes its pixels. Output bytes are owned by the receiving buffer/string or caller as specified by the binding.

## Behavioral contract and boundaries

PNG data validity, image dimensions, buffer limits and unsupported pixel representations must produce distinguishable outcomes. The portable contract is PNG interoperation, not a mandate to use the same compression engine or Base64 algorithm in every implementation.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Decode a PNG file into `clm_image`, transform its pixels, and save it as a new PNG.

## Related CLM modules

[clm_bytes](clm_bytes.md), [clm_image](clm_image.md).
