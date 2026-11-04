# CLM Sphere

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_spatial_manager](clm_spatial_manager.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Creates spherical query shapes managed by `clm_spatial_manager` without requiring a drawable scene or physics engine.

## Capabilities and operations

- Create/destroy a sphere in a manager.
- Read its center and radius.
- Move, reposition or resize a sphere.
- Associate sphere identity with application objects and tags through the manager.

## Resource, data and ownership model

A sphere is a solid world-space query shape, not a rendered mesh or dynamic rigid body. Its radius must remain positive and finite.

## Behavioral contract and boundaries

Transform updates refresh cached geometry, but prepared tag-localities retain snapshot membership until rebuilt. Shapes share a common ID namespace with boxes, so ID reuse across the two kinds is not independent.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Represent a spherical interaction volume around an object and ray-cast against it to determine selection.

## Related CLM modules

[clm_spatial_manager](clm_spatial_manager.md).
