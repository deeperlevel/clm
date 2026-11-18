# clm_ua

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

User-agent text classification and version extraction.

## Behavior and constraints

User-agent text classification and version extraction.

Case-insensitive token matching supplies heuristic browser, OS, and device information.
Classification does not authenticate the remote client.

## Integration

- C reference: [**clm_ua**](../c/ua.md).
- Related CLM declarations in this archive: `clm_string`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
