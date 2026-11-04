# CLM UUID

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** Platform entropy source may be used for UUID generation; no third-party library required by contract.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Represents and manages UUID values with validation, generation, comparison, variant/version inspection and text conversion.

## Capabilities and operations

- Validate UUID text and stored values.
- Generate, parse, format, copy and delete UUID values by ID.
- Compare UUIDs and test equality, nil and all-maximum forms.
- Inspect standard UUID version, variant and field segments.

## Resource, data and ownership model

A UUID is a 128-bit identity value represented independently of its canonical text form. CLM IDs that store UUID values are resource handles, not the UUID value themselves.

## Behavioral contract and boundaries

The reference accepts precisely dashed 8-4-4-4-12 or compact 32-digit hexadecimal strings, without braces, URN prefixes or surrounding whitespace. Binary values use canonical/network byte order, not a platform-specific native GUID layout. Generation delegates to CLM random generation using an OS entropy source. Text validation does not reject the nil/max value or nonstandard version/variant patterns; the version and variant can be inspected separately.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Generate a UUID for a content record, format it for interchange and later parse and compare it.

## Related CLM modules

[clm_base](clm_base.md).
