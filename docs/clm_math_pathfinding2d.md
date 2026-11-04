# CLM Math Pathfinding 2D

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides shortest-path A* search on a rectangular 2D grid with four axis-aligned neighboring cells and unit movement cost.

## Capabilities and operations

- Search rectangular grids with four-way (no diagonal) adjacency.
- Treat each occupied cell as blocked and each free step as cost 1.
- Return an ordered path including start and goal, or an explicit not-found result.

## Resource, data and ownership model

The grid has width and height and an optional row-major blocked-cell mask. Positions are zero-based integer cell coordinates rather than world-space vectors. The returned path is newly owned storage and must be released by its caller.

## Behavioral contract and boundaries

Uses Manhattan heuristic and uniform unit costs. Search is finite, dense-grid A*, not a customizable weighted graph, diagonal navigation, path smoothing, or obstacle-generation engine. An unreachable or blocked endpoint returns a successful not-found result; a walkable start equal to goal returns a one-cell path. Unavailable memory, overflow and invalid dimensions are distinguishable errors.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Route an actor around occupied tiles on a two-dimensional game map.

## Related CLM modules

[clm_math_core](clm_math_core.md).
