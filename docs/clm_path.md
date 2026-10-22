# CLM Path (`clm_path`)

> **CLM — Cross Language Modules**  
> An initiative by Deeperlevel, developed with community contributions.  
> Official repository: https://github.com/deeperlevel/clm  
> Contact: deeparchive@outlook.com  
> License: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)  
> **Different languages. Shared technologies. No borders.**

## Overview

`clm_path` provides a small, **procedural-first interface for working with filesystem paths**. It covers path composition, name and extension extraction, relative and absolute paths, separators, basic existence checks, and temporary directory creation.

CLM specifies the *meaning* of these operations, not the underlying implementation. An implementation may use the host language's path library or native operating-system facilities. No mandatory class hierarchy, reflection, or custom filesystem abstraction is required.

**Scope:** Most operations manipulate path strings without opening files. Two operations interact with the filesystem: `clm_path_exists` queries it, and `clm_path_get_temporary_directory` creates directories.

## Naming and implementation conventions

The language-neutral names in this document use the `clm_path_` prefix. The supplied reference source still uses the older `cla_path_` prefix; adopting `clm_path_` is a naming migration, not a claim that the reference code already exports those names.

In languages that support it, a `Path` convenience facade may forward to the procedural functions. The functions remain the canonical interface.

Examples below use **pseudocode**. String literals, return-value representations, and error mechanisms should be adapted to each language.

## Core path concepts

- **Path:** A sequence identifying a filesystem entry according to host-platform conventions. A path need not exist to be manipulated.
- **Path component:** A directory or filename segment separated by directory separators.
- **Root:** The platform-recognized root prefix. Examples include `/` on POSIX and a drive or share root on Windows.
- **Absolute/full path:** A path the host considers absolute, without requiring the entry to exist.
- **Filename:** The final component after the last separator; an ending separator yields an empty filename in the reference behavior.
- **Extension:** The final filename suffix including the leading dot, where recognized by the host's splitting rules.

Path interpretation is **host-native**. Windows drive paths, UNC shares, alternate separators, and POSIX roots may therefore produce different results on different systems. CLM is not intended to interpret every path using a universal path grammar.

## Quick start

```text
base = "/srv/app"
config = clm_path_join(base, "config/settings.json")

filename = clm_path_get_filename(config)              // "settings.json"
extension = clm_path_get_file_extension(config)      // ".json"
stem = clm_path_get_file_name_without_extension(config) // "settings"

backup = clm_path_change_extension(config, ".bak")
full = clm_path_get_full_path(config)

if clm_path_exists(config):
    // Read the file using an appropriate file module.
```

These examples illustrate POSIX-style paths; the actual joining separator and absolute-path representation follow the host operating system.

## API reference

### Path composition and normalization

| Function | Parameters | Result | Contract |
|---|---|---|---|
| `clm_path_join` | `path1`, `path2` | Path string | Join two native path components. If the second path overrides the first under host rules (for example, an absolute second path), preserve that native behavior. |
| `clm_path_get_full_path` | `path` | Absolute path string | Convert to an absolute, normalized path using the current working directory where needed. Does **not** require existence or resolve symbolic links in the reference implementation. |
| `clm_path_get_relative_path` | `relative_to`, `path` | Relative path string | Express `path` relative to the directory `relative_to`. Fail when no meaningful relative representation can be produced, such as different Windows drives. |
| `clm_path_is_full_path` | `path` | Boolean | Report whether the host treats the path as absolute. |
| `clm_path_get_path_root` | `path` | Root string | Return the host-recognized root portion; an ordinary relative path without a drive has an empty root. Drive-relative forms may return a drive prefix without a root separator. |

**Important:** `get_full_path` performs lexical normalization, not canonical filesystem resolution. It must not be treated as proof that the resulting file exists, nor as a security boundary against symlinks or races.

### Names and extensions

| Function | Parameters | Result | Contract |
|---|---|---|---|
| `clm_path_get_directory_name` | `path` | String | Return the directory portion of the path, or an empty string if none is present. |
| `clm_path_get_filename` | `path` | String | Return the final filename component; trailing separators produce an empty string. |
| `clm_path_get_file_name_without_extension` | `path` | String | Return the filename with its final recognized extension removed. |
| `clm_path_get_file_extension` | `path` | String | Return the final recognized extension, including `.`, or an empty string. |
| `clm_path_has_extension` | `path` | Boolean | Report whether `get_file_extension` would return a nonempty extension. |
| `clm_path_change_extension` | `path`, `extension` | Path string | Replace the last recognized extension; an empty extension removes it. Accept an extension with or without its leading dot. |

Typical examples, subject to host splitting conventions:

```text
clm_path_get_filename("reports/summary.csv")             => "summary.csv"
clm_path_get_file_extension("reports/summary.csv")       => ".csv"
clm_path_get_file_name_without_extension("summary.csv")  => "summary"
clm_path_change_extension("summary.csv", "json")         => "summary.json"
clm_path_change_extension("summary.csv", "")             => "summary"
clm_path_get_file_extension(".gitignore")                 => ""
```

A dot-prefixed filename such as `.gitignore` does not count as having an extension under the reference behavior. Extension helpers operate on the **last** extension; for example, replacing the extension of `archive.tar.gz` produces `archive.tar.zip` when the requested extension is `zip`.

### Separators and path validation helpers

| Function | Parameters | Result | Contract |
|---|---|---|---|
| `clm_path_get_directory_separator` | None | Character/string | Return the host's primary directory separator. |
| `clm_path_ends_in_directory_separator` | `path` | Boolean | Test for a host-native or supported alternate directory separator at the end. |
| `clm_path_trim_ending_separator` | `path` | Path string | Remove trailing separators while preserving a recognized path root. |
| `clm_path_get_invalid_path_characters` | None | Collection of characters | Return a set of characters that are invalid in host paths under the implementation's documented policy. **Not** a complete pathname validator. |

Examples:

```text
clm_path_trim_ending_separator("reports///") => "reports"
clm_path_ends_in_directory_separator("reports/") => true  // On POSIX
```

The implementation must preserve root meaning when trimming: `/` must not become an empty path, and a Windows drive or share root must retain its root prefix.

**Validation caveat:** Invalid path names also depend on component position, reserved device names, filesystem rules, trailing spaces or dots, encoding, and more. `get_invalid_path_characters` is informational and must not be used alone as a security or validity check. The supplied reference returns the NUL character on POSIX and a common character set on Windows; it does not enumerate all forbidden paths.

### Filesystem interaction and generated names

| Function | Parameters | Result | Contract |
|---|---|---|---|
| `clm_path_exists` | `path` | Boolean | Report whether a filesystem entry currently exists, including directories. Broken symbolic links report false under the reference behavior. |
| `clm_path_get_random_filename` | Optional `extension` | Filename string | Generate a random filename without creating or reserving it. Accept an optional extension with or without its leading dot. |
| `clm_path_get_temporary_directory` | `path` | Directory path string | Ensure the supplied base directory exists, create a unique child directory below it, and return its path. The caller is responsible for cleanup. |

The reference implementation generates random filenames as **12 hexadecimal characters** (six cryptographically random bytes), followed by the optional extension:

```text
name = clm_path_get_random_filename("tmp")
// Example shape: "a18b7e29c4fd.tmp" (the actual value varies)
```

This generates a candidate name only. It does **not** create the file, guarantee uniqueness, or eliminate race conditions. Callers should use an atomic creation operation when uniqueness matters.

`get_temporary_directory(base)` **does** modify the filesystem. It may create both the base directory and a new child directory. The current design includes this operation for compatibility; a future `clm_directory` module might be a more natural home for directory creation.

## Return values and errors

The shared contract identifies successful results and relevant failure conditions. Specific language implementations may use exceptions, result/status types, or explicit output parameters, provided that outcomes remain distinguishable and documented.

Expected failure scenarios include:

- A relative path cannot be computed across incompatible roots or drives.
- A requested temporary directory cannot be created because of permissions, missing facilities, or filesystem errors.
- Invalid dimensions or formats are supplied to host APIs (where applicable).
- Path conversion cannot be represented using the host's supported encoding or path model.

A Boolean from `clm_path_exists` is not a guarantee of continued existence: the filesystem may change immediately after the check. The reference Python behavior can also report certain OS permission or representation errors as `false`.

## Path semantics and portability

### POSIX and Windows

Implementations should follow the host's actual rules for separators, absolute paths, volume/drive identifiers, shares, and special root syntax. Do not assume `/` and `\` are interchangeable across all environments.

Examples of distinctions worth testing:

- POSIX root (`/`) and possibly distinct double-slash root (`//`).
- Windows drive-absolute (`C:\\folder`) versus drive-relative (`C:folder`) paths.
- Windows UNC/share roots.
- Alternate separators accepted by the host implementation.
- A second absolute path supplied to `join`.

### Lexical versus filesystem operations

Most functions should not probe the filesystem. In particular, `get_filename`, `change_extension`, `join`, and `get_path_root` should work on paths that do not exist.

`get_full_path` is not a symlink resolver. `exists` and temporary-directory creation do access the filesystem.

### Generated names and directories

For `get_random_filename`, use a high-quality randomness source when possible; the reference uses a cryptographically secure generator. Creating a filename is not the same as opening it exclusively.

For `get_temporary_directory`, prefer operating-system or runtime facilities that create a unique directory atomically. The caller controls its lifetime and removal.

## Optional convenience facade

Implementations with object-oriented or namespace facilities may provide a `Path` facade with shortened names, such as:

```text
Path.get_filename(path)
Path.get_full_path(path)
Path.join(base, name)
```

This facade is **optional** and should delegate to the same behavior as `clm_path_get_filename`, `clm_path_get_full_path`, and `clm_path_join`. CLM does not require classes or object instances.

## Cross-language compatibility checklist

An implementation should verify that:

- [ ] All documented procedural operations are available, or unsupported ones are explicitly documented.
- [ ] Path syntax follows the host platform and does not silently impose another platform's conventions.
- [ ] Extension handling is consistent for dotfiles, multiple dots, and filenames without extensions.
- [ ] Filename extraction from paths ending in separators is documented and tested.
- [ ] Root-aware separator trimming never destroys a valid root.
- [ ] Absolute-path computation does not require an existing file and does not claim to resolve symbolic links.
- [ ] Relative-path behavior handles incompatible roots explicitly.
- [ ] Random filename generation does not claim to reserve the result.
- [ ] Temporary-directory creation is unique and documents caller cleanup responsibilities.
- [ ] File-existence results are not presented as race-free guarantees.
- [ ] Errors and unavailable host capabilities are reported predictably.
- [ ] An optional OOP facade does not become a dependency of the procedural API.

## Reference-source compatibility notes

The provided reference implementation exports `cla_path_*` names rather than `clm_path_*`. The documentation uses the **target CLM naming convention** for consistency with the other modules.

The reference relies on native path semantics and provides 18 operations through its `Path` facade. Its `get_temporary_directory` signature requires a base path; it is **not** the usual zero-argument function that returns the system's default temporary folder.

No separate cross-language return-code enumeration is defined by the provided source. Implementations should not invent one and describe it as an existing CLM standard.

---

**CLM — Cross Language Modules**  
**Don't start from scratch, bro.**
