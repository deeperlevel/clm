# CLM Math Physics2D

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Small 2D motion helpers suitable for lightweight simulations and games.

## Capabilities and operations

- Advance 2D position and velocity via semi-implicit Euler integration.
- Apply acceleration and gravity in world units per second squared with positive Y upward.
- Set jump velocity and optionally cut a rising jump for variable jump height.
- Does not provide collision resolution or a full physics engine.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_math_core](clm_math_core.md).

## Example use cases

Advance a moving character’s velocity and position with gravity for a frame.
