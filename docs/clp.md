# clm_clp

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Command-line tokenization and option-to-value mapping.

## Behavior and constraints

Command-line tokenization and option-to-value mapping.

Option names and token buffers have explicit cleanup paths. Parsed values are resolved
through the CLM ID and dictionary modules.

## Integration

- C reference: [**clm_clp**](../c/clp.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
