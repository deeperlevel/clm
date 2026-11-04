# CLM Math Calculus

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Adds numerical calculus operators above the common CLM mathematics representation.

## Capabilities and operations

- Approximate first and second derivatives of scalar functions.
- Compute numerical definite integrals with trapezoid and Simpson variants, including adaptive integration.
- Approximate gradients, Jacobians and Hessians.
- Differentiate or integrate sampled data.
- Advance initial-value ordinary differential equations using Euler or Runge–Kutta methods.

## Resource, data and ownership model

Calculus operations consume scalar or vector function evaluations, sampling grids or state vectors. They return numeric results and status information, without owning a long-lived simulation or storing a global expression tree.

## Behavioral contract and boundaries

Finite-difference and quadrature methods are approximations; accuracy depends on step size, sample continuity and floating-point behavior. Invalid intervals, nonfinite function values and dimension mismatches should be reported, not treated as exact mathematics.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Estimate a velocity derivative from samples or integrate a rate function, using a chosen numerical tolerance.

## Related CLM modules

[clm_math_core](clm_math_core.md).
