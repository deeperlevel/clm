# CLM Box

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_spatial_manager](clm_spatial_manager.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Defines bounded 3D box shapes that are registered in an invisible spatial manager for queries and association with application objects.

## Capabilities and operations

- Create/destroy a box belonging to a spatial manager.
- Read or replace its center, dimensions, orientation and mobility/association metadata.
- Move, rotate and scale existing geometry while updating query caches.
- Control which box faces are eligible for picking/hit reporting.

## Resource, data and ownership model

A box has one shape ID in a shared 3D shape namespace and belongs to exactly one manager. A box is a geometric/query resource rather than a rendered mesh. Orientation is represented by a quaternion, and its dimensions are positive.

## Behavioral contract and boundaries

The spatial manager computes hit results and owns cached query structures. Transform changes update geometry immediately, but precomputed tag-locality memberships must be explicitly prepared again where relevant. Face selection affects picking, not the shape being a solid for other geometric operations.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Register an oriented box in a room manager, associate it with an application object ID, then perform a ray cast to determine which face was selected.

## Related CLM modules

[clm_spatial_manager](clm_spatial_manager.md).
