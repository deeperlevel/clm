# clm_datetime

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Civil date conversion, timestamps, durations, and formatting.

## Behavior and constraints

Civil date conversion, timestamps, durations, and formatting.

Calendar calculations are kept separate from platform clock and timezone queries. Units
and representable ranges matter at conversion boundaries.

## Integration

- C reference: [**clm_datetime**](../c/datetime.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
