# clm_platform

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Compile-target identity and portable environment constants. OS, architecture and compiler detection is driven by preprocessor macros. The resulting identifiers describe the target for which this program was compiled, not a runtime scan of the current host. OS-version strings identify SDK/header/target hclm_i32s, not necessarily a running kernel version. Several *_name functions create/reuse an associated process-global BPA string.

## Behavior and constraints

Compile-target identity and portable environment constants.
OS, architecture and compiler detection is driven by preprocessor macros.
The resulting identifiers describe the target for which this program was
compiled, not a runtime scan of the current host. OS-version strings identify
SDK/header/target hclm_i32s, not necessarily a running kernel version.
Several *_name functions create/reuse an associated process-global BPA string.

## Integration

- C reference: [**clm_platform**](../c/platform.md).
- Related CLM declarations in this archive: `clm_bpa`, `clm_string`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
