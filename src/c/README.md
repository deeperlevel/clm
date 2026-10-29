# CLM in C

This project is written in C11 and uses CMake with the Ninja build system. The recommended compiler is **Clang** on both Windows and Linux.

## Toolchain downloads

### Windows

Install the x64 versions of:

- **LLVM for Windows** from the [LLVM releases page](https://github.com/llvm/llvm-project/releases). The LLVM installer provides Clang; make sure `clang` is available on `PATH`. The project configuration also uses `lld` as its linker.
- **CMake 4.3 or later** from [cmake.org/download](https://cmake.org/download/). Enable the installer option to add CMake to `PATH`.
- **Ninja** from the [Ninja releases page](https://github.com/ninja-build/ninja/releases), and add the directory containing `ninja.exe` to `PATH`.

### Linux

Install **Clang**, **LLD**, and **Ninja** using your distribution's package manager. For example, on Debian or Ubuntu:

```sh
sudo apt update
sudo apt install clang lld ninja-build
```

Install **CMake 4.3 or later** as well. If your distribution's package is older, use a current Linux release from [cmake.org/download](https://cmake.org/download/) instead.

## VS Code setup

Install these VS Code extensions:

- [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) to configure and build the CMake project.
- [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb) to debug the program with LLDB.

Open the `src/c` directory as the VS Code workspace so `${workspaceFolder}` points to this project's `CMakeLists.txt`. If CMake Tools asks for a kit, select the Clang kit.

## Shared build directory

The checked-in `CMakePresets.json` defines the shared `default` configure and build presets. Both IDEs should use the `build` directory next to `CMakeLists.txt`; do not create a separate build directory for either IDE.

- **VS Code:** the checked-in tasks select the `default` preset.
- **CLion:** load and enable the `Clang + Ninja` preset-based CMake profile. If an older CLion profile still uses `cmake-build-debug`, disable or remove that profile.

The old `cmake-build-debug` directory, if present, is left untouched and is no longer used by the shared preset.

### `tasks.json`

The checked-in `.vscode/tasks.json` provides two tasks:

- **CMake: Configure (Clang + Ninja)** configures the project with Clang and Ninja.
- **CMake: Build** builds the `main` executable and is the default build task.

Run the default build task with **Terminal > Run Build Task** (or `Ctrl+Shift+B`). CMake, Clang, and Ninja must be available on `PATH`.

### `launch.json`

The checked-in `.vscode/launch.json` provides **CMake: Launch main (Clang)**. It uses CodeLLDB, launches the executable selected by CMake Tools, and runs the build task before starting the debugger. Start it from **Run and Debug** in VS Code.

## BPA module

Include `clm/bpa.h`; to provide the shared global storage, define `CLM_BPA_IMPLEMENTATION` in exactly one C source file before including the header. All block and array IDs use `clm_id`. Global ID registries use open-addressed hash tables; concurrent access must be synchronized by the caller. Arrays retain reserved capacity for reuse after items are removed. Sealing preserves the current capacity and prevents explicit capacity changes or automatic growth; insertions that fit within the reserve and other mutations remain available.

Fixed-size `STRUCT` blocks and arrays use `clm_bpa_create_sized` and `clm_bpaa_create_sized`; the original create functions support built-in types only. String buffer sizes are byte counts, and returned strings include a null terminator in the destination buffer. Use `clm_bpaa_sort_with` with a `clm_bpaa_compare_fn` comparator for fixed-size arrays; the legacy `clm_bpaa_sort` signature is retained but returns failure because its predicate type cannot safely represent a comparator.

Use `clm_bpa_replace` to replace block content: fixed-size blocks require an exact-size buffer, while string blocks accept a new length and copy the replacement through the internal memory pool.

## Dictionary module

Include `clm/dictionary.h` to use typed integer-key dictionaries. Create each dictionary with a unique ID and a fixed BPA value type; use `CLM_BPA_TYPE_STRUCT` with an explicit size for custom fixed-width values. `clm_dictionary_set_from` and `clm_dictionary_set` copy values into BPA/BPAA-managed storage, while `get` and `get_to` copy them back out. String values are bounded NUL-terminated byte strings; this module does not validate their encoding. Key iteration yields keys only, and iteration order is unspecified.

## ID module

Include `clm/id.h` to register names for numeric IDs, resolve names, update or remove mappings, and generate sequential IDs beginning at zero. Define `CLM_ID_IMPLEMENTATION` in exactly one source file. The registry uses BPA-backed dictionaries and is shared process-wide; it is not internally synchronized. Names must be valid UTF-8 and are case-sensitive by default. Optional case-insensitive mode folds ASCII letters only, not full Unicode case folding. Multiple names may map to the same ID; `clm_id_unmap(id)` removes all such aliases.

## Generation module

Include `clm/gen.h` for random integers, floating-point values, UUIDs, hexadecimal
strings, alphanumeric strings, and passwords. C numeric functions require explicit
`min` and `max` arguments; the signed-byte function is `clm_gen_random_i8`.
Bounds are inclusive, reversed bounds are swapped, and equal bounds return that
value. Full-width integer ranges are supported without signed overflow or modulo
bias. Floating-point bounds must be finite; invalid bounds return NaN.

Randomness comes from Windows BCrypt (link `bcrypt`, as this CMake target does)
or `/dev/urandom` on Unix-like systems. Numeric generators abort if OS randomness
is unavailable because their return types cannot report failure. Text generators
instead return `GENERATION_FAILURE` on randomness, allocation, or validation errors
without replacing the destination. They return `GENERATION_SUCCESS` on success.

Create an existing `CLM_BPA_TYPE_STRING` destination before generating text. UUIDs
use the canonical 36-character version-4 format and RFC variant. Hexadecimal and
alphanumeric generators accept zero length and the mixed, upper, or lower casing
constants; mixed casing allows both cases but does not guarantee both in each
result. Passwords require at least four characters and always contain uppercase,
lowercase, digits, and symbols, with their positions shuffled. Use a longer length
(for example, 16 or more) for practical passwords.

Use `clm_gen_hex_to(length, casing, buffer, buffer_size)`,
`clm_gen_uuid_to(casing, buffer, buffer_size)`,
`clm_gen_alphanum_to(length, casing, buffer, buffer_size)`, or
`clm_gen_strong_password_to(length, buffer, buffer_size)` to write directly to
caller-owned memory without a BPA destination. The buffer is a `void*` and its
capacity is a `clm_uint` byte count including room for the NUL terminator: UUIDs
need 37 bytes, and the other generators need `length + 1`. These functions return
the same generation status codes, reject NULL or undersized buffers without
writing, and NUL-terminate successful results. Discard output on failure: an OS
randomness failure may leave partial output in a buffer.

## String module

Include `clm/string.h` for string blocks and string arrays backed by BPA/BPAA. Text is validated and stored internally as UTF-8; indices and lengths count Unicode code points. The runtime default byte-export encoding is UTF-8 and can be changed to UTF-8, Latin-1, or UTF-16LE with `clm_string_set_default_encoding`. This selects the preferred encoding for newly created strings; `clm_string_from_bytes` and `clm_string_change_encoding` provide per-string control. These encodings are not arbitrary operating-system ANSI code pages. String append/insert operations return the number of characters added, removal returns the number removed, replacement returns the number of matches replaced, and `clm_string_length` returns the character count; failures return `CLM_BPA_FAILURE`. String array helpers support indexed editing, searching, sorting, cursor iteration, and drawing; draw shuffles the array when its active deck is exhausted.

## Path and URL modules

Include `clm/path.h` for native path manipulation, UNC helpers, filesystem metadata,
canonicalization, and working-directory access. Include `clm/url.h` for RFC 3986
parsing and resolution, component editing, encoding, query fields, IDNA/IRI, and
file-URL conversion. URL includes Path for shared UTF-8/native filesystem helpers.

Both modules operate on existing BPA string IDs. Operations return 0 on success
and 1 on error; predicates return 0/1 or -1 on invalid input or query failure.
Syntax validators return false for malformed text and -1 for invalid string IDs.
Destinations must already be strings. Aliasing is supported except identical key
and value outputs in `clm_url_query_get_at`. Failed operations do not change output
strings. Successful writes select UTF-8 export encoding. Use `CLM_URL_ABSENT_ID`
for absent optional builder components, never as the builder's path argument.

CMake fetches pinned uriparser 0.9.9 on first configuration and links it statically.
Consumers building outside this CMake target must provide uriparser headers and
link its library (`URI_STATIC_BUILD` when using a static build). Windows also
requires `normaliz` for native IDNA. Unix-like builds require `pkg-config` and the
libidn2 development package (for example `libidn2-dev` on Debian/Ubuntu), and expose
POSIX.1-2008 interfaces with `_XOPEN_SOURCE=700`.

Windows paths use UTF-16 filesystem calls at the OS boundary; POSIX paths use
`stat`/`lstat` and `realpath`. Default metadata predicates follow links except
`is_symlink` and `is_hidden`, which inspect entries. IDNA uses Windows Normaliz or
libidn2 and therefore follows the installed platform's IDNA rules, not a promised
identical Unicode mapping across operating systems. Unix branches have not been
runtime-verified in the current Windows environment.

Both suites run from `main.c` and through CTest (`clm`). They include native path,
UNC, temporary file/directory, symbolic-link, URI component, RFC resolution,
encoding, query, Unicode host, file-URL, aliasing, and invalid-input checks.
Symbolic-link fixture tests explicitly report a skip if OS permissions forbid creation.

## File and Directory Modules

Include `clm/file.h` for ID-managed open-file resources and whole-file/text I/O;
include `clm/directory.h` for tree operations, listings, and callback traversal.
Define `CLM_FILE_IMPLEMENTATION` in exactly one translation unit for the shared
resource registry. Call `clm_file_close(id)` for individual resources and
`clm_file_shutdown()` at teardown, before releasing BPA state. Closed IDs are
retired until shutdown to avoid stale IDs silently referring to new handles.
Applications must synchronize registry access themselves.

Operations return 0, -1 (failure), or -2 (unsupported). Directory callback stop
returns 1; existence/open-state predicates return 0/1 or -1 for errors. Existing
BPA string, binary-block, and string-array destinations must have the right type.
Caller buffers are bounded, size queries do not create temporary entries, and
streaming calls expose actual transfer counts. Text memory is UTF-8; file encodings
are explicit ASCII, UTF-8, and little/big-endian UTF-16 with matching BOM variants.

File opens reject final symlinks/reparse points. Recursive directory deletion does
not traverse links; recursive copy returns unsupported for link/special entries.
Callbacks can opt into following links, with native identity cycle checks and a
256-depth safety limit. Globs support case-sensitive `*` and `?`; the latter matches
one UTF-8 code point. Whole-tree mutations are not transactions, and these APIs
are not a sandbox against malicious concurrent ancestor-directory replacement.

Windows uses Unicode native handles, requires `bcrypt` for secure temporary names,
and inherits parent ACLs for new entries.
POSIX uses private creation modes and descriptor-relative recursive deletion;
Linux no-overwrite moves use `renameat2`, while unsupported non-Linux rename
semantics return -2. POSIX consumers require `_XOPEN_SOURCE=700`, `_GNU_SOURCE`
on Linux, and `_FILE_OFFSET_BITS=64`. POSIX runtime behavior is not verified by
the Windows test run. Both suites are registered in the main runner and CTest.

## Console module

Include `clm/console.h` for standard-stream input/output and ANSI terminal controls. It uses C standard-library I/O and emits ANSI escape sequences for screen clearing, cursor movement/visibility, and text colors; those controls require an ANSI-compatible terminal and may appear as literal characters when output is redirected. The C implementation does not use OS-specific APIs.

Line input is limited to 4095 bytes and returned text lives in shared internal storage that is overwritten by the next read. Character/key input reads from the standard C input stream, so interactive terminals may wait for the user to press Enter. Cursor-position/visibility queries, terminal attachment detection, and echo/line-mode changes are not exposed because standard C and ANSI output alone cannot provide them reliably. `cla_console_write_effect` uses a standard-C clock busy wait and is intended only for presentation, not timing-sensitive work.

The test runner continues to print colored assertions to the terminal and also writes plain, uncolored output to `main.log` in its current working directory. The log is overwritten at the start of each run and flushed after each output line. Failure to create or write the log causes the test executable to report an error and return a nonzero exit code.
