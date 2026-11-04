# CLM Math Physics 3D

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides stateless physics helpers for 3D translational and rotational motion, useful for games and simulations that do not require a complete rigid-body engine.

## Capabilities and operations

- Advance velocity and position; derive acceleration and apply forces or impulses.
- Apply damping and limit speed.
- Compute point gravity, bounce and slide responses.
- Calculate basic sphere/box inertia and angular motion.
- Apply impulses at points; compute jump speed, jump parameters and early jump cuts.

## Resource, data and ownership model

Functions operate on 3D vector, quaternion and numeric values, with optional managed-value variants. They do not create an authoritative physics scene or automatically find contacts.

## Behavioral contract and boundaries

Mass, elapsed time, acceleration and geometry must satisfy validity constraints. This module is a motion/response toolbox, not broad-phase collision, constraint solving, fluid simulation or networked physics synchronization.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Integrate a moving object's velocity for one frame, apply an impulse at contact, and compute a slide vector using a normal.

## Related CLM modules

[clm_math_core](clm_math_core.md).
