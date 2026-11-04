# CLM Math Collision 3D

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md), [clm_math_geometry](clm_math_geometry.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Supplies collision tests for 3D volumes and geometric primitives, complementing the separate 2D collision module.

## Capabilities and operations

- Test points against spheres and axis-aligned/oriented boxes.
- Test sphere/sphere, box/box and box/sphere overlap.
- Intersect rays against spheres, boxes and triangles.
- Intersect line segments with supported volumes.
- Perform supported 3D swept-sphere and swept-AABB tests.

## Resource, data and ownership model

Inputs are geometric values such as 3D vectors, rays, spheres, triangles, AABBs and OBBs. Results are query outcomes, not mutable colliders with their own lifecycle.

## Behavioral contract and boundaries

Contact detection is separate from physics response. Different tests have different output detail, so language ports should preserve observable hit/miss and parameter meaning rather than force a universal hit record.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Determine which scene object a camera ray intersects before the renderer draws a selection highlight.

## Related CLM modules

[clm_math_core](clm_math_core.md), [clm_math_geometry](clm_math_geometry.md).
