# CLM File

## Purpose

`clm_file` provides whole-file, text/line, and block-streaming file operations through mapped **CLM Path IDs** and caller-created **CLM Storage IDs**. It hides native file handles; persistent Storage ownership is always controlled by the caller. Python raises `FileError`, `OSError`, `PermissionError`, `EOFError`, or `UnsupportedFilesystemOperation` as appropriate. Cross-language implementations may translate those to stable status codes.

## Storage requirements

| Role | Storage type | Interpretation |
|---|---|---|
| Whole binary | `STORAGE_TYPE_BYTE` | One **signed** 8-bit element per raw byte; `-1` represents `0xFF`. Saving reconstructs original octets. |
| Whole text | `STORAGE_TYPE_STRING` | Exactly **one** UTF-8 (or selected text-decoding) string element, even if empty. `size` must fit its UTF-8 encoded byte count. |
| Text lines | `STORAGE_TYPE_STRING` | One element per line, without terminators; no embedded `\r` or `\n`. |
| Streaming | `STORAGE_TYPE_DYNAMIC` | One **fixed-width binary record** per block, record `size == block_size`. |

Whole-file loads replace Storage and validate its type, lock state and width before modifying it; Storage auto-growth behavior applies. They hold the complete file contents in memory and are intended for relatively small or medium files. File mutations based on whole-file loading are **not transactional or safe against concurrent external writers**. The current Python implementation uses explicit permission-bit checks before modifying existing read-only files; OS permissions remain authoritative.

## File functions

| Function | Behavior |
|---|---|
| `clm_file_load(path_id, storage_id)` | Read entire file as BYTE elements, replace Storage; return raw byte count. |
| `clm_file_save(path_id, storage_id)` | Write BYTE Storage, truncating/creating file; return raw byte count. |
| `clm_file_load_text(path_id, storage_id, encoding='utf-8')` | Decode entire text file into one STRING; return character count. |
| `clm_file_save_text(path_id, storage_id, encoding='utf-8')` | Write the only STRING element, truncating/creating file. |
| `clm_file_load_lines(path_id, storage_id, encoding='utf-8')` | Decode and split into STRING lines without newline characters; return line count. |
| `clm_file_save_lines(path_id, storage_id, encoding='utf-8')` | Join STRING elements using host line separator, no extra terminating newline. |
| `clm_file_append(path_id, storage_id)` | Append raw BYTE Storage. |
| `clm_file_prepend(path_id, storage_id)` | Insert raw BYTE Storage at beginning. |
| `clm_file_insert(path_id, storage_id, offset)` | Insert BYTE values at **zero-based byte offset**, `0..file_size`. |
| `clm_file_clear(path_id)` | Truncate/create file to length zero. |
| `clm_file_append_line(path_id, line_storage_id)` | Append the single STRING line to file. |
| `clm_file_prepend_line(path_id, line_storage_id)` | Prepend a single STRING line. |
| `clm_file_insert_line(path_id, line_storage_id, index)` | Insert a single STRING at zero-based line index. |
| `clm_file_append_lines(path_id, storage_id)` | Append zero or more STRING lines. |
| `clm_file_prepend_lines(path_id, storage_id)` | Prepend zero or more lines. |
| `clm_file_insert_lines(path_id, storage_id, index)` | Insert zero or more lines at zero-based index. |
| `clm_file_is_file_existing(path_id)` | Return whether the mapped path resolves to a regular file. |
| `clm_file_get_size(path_id)` | Get file size in bytes (64-bit-capable integer). |

Line edits read and rewrite the file as text, normalize resulting newlines to **host OS line separators**, and retain a preexisting terminal newline. Inputs must not contain embedded newlines. Binary insertion works in raw bytes and never decodes file contents. Saving a text file requires the provided STRING to fit Storage's element size; `load_lines` may lose the distinction between some unusual line endings or a final empty line.

## Block streaming — explicitly navigated

```python
clm_path_map(MOVIE_PATH, '/videos/movie.mkv')
clm_storage_create(BUFFER, STORAGE_TYPE_DYNAMIC, size=128, reserved=4)
clm_file_begin_streaming(FILE_ID, MOVIE_PATH, block_size=128)
valid_bytes = clm_file_load_block(FILE_ID, BUFFER)
clm_file_seek_next(FILE_ID)
valid_bytes = clm_file_load_block_append(FILE_ID, BUFFER)
clm_file_end_streaming(FILE_ID)
# BUFFER remains allocated; use clm_file_free(BUFFER) to clear contents.
```

| Function | Contract |
|---|---|
| `clm_file_begin_streaming(file_id, path_id, block_size)` | Open file for binary reading; reject duplicate file ID or nonpositive block size; initialize block index to 0, or -1 when empty. |
| `clm_file_get_stream_info(file_id)` | Snapshot: `file_id`, `path`, `file_size`, `block_size`, `block_count`, `seek_index`, `last_read_size`, `auto_move`. |
| `clm_file_seek(file_id, index)` | Navigate by **block index**, zero-based; invalid seeks leave the cursor unchanged. |
| `clm_file_seek_first/last/next/previous(file_id)` | Navigate to specified valid block; no wrap-around or sentinel. |
| `clm_file_seek_auto_move_enable/disable(file_id)` | Toggle automatic next-block advance; **off by default**. |
| `clm_file_load_block(file_id, storage_id)` | Overwrite Storage with **one DYNAMIC record**; return valid byte count. |
| `clm_file_load_block_append(file_id, storage_id)` | Append **one** DYNAMIC record; return valid byte count. |
| `clm_file_free(storage_id)` | Clear DYNAMIC Storage contents, preserving allocation/registration. |
| `clm_file_end_streaming(file_id)` or `clm_file_close(file_id)` | Close file and remove its stream metadata; do **not** destroy associated Storage. |

**Partial block rule:** The last incomplete block is zero-padded on the right to exactly `block_size`. The **return value of every load** is the authoritative valid length of the record just written; `last_read_size` stores the most recent valid length. When appending multiple blocks, callers must retain those returned lengths (or derive from the unchanged original file size/block positions) to distinguish padding from real data. Successful loads do not change `seek_index` unless auto-move is enabled. Auto-move advances at most to the final valid index, **never past it**; later loads reread the final block. Empty files reject block loads with EOF. A failed load leaves both the logical cursor and destination Storage unchanged.

The stream is read-only and does not support writing blocks in this release. Streaming does not pin underlying file contents against external changes; changes during streaming are unspecified.

## Attributes and metadata

For each `read`, `write`, `execute`, `hidden`: `clm_file_get_<name>(path_id) -> boolean` and `clm_file_set_<name>(path_id, 0|1)`.

For each `creation`, `last_accessed`, `last_write`: `clm_file_get_<name>_time(path_id) -> UTC Unix milliseconds` and `clm_file_set_<name>_time(path_id, timestamp_ms)`.

Creation time may be unavailable and setting it is currently **unsupported**. POSIX hidden reflects dot-prefix and cannot be toggled without renaming; Windows hidden uses OS attributes. Windows execute and read permission setters are not offered as portable chmod equivalents. Permissions getters describe selected native bits, not effective ACL-based access.

## Design boundaries and cautions

- The public API accepts Path IDs and Storage IDs, **not** raw path buffers or native file handles. Line content lives in STRING Storage, including single-line helpers.
- A File ID is registered **inside clm_file** for a stream and may numerically equal any CLM Storage or Path ID.
- The current Python whole-file edit functions read the complete existing file and rewrite it; large media files should use streaming instead.
- The generic CLM Storage BYTE type is signed, so binary data uses a two's-complement octet conversion.
- File/Directory/Path have no automatic reference relationship; unmapping a path does not implicitly close its open file stream.
- The implementation documents behavior for future C, C++, Go, D, Pascal, and Odin ports; it **does not** claim those ports exist yet.
