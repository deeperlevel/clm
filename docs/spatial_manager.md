# clm_spatial_manager

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Spatial shape registries, queries, and selection state.

## Behavior and constraints

Spatial shape registries, queries, and selection state.

Manager membership and shape IDs are tracked separately. World transforms and query
masks control picking and overlap results.

## Integration

- C reference: [**clm_spatial_manager**](../c/spatial_manager.md).
- Related CLM declarations in this archive: `clm_math_collision3d`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
