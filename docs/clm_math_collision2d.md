# CLM Math Collision 2D

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md), [clm_math_geometry](clm_math_geometry.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides deterministic geometric collision tests on 2D points, axis-aligned/oriented boxes, circles, segments and convex shapes.

## Capabilities and operations

- Test containment and overlap between common 2D primitives.
- Intersect rays or segments with AABBs, OBBs and circles.
- Check segment/segment and convex-polygon interactions.
- Perform supported swept-shape collision queries.
- Offer direct-value and CLM-managed data variants for selected operations.

## Resource, data and ownership model

Each test receives geometric inputs and returns geometric collision information; no persistent physics world is implied. AABB and OBB use different orientation assumptions; a sweep adds relative motion to the static geometry.

## Behavioral contract and boundaries

The module answers whether/where shapes intersect and, where implemented, their contact or sweep parameters. It does not integrate velocity, resolve interpenetration, trigger application events or manage entities.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Cast a pointer ray against rotated sprites, or sweep a moving circle against a stationary circle to find first contact.

## Related CLM modules

[clm_math_core](clm_math_core.md), [clm_math_geometry](clm_math_geometry.md).
