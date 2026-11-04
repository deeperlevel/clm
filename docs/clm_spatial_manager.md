# CLM Spatial Manager

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_math_collision3d](clm_math_collision3d.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Manages independent collections of 3D boxes and spheres for tags, localities, ray casting, overlap and containment queries.

## Capabilities and operations

- Create/destroy managers and inspect registered shapes.
- Register shapes and attach application IDs and optional user context.
- Tag shapes; precompute neighboring tag localities and select an active query scope.
- Cast rays globally, by tag or against a specific shape.
- Test containment/overlap and reflect a vector using a surface normal.

## Resource, data and ownership model

Manager IDs identify distinct spatial worlds. Box and sphere IDs share a shape namespace, and each shape belongs to one manager. Query-locality membership is a prepared snapshot; moving shapes updates geometry immediately but does not recompute membership until requested.

## Behavioral contract and boundaries

The manager is independent of rendering and physics. Zero tag selects global queries. Ray results include hit/miss, distance, point, normal, face and associated object metadata. Touching counts as overlap or containment. Calls require external synchronization in the reference. Query-locality preparation is explicit and must be repeated after relevant changes.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Tag interactive objects in a level, prepare nearby object localities, and ray-cast only that locality when the player points at an object.

## Related CLM modules

[clm_base](clm_base.md), [clm_math_collision3d](clm_math_collision3d.md).
