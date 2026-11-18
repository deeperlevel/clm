# clm_string_dsv

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Delimited-record parsing and generation.

## Behavior and constraints

Delimited-record parsing and generation.

Quoted fields can contain delimiters and line breaks. Parser state distinguishes
structural separators from escaped field content.

## Integration

- C reference: [**clm_string_dsv**](../c/string_dsv.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_file`, `clm_string`, `clm_table`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
