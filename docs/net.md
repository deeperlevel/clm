# clm_net

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Portable socket registration, addressing, and network I/O.

## Behavior and constraints

Portable socket registration, addressing, and network I/O.

Numeric socket IDs resolve to native handles. Nonblocking operations distinguish
temporary lack of readiness from closure and permanent errors.

Networking functions and shared socket state are implemented in net.c and
provided by the CLM library; no implementation macro is required. String-ID
adapters use the CLM string API. On strict C11 POSIX builds, define
_POSIX_C_SOURCE=200809L before including any POSIX system headers; the CLM
CMake target sets the required feature-test definitions for its consumers.

## Integration

- C reference: [**clm_net**](../c/net.md).
- Related CLM declarations in this archive: `clm_string`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
