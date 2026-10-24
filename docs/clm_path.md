# CLM Path

## Purpose and design

`clm_path` provides stable, caller-selected numeric **path IDs**. Paths are mapped once, then transformations operate on IDs instead of exchanging application-owned string buffers. The Path module owns a private **CLM Container of STRING values** keyed by path ID; it does **not** require a new CLM Storage allocation for every path. `clm_storage` is used for source strings produced by directory searches or other code.

- **Mapping boundary:** `clm_path_map(path_id, string)` introduces external strings; `clm_path_map_from_storage(path_id, storage_id, index)` copies an indexed STRING storage element into the registry.
- **Transformations:** each source is a path ID; a `destination` path ID is optional and defaults to the primary source ID. Nonexistent destination IDs are created. Sources are captured before committing so destination/source ID aliasing is safe.
- **Queries:** checks return simple Boolean values. Path-valued results are mapped back into the registry instead of returning caller-owned output buffers. `clm_path_get(path_id)` is available for filesystem boundary integration, diagnostics and output.
- **Lifetime:** `map` replaces an existing mapping; `unmap` removes one mapping; `destroy` destroys all mappings. The Path module owns its internal container and may reinitialize after destruction. `unmap` does not delete a filesystem entry.
- **Memory:** persistent path strings are owned by CLM Container. Implementations may use short-lived stack or temporary heap buffers internally, but they must not escape the operation. A STRING Storage source is copied; its cursor and contents do not change.

This Python reference raises `PathError` on bad/missing IDs, malformed values or failed transformations. Language implementations may express the same failures using result status codes; they must not substitute an empty path silently. The global registry has module-level locking for operations in this Python version; this is **not** a universal cross-process synchronization guarantee.

## Core API

| Operation | Parameters | Meaning |
|---|---|---|
| `clm_path_initialize` | `container_id?` | Initialize the privately owned STRING container (automatic on first use; optional ID is an implementation diagnostic) |
| `clm_path_destroy` | — | Remove all mapped paths; idempotent |
| `clm_path_map` | `path_id, path_string` | Register or replace a path or path fragment |
| `clm_path_map_from_storage` | `path_id, storage_id, index` | Copy one linear STRING Storage element at explicit index; cursor unchanged |
| `clm_path_unmap` | `path_id` | Remove mapping; missing ID is an error |
| `clm_path_is_mapped` | `path_id` | Boolean existence of registry entry |
| `clm_path_get` | `path_id` | Read mapped string (Python: immutable string snapshot) |
| `clm_path_join` | `first_id, second_id, destination?` | Join mapped components; default destination `first_id` |
| `clm_path_sanitize` | `path_id, destination?` | Lexical `.` and `..` normalization, without filesystem access |
| `clm_path_change_extension` | `path_id, extension_id, destination?` | Replace final extension; extension ID may contain `exe` or `.exe`, or empty string to remove |
| `clm_path_trim_ending_separator` | `path_id, destination?` | Remove trailing separators without deleting root |
| `clm_path_get_full_path` | `path_id, destination?` | Make absolute using host current working directory; do not resolve symlinks |
| `clm_path_get_relative_path` | `base_id, path_id, destination?` | Relative path from base to target; default destination `path_id` |
| `clm_path_get_directory_name` | `path_id, destination?` | Directory component into destination |
| `clm_path_get_filename` | `path_id, destination?` | Final filename into destination |
| `clm_path_get_file_name_without_extension` | `path_id, destination?` | Final filename minus last extension into destination |
| `clm_path_get_file_extension` | `path_id, destination?` | Last extension (including leading dot) into destination |
| `clm_path_get_path_root` | `path_id, destination?` | Path root into destination |
| `clm_path_get_random_filename` | `destination_id, extension_id?` | Secure 12-hex-character name, optional mapped extension; does not create file |
| `clm_path_get_temporary_directory` | `base_id, destination?` | Create a unique directory inside base; map its path; caller cleans it up |
| `clm_path_has_extension` | `path_id` | Boolean |
| `clm_path_ends_in_directory_separator` | `path_id` | Boolean |
| `clm_path_is_full_path` | `path_id` | Boolean |
| `clm_path_exists` | `path_id` | Boolean filesystem existence check (broken links: false) |
| `clm_path_get_invalid_path_characters` | — | Host-specific invalid-character sample (not complete validation) |
| `clm_path_get_directory_separator` | — | Host-native separator character |

## Examples

### Reuse mapped fragments

```python
PATH_1, PATH_2, EXTENSION = 101, 102, 103
clm_path_map(PATH_1, "c:/media")
clm_path_map(PATH_2, "movies")
clm_path_join(PATH_1, PATH_2, destination=PATH_2)
# PATH_2 = "c:/media/movies", PATH_1 unchanged
clm_path_unmap(PATH_1)
```

### Lexical normalization

```python
clm_path_map(201, "d:/programs/../movies/movie.mkv")
clm_path_sanitize(201)
# 201 now maps to "d:/movies/movie.mkv"
```

### Change extension through mapped IDs

```python
clm_path_map(301, "program.exx")
clm_path_map(302, "exe")
clm_path_change_extension(301, 302)
# 301 now maps to "program.exe"
```

### Map one directory-search result, preserve the Storage iterator

```python
# RESULTS is an existing linear STRING storage containing directory-search paths.
# The caller is responsible for creating it with sufficient string element size.
clm_path_map_from_storage(CURRENT_PATH, RESULTS, index=0)
clm_path_sanitize(CURRENT_PATH)
# Work with CURRENT_PATH; later:
clm_path_unmap(CURRENT_PATH)
```

## Path grammar and portability

Path manipulation uses a native implementation strategy with `/` as the stored normalized separator for transformed paths. The Python reference recognizes Windows drive and backslash forms even on Linux for **lexical** operations such as `sanitize`, `join` and extension splitting. Filesystem operations (`exists`, `get_temporary_directory`, `get_full_path`) use the host operating system; a foreign Windows drive cannot be reliably resolved to a POSIX absolute filesystem path. Output from `join` is not automatically sanitized—call `sanitize` when needed.

`sanitize` removes lexical `.` and `..` components but is **not** a security sanitizer. It does not ensure a path stays beneath a trusted root, resolve symlinks, reject `../../`, or prevent time-of-check/time-of-use races. If path containment matters, filesystem-specific secure resolution must happen at the I/O boundary.

`get_temporary_directory` is a compatibility helper that **creates** a directory and can create the base if missing; ordinary Path transformations do not touch the filesystem. New directory-specific capabilities should live in `clm_directory`.

## Compatibility and migration

This is an intentional API change from the old `cla_path_*` functions that accept/return path strings. The original `cla_path.py` remains separate and unchanged. `clm_path` now takes path IDs; direct old usages are **not** drop-in compatible. Typical migration:

```text
Old: cla_path_change_extension("report.txt", "json") -> "report.json"
New: clm_path_map(FILE_ID, "report.txt")
     clm_path_map(EXT_ID, "json")
     clm_path_change_extension(FILE_ID, EXT_ID)
     clm_path_get(FILE_ID) -> "report.json"
```

## Testing

Run with installed `clm_container.py`, `clm_storage.py`, `clm_block.py`, and `clm_block3d.py`:

```bash
python -m unittest -v test_clm_path
```

Tests cover mapping, replacement, lifecycle, aliased destinations, lexical sanitization, extensions, component extraction, mapping from Storage with preserved cursor, failure atomicity, relative/full paths, filesystem queries, temporary directories, and random names.
