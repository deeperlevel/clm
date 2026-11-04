# CLM Math Pathfinding 3D

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides shortest-path A* search on a dense voxel grid with six axis-aligned neighbors and unit movement cost.

## Capabilities and operations

- Search voxel grids with six-way (no diagonal) adjacency.
- Treat each blocked voxel as untraversable with uniform step cost for all free neighbors.
- Return a path including the start and goal or an explicit not-found result.

## Resource, data and ownership model

The grid has width, height and depth and an optional row-major blocked-voxel mask. Coordinates are zero-based integer cell locations. Returned path storage is owned by the caller and must be released explicitly.

## Behavioral contract and boundaries

Uses the Manhattan heuristic and treats X, Y and Z uniformly: it models free 3D movement rather than grounded jumping or gravity. This is a dense voxel search, not an arbitrary weighted graph or navigation-mesh algorithm. Blocked/unreachable endpoints are successful no-path outcomes; malformed dimensions, overflow and allocation failure are errors.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Find a route for a flying entity among traversable 3D grid cells.

## Related CLM modules

[clm_math_core](clm_math_core.md).
