# clm_string_toml

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

TOML document parsing and generation.

## Behavior and constraints

TOML document parsing and generation.

Tables, arrays, quoted values, and scalar forms are recognized before updating the
destination tree.

## Integration

- C reference: [**clm_string_toml**](../c/string_toml.md).
- Related CLM declarations in this archive: `clm_file`, `clm_string_format`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
