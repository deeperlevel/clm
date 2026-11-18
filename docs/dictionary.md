# clm_dictionary

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

ID-keyed dictionaries backed by typed BPA arrays.

## Behavior and constraints

ID-keyed dictionaries backed by typed BPA arrays.

Open-addressed slots index stored values. Tombstones preserve probe chains, and
rebuilding slots must preserve value ordering.

## Integration

- C reference: [**clm_dictionary**](../c/dictionary.md).
- Related CLM declarations in this archive: `clm_bpa`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
