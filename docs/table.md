# clm_table

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

ID-keyed tabular rows, columns, and cell values.

## Behavior and constraints

ID-keyed tabular rows, columns, and cell values.

Column lookup and row dictionaries separate table structure from stored values. Internal
allocation IDs must remain distinct from caller IDs.

## Integration

- C reference: [**clm_table**](../c/table.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_id`, `clm_string`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
