# CLM Path

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Platform-aware filesystem path inspection and manipulation without directly performing file I/O.

## Capabilities and operations

- Classify absolute/relative paths and inspect path components.
- Join, split, normalize and transform path segments and separators.
- Handle platform-specific drive and UNC forms where supported.
- Do not conflate filesystem paths with URLs.

## Detailed module contract

## Status and Scope

This is a **language-neutral API catalog** for filesystem path manipulation and
selected filesystem queries. The reference binding implements all 43 listed operations.
Shared cross-language policy remains a draft where bindings can differ; references
below to pending policies concern that broader specification. The current C
behavior is defined in the binding contract section below and supersedes those
draft notes for C callers.

This guide supersedes the older private-registry design. It does not require an
independent Path registry, mapping lifecycle, or optional destination arguments.

Path handles filesystem names, including Windows UNC paths. URL references,
percent encoding, and URL resolution belong to [CLM URL](clm_url.md). Creating,
deleting, reading, and writing filesystem entries belong to file and directory
modules, rather than lexical path manipulation.

## Language-Neutral Model

### Portable behavior

- Text arguments are existing BPA string IDs. Invalid or wrong-type IDs fail.
- Operations return 0 on success and 1 on error. Predicates return 0/1 or -1 on error.
- All text outputs require existing string destinations; inputs may alias outputs.
- Writes are committed only after preparation; failure leaves outputs unchanged.
- Successful text writes select UTF-8 export encoding. Native grammar is used for
	lexical paths; explicit UNC helpers recognize Windows UNC syntax on any host.
- `is_absolute` and `is_fully_qualified` both mean independent of current-directory
	context. Windows drive-relative and root-relative paths are not fully qualified.
- `join` replaces the first path when the second has a native root/drive prefix.
	Otherwise it joins without normalizing dot segments; empty suffixes leave it unchanged.
- `normalize` collapses ordinary dot segments, preserves unresolved leading `..`,
	and preserves extended Windows paths verbatim. `sanitize` additionally rejects
	invalid native filename syntax; it does not invent replacement characters.
- Relative conversion requires fully qualified inputs with matching normalized
	roots. Equal paths produce `.`. Lexical containment includes equality and uses
	ASCII case-insensitive comparison on Windows, exact byte comparison on POSIX.
- Filename extraction ignores trailing separators. Parents of roots preserve
	their roots; parentless relative names produce `.`. A rootless root extraction
	produces empty text. Extensions include their leading dot; a leading dot alone
	does not create an extension; only the last extension is removed. An empty new
	extension removes it, while a nonempty one may include its own leading dot.
- Ensuring a separator rejects empty paths. Ending trimming preserves roots;
	starting trimming intentionally removes every leading native separator.
- Literal replacement is case-sensitive, global, and non-overlapping; an empty
	search value fails. Printable invalid-character guidance excludes NUL and controls.
- UNC conversions reject dot-segment/trailing-dot/space ambiguities; conversion
	back to ordinary syntax rejects results of 260 or more UTF-8 bytes.
- Metadata follows links when requested. A missing entry is a successful query
	with all fields false; access/query errors leave the result untouched.
	No-follow symlink results classify the link, not its target. Windows junctions
	are not classified as symbolic links. `exists`, `is_file`, and `is_directory`
	follow links; `is_symlink` and `is_hidden` inspect the entry itself.
- Windows hidden status uses attributes; POSIX hidden status uses the final
	dot-prefixed name. Canonicalization requires existence and may return extended
	Windows syntax. Current-directory setters affect process-wide state.

Windows uses UTF-16 OS calls; POSIX uses native UTF-8 byte paths and rejects
invalid UTF-8 inputs. Native non-UTF-8 Unix filenames are outside this binding's
text model. The current regression run verifies Windows; POSIX code requires
separate runtime validation. Filesystem tests create temporary fixtures and
explicitly skip link creation if the operating system denies permission.

A **path ID** identifies text containing a path or path fragment. Other text
arguments, such as an extension, server name, or replacement value, also identify
text. An ID is not a filesystem handle and does not prove that an entry exists.
Bindings may represent text references differently, provided their ownership,
encoding, and lifetime rules are explicit. This draft does not require C types,
pointers, a particular allocator, or a particular private registry.

The current design uses UTF-8 text. Bindings must document conversion to native
filesystem representations. Windows filesystem access should use Unicode APIs;
support for Unix filenames that are not valid UTF-8 remains undecided. Embedded
NUL cannot be treated as an ordinary path character at native filesystem boundaries.

In the API tables:

- **Input** means the referenced text is inspected without being changed.
- **In place** means the primary path's text is replaced, not its ID value.
- **Output** means a separate destination text reference or result record.
- **Predicate** means a conceptual Boolean result, distinct from an operation status.
- **Lexical** means no filesystem lookup is required.

Status codes, exceptions, output parameters, tuples, and native result types are
binding choices. The numeric values of C operation statuses are not portable
Boolean values. A binding must distinguish a negative predicate answer from an
invalid reference or operating-system error. Those exact error contracts are
still pending, as are destination creation, source/destination aliasing, and
whether failures leave all output unchanged.

## Path Grammar

Path syntax depends on the selected platform grammar; it is not URL syntax.
Support for manipulating a foreign platform's grammar on the current host remains
to be specified. Recognition of a Windows path does not make it accessible on Unix.

| Example | Interpretation |
| --- | --- |
| `/var/log/app.log` | Fully qualified POSIX path. |
| `logs/app.log` | Relative path. |
| `C:\logs\app.log` | Fully qualified Windows drive path. |
| `C:logs\app.log` | Windows drive-relative path; requires drive current-directory context. |
| `\logs\app.log` | Windows root-relative path; requires current-drive context. |
| `\\server\share\logs\app.log` | Ordinary UNC path. |
| `\\?\UNC\server\share\logs\app.log` | Extended UNC path with different processing rules. |
| `\\server` | Incomplete UNC filesystem path: the share is missing. |

**Rooted**, **absolute**, and **fully qualified** must not be treated as
interchangeable without an explicit platform definition. Fully qualified paths
require no current-directory context. A Windows drive-relative path has a drive
prefix but is not fully qualified.

For UNC paths, the root includes both server and share. Removing separators must
not reduce `\\server\share\` to a server-only path. Extended-length paths must not
be passed through ordinary normalization blindly.

## Classification API

These operations inspect text unless their filesystem behavior is stated.
All path arguments are inputs.

| Operation | Parameters | Intended result |
| --- | --- | --- |
| `clm_path_is_absolute` | `path` | Whether the path is absolute under the selected grammar. |
| `clm_path_is_relative` | `path` | Whether the path is relative under that grammar. |
| `clm_path_is_rooted` | `path` | Whether a root or recognized drive prefix is present. |
| `clm_path_is_fully_qualified` | `path` | Whether resolution needs no current-directory context. |
| `clm_path_is_empty` | `path` | Whether the text is empty, not whether a directory has no entries. |
| `clm_path_is_valid` | `path` | Whether platform-specific path syntax is valid; not an existence test. |
| `clm_path_is_within` | `path, base` | Lexical component-based containment; equality and case rules are pending. |
| `clm_path_has_extension` | `path` | Whether the final filename has an extension under the chosen dot policy. |
| `clm_path_ends_with_separator` | `path` | Whether the text ends in a separator accepted by the selected grammar. |

An invalid ID must not be silently interpreted as an empty or relative path.
`is_within` compares components, not raw string prefixes: `/data/app2` is not a
child of `/data/app` merely because its text starts with the same characters.

## Transformation API

These operations are lexical unless noted otherwise. The first path is modified
in place; other arguments are input text references.

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_path_sanitize` | `path` | Apply an explicit platform-specific sanitization policy, still undefined. |
| `clm_path_join` | `path1, path2` | Join paths and replace `path1`; absolute-second-path behavior is pending. |
| `clm_path_normalize` | `path` | Normalize components without resolving filesystem links. |
| `clm_path_get_full_path` | `path` | Make absolute against current-directory context; existence is not required. |
| `clm_path_get_relative_path` | `path, base` | Replace target `path` with its relative path from directory `base`; reject incompatible roots. |
| `clm_path_get_root` | `path` | Replace the path with its root, preserving UNC and extended root structure. |
| `clm_path_get_parent` | `path` | Replace the path with its lexical parent directory. |
| `clm_path_get_filename` | `path` | Replace the path with its final filename component. |
| `clm_path_get_filename_without_extension` | `path` | Replace the path with the final filename stem. |
| `clm_path_get_extension` | `path` | Replace the path with the final filename extension. |
| `clm_path_change_extension` | `path, new_extension` | Replace or add the filename extension, retaining its directory. |
| `clm_path_remove_extension` | `path` | Remove the filename's final extension, retaining its directory. |
| `clm_path_ensure_ending_separator` | `path` | Append a native separator only when one is absent. |
| `clm_path_trim_starting_separator` | `path` | Remove leading separators only under a root-removal policy still to be specified. |
| `clm_path_trim_ending_separator` | `path` | Remove trailing separators while preserving roots. |
| `clm_path_replace` | `path, old_value, new_value` | Replace matching text under a matching and component-boundary policy still pending. |

Despite their `get_` names, the single-path extraction operations above currently
replace their input text. Callers must preserve an independent copy if they need
the original. This differs from the explicit-destination extraction APIs below.

Normalization, making absolute, and canonicalization are separate operations.
Normalization changes lexical representation; making absolute uses directory
context; canonicalization accesses the filesystem and resolves links. Removing
`..` lexically can differ from resolving it through a symbolic link.

## UNmodule API

UNC recognition is syntactic, not a network reachability test. Recognition covers
complete ordinary and extended UNC paths; generic Windows device paths and local
extended drive paths are not automatically UNC paths.

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_path_is_unc` | `path` | Predicate: a complete ordinary or extended UNC path has a server and share. |
| `clm_path_is_extended_unc` | `path` | Predicate: a complete UNC path uses the extended-length namespace. |
| `clm_path_get_unc_server` | `path, destination` | Copy the server name without separators to output `destination`. |
| `clm_path_get_unc_share` | `path, destination` | Copy the share name without separators to output `destination`. |
| `clm_path_build_unc` | `server, share, destination` | Validate input components and write an ordinary UNC share root. |
| `clm_path_to_extended_unc` | `path` | Convert in place only when extended syntax preserves meaning. |
| `clm_path_from_extended_unc` | `path` | Convert in place only when ordinary syntax can represent the result. |

UNC extraction and construction do not connect to a server or create a share.
Relative conversion between different servers or shares needs an explicit root
compatibility rule and must not fabricate a traversal across independent roots.

## Filesystem and Environment API

These operations may access operating-system state. They are not lexical checks.

| Operation | Parameters | Intended result or effect |
| --- | --- | --- |
| `clm_path_exists` | `path` | Predicate: an entry exists; default link-following policy is pending. |
| `clm_path_is_directory` | `path` | Predicate: directory classification under a link policy still pending. |
| `clm_path_is_file` | `path` | Predicate: regular-file classification under a link policy still pending. |
| `clm_path_is_symlink` | `path` | Predicate: inspect the link entry itself; platform link coverage is pending. |
| `clm_path_is_hidden` | `path` | Predicate: hidden-name or filesystem-attribute policy is platform-specific and pending. |
| `clm_path_get_info` | `path, follow_symlinks, info` | Populate output metadata; following is explicitly selected. |
| `clm_path_canonicalize` | `path` | Resolve an existing filesystem path and links, replacing the path in place. |
| `clm_path_get_current_directory` | `destination` | Write the process's current working directory to the destination text. |
| `clm_path_set_current_directory` | `path` | Change the process-wide working directory to an existing directory. |

The metadata record has `exists`, `is_directory`, `is_file`, `is_symlink`, and
`is_hidden` fields. `follow_symlinks` conceptually selects the target when true
and the entry itself when false. Bindings must define how those fields interact
with link following, what happens for a missing entry or dangling link, and which
outputs remain valid after failure. The fields are not necessarily exclusive:
their exact classification rules must be specified before implementation.

Changing the current directory changes shared process state and affects other
threads' relative-path operations. It is not a per-path setting.

## Platform Information API

| Operation | Parameters | Intended result |
| --- | --- | --- |
| `clm_path_get_separator` | `destination` | Write the native filesystem separator as text. |
| `clm_path_get_invalid_characters` | `destination` | Write a platform-policy character list; scope and NUL representation remain pending. |

A character list alone cannot describe Windows reserved names, component-length
limits, trailing spaces or dots, filesystem restrictions, or all root grammars.
It is not a complete validation or security policy.

## Illustrative Examples

These examples describe concepts, not executable bindings or verified stub output.

```text
ordinary UNC path: \\fileserver\team\reports\annual.txt
server component:  fileserver
share component:   team
share root:        \\fileserver\team\
```

```text
base directory: /srv/app
target path:    /srv/app/assets/logo.png
relative path:  assets/logo.png
```

```text
filesystem path: \\server\share\report.txt   -> Path module
mounted share:   /mnt/team/report.txt        -> Path module
resource URL:    smb://server/share/report.txt -> URL module
```

## Safety Boundaries

- Syntax validation and sanitization do not authorize a filesystem operation.
- Lexical containment does not account for symbolic links, junctions, mounts, or races.
- An existence check can become stale before an entry is opened or modified.
- Canonicalization does not itself prevent time-of-check/time-of-use races.
- Secure file access must enforce containment and permissions at the native I/O boundary.
- Do not use filesystem normalization on URL text or ordinary rules on extended UNC text.

## Decisions Required Before Implementation

1. Define predicate errors, operation results, output ownership, creation, and aliasing.
2. Select host-native versus explicitly selectable foreign path grammars and case rules.
3. Distinguish absolute, rooted, relative, and fully qualified forms on each platform.
4. Define empty paths, roots, parentless paths, trailing separators, and repeated separators.
5. Define dotfiles, multi-part extensions, leading dots, trailing dots, and empty extensions.
6. Define sanitization and replacement policies without implying security guarantees.
7. Define link following, missing entries, hidden attributes, and metadata output validity.
8. Define relative-path requirements and drive/share/root compatibility.
9. Specify Unicode/native conversion, invalid native names, and extended UNC round trips.
10. Specify binding-specific raw-text and buffer convenience APIs separately from the shared model.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_platform](clm_platform.md), [clm_string](clm_string.md).

## Example use cases

Classify absolute/relative paths and inspect path components.
