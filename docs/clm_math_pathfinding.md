# CLM Math Pathfinding

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_math_core](clm_math_core.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

A* shortest-path search over application-provided graph neighbors.

## Capabilities and operations

- Visit graph nodes through supplied neighbor and cost callbacks.
- Use a supplied heuristic to guide the search.
- Return path results with explicit failure/no-path outcomes.
- Correctness depends on sensible, nonnegative edge costs and an appropriate heuristic.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Find a route between nodes in an application-defined grid or graph.
