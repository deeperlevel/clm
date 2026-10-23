# CLM Directory — Language-agnostic contract (v0.1)

**CLM — Cross Language Modules** · Deeperlevel · [github.com/deeperlevel/clm](https://github.com/deeperlevel/clm) · Public Domain [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

*Don't start from scratch, bro.*

## Purpose and dependencies

`clm_directory` provides directory operations **only through registered CLM Path IDs**. It never requires callers to pass mutable output buffers or raw path strings to directory functions. It depends on `clm_path` for path resolution and `clm_storage` for search-result transfers. IDs belong to their respective modules' namespaces.

The Python implementation uses exceptions for invalid mappings, filesystem errors, failed preconditions, and unsupported operations. Search operations return a count; predicates return booleans; getters return their values; mutations normally return nothing. C/Go/etc. implementations may use status codes with out-parameters for scalar results, but must preserve the same behavior and failure boundaries.

## Functions

| Function | Effect / result |
|---|---|
| `clm_directory_create(path_id, recursive=True)` | Create directory. Recursive creates missing parents and tolerates an existing directory. Nonrecursive requires parent to exist and target to be absent. |
| `clm_directory_move(source_id, destination_id)` | Rename/move a directory; fails if destination already exists, and may fail across filesystems. |
| `clm_directory_remove(path_id, recursive=False)` | Remove empty directory; with `True`, delete its entire contents. Destructive. Reject directory symlink as a removal target. |
| `clm_directory_is_directory_existing(path_id)` | Test whether path currently resolves to a directory. |
| `clm_directory_is_directory_empty(path_id)` | Return whether directory has zero entries; missing or inaccessible paths fail. |
| `clm_directory_get_current_directory(destination_id)` | Map current working directory into destination Path ID. |
| `clm_directory_set_current_directory(path_id)` | Change **process-global** current working directory; not thread-local. |
| `clm_directory_find_files(path_id, recursive=False, destination=storage_id)` | Replace destination STRING Storage with discovered file paths; return count. |
| `clm_directory_find_directories(...)` | Same, restricted to directories. |
| `clm_directory_find_entries(...)` | Both files and directories, including other directory entries. |

### Search contract

1. **Caller creates the destination** using `clm_storage_create(ID, STORAGE_TYPE_STRING, size=MAX_UTF8_BYTES_PER_PATH, reserved=MAX_RESULTS)`.
2. Paths are **absolute**, standardized to `/`, sorted lexicographically; storage elements are independent copies, not borrowed buffers.
3. Operations **replace** destination contents. When number of results exceeds `reserved`, or any UTF-8 path exceeds STRING `size`, or destination is locked/wrong type, reject **before clearing** it.
4. Successful search does not change the Storage navigation cursor if its previous index still exists; it clamps if necessary. Existing CLM Storage append behavior is used internally.
5. Recursive searches don't follow symbolic-link directories. Symlink entries may be included as names in directory enumeration, according to the documented behavior of each underlying backend. Results represent a non-atomic filesystem snapshot and can become stale.
6. A search result can be copied via `clm_path_map_from_storage(SELECTED_ID, RESULTS_STORAGE_ID, index)`. Destroying Results Storage does not unmap Selected Path.

## Attributes and times

For **each** of `read`, `write`, `execute`, `hidden`:

- `clm_directory_get_<attribute>(path_id) → boolean`
- `clm_directory_set_<attribute>(path_id, enabled)` with `enabled` equal to `0` or `1`.

For **each** of `creation`, `last_accessed`, `last_write`:

- `clm_directory_get_<name>_time(path_id) → integer`
- `clm_directory_set_<name>_time(path_id, unix_ms)`.

Timestamps are **Unix epoch UTC milliseconds**. `creation` is unavailable on some operating systems, and setting creation time is explicitly unsupported in the current Python backend. Do not silently substitute metadata-change time (`ctime`) on POSIX. Read/write/execute are basic native permission-bit operations, **not** complete effective-access or ACL checks. POSIX directory execute means traversal. POSIX hidden means the basename starts with `.`; `set_hidden` returns unsupported there rather than renaming. Windows uses the native hidden attribute. Attributes may not all be settable on every OS.

## Example (Python)

```python
clm_path_map(ROOT, '/assets')
clm_storage_create(RESULTS, STORAGE_TYPE_STRING, size=4096, reserved=1000)
count = clm_directory_find_files(ROOT, recursive=True, destination=RESULTS)
for i in range(count):
    clm_path_map_from_storage(CURRENT, RESULTS, i)
    # Use CURRENT in clm_file operations.
clm_storage_destroy(RESULTS)
```

## Safety and scope

Mapped Path IDs are stable names, but they are **not security boundaries**. Lexically sanitized paths can still escape a sandbox via symlinks. Recursive deletion is irreversible. Search and metadata operations reflect host filesystem permissions and platform limitations. This is a **Python reference implementation**, not a claim of already implemented cross-language backends.
