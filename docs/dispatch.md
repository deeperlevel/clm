# clm_dispatch

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Command, event, and query dispatch with service resolution.

## Behavior and constraints

Command, event, and query dispatch with service resolution.

Handler registrations connect operation IDs to callbacks. Dependencies are supplied
through the container rather than implicit global discovery.

## Integration

- C reference: [**clm_dispatch**](../c/dispatch.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
