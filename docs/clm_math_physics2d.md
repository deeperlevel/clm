# CLM Math Physics2D

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

**Dimensional separation:** Use [clm_math_physics3d](clm_math_physics3d.md) for 3D motion, and dedicated [2D](clm_math_collision2d.md)/[3D](clm_math_collision3d.md) collision modules for geometry tests.

## Purpose

Small 2D motion helpers suitable for lightweight simulations and games. Use the separate 3D physics module for three-dimensional motion.

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

## Example use cases

Advance a moving character’s velocity and position with gravity for a frame.
