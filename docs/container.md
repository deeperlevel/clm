# clm_container

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Explicit service registration, resolution, and scoped instances.

## Behavior and constraints

Explicit service registration, resolution, and scoped instances.

Registrations describe construction and lifetime policy. Resolving dependencies follows
those registrations rather than discovering types through reflection.

## Integration

- C reference: [**clm_container**](../c/container.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
