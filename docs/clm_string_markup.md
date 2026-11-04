# CLM String Markup

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_string](clm_string.md), [clm_string_format](clm_string_format.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Parses supported markup input and produces normalized structured CLM data, with explicit XML-versus-HTML parsing modes.

## Capabilities and operations

- Configure markup parsing mode and safety limits.
- Parse markup and produce JSON-compatible structured output.
- Load parsed markup into the shared structured-data/tree representation.
- Report malformed markup, exceeded limits and output conversion errors.

## Resource, data and ownership model

The parser turns markup syntax into a structured representation rather than preserving every byte of source. Callers choose XML-style or HTML-style rules and own the resulting destination value or tree.

## Behavioral contract and boundaries

XML mode is case sensitive and requires quoted attributes; HTML mode has different permissiveness. This is not a complete standards-conforming browser DOM, CSS renderer, XPath engine or XML schema validator. Conversion can lose source formatting and lexical distinctions.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Read a small configuration XML fragment and convert it into the same structured-data traversal model used by other CLM format adapters.

## Related CLM modules

[clm_base](clm_base.md), [clm_string](clm_string.md), [clm_string_format](clm_string_format.md).
