# CLM File

## Status and Scope

This is a **language-neutral API reference** with a current C binding implementing
86 entry points, including registry shutdown. The portable concepts below do not
require a particular native handle type or memory layout. Binding-specific behavior
and platform limits are recorded explicitly; remaining cross-platform policy
questions are not claims of features already provided on every operating system.

The current suites exercise the Windows implementation. POSIX code is present
but requires a separate runtime validation environment.

### Current C Binding Contract

- Define `CLM_FILE_IMPLEMENTATION` in exactly one translation unit. Open-file
  resources are held in a shared registry independent of BPA object lifetimes.
  Callers must synchronize access; there is no internal thread-safety guarantee.
- Duplicate IDs fail without replacing live resources. Close retires the native
  resource even when a close error is reported. Closed IDs cannot be reused until
  `clm_file_shutdown()` closes remaining resources and releases all registry IDs.
  Teardown attempts every close and returns failure if any close fails.
- Existing string, binary-block, and string-array destinations must have the right
  type and sufficient capacity. Whole-file outputs are prepared before publication.
  Output strings may alias input path IDs because reads complete before publishing.
  Streaming transfers report actual partial byte counts on native I/O errors.
- File opens are regular-file-only and reject final-component symlinks/reparse
  points. Existence and metadata predicates can follow links, so existence does
  not imply that the stricter open policy will accept a path.
- Zero-byte EOF is a successful read. EOF is set on a short sequential read and
  cleared by successful seek; a zero-size read does not change it. Positioned I/O
  uses independent native operations and does not change sequential cursor state.
- Native transfers are unbuffered at the CLM layer: `flush` validates a writable
  resource; `sync` requests OS-level synchronization. This is not a directory-entry
  durability promise. Append-only resources reject positioned writes and truncation.
- UTF-8/ASCII/UTF-16 codecs are strict. Non-BOM modes reject a leading BOM; BOM
  modes require the matching BOM. Append validates existing content in the selected
  encoding and omits the extra BOM. Text append currently reads existing content
  for validation, so it is not a constant-memory operation on large files.
- Ordinary whole-file writes truncate; atomic writes stage in the same parent and
  use native replacement. Copy checks native source/destination identity before
  truncating a destination. Attribute preservation is not promised.
- Windows uses wide-character APIs and parent-inherited ACLs. POSIX creation uses
  mode 0600. Temporary size queries create/register nothing; successful temporary
  creation transfers both handle-close and file-removal responsibility to callers.
- Native attribute support is represented by masks. On POSIX, the implemented
  file flags are hidden-name and symbolic-link classification; other attribute
  queries return unsupported. Creation time/updates are unsupported rather than
  substituted with metadata-change time. Timestamp precision is whole seconds.
- No-overwrite moves use native Windows rename or Linux `renameat2`; other POSIX
  platforms return unsupported instead of a race-prone overwrite fallback.
- These path-based operations are not a sandbox against malicious concurrent
  replacement of ancestor directories. File registry shutdown is explicit, not
  automatic at BPA teardown or program exit.

CLM File covers whole-file binary/text operations, open-file streaming, positioned
I/O, metadata, synchronization, locking, temporary files, and atomic replacement.
Directory trees belong to [CLM Directory](clm_directory.md), lexical path handling
to [CLM Path](clm_path.md), and URI conversion to [CLM URL](clm_url.md).

Examples describe concepts only. Do not interpret them as executable bindings,
verified runtime behavior, or promises that unresolved policies are final.

## Identities and Ownership

The shared model distinguishes three identities:

| Identity | Meaning |
| --- | --- |
| Path ID | Identifies text naming a filesystem location. |
| Open-file ID | Identifies an independently owned native open-file resource. |
| Content ID | Identifies an existing binary block, text string, or string array. |

A path ID is not an open-file ID. Opening a path registers a resource under an
explicit unused ID. Closing operates on that resource ID, not on a path or a raw
filename. Renaming a file does not logically replace its already open resource.
The native resource layout is opaque; a C integer descriptor is not a portable
representation of every operating-system file handle.

Whole-file operations open and close internally without registering open-file
IDs. They must release their resources on success and failure. Successful
temporary-file creation is different: it deliberately returns both an open-file
registration and the created path, transferring cleanup responsibility to the caller.

The current C binding uses UTF-8 BPA path strings, fixed-capacity binary BPA
blocks, CLM strings, and CLM string arrays. Other bindings may use native text,
buffers, references, and result records without copying C pointer syntax or memory
layouts. Ownership, aliasing, buffer lifetimes, publication, and ID reuse rules
must be explicit before implementation.

## API Forms and Results

| Form | Meaning |
| --- | --- |
| Unsuffixed path argument | Existing UTF-8 path-string ID. |
| `_as` | Caller-owned UTF-8 path text instead of a path ID. |
| `_to` | Export bytes or text into caller-owned writable memory. |
| `_from` | Consume caller-owned read-only data instead of a content ID. |
| Open-file operation | Takes an open-file ID even when its data uses `_to` or `_from`. |

Tables preserve the current function names and parameter order, but omit C types.
An output ID names referenced contents, not a numeric ID variable to reassign.
Destination paths for copy and move are inputs, not writable result strings.

Status operations distinguish success, failure, and unsupported capability.
Their C constants are `CLM_FILE_OPERATION_SUCCESS`, `CLM_FILE_OPERATION_FAILURE`,
and `CLM_FILE_OPERATION_UNSUPPORTED`, currently 0, -1, and -2. `exists` and
`is_open` are direct predicates with 1/0/-1 true/false/error results. Other Boolean
queries return a status and supply their answer separately. Native result types
or exceptions may express these concepts in other languages.

Streaming operations expose partial transfers where documented; a failure after
writing does not roll back file contents or sequential cursor progress. Whole-file
collection outputs are published only after successful reading/conversion.
Size-query outputs on insufficient capacity are deliberate exceptions: caller
buffers remain unchanged while required sizes/counts are reported.

## Sizes, Positions, and Buffers

- File sizes and positions use conceptual unsigned 64-bit integers; relative
  seek displacements use signed 64-bit integers.
- Per-call capacities and byte counts must fit the binding's addressable buffer size.
- Binary transfers do not add a terminator or perform text conversion.
- In-memory text is UTF-8 even when file bytes use another encoding.
- Text capacities include a final NUL in the C buffer binding; successful text
  byte counts exclude it unless explicitly named `required_size`.
- Missing/invalid output references are errors, not permission to discard results.
- An insufficient whole-file buffer must not receive silently truncated content.
- Measurements can become stale if another process changes the file.

The current binary content ID refers to a preexisting fixed-capacity block, not
an automatically resized allocation. Text output IDs refer to existing strings
or string arrays, never a scalar size stored in a string array.

## Whole-File Read API

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_read_all` | `path, content, size` | Read binary bytes into a fixed-capacity content block; output actual byte count. |
| `clm_file_read_all_as` | `path, content, size` | Raw path and binary content-ID destination. |
| `clm_file_read_all_to` | `path, content, capacity, size` | Raw path and caller binary buffer; output actual/required byte count. |
| `clm_file_read_all_text` | `path, encoding, content, size` | Decode a text file into a UTF-8 string; output decoded bytes excluding NUL. |
| `clm_file_read_all_text_as` | `path, encoding, content, size` | Raw path and UTF-8 string-ID destination. |
| `clm_file_read_all_text_to` | `path, encoding, content, capacity, required_size` | Decode into caller UTF-8 memory; required capacity includes NUL. |
| `clm_file_read_all_text_lines` | `path, encoding, content, line_count` | Decode lines into an existing string array. |
| `clm_file_read_all_text_lines_as` | `path, encoding, content, line_count` | Raw path and decoded string-array output. |
| `clm_file_read_all_text_lines_to` | `path, encoding, content, capacity, required_size, line_count` | Export decoded lines in packed UTF-8 form. |

For binary `read_all_to`, an absent buffer with zero capacity measures only, and
`size` reports required bytes on insufficient capacity. There is no binary NUL
terminator. Text measurement similarly uses an absent buffer and zero capacity,
but `required_size` includes text terminators. These queries do not reserve a
file snapshot. Unsupported huge/address-unrepresentable allocations must fail
instead of narrowing a size or reading a truncated prefix.

## Whole-File Write and Append API

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_write_all` | `path, content, size` | Create or truncate and write exact binary bytes from a content block. |
| `clm_file_write_all_as` | `path, content, size` | Raw destination path and binary content-ID source. |
| `clm_file_write_all_from` | `path, content, size` | Raw path and caller-owned binary source. |
| `clm_file_write_all_text` | `path, encoding, content, size` | Encode UTF-8 string data into a created/truncated text file. |
| `clm_file_write_all_text_as` | `path, encoding, content, size` | Raw path and UTF-8 string-ID source. |
| `clm_file_write_all_text_from` | `path, encoding, content, size` | Raw path and bounded caller-owned UTF-8 source. |
| `clm_file_append` | `path, content, size` | Append binary bytes by path; create if absent. |
| `clm_file_append_as` | `path, content, size` | Raw path and binary content-ID append source. |
| `clm_file_append_from` | `path, content, size` | Raw path and caller-owned binary append source. |
| `clm_file_append_text` | `path, encoding, content, size` | Encode UTF-8 text and append by path without an added newline. |
| `clm_file_append_text_as` | `path, encoding, content, size` | Raw path and UTF-8 string-ID append source. |
| `clm_file_append_text_from` | `path, encoding, content, size` | Raw path and caller UTF-8 append source. |
| `clm_file_append_line_text` | `path, encoding, content, size` | Append encoded text followed by exactly one LF. |
| `clm_file_append_line_text_as` | `path, encoding, content, size` | Raw path and string-ID line source. |
| `clm_file_append_line_text_from` | `path, encoding, content, size` | Raw path and caller UTF-8 line source. |

For binary writes, `size` is readable source bytes. For text writes, it is UTF-8
input bytes excluding a terminator, not the resulting encoded file size or a
character count. A bounded UTF-8 source must end on a code-point boundary.
Encoding errors should be checked before truncating or appending to a file.

Whole-file writes are not atomic replacement and may leave partial filesystem
output after failure. Calls that expose no transfer-count output require an
explicit partial-progress diagnostic policy before implementation. Separate
append calls, or several native writes used by one line helper, are not promised
to be an atomic application record across processes.

## Text Encoding and Lines

| Encoding symbol | File representation |
| --- | --- |
| `CLM_FILE_TEXT_ENCODING_ASCII` | ASCII. |
| `CLM_FILE_TEXT_ENCODING_UTF8` | UTF-8 without an emitted BOM. |
| `CLM_FILE_TEXT_ENCODING_UTF8_BOM` | UTF-8 with a required/emitted matching BOM. |
| `CLM_FILE_TEXT_ENCODING_UTF16_LE` | Explicit little-endian UTF-16. |
| `CLM_FILE_TEXT_ENCODING_UTF16_LE_BOM` | Little-endian UTF-16 with matching BOM. |
| `CLM_FILE_TEXT_ENCODING_UTF16_BE` | Explicit big-endian UTF-16. |
| `CLM_FILE_TEXT_ENCODING_UTF16_BE_BOM` | Big-endian UTF-16 with matching BOM. |

`CLM_FILE_TEXT_ENCODING_UTF16` and `CLM_FILE_TEXT_ENCODING_UTF16_BOM` are
compatibility names for the corresponding little-endian forms, not host-endian
or automatic-detection modes. BOM variants require a matching source BOM when
reading and emit one when creating encoded output. Decoded memory does not contain
the consumed BOM. Handling an unexpected BOM in a non-BOM read mode remains a
decision; no automatic encoding detection is implied.

Appends must not insert a new BOM into existing content. Existing file encoding
and BOM compatibility must be checked according to a policy still to be finalized.
ASCII conversion must not silently replace unrepresentable characters. Text
conversion rejects malformed sequences and embedded NUL.

Line readers recognize CRLF, LF, and CR, omit delimiters, and retain blank interior
lines. Empty files have zero lines; a final delimiter does not invent an extra
empty line. Line appenders add exactly one LF encoded for the target file, even
when the input already ends in a newline. Native CRLF translation is not implicit.

Packed line output contains a NUL after each UTF-8 line and one extra final NUL.
An empty list needs one NUL byte; `line_count` distinguishes empty lines from
the terminating sentinel. `required_size` includes all those bytes, and must be
reported without partial buffer writes when capacity is insufficient.

```text
decoded input notation: "alpha\r\n\r\nbeta\n"
lines:                 ["alpha", "", "beta"]
packed output:         "alpha\0\0beta\0\0"
line_count:            3
```

## Open-File Lifecycle and Modes

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_open` | `id, path, mode` | Open a path ID under an explicit unused resource ID. |
| `clm_file_open_as` | `id, path, mode` | Open a raw UTF-8 path under an explicit unused resource ID. |
| `clm_file_is_open` | `id` | Direct predicate for registered live resources, not path existence. |
| `clm_file_close` | `id` | Close the resource owned by the open-file ID. |
| `clm_file_shutdown` | none | Attempt every outstanding close and release active and retired registry IDs. |

Access flags are `CLM_FILE_OPEN_MODE_READ`, `CLM_FILE_OPEN_MODE_WRITE`, and
`CLM_FILE_OPEN_MODE_APPEND`; optional disposition flags are
`CLM_FILE_OPEN_MODE_CREATE`, `CLM_FILE_OPEN_MODE_TRUNCATE`, and
`CLM_FILE_OPEN_MODE_EXCLUSIVE`. Append implies write and excludes truncation.
Exclusive requires create; truncate requires write. A mode with no access
permission is invalid. Exact combinations, sharing rules, link handling, and
resource-ID namespace rules must be finalized before implementation.

Registration must not overwrite an existing resource. Failed publication must
close the new native handle. Closing must report late flush/close errors while
retiring a consumed native resource safely; callers must not blindly retry closing
a native handle that may already have been released. No raw-path close API exists.

## Sequential and Positioned I/O

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_read` | `id, content, size, bytes_read` | Read up to `size` binary bytes into a content block. |
| `clm_file_read_to` | `id, content, size, bytes_read` | Sequential read into caller-owned binary memory. |
| `clm_file_write` | `id, content, size, bytes_written` | Write from a content block at the sequential/native append position. |
| `clm_file_write_from` | `id, content, size, bytes_written` | Sequential write from caller-owned binary memory. |
| `clm_file_append_open` | `id, content, size, bytes_written` | Append from a content block through an append-mode resource. |
| `clm_file_append_open_from` | `id, content, size, bytes_written` | Append caller-owned bytes through an append-mode resource. |
| `clm_file_read_at` | `id, offset, content, size, bytes_read` | Read from an absolute offset into a content block without moving the cursor. |
| `clm_file_read_at_to` | `id, offset, content, size, bytes_read` | Positioned read into caller-owned memory. |
| `clm_file_write_at` | `id, offset, content, size, bytes_written` | Positioned write from a content block without moving the cursor. |
| `clm_file_write_at_from` | `id, offset, content, size, bytes_written` | Positioned write from caller-owned memory. |

These operations are encoding-agnostic. Read `size` is a maximum transfer and
must fit the writable destination. Short reads are permitted; a zero-byte EOF
read is success rather than an I/O failure. Cursor movement follows transferred
bytes, not requested bytes. Read-error output and count semantics require a final
policy rather than an assumption that all failed reads are atomic.

Write counts must report bytes actually transferred, including partial I/O
progress on errors. Source buffers are read-only and caller-owned. Zero-byte
transfers may allow absent buffers; nonzero transfers require valid readable or
writable memory as appropriate.

Positioned offsets must fit native APIs without narrowing. Positioned writes
reject append-mode resources. Implementations must not emulate cursor-preserving
I/O with unsynchronized seek/read/restore sequences; unsupported guarantees must
be explicit. Concurrent sharing and synchronization remain binding-level policies.

## Position, Size, EOF, and Synchronization

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_seek` | `id, offset, origin` | Checked signed 64-bit displacement relative to beginning/current/end. |
| `clm_file_tell` | `id, index` | Output current unsigned 64-bit absolute byte position. |
| `clm_file_eof` | `id, out` | Status plus observed EOF Boolean from prior sequential reading. |
| `clm_file_get_open_size` | `id, size` | Output logical size through the open resource, not its original name. |
| `clm_file_truncate` | `id, size` | Set logical length through a writable open resource, preserving cursor position. |
| `clm_file_flush` | `id` | Flush user-space writes to the operating system. |
| `clm_file_sync` | `id` | Flush and request native durable file synchronization. |

Seek origins are `CLM_FILE_SEEK_BEGIN`, `CLM_FILE_SEEK_CURRENT`, and
`CLM_FILE_SEEK_END`. Negative resulting positions, overflows, and unrepresentable
native offsets are errors. Seeking beyond EOF does not itself extend the file.
Truncation can leave the cursor beyond the new EOF; zero-fill and sparse-extension
behavior require native documentation. EOF does not predict later file growth;
its reset-on-seek and subsequent-read rules remain pending.

Flush is not a durability guarantee. Sync depends on native filesystem and storage
guarantees and does not alone guarantee durable directory-entry creation or rename.
Late buffered-write, synchronization, and close errors must not be hidden.

## Range Locking

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_lock` | `id, offset, length, exclusive, wait` | Acquire a shared/exclusive native byte-range lock, optionally waiting. |
| `clm_file_unlock` | `id, offset, length` | Release the corresponding range under native ownership rules. |

Offsets and lengths are unsigned 64-bit values. A zero length means from the
offset through future EOF, not an empty no-op lock. Overflow and unsupported
ranges must fail explicitly. Advisory versus mandatory enforcement, ownership
by process versus resource, interactions with threads/close, and blocking
interruption rules are platform-specific and must be documented. A lock does not
automatically provide a cross-platform application transaction.

## File Entries, Temporary Files, and Atomic Replacement

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_exists` | `path` | Direct predicate: path resolves to a regular file, following links. |
| `clm_file_exists_as` | `path` | Raw-path regular-file existence predicate. |
| `clm_file_create` | `path` | Exclusively create and close a new empty regular file. |
| `clm_file_create_as` | `path` | Raw-path exclusive creation. |
| `clm_file_copy` | `source, destination, overwrite` | Copy bytes with explicit overwrite permission and identity checks. |
| `clm_file_copy_as` | `source, destination, overwrite` | Raw-path file copy. |
| `clm_file_move` | `source, destination` | Move or rename without overwrite or cross-filesystem fallback. |
| `clm_file_move_as` | `source, destination` | Raw-path move or rename. |
| `clm_file_delete` | `path` | Remove a file entry or link itself, never a directory tree. |
| `clm_file_delete_as` | `path` | Raw-path file-entry removal. |
| `clm_file_create_temporary` | `id, directory, path` | Exclusively create/open a private file and publish its path string. |
| `clm_file_create_temporary_as` | `id, directory, path` | Raw parent directory with resource-ID and string-ID results. |
| `clm_file_create_temporary_to` | `id, directory, path, capacity, required_size` | Create/open and export its path into caller memory, or measure only. |
| `clm_file_write_all_atomic` | `path, content, size` | Stage binary block data in a sibling and replace using native atomic rename. |
| `clm_file_write_all_atomic_as` | `path, content, size` | Raw destination path and binary content-ID source. |
| `clm_file_write_all_atomic_from` | `path, content, size` | Raw destination path and caller-owned binary source. |

Regular-file existence excludes directories and dangling targets. It is not a
permission or safety check. Exclusive creation rejects existing entries and does
not create parent directories. Copy must reject identical file identities, even
when their path spellings differ. Move does not silently become copy/delete.
Delete removes a symbolic link itself, not its target; missing-entry and
open-file deletion policies need to be explicit.

Temporary creation reserves an unused open-file ID and publishes a created path.
If publication fails, close and delete the newly created file. Successful callers
must close the resource and remove the entry explicitly. For the buffer form,
an absent buffer and zero capacity measure without creating or registering
anything. Insufficient capacity must not create a file. Bounded name generation,
private permissions, and cleanup-error reporting still need implementation rules.

Atomic replacement requires a same-directory staging file and supported native
replacement semantics. Lack of support must be reported, not downgraded to an
ordinary partial rewrite. Atomic visibility is not crash durability; native link,
metadata preservation, destination-existence, and synchronization policies remain
to be finalized.

## Metadata, Timestamps, and Attributes

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_file_get_info` | `path, follow_symlinks, out` | Query metadata and explicit support masks. |
| `clm_file_get_info_as` | `path, follow_symlinks, out` | Raw-path metadata query. |
| `clm_file_get_size` | `path, size` | Output logical regular-file size by path, not handle ID. |
| `clm_file_get_size_as` | `path, size` | Raw-path logical-size query. |
| `clm_file_get_creation_time` | `id, out` | Query actual creation time using a path-string ID. |
| `clm_file_get_creation_time_as` | `path, out` | Raw-path creation-time query. |
| `clm_file_get_modification_time` | `id, out` | Query modification time using a path-string ID. |
| `clm_file_get_modification_time_as` | `path, out` | Raw-path modification-time query. |
| `clm_file_get_access_time` | `id, out` | Query access time using a path-string ID. |
| `clm_file_get_access_time_as` | `path, out` | Raw-path access-time query. |
| `clm_file_is_hidden` | `id, out` | Status plus native hidden classification, inspecting the entry. |
| `clm_file_is_hidden_as` | `path, out` | Raw-path hidden query. |
| `clm_file_is_read_only` | `id, out` | Status plus native read-only attribute, not access permissions. |
| `clm_file_is_read_only_as` | `path, out` | Raw-path read-only query. |
| `clm_file_is_compressed` | `id, out` | Status plus native filesystem compression attribute. |
| `clm_file_is_compressed_as` | `path, out` | Raw-path compression query. |
| `clm_file_is_archive` | `id, out` | Status plus native archive attribute, not file-format detection. |
| `clm_file_is_archive_as` | `path, out` | Raw-path archive-attribute query. |
| `clm_file_set_attributes` | `path, attributes, mask` | Update supported mutable attribute bits only. |
| `clm_file_set_attributes_as` | `path, attributes, mask` | Raw-path attribute update. |
| `clm_file_set_times` | `path, creation_time, modification_time, access_time, mask` | Update selected timestamp fields only. |
| `clm_file_set_times_as` | `path, creation_time, modification_time, access_time, mask` | Raw-path timestamp update. |

The metadata result contains path classification, logical `size`, attribute values
and their `supported_attributes` mask, creation/modification/access timestamps,
and a `supported_times` mask. Missing-entry handling, link metadata validity, and
which fields remain meaningful on each result must be finalized. Unsupported
data is not a false attribute or a zero timestamp.

Attribute symbols use `CLM_FILE_ATTRIBUTE_*`: read-only, hidden, system, archive,
temporary, offline, encrypted, compressed, reparse-point, and symbolic-link.
Classification-only bits cannot be set to create an entry type. Native attributes
are not ACLs, transport security, network availability, or archive-format parsing.
Hidden status follows native attributes or POSIX dot naming. Read-only status
does not determine effective write permissions.

Timestamp selection uses `CLM_FILE_TIME_CREATION`,
`CLM_FILE_TIME_MODIFICATION`, and `CLM_FILE_TIME_ACCESS`. Unselected fields are
unchanged. Creation time is birth time, never Unix metadata-change time. The
current C binding uses native `time_t` precision rather than promising a universal
epoch/range or nanosecond resolution. Access-time updates can be deferred or
disabled. Unsupported requested metadata updates fail explicitly.

## Illustrative Resource Flow

```text
PATH identifies an existing filename string.
CONTENT identifies a preallocated binary destination.
HANDLE is an unused resource ID, separate from PATH.

open(HANDLE, PATH, READ) -> successful resource registration
read(HANDLE, CONTENT, requested_bytes) -> actual bytes and status
tell(HANDLE) -> byte position, not a character index
close(HANDLE) -> releases the resource; PATH text remains unchanged
```

This is conceptual pseudocode; it is not the current C signature or runnable stub
behavior. Whole-file helpers do not need this explicit resource flow.

## Safety and Remaining Decisions

- Path checks can become stale before I/O. Enforce link and identity policy at
  the native open/replace boundary rather than trusting earlier lexical checks.
- Validate counts, offsets, mode combinations, and source/destination capacity
  before side effects; never silently narrow 64-bit positions.
- Do not equate successful writes, flushes, atomic rename, and durable storage.
- Define partial-write/read diagnostics, close-error retirement, handle-ID reuse,
  cancellation, sharing, thread safety, and synchronization before implementation.
- Define non-BOM unexpected-BOM behavior, encoded append compatibility, source
  aliasing, metadata capabilities, permissions, and output publication rules.
- Define copy/move link treatment, atomic-replacement metadata handling, temporary
  cleanup errors, and native filesystem limits explicitly.

## Validation Plan

The C regression suite covers binary and text adapters, all declared encodings,
BOM append behavior, line packing, unknown/duplicate/retired IDs, EOF, 64-bit seek,
positioned cursor preservation, range locks, metadata, atomic replacement,
exclusive temporary creation, shutdown, and fixture cleanup on Windows. The
broader fault-injection and cross-platform checks below remain future work.

Future tests should use isolated fixtures for invalid IDs/types, duplicate handle
registration, binary zero bytes, exact/insufficient capacities, size-query races,
malformed text/BOMs, line boundaries, short reads, EOF, partial writes, large and
negative seeks, positioned cursor preservation, truncate/extend, append modes,
unsupported metadata, native locks, exclusive temporary creation, cleanup, and
atomic-replacement failures. Fault injection is needed for meaningful late-write,
allocation, sync, and close-error coverage. These are proposed tests, not claims
of fault-injection or cross-platform verification beyond the current suite.