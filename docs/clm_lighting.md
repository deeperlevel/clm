# CLM Lighting

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_drawing](clm_drawing.md), [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Defines light parameters and named groups of lights for rendering scenes.

## Capabilities and operations

- Initialize and create individual light resources.
- Get, update or delete a light configuration.
- Create a lighting collection.
- Add/remove light resource IDs in a collection and inspect its contents.

## Resource, data and ownership model

A lighting collection references light IDs without owning the underlying light resources; it represents a scene lighting arrangement. Light data is a rendering input rather than a light source simulated by physics.

## Behavioral contract and boundaries

A collection must cope with referenced light deletion without silently deleting other lights. Validity and unsupported light kinds should be distinguishable. Actual shading models and GPU upload strategies are renderer-specific.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Construct a key light and a fill light, collect them under one lighting ID, and attach that lighting ID to a scene.

## Related CLM modules

[clm_drawing](clm_drawing.md), [clm_resource](clm_resource.md).
