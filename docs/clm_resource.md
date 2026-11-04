# CLM Resource

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Defines a common identity, kind taxonomy and lifetime operations for reusable assets such as images, fonts and graphical resource descriptions.

## Capabilities and operations

- Query a resource’s ID, kind and CPU payload size.
- Retain and release supported resources.
- Delete a supported resource explicitly.
- Distinguish invalid, unknown, unsupported and type-mismatched resources.

## Resource, data and ownership model

Resource identity is an ID plus a recognized resource kind. Retain/release semantics apply only where a concrete resource implementation supports them. CPU payload byte counts are not GPU device memory measurements.

## Behavioral contract and boundaries

The reference implementation currently supports generic lifecycle operations for BPA-backed images, fonts and CPU-side texture sources; other named kinds are planned or partial. A listed resource-kind enumeration does not imply every kind is allocatable or renderable.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Hold a font resource across multiple render operations, release it after all consumers finish, and inspect resource metadata for diagnostics.

## Related CLM modules

[clm_base](clm_base.md).
