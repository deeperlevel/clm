# CLM Ecs

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

An entity-component-system registry with reusable blueprints, entity instances, components and systems.

## Capabilities and operations

- Register component types and entity blueprints; instantiate entities and add/remove components.
- Query entities by required component sets and invoke registered systems.
- Keep entity and component IDs consistent across the application.
- A matching query returns entities that contain the required components, not necessarily an exact match.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md).

## Example use cases

Register component types and entity blueprints; instantiate entities and add/remove components.
