# CLM Stringtify

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

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

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_dictionary](clm_dictionary.md), [clm_id](clm_id.md), [clm_storage](clm_storage.md), [clm_string_format](clm_string_format.md), [clm_string_json](clm_string_json.md), [clm_string_yaml](clm_string_yaml.md), [clm_table](clm_table.md), [clm_tree](clm_tree.md).

## Example use cases

Show a CLM table, dictionary or tree in a console without writing a formatter for each type.
