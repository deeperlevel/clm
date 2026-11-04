# CLM Shader

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Loads shader data into reusable CLM resource IDs with explicit format and stage metadata, leaving compilation and GPU pipeline creation to backends.

## Capabilities and operations

- Load shader assets from memory or file.
- Identify shader format and stage.
- Query stored shader metadata and source/binary size.

## Resource, data and ownership model

A shader asset is data. It is not automatically an initialized GPU program, pipeline, linked shader set or binding layout.

## Behavioral contract and boundaries

Formats and stages must be validated, and unsupported formats reported without assuming runtime compilation support. Renderer-specific shader language, compilation diagnostics and device lifetime are separate concerns.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Store a vertex shader asset and pass its ID to a future renderer pipeline builder.

## Related CLM modules

[clm_resource](clm_resource.md).
