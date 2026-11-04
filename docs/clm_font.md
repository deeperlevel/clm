# CLM Font

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_compress](clm_compress.md), [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** WOFF 1.0 compressed table decoding uses clm_compress in the C reference (transitive codec dependency).

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Loads font container data and selected face metadata into reusable resource IDs without assuming a particular shaping or rasterization library.

## Capabilities and operations

- Load a font from memory or a file.
- Select a face from a standalone font or collection.
- Inspect selected face and stored source size.
- Read the normalized/stored source data.
- Retain and release a font resource through reference operations.

## Resource, data and ownership model

A font resource stores font source bytes and selected-face identity. It is not a shaped text run, glyph atlas or GPU font cache. The C snapshot stores selected source material through CLM resource/BPA services.

## Behavioral contract and boundaries

The current reference accepts SFNT TrueType/OpenType and TTC/OTC collections. WOFF 1.0 is decoded to SFNT; only face 0 is accepted for WOFF. WOFF2 is **not** supported in this snapshot. Validation covers container and table-directory bounds, not a complete glyph-program audit. Shaping, glyph rasterization, fallback fonts and metrics are outside the loader contract.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Load a bundled font, inspect its face metadata, then pass the font resource ID to an appropriate text-rendering backend.

## Related CLM modules

[clm_compress](clm_compress.md), [clm_resource](clm_resource.md).
