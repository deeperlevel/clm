# CLM Stringtify

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Converts ID-based CLM-managed objects into readable text for display and diagnostics.

## Capabilities and operations

- Render CLM scalar values and collections in compact, pretty or detailed styles.
- Customize individual value conversions through adapters or intercept an entire object rendering.
- Integrate with console output while keeping presentation separate from canonical serialization.
- Observe borrowed view lifetimes and bound formatting of nested/cyclic structures.

## Formats and customization

The formatter supports **compact**, **pretty** and **detailed** styles. It can present blocks, dictionaries, storage grids, tables and trees. A value adapter may override the way an individual value is displayed; an object-level interceptor may replace presentation of the entire requested object. Adapters may request default formatting when they do not handle a value.

## Presentation versus data interchange

`clm_stringtify` is for human-readable output and diagnostics. When a canonical interchange representation is needed, use the corresponding JSON, YAML or other serializer. Console object writing uses stringtify so an ID-based object can be displayed without hand-writing a separate formatter.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Show a CLM table, dictionary or tree in a console without writing a formatter for each type.
