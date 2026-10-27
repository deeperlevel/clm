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

## String module

Include `clm/string.h` for string blocks and string arrays backed by BPA/BPAA. Text is validated and stored internally as UTF-8; indices and lengths count Unicode code points. The runtime default byte-export encoding is UTF-8 and can be changed to UTF-8, Latin-1, or UTF-16LE with `clm_string_set_default_encoding`. This selects the preferred encoding for newly created strings; `clm_string_from_bytes` and `clm_string_change_encoding` provide per-string control. These encodings are not arbitrary operating-system ANSI code pages. String append/insert operations return the number of characters added, removal returns the number removed, replacement returns the number of matches replaced, and `clm_string_length` returns the character count; failures return `CLM_BPA_FAILURE`. String array helpers support indexed editing, searching, sorting, cursor iteration, and drawing; draw shuffles the array when its active deck is exhausted.

## Console module

Include `clm/console.h` for standard-stream input/output and ANSI terminal controls. It uses C standard-library I/O and emits ANSI escape sequences for screen clearing, cursor movement/visibility, and text colors; those controls require an ANSI-compatible terminal and may appear as literal characters when output is redirected. The C implementation does not use OS-specific APIs.

Line input is limited to 4095 bytes and returned text lives in shared internal storage that is overwritten by the next read. Character/key input reads from the standard C input stream, so interactive terminals may wait for the user to press Enter. Cursor-position/visibility queries, terminal attachment detection, and echo/line-mode changes are not exposed because standard C and ANSI output alone cannot provide them reliably. `cla_console_write_effect` uses a standard-C clock busy wait and is intended only for presentation, not timing-sensitive work.

The test runner continues to print colored assertions to the terminal and also writes plain, uncolored output to `main.log` in its current working directory. The log is overwritten at the start of each run and flushed after each output line. Failure to create or write the log causes the test executable to report an error and return a nonzero exit code.
