# clm_types

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Fixed-width CLM scalar aliases and plain data geometry types. The scalar aliases intentionally fix serialized/FFI-visible widths independently of the native C int/long sizes. Geometry types are lightweight value records: position = origin, size = extents, block = origin plus extents. The unqualified variants use signed 32-bit fields; explicit suffixes select other widths. No dynamic storage or ownership is associated with these records.

## Behavior and constraints

Fixed-width CLM scalar aliases and plain data geometry types.
The scalar aliases intentionally fix serialized/FFI-visible widths independently
of the native C int/long sizes. Geometry types are lightweight value records:
position = origin, size = extents, block = origin plus extents. The unqualified
variants use signed 32-bit fields; explicit suffixes select other widths.
No dynamic storage or ownership is associated with these records.

## Integration

- C reference: [**clm_types**](../c/types.md).
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
