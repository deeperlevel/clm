# CLM Platform

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Exposes consistent descriptors of the current operating system, architecture and runtime platform.

## Capabilities and operations

- Identify OS family, architecture, bitness and endianness.
- Inspect compiler/build or platform properties where meaningful to the implementation.
- Expose standard conventions such as path separators and common directories.
- Treat unsupported or unknown platform attributes as legitimate results.

## Detailed module contract

**Module:** `clm_platform`  
**Purpose:** Identify the compilation target and expose conventional platform values through consistent Cross Language Modules (CLM) operations.

## Overview

`clm_platform` provides lightweight **build-time** information about the operating system, processor architecture, compiler, pointer width, and byte order. It also exposes the conventional path separator and newline sequence and simple OS-category checks.

The module **does not inspect the running machine**. Values describe the environment for which the CLM implementation was compiled. An OS version is a *hint* derived from a target SDK, API level, or included headers; it is not the installed operating system's version.

## Results and conventions

- **Identity:** A stable CLM numeric ID representing an OS, architecture, compiler, or byte order.
- **BPA block:** A value stored in the Block Pool Allocator and accessed by its CLM ID.
- **Name getters:** `get_os_name`, `get_arch_name`, and `get_compiler_name` ensure that the corresponding name exists in BPA and return the **same ID as their identity getter**. This does not mean every identity getter creates a BPA block.
- **Property getters:** Version and newline getters return dedicated BPA block IDs, distinct from OS or compiler identities.
- **`_to` getters:** Copy text into a caller-provided buffer with a specified capacity, using the underlying BPA copy operation. Check the copy result and provide adequate space. They do not return the string itself.
- **Predicates:** `is_*` results use `1` for true and `0` for false.

Names below retain the shared CLM API naming, but descriptions are independent of programming-language syntax.

## API reference

### Operating system

| Operation | Result | Description |
| --- | --- | --- |
| `clm_platform_get_os()` | OS identity ID | Returns the compiled target OS. Does not require a name block. |
| `clm_platform_get_os_name()` | BPA ID (= OS identity) | Ensures and identifies the OS name string. |
| `clm_platform_get_os_name_to(destination, capacity)` | Copy result | Copies the OS name into a buffer. |
| `clm_platform_get_os_version()` | BPA ID | Ensures an OS target/header/version **hint** string. |
| `clm_platform_get_os_version_to(destination, capacity)` | Copy result | Copies the OS version hint. |

Supported OS identities: `PLATFORM_INFO_OS_WINDOWS`, `PLATFORM_INFO_OS_MACOS`, `PLATFORM_INFO_OS_LINUX`, `PLATFORM_INFO_OS_BSD`, `PLATFORM_INFO_OS_ANDROID`, `PLATFORM_INFO_OS_IOS`, `PLATFORM_INFO_OS_UNKNOWN`.

**Known limitations:** The BSD identity groups supported BSD variants. tvOS, watchOS, and visionOS currently map to `UNKNOWN` (though a descriptive name may be available). A cross-compiled binary reports its target, not its build host. Version hints vary in meaning between operating systems and may be `Unknown`.

### Architecture and byte order

| Operation | Result | Description |
| --- | --- | --- |
| `clm_platform_get_arch()` | Architecture identity ID | Reports compiled architecture. |
| `clm_platform_get_arch_name()` | BPA ID (= architecture identity) | Ensures and identifies the architecture name. |
| `clm_platform_get_arch_name_to(destination, capacity)` | Copy result | Copies the architecture name. |
| `clm_platform_get_bits()` | Integer | Pointer width of the compiled implementation, in bits. |
| `clm_platform_get_endian()` | Endianness identity ID | Reports known compile-time byte ordering, or `UNKNOWN`. |

Supported architectures: `PLATFORM_INFO_ARCH_X86`, `X64`, `ARM32`, `ARM64`, `RISCV32`, `RISCV64`, `WASM32`, `WASM64`, `UNKNOWN` (all with the common `PLATFORM_INFO_ARCH_` prefix).

Endianness identities: `PLATFORM_INFO_ENDIAN_LITTLE`, `PLATFORM_INFO_ENDIAN_BIG`, `PLATFORM_INFO_ENDIAN_UNKNOWN`.

`get_bits()` refers to **pointer size**, not necessarily an instruction-set width or physical CPU capability. On unsupported targets, architecture and endianness can be `UNKNOWN`. The module provides no endianness-name getter in the current API.

### Compiler

| Operation | Result | Description |
| --- | --- | --- |
| `clm_platform_get_compiler()` | Compiler identity ID | Returns the compiler used to build the CLM implementation. |
| `clm_platform_get_compiler_name()` | BPA ID (= compiler identity) | Ensures and identifies compiler name. |
| `clm_platform_get_compiler_name_to(destination, capacity)` | Copy result | Copies compiler name. |
| `clm_platform_get_compiler_version()` | BPA ID | Ensures compiler version string. |
| `clm_platform_get_compiler_version_to(destination, capacity)` | Copy result | Copies compiler version string. |

Compiler identities: `PLATFORM_INFO_COMPILER_MSVC`, `PLATFORM_INFO_COMPILER_GCC`, `PLATFORM_INFO_COMPILER_CLANG`, `PLATFORM_INFO_COMPILER_UNKNOWN`.

The compiler version describes the **CLM build**, not necessarily the consumer application's compiler. Toolchains with compatibility definitions are classified by the module's detection rules (for example, Clang is checked before GCC/MSVC compatibility macros).

### Platform conventions

| Operation | Result | Description |
| --- | --- | --- |
| `clm_platform_get_path_separator()` | Character | `\\` on Windows; `/` on other targets. |
| `clm_platform_get_newline()` | BPA ID | Ensures the platform-conventional newline string. |
| `clm_platform_get_newline_to(destination, capacity)` | Copy result | Copies the newline string into a buffer. |

Newline is `\r\n` on Windows and `\n` elsewhere. These values are **informational**: they do not impose behavior on `append_line`, `write_line`, or other writing operations.

The separator is a *convention*, not a full set of path-parsing rules. The forthcoming `clm_path` module can use this information while owning path normalization, joining, and interpretation.

### Classification and predicates

| Operation | True when target OS is ... |
| --- | --- |
| `clm_platform_is_windows()` | Windows |
| `clm_platform_is_linux()` | Linux |
| `clm_platform_is_bsd()` | BSD |
| `clm_platform_is_macos()` | macOS |
| `clm_platform_is_android()` | Android |
| `clm_platform_is_ios()` | iOS |
| `clm_platform_is_mobile()` | Android or iOS |
| `clm_platform_is_desktop()` | Windows, Linux, BSD, or macOS |

`is_mobile` and `is_desktop` are **simple OS groupings**, not detection of actual device form factor. Neither is necessarily true for `UNKNOWN`.

## Conceptual examples

**Identify the current CLM build target:**

1. Read `clm_platform_get_os()` to obtain an OS identity.
2. Optionally call `clm_platform_get_os_name()` to ensure its readable BPA string exists; the returned ID equals the identity from step 1.
3. Read the name from BPA by that ID, or use `clm_platform_get_os_name_to(...)` to copy it into caller storage.

**Choose a platform convention:**

1. Ask `clm_platform_get_path_separator()` for the default separator to present or construct a target-style path.
2. Leave actual path interpretation to `clm_path`.

**Select a target-specific implementation:**

1. Check `clm_platform_is_windows()` or `clm_platform_is_android()`.
2. Choose target-specific code where necessary; do not interpret these results as runtime capability checks.

## Design boundaries

`clm_platform` does **not** perform runtime OS detection, query running kernel versions, enumerate CPU features, inspect devices, create windows, access files, or normalize paths. Such operations belong to other CLM modules when implemented.

## Related modules

- **BPA:** Storage and retrieval of platform names and descriptive strings.
- **`clm_path` (planned):** Path handling that may consult the conventional separator.
- **`clm_surface`:** Surface/window lifecycle, separate from platform identification.

---

**CLM — Cross Language Modules** · [github.com/deeperlevel/clm](https://github.com/deeperlevel/clm)  
**License:** CC0 1.0 Universal · Public Domain · Provided AS IS.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_bpa](clm_bpa.md), [clm_string](clm_string.md).

## Example use cases

Identify OS family, architecture, bitness and endianness.
