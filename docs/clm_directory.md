# CLM Directory

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_file](clm_file.md), [clm_path](clm_path.md), [clm_string](clm_string.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Filesystem directory creation, traversal, querying, copying and removal.

## Capabilities and operations

- Inspect existence, type, emptiness and entries; create directories and directory trees.
- Traverse paths and perform supported rename, copy and deletion operations.
- Use platform path conventions through clm_path rather than embedding separators.
- Handle permissions, symlinks, races and platform limitations as filesystem-specific outcomes.

## Detailed module contract

## Status and Scope

This is a **language-neutral reference** for the directory operation families in the current CLM snapshot. Portable concepts do not require a particular native handle
representation. Binding-specific behavior and platform limits are recorded below;
remaining cross-platform design questions are not promises of identical native
capabilities on all operating systems.

Platform-specific behavior and test coverage should be verified separately for each language binding.

### Portable behavior

- Directory operations reuse native file/path metadata and UTF-8 conversion.
  Existing output strings/arrays require the proper type; scalar outputs are
  assigned only on success. Packed-buffer calls report requirements/counts on
  insufficient capacity without partial writes. Inputs may alias string outputs.
- Simple listings contain parent-prefixed child paths, preserving whether the
  supplied parent is relative or absolute. They include hidden entries and use
  unsorted native enumeration order. File/directory-only listings exclude links.
  Arrays are replaced only after all strings and backing storage are prepared.
- The traversal engine collects one directory's entries before callbacks, not a
  whole-tree snapshot. Callbacks receive borrowed text valid during the call.
  Changing the filesystem from a callback can make later traversal fail.
- Globs use case-sensitive UTF-8 literals, `*` for any sequence, and `?` for one
  code point; there is no bracket, escape, or recursive-glob grammar. Name filters
  select callbacks but do not prevent descent through unmatched directories.
- Default options mean immediate children, all types, hidden included. Explicit
  options control hidden inclusion; maximum depth counts immediate children as 1.
  There is a hard recursion limit of 256 even when requested depth is unlimited.
  Following links checks native ancestor identities and fails on a cycle.
- Recursive size sums logical regular-file bytes including hidden files, skips
  link targets, counts hard-linked entries independently, and stays on the root
  filesystem. A failed scan leaves its total output unchanged.
- Recursive creation is idempotent for real existing directories and rejects
  existing link/reparse roots. Native Windows creation inherits parent ACLs;
  POSIX uses mode 0700. Creation can leave already created parents on failure.
- Tree copy merges directory structure when appropriate, obeys regular-file
  overwrite policy, and rejects canonical source/self/subtree destinations.
  Copying link or special entries returns unsupported rather than recreating or
  traversing them. Crossing a filesystem boundary during tree copy is unsupported.
  Files copied before failure remain; metadata is not copied implicitly.
- Delete/clear reject lexical roots, `.`/`..` roots, and link roots, and do not
  traverse descendant links or junctions. Windows guards keep scanned directories
  open without delete sharing; POSIX deletion is descriptor-relative/no-follow.
  Cross-device recursive deletion is unsupported. Mutations can leave partial work.
- These checks are not a sandbox against arbitrary concurrent replacement of
  untrusted ancestor directories. Callers must control destructive-operation roots.
- Metadata uses support masks. Missing entries produce a zeroed successful query;
  a present regular-file argument to directory metadata fails. Windows junctions
  and reparse points are classified separately; mount-point classification is
  unsupported there. POSIX mount classification recognizes device transitions
  and filesystem roots, not all same-device bind-mount cases.
- Directory attributes/timestamps share the file implementation. POSIX native
  creation time and most Windows attribute concepts return unsupported. Creation
  time is never replaced with Unix metadata-change time.
- Working-directory setters affect process state. Temporary-base queries create
  nothing; exclusive temporary child creation publishes a path only after create,
  and removes the new directory if publication fails. Buffer measurement and
  insufficient capacity create nothing. Caller cleanup is required after success.

The module concerns directory creation, removal, copying, enumeration, metadata,
and temporary-directory management. Lexical path processing belongs to
[CLM Path](clm_path.md); regular-file contents and open-file resources belong to
[CLM File](clm_file.md). Working-directory wrappers should share Path's native
facilities rather than implement a second operating-system integration layer.

Examples below illustrate intended behavior only. They are not executable code,
test results, or evidence that the operations are implemented.

## Portable Data Model

A **path ID** identifies text containing a filesystem path. A path ID is not a
directory handle, does not create an entry, and does not prove that one exists.
The current reference binding uses existing UTF-8 BPA strings. Other bindings must make
their equivalent ownership, encoding, and lifetime rules explicit without being
required to copy C types, pointer conventions, or memory layouts.

| API form | Meaning |
| --- | --- |
| Unsuffixed path input | Input is a path-string ID. |
| `_as` | Path input is caller-owned UTF-8 text rather than a path ID. |
| `_to` | Text output is written to a caller-owned buffer. |
| Scalar output | Conceptual Boolean, timestamp, count, or metadata result; not a string array. |
| String-array output | An existing array of path strings, replaced only on successful collection. |

Function tables retain the procedural names and parameter order of the API,
but omit programming-language types. An output ID designates referenced contents;
its numeric value is not reassigned. A destination path argument to `copy` or
`move` is an input naming a filesystem location, not a writable string output.

All path text is UTF-8 in the current design. Native filesystem conversion,
non-UTF-8 Unix names, source/output aliasing, array publication, and resource
lifetime details must be finalized per binding before implementation.

## Results and Failure Behavior

| Conceptual result | Intended meaning |
| --- | --- |
| Success | The requested operation completed. |
| Failure | Invalid arguments, insufficient capacity, filesystem errors, or incomplete work. |
| Unsupported | The requested capability or attribute has no supported native meaning. |
| Stopped | Callback enumeration was deliberately stopped by the caller. |

Representative status identifiers are `CLM_DIRECTORY_OPERATION_SUCCESS`,
`CLM_DIRECTORY_OPERATION_FAILURE`, `CLM_DIRECTORY_OPERATION_UNSUPPORTED`, and
`CLM_DIRECTORY_OPERATION_STOPPED`, currently encoded as 0, -1, -2, and 1.
Other bindings may use explicit result types or exceptions while preserving the
distinctions. Do not treat all nonzero results as interchangeable.

`exists` and `exists_as` are direct predicates: 1 means an existing directory,
0 means absent or not a directory, and -1 means a query error. Other `is_*`
operations return an operation status and provide their Boolean answer separately.
A successful query with a false answer is not an operation failure.

Query outputs are assigned only when valid. Packed-buffer measurement operations
report capacity/count outputs even when capacity is insufficient, without touching
the destination buffer. Mutations can leave partial filesystem progress, and
callbacks already delivered cannot be undone by a later traversal error.

Filesystem mutations are not generally transactional. Recursive creation,
copying, clearing, or deletion can make progress before an error; implementations
must document partial progress and cleanup rather than promise rollback.

## Lifecycle and Mutation API

These operations act on filesystem entries without remapping their path IDs.
Single-level creation rejects an existing entry; recursive creation accepts an
already existing directory but not an existing non-directory.

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_directory_create` | `id` | Create one directory without creating missing parents. |
| `clm_directory_create_as` | `path` | Raw-path form of single-directory creation. |
| `clm_directory_create_recursive` | `id` | Create the directory and missing parents. |
| `clm_directory_create_recursive_as` | `path` | Raw-path form of recursive creation. |
| `clm_directory_move` | `id, new_id` | Move or rename without overwrite or implicit cross-filesystem fallback. |
| `clm_directory_move_as` | `path, new_path` | Raw source and destination paths for moving. |
| `clm_directory_copy` | `id, destination, overwrite` | Copy a tree with explicit regular-file overwrite permission. |
| `clm_directory_copy_as` | `path, destination, overwrite` | Raw-path form of tree copying. |
| `clm_directory_delete` | `id` | Remove an empty directory only. |
| `clm_directory_delete_as` | `path` | Raw-path form of empty-directory removal. |
| `clm_directory_delete_recursive` | `id` | Explicitly remove a tree and its root entry. |
| `clm_directory_delete_recursive_as` | `path` | Raw-path form of tree deletion. |
| `clm_directory_clear` | `id` | Remove descendants while retaining the directory itself. |
| `clm_directory_clear_as` | `path` | Raw-path form of clearing. |

Moving across filesystems fails instead of silently turning into copy/delete.
Copying into the source subtree must be rejected; lexical string checks alone are
not sufficient to enforce that safely. The reference copy behavior rejects links/special
entries as unsupported rather than recreating or traversing their targets.
Type conflicts are errors, not permission to
replace a directory with a file or vice versa.

Deletion and clearing must reject filesystem roots and unsafe link roots and
must not traverse symbolic-link or junction targets. Native traversal safety,
mount-boundary handling, concurrent entry replacement, metadata preservation,
and partial-copy cleanup require implementation decisions before these are usable.

## Existence and Emptiness API

| Operation | Parameters | Intended result |
| --- | --- | --- |
| `clm_directory_exists` | `id` | Direct directory-existence predicate, following links. |
| `clm_directory_exists_as` | `path` | Raw-path directory-existence predicate. |
| `clm_directory_is_empty` | `id, b` | Status plus Boolean output indicating no children. |
| `clm_directory_is_empty_as` | `path, b` | Raw-path emptiness query. |

A dangling link does not resolve to an existing directory. An emptiness query
excludes only the special `.` and `..` entries: hidden children make a directory
nonempty. A missing path or non-directory is an error for emptiness, not true.
Neither existence nor emptiness grants permission for a later operation or
guarantees that the filesystem remains unchanged.

## Working and Temporary Directories

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_directory_get_current` | `dest` | Write the current directory to an existing string destination. |
| `clm_directory_get_current_to` | `dest, dest_size` | Export the current directory to a writable UTF-8 buffer. |
| `clm_directory_set_current` | `src` | Set process-wide current directory using a path ID. |
| `clm_directory_set_current_as` | `src` | Set current directory using raw UTF-8 text. |
| `clm_directory_get_temporary` | `out` | Write the configured temporary base directory to a string. |
| `clm_directory_get_temporary_to` | `out, out_size` | Export the temporary base directory into a buffer. |
| `clm_directory_create_temporary` | `base, out` | Exclusively create a unique child inside an existing base; publish its path. |
| `clm_directory_create_temporary_as` | `path, out` | Raw-base-path form of exclusive temporary creation. |
| `clm_directory_create_temporary_to` | `path, buffer, buffer_size, required_size` | Create and export the path, or measure capacity without creating. |

Current-directory setters change shared process state and affect other threads'
relative paths. They are not per-ID or per-thread settings.

Obtaining the temporary base creates nothing and does not provide private storage.
Creating a temporary child must be exclusive, not a separate name-generation and
nonexclusive-create sequence. No implicit base creation is planned. The caller
owns cleanup after success; if publishing the new path fails, the new directory
must be removed. Safe permissions and cleanup-error reporting still need details.

For `create_temporary_to`, absent buffer with zero capacity measures only and must
not create anything. Insufficient capacity must not create a directory either.
Bounded temporary-name generation and size-query result semantics remain to be
specified; callers must not assume a measured name reserves a directory.

## Immediate Enumeration and Counts

Simple listing operations collect full child paths, include hidden entries,
exclude `.` and `..`, do not recurse, and do not sort. Native enumeration order
is not stable or portable. Real-directory and regular-file listings do not follow
links or classify link entries as their targets. Mixed-entry listing includes
links and other entry kinds.

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_directory_find_directories` | `id, results` | Replace an existing string array with immediate directory paths. |
| `clm_directory_find_directories_as` | `path, results` | Raw parent path and string-array output. |
| `clm_directory_find_directories_to` | `path, buffer, buffer_size, required_size, count` | Export directory paths as packed UTF-8 text. |
| `clm_directory_find_files` | `id, results` | Replace an existing string array with immediate regular-file paths. |
| `clm_directory_find_files_as` | `path, results` | Raw parent path and string-array output. |
| `clm_directory_find_files_to` | `path, buffer, buffer_size, required_size, count` | Export regular-file paths as packed UTF-8 text. |
| `clm_directory_find_entries` | `id, results` | Replace an existing string array with all immediate child paths. |
| `clm_directory_find_entries_as` | `path, results` | Raw parent path and mixed-entry string-array output. |
| `clm_directory_find_entries_to` | `path, buffer, buffer_size, required_size, count` | Export all immediate child paths as packed UTF-8 text. |
| `clm_directory_get_directories_count` | `id, out` | Count immediate real directories into a 64-bit result. |
| `clm_directory_get_directories_count_as` | `path, out` | Raw-path directory count. |
| `clm_directory_get_files_count` | `id, out` | Count immediate regular files into a 64-bit result. |
| `clm_directory_get_files_count_as` | `path, out` | Raw-path regular-file count. |
| `clm_directory_get_entries_count` | `id, out` | Count all immediate entry types. |
| `clm_directory_get_entries_count_as` | `path, out` | Raw-path mixed-entry count. |

Counts and corresponding listings use the same classification and hidden-entry
rules. Overflow and scan errors must not publish incomplete counts. Whether
relative parent inputs produce absolute full paths or parent-prefixed paths must
be settled before implementation; full child paths must not be interpreted as
bare names. Independent calls are not a shared filesystem snapshot.

### Packed Buffer Format

Each path is NUL-terminated, followed by one extra final NUL after the last path.
An empty list requires one NUL byte. Capacities and required sizes count bytes
including every terminator. `required_size` and `count` are mandatory outputs.
An absent buffer with zero capacity requests measurement only. Insufficient
capacity must report requirements without partial writes or truncated names.

```text
two entries: "first-path\0second-path\0\0", count = 2
empty list:  "\0",                        count = 0
```

These are notation examples, not literal file names containing NUL. Unicode
characters can occupy multiple UTF-8 bytes. Filesystem changes can invalidate a
capacity measurement before a later fill call.

## Callback Enumeration

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_directory_enumerate` | `id, options, visit, context` | Enumerate with traversal controls and an entry callback. |
| `clm_directory_enumerate_as` | `path, options, visit, context` | Raw-root-path form of callback enumeration. |

The root is not visited. Default options mean immediate children, all types, and
hidden entries included. Explicit options contain:

| Field | Intended meaning |
| --- | --- |
| `flags` | Recursive traversal, hidden inclusion, link following, and staying on one filesystem. |
| `types` | File, directory, link, and other selection bits; zero selects all types. |
| `max_depth` | Immediate children have depth 1; zero permits unlimited depth when recursive. |
| `pattern` | Optional name filter; absent means match all; glob grammar remains undecided. |

Representative flag names are `CLM_DIRECTORY_ENUM_RECURSIVE`,
`CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN`, `CLM_DIRECTORY_ENUM_FOLLOW_SYMLINKS`, and
`CLM_DIRECTORY_ENUM_STAY_ON_FILESYSTEM`. Entry kinds are selected by
`CLM_DIRECTORY_ENTRY_FILE`, `CLM_DIRECTORY_ENTRY_DIRECTORY`,
`CLM_DIRECTORY_ENTRY_LINK`, and `CLM_DIRECTORY_ENTRY_OTHER`.

Each callback receives `path`, final-component `name`, one `type` bit, and `depth`.
Its text is borrowed and valid only during that invocation. Copy text before
retaining it. `context` is caller-owned state, not module-owned memory.

Callback results are continue, stop, or error, named
`CLM_DIRECTORY_VISIT_CONTINUE`, `CLM_DIRECTORY_VISIT_STOP`, and
`CLM_DIRECTORY_VISIT_ERROR`. Deliberate stop produces the distinct stopped
operation result. Callbacks already delivered and their side effects cannot be
rolled back. No ordering, stable snapshot, or thread-safety guarantee is implied.
Following links requires cycle detection and enforcement of depth/mount policies.

## Metadata, Size, and Timestamp API

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_directory_get_info` | `id, follow_symlinks, out` | Query typed metadata and support masks. |
| `clm_directory_get_info_as` | `path, follow_symlinks, out` | Raw-path metadata query. |
| `clm_directory_get_size` | `id, out` | Sum recursive logical regular-file bytes without following links. |
| `clm_directory_get_size_as` | `path, out` | Raw-path recursive logical size query. |
| `clm_directory_get_creation_time` | `id, out` | Query actual filesystem creation time when supported. |
| `clm_directory_get_creation_time_as` | `path, out` | Raw-path creation-time query. |
| `clm_directory_get_modification_time` | `id, out` | Query directory modification time, not newest descendant modification. |
| `clm_directory_get_modification_time_as` | `path, out` | Raw-path modification-time query. |
| `clm_directory_get_access_time` | `id, out` | Query reported directory access time. |
| `clm_directory_get_access_time_as` | `path, out` | Raw-path access-time query. |
| `clm_directory_set_attributes` | `id, attributes, mask` | Update selected supported mutable attribute bits. |
| `clm_directory_set_attributes_as` | `path, attributes, mask` | Raw-path attribute update. |
| `clm_directory_set_times` | `id, creation_time, modification_time, access_time, mask` | Update selected timestamp fields only. |
| `clm_directory_set_times_as` | `path, creation_time, modification_time, access_time, mask` | Raw-path timestamp update. |

The directory-info record contains path classification (`exists`, directory,
regular-file, symbolic-link, and hidden fields), attribute values and their
`supported_attributes` mask, creation/modification/access timestamps, and a
`supported_times` mask. Unsupported fields are not equivalent to supported false
or zero values. Link following explicitly selects entry versus target metadata.
Missing-entry handling and which fields are valid on each outcome remain pending.

Recursive size means logical bytes of regular-file entries, not allocated disk
usage or directory metadata size. Hard-linked entries count independently. Failed
or overflowing scans must not publish incomplete totals. Mount boundaries and
unreadable-descendant behavior still need explicit rules.

Creation time is birth time, never Unix metadata-change time. Timestamps use native
binding precision; the current reference binding uses `time_t` and does not promise
nanoseconds. Access times may be deferred or disabled by filesystem policies.
Timestamp setters use `CLM_DIRECTORY_TIME_CREATION`,
`CLM_DIRECTORY_TIME_MODIFICATION`, and `CLM_DIRECTORY_TIME_ACCESS`; unselected
fields remain unchanged. Unsupported creation-time updates fail explicitly.

## Attribute and Link Queries

Every operation in this section returns a status plus a separate Boolean output.
Unavailable native concepts return unsupported, not false. Attribute masks use
the corresponding `CLM_DIRECTORY_ATTRIBUTE_*` symbols.

| Operation | Parameters | Intended classification |
| --- | --- | --- |
| `clm_directory_is_read_only` | `id, out` | Native read-only attribute, not effective write permissions. |
| `clm_directory_is_read_only_as` | `path, out` | Raw-path read-only attribute query. |
| `clm_directory_is_hidden` | `id, out` | Native hidden attribute or POSIX dot naming, inspecting the entry. |
| `clm_directory_is_hidden_as` | `path, out` | Raw-path hidden query. |
| `clm_directory_is_system` | `id, out` | Native system attribute, not ownership by the operating system. |
| `clm_directory_is_system_as` | `path, out` | Raw-path system-attribute query. |
| `clm_directory_is_archive` | `id, out` | Native archive attribute, not an archive file format. |
| `clm_directory_is_archive_as` | `path, out` | Raw-path archive-attribute query. |
| `clm_directory_is_temporary` | `id, out` | Native temporary attribute, not location under a temporary base. |
| `clm_directory_is_temporary_as` | `path, out` | Raw-path temporary-attribute query. |
| `clm_directory_is_offline` | `id, out` | Native offline attribute, not network reachability. |
| `clm_directory_is_offline_as` | `path, out` | Raw-path offline-attribute query. |
| `clm_directory_is_encrypted` | `id, out` | Native entry encryption metadata, not transport/volume security. |
| `clm_directory_is_encrypted_as` | `path, out` | Raw-path encryption query. |
| `clm_directory_is_compressed` | `id, out` | Native filesystem compression metadata. |
| `clm_directory_is_compressed_as` | `path, out` | Raw-path compression query. |
| `clm_directory_is_reparse_point` | `id, out` | Windows reparse-point identity, inspecting the entry itself. |
| `clm_directory_is_reparse_point_as` | `path, out` | Raw-path reparse-point query. |
| `clm_directory_is_mount_point` | `id, out` | Native mount classification, with platform policy pending. |
| `clm_directory_is_mount_point_as` | `path, out` | Raw-path mount query. |
| `clm_directory_is_junction` | `id, out` | Windows junction identity without following its target. |
| `clm_directory_is_junction_as` | `path, out` | Raw-path junction query. |
| `clm_directory_is_symbolic_link` | `id, out` | Symbolic-link identity independent of target existence. |
| `clm_directory_is_symbolic_link_as` | `path, out` | Raw-path symbolic-link query. |

Symbolic links, junctions, mount points, and reparse points are related but not
synonyms. Setters may change only supported mutable attribute bits; they must not
pretend to create these entry types by setting classification flags. Read-only
attributes do not alter ACLs or establish effective access permissions.

## Safety and Remaining Decisions

- Filesystem existence checks can become stale before an operation starts.
- Destructive operations must enforce native containment/link rules, not rely
  solely on lexical prefix checks or prior canonicalization.
- Hidden entries are real contents; omitting them during deletion or emptiness
  checks can produce incorrect or unsafe results.
- Recursive scans need cycle, depth, resource, and mount-boundary policies.
- Temporary creation must use exclusive native creation and private permissions.
- Copying and recursive mutations need defined partial-progress and cleanup behavior.
- Pattern syntax, relative-result path representation, sorting, metadata missing
  cases, output aliasing, concurrency, and cancellation cleanup remain to be settled.

## Validation Plan

The reference regression suite covers isolated creation, recursive trees, hidden entries,
counts, array/buffer adapters, exact size reporting, callback filtering and stop,
depth limits, tree copy/clear/move, source-subtree rejection, working-directory
restoration, metadata support masks, exclusive temporary children, no-follow
outside-link targets, cycle detection, root guards, and cleanup on Windows.
Link fixtures explicitly report a skip when native creation permission is absent.
The additional fault and cross-platform scenarios below remain future work.

Future tests should cover duplicate/invalid IDs, wrong result types, buffers of
zero/exact/insufficient capacity, empty and hidden-only directories, mixed entry
types, Unicode names, UNC paths, recursive creation, subtree-copy rejection,
cross-filesystem moves, root guards, link cycles, dangling links, mount boundaries,
callback cancellation, unsupported metadata, overflow, and temporary cleanup.
Fixtures must be isolated and restore working-directory state. The remaining
fault-injection and cross-platform scenarios are validation work beyond the
current Windows regression suite.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Inspect existence, type, emptiness and entries; create directories and directory trees.
