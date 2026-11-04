# CLM Math Interpolation

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math](clm_math.md), [clm_math_vector](clm_math_vector.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Interpolation and easing for scalars and stored numeric sequences.

## Capabilities and operations

- Perform linear and inverse-linear interpolation and smoothstep.
- Apply ease-in, ease-out and ease-in-out curves.
- Evaluate cubic interpolation and interpolate stored vector data.
- Keep extrapolation, clamping and invalid-range behavior explicit.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Perform linear and inverse-linear interpolation and smoothstep.
