#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "base.h"
#include "path.h"
#include "bpa.h"
#include "string.h"
#include "file.h"

#include <stdbool.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif

#define CLM_DIRECTORY_OPERATION_SUCCESS 0
#define CLM_DIRECTORY_OPERATION_FAILURE -1
#define CLM_DIRECTORY_OPERATION_UNSUPPORTED -2
#define CLM_DIRECTORY_OPERATION_STOPPED 1

/** @file
 * Directory operations sharing native file/path metadata and UTF-8 conversion.
 * Unsuffixed text inputs are UTF-8 BPA string IDs; _as accepts NUL-terminated
 * UTF-8 paths, and _to writes caller-owned text buffers. Scalar outputs are
 * typed pointers for both ID and raw-path forms. Existing string/array output
 * IDs must have the proper type. Array outputs are prepared before publication.
 * Status functions return SUCCESS, FAILURE, or UNSUPPORTED. exists returns
 * 1 for an existing directory, 0 for absent/non-directory, and -1 for query errors.
 * Recursive mutations are not transactional and may leave partial progress on
 * error. Tree copying rejects link/special entry types with UNSUPPORTED rather
 * than following targets. Root guards and no-follow traversal are not a sandbox
 * against arbitrary concurrent changes in untrusted ancestor directories.
 * Current-directory operations should delegate to the Path module's OS helpers.
 * Buffers count bytes including NUL. Insufficient capacity must not truncate text.
 * Traversal has a hard depth limit of 256. Windows directory guards deny deletion
 * while scanning; POSIX recursive deletion uses descriptor-relative no-follow I/O.
 */

#define CLM_DIRECTORY_ATTRIBUTE_READ_ONLY       (UINT64_C(1) << 0)
#define CLM_DIRECTORY_ATTRIBUTE_HIDDEN          (UINT64_C(1) << 1)
#define CLM_DIRECTORY_ATTRIBUTE_SYSTEM          (UINT64_C(1) << 2)
#define CLM_DIRECTORY_ATTRIBUTE_ARCHIVE         (UINT64_C(1) << 3)
#define CLM_DIRECTORY_ATTRIBUTE_TEMPORARY       (UINT64_C(1) << 4)
#define CLM_DIRECTORY_ATTRIBUTE_OFFLINE         (UINT64_C(1) << 5)
#define CLM_DIRECTORY_ATTRIBUTE_ENCRYPTED       (UINT64_C(1) << 6)
#define CLM_DIRECTORY_ATTRIBUTE_COMPRESSED      (UINT64_C(1) << 7)
#define CLM_DIRECTORY_ATTRIBUTE_REPARSE_POINT   (UINT64_C(1) << 8)
#define CLM_DIRECTORY_ATTRIBUTE_MOUNT_POINT     (UINT64_C(1) << 9)
#define CLM_DIRECTORY_ATTRIBUTE_JUNCTION        (UINT64_C(1) << 10)
#define CLM_DIRECTORY_ATTRIBUTE_SYMBOLIC_LINK   (UINT64_C(1) << 11)

#define CLM_DIRECTORY_TIME_CREATION     1u
#define CLM_DIRECTORY_TIME_MODIFICATION 2u
#define CLM_DIRECTORY_TIME_ACCESS       4u

#define CLM_DIRECTORY_ENTRY_FILE       1u
#define CLM_DIRECTORY_ENTRY_DIRECTORY  2u
#define CLM_DIRECTORY_ENTRY_LINK       4u
#define CLM_DIRECTORY_ENTRY_OTHER       8u

#define CLM_DIRECTORY_ENUM_RECURSIVE          1u
#define CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN     2u
#define CLM_DIRECTORY_ENUM_FOLLOW_SYMLINKS    4u
#define CLM_DIRECTORY_ENUM_STAY_ON_FILESYSTEM 8u

#define CLM_DIRECTORY_VISIT_CONTINUE 0
#define CLM_DIRECTORY_VISIT_STOP     1
#define CLM_DIRECTORY_VISIT_ERROR   -1

/** Metadata for the selected directory entry or followed target.
 * Attribute/time masks distinguish unsupported fields from false or zero values.
 * Timestamps use native time_t precision; creation time is not Unix change time.
 */
typedef struct {
    clm_path_info path;
    clm_ulong attributes;
    clm_ulong supported_attributes;
    time_t creation_time;
    time_t modification_time;
    time_t access_time;
    clm_uint supported_times;
} clm_directory_info;

/** Enumeration controls; NULL selects immediate children, all types, hidden included.
 * With explicit options, types=0 selects all types. A NULL pattern matches all
 * names; globs support case-sensitive '*' and '?' over UTF-8 code points. Recursive depth
 * starts at 1 for immediate children; max_depth=0 means unlimited recursion.
 * Link following requires cycle detection and must not silently cross policy limits.
 */
typedef struct {
    clm_uint flags;
    clm_uint types;
    clm_uint max_depth;
    const char* pattern;
} clm_directory_enumeration_options;

/** Borrowed UTF-8 text valid only during the enumeration callback.
 * path is the full child path, name is its final component; type is one entry bit.
 */
typedef struct {
    const char* path;
    const char* name;
    clm_uint type;
    clm_uint depth;
} clm_directory_entry;

/** Return VISIT_CONTINUE, VISIT_STOP, or VISIT_ERROR; retain no borrowed text.
 * context is the caller's opaque pointer and is never owned by the module.
 */
typedef clm_int (*clm_directory_visit_fn)(const clm_directory_entry* entry, void* context);

typedef struct {
    char* path;
    char* name;
    clm_uint type;
    bool hidden;
    clm_ulong reparse;
} clm_directory_internal_child;

typedef struct {
    clm_directory_internal_child* children;
    size_t count;
    clm_ulong device;
    clm_ulong identity;
#if defined(_WIN32)
    HANDLE guard;
#else
    DIR* guard;
#endif
} clm_directory_internal_snapshot;

char* clm_directory_internal_join(const char* base, const char* name);

void clm_directory_internal_snapshot_free(clm_directory_internal_snapshot* snapshot);

int clm_directory_internal_snapshot_add(clm_directory_internal_snapshot* snapshot, const char* base, const char* name);

int clm_directory_internal_snapshot_open(const char* path, bool follow, clm_directory_internal_snapshot* snapshot);

int clm_directory_internal_create(const char* path);

int clm_directory_internal_remove(const char* path);

int clm_directory_internal_guard_root(const char* path);

int clm_directory_internal_create_recursive(const char* path, unsigned depth);

char* clm_directory_internal_temporary_base(void);

int clm_directory_internal_buffer(char* buffer, size_t capacity, const char* text);

char* clm_directory_internal_packed(const char* path, clm_uint types, size_t* bytes, clm_ulong* count);

int clm_directory_internal_collect(const char* path, clm_uint types, clm_id destination,
    char* buffer, size_t capacity, size_t* required, clm_ulong* count, bool managed);

int clm_directory_internal_count(const char* path, clm_uint types, clm_ulong* count);

int clm_directory_internal_info(const char* path, bool follow, clm_directory_info* out);

int clm_directory_internal_attribute(const char* path, clm_ulong attribute, bool* out);

int clm_directory_internal_match(const char* pattern, const char* name);

typedef struct clm_directory_internal_ancestor {
    clm_ulong device;
    clm_ulong identity;
    const struct clm_directory_internal_ancestor* previous;
} clm_directory_internal_ancestor;

int clm_directory_internal_walk(const char* path, const clm_directory_enumeration_options* options,
    clm_directory_visit_fn visit, void* context, clm_uint depth, const clm_directory_internal_ancestor* ancestors);

int clm_directory_internal_enumerate(const char* path, const clm_directory_enumeration_options* options,
    clm_directory_visit_fn visit, void* context);

char* clm_directory_internal_resolved(const char* path, unsigned depth);

int clm_directory_internal_within(const char* target, const char* base);

int clm_directory_internal_copy_tree(const char* source, const char* destination,
                                                  bool overwrite, unsigned depth, clm_ulong device);

int clm_directory_internal_copy(const char* source, const char* destination, bool overwrite);

#if !defined(_WIN32)
#if ((!defined(_WIN32)))
int clm_directory_internal_remove_fd(int descriptor, clm_ulong device, unsigned depth);
#endif
#endif

int clm_directory_internal_remove_tree(const char* path, bool keep, unsigned depth, clm_ulong device);

int clm_directory_internal_total_visit(const clm_directory_entry* entry, void* context);

int clm_directory_internal_size(const char* path, clm_ulong* out);

int clm_directory_internal_empty(const char* path, bool* out);

int clm_directory_internal_temporary(const char* base, clm_id destination,
    char* buffer, size_t capacity, size_t* required, bool managed);

int clm_directory_internal_delete_empty(const char* path);

int clm_directory_internal_set_attributes(const char* path, clm_ulong attributes, clm_ulong mask);

int clm_directory_internal_set_times(const char* path, time_t creation, time_t modification, time_t access, clm_uint mask);

/** Create one directory without implicitly creating missing parents.
 * @param id ID of the target directory path string.
 * @return Operation status; existing entries are errors.
 */
clm_int clm_directory_create(clm_id id);

/** Create one directory from a raw UTF-8 path.
 * @param path NUL-terminated target path; missing parents are errors.
 * @return Operation status; not a recursive create.
 */
clm_int clm_directory_create_as(const char* path);

/** Test whether a path resolves to an existing directory, following links.
 * @param id ID of the path string; a regular file is not an existing directory.
 * @return 1 for a directory, 0 for absent/non-directory, -1 for errors.
 */
clm_int clm_directory_exists(clm_id id);

/** Test directory existence using a raw UTF-8 path, following links.
 * @param path NUL-terminated path; a dangling link does not resolve to a directory.
 * @return 1 for a directory, 0 for absent/non-directory, -1 for errors.
 */
clm_int clm_directory_exists_as(const char* path);

/** Move or rename a directory without replacing an existing destination.
 * @param id ID of the existing source directory path string.
 * @param new_id ID of the destination path string, not a new identity for the source ID.
 * @return Operation status; cross-filesystem moves fail instead of implicit copy/delete.
 */
clm_int clm_directory_move(clm_id id, clm_id new_id);

/** Move or rename a directory using raw UTF-8 path inputs.
 * @param path NUL-terminated source path.
 * @param new_path NUL-terminated destination path; not a writable output buffer.
 * @return Operation status; no overwrite or cross-filesystem fallback.
 */
clm_int clm_directory_move_as(const char* path, const char* new_path);

/** Delete an empty directory only; recursive removal requires the explicit API.
 * @param id ID of the directory path string; roots and link roots are rejected.
 * @return Operation status; a nonempty directory is an error.
 */
clm_int clm_directory_delete(clm_id id);

/** Delete an empty directory addressed by a raw UTF-8 path.
 * @param path NUL-terminated directory path; this never deletes descendants.
 * @return Operation status; roots and nonempty directories are errors.
 */
clm_int clm_directory_delete_as(const char* path);

/** Test whether an existing directory has no children, excluding dot entries only.
 * @param id ID of the directory path string.
 * @param[out] b Non-NULL Boolean result; hidden entries make a directory nonempty.
 * @return Operation status; missing/non-directory inputs fail, not true.
 */
clm_int clm_directory_is_empty(clm_id id, bool* b);

/** Query directory emptiness through a raw UTF-8 path.
 * @param path NUL-terminated existing directory path.
 * @param[out] b Non-NULL Boolean result, assigned only on success.
 * @return Operation status; excludes dot entries but not hidden children.
 */
clm_int clm_directory_is_empty_as(const char* path, bool* b);

/** Read the process-wide current directory through shared Path facilities.
 * @param[out] dest ID of an existing string destination for the UTF-8 path.
 * @return Operation status; output is unchanged on failure.
 */
clm_int clm_directory_get_current(clm_id dest);

/** Read the current directory into a caller-owned UTF-8 buffer.
 * @param[out] dest Writable non-NULL destination including space for NUL.
 * @param dest_size Available byte capacity, including the terminator.
 * @return Operation status; insufficient capacity leaves the buffer unchanged.
 */
clm_int clm_directory_get_current_to(char* dest, size_t dest_size);

/** Change the process-wide current directory using the shared Path implementation.
 * @param src ID of an existing directory path string; its text is not modified.
 * @return Operation status; affects relative-path resolution in other threads.
 */
clm_int clm_directory_set_current(clm_id src);

/** Change the process-wide current directory using raw UTF-8 input.
 * @param src NUL-terminated path of an existing accessible directory.
 * @return Operation status; changes shared process state, not per-thread state.
 */
clm_int clm_directory_set_current_as(const char* src);

/** List immediate real directory children, without traversing or classifying links as directories.
 * @param id ID of the parent directory path string.
 * @param[out] results ID of an existing string array receiving full UTF-8 paths.
 * @return Operation status; exclude dot entries, include hidden directories, no sorting.
 */
clm_int clm_directory_find_directories(clm_id id, clm_id results);

/** List immediate directory children using a raw UTF-8 parent path.
 * @param path NUL-terminated parent directory path.
 * @param[out] results ID of an existing string array, replaced only on success.
 * @return Operation status; same hidden and link rules as the ID form.
 */
clm_int clm_directory_find_directories_as(const char* path, clm_id results);

/** Export directory-child paths as NUL-separated strings with one extra final NUL.
 * @param path NUL-terminated parent directory path.
 * @param[out] buffer Writable UTF-8 result; NULL with zero capacity measures only.
 * @param buffer_size Capacity in bytes, including all terminators.
 * @param[out] required_size Mandatory required byte capacity, including on insufficient capacity.
 * @param[out] count Mandatory count of complete directory paths.
 * @return Operation status; no partial buffer writes or truncation.
 * @note Empty lists require one NUL byte; results can change between calls.
 */
clm_int clm_directory_find_directories_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size, clm_ulong* count);

/** List immediate regular-file children; link entries are not followed or included.
 * @param id ID of the parent directory path string.
 * @param[out] results ID of an existing output string array of full UTF-8 paths.
 * @return Operation status; hidden files included, native unsorted order.
 */
clm_int clm_directory_find_files(clm_id id, clm_id results);

/** List immediate regular files using a raw UTF-8 parent path.
 * @param path NUL-terminated parent directory path.
 * @param[out] results ID of an existing string array, replaced only on success.
 * @return Operation status; no recursion or implicit sorting.
 */
clm_int clm_directory_find_files_as(const char* path, clm_id results);

/** Export file-child paths as NUL-separated strings with one extra final NUL.
 * @param path NUL-terminated parent directory path.
 * @param[out] buffer Writable UTF-8 result; NULL with zero capacity measures only.
 * @param buffer_size Byte capacity including all terminators.
 * @param[out] required_size Mandatory required byte capacity, also reported when capacity is insufficient.
 * @param[out] count Mandatory number of complete regular-file paths.
 * @return Operation status; no truncated names or partial buffer output.
 */
clm_int clm_directory_find_files_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size, clm_ulong* count);

/** Read a directory's actual creation time when supported by the filesystem.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL native timestamp result; not a string or string array.
 * @return Operation status; unavailable birth time is UNSUPPORTED, never substituted with ctime.
 */
clm_int clm_directory_get_creation_time(clm_id id, time_t* out);

/** Read creation time from a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL timestamp output with native time_t precision.
 * @return Operation status; report unavailable creation time explicitly.
 */
clm_int clm_directory_get_creation_time_as(const char* path, time_t* out);

/** Read the directory entry-list modification timestamp, not its newest descendant time.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL native modification timestamp result.
 * @return Operation status; output remains unchanged on failure.
 */
clm_int clm_directory_get_modification_time(clm_id id, time_t* out);

/** Read a directory modification timestamp through a raw UTF-8 path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; no recursive timestamp aggregation.
 */
clm_int clm_directory_get_modification_time_as(const char* path, time_t* out);

/** Read the filesystem-reported directory access timestamp.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; mount policies may defer or disable access-time updates.
 */
clm_int clm_directory_get_access_time(clm_id id, time_t* out);

/** Read directory access time using a raw UTF-8 path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL native timestamp result; not proof of recent user activity.
 * @return Operation status; output remains unchanged on failure.
 */
clm_int clm_directory_get_access_time_as(const char* path, time_t* out);

/** Sum logical regular-file bytes recursively without following link targets.
 * @param id ID of the root directory path string.
 * @param[out] out Non-NULL 64-bit total, assigned only after a successful complete scan.
 * @return Operation status; overflow, mount-boundary and unreadable-entry policies need implementation.
 * @note Counts directory entries independently, including hard-linked files; not allocated disk usage.
 */
clm_int clm_directory_get_size(clm_id id, clm_ulong* out);

/** Compute recursive logical regular-file bytes from a raw UTF-8 directory path.
 * @param path NUL-terminated root directory path.
 * @param[out] out Non-NULL 64-bit total; links and directory metadata sizes are excluded.
 * @return Operation status; failed scans must not publish incomplete totals.
 */
clm_int clm_directory_get_size_as(const char* path, clm_ulong* out);

/** Obtain the configured temporary base directory without creating an entry.
 * @param[out] out ID of an existing string receiving its UTF-8 path.
 * @return Operation status; this does not create a private temporary directory.
 */
clm_int clm_directory_get_temporary(clm_id out);

/** Obtain the temporary base directory into a caller-owned UTF-8 buffer.
 * @param[out] out Writable non-NULL destination including room for NUL.
 * @param out_size Capacity in bytes, including the terminator.
 * @return Operation status; insufficient capacity must not truncate or write output.
 */
clm_int clm_directory_get_temporary_to(char* out, size_t out_size);

/** Inspect the platform read-only attribute, not effective write permissions.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unavailable attributes return UNSUPPORTED.
 */
clm_int clm_directory_is_read_only(clm_id id, bool* out);

/** Inspect a read-only attribute using a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result; does not determine ACL or access rights.
 * @return Operation status; unavailable platform semantics return UNSUPPORTED.
 */
clm_int clm_directory_is_read_only_as(const char* path, bool* out);

/** Query hidden-directory status under the native Path metadata policy.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean result; inspect the entry without following a link.
 * @return Operation status; native attributes or POSIX dot naming apply.
 */
clm_int clm_directory_is_hidden(clm_id id, bool* out);

/** Query native hidden status through a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result, assigned only on success.
 * @return Operation status; missing entries are not silently classified.
 */
clm_int clm_directory_is_hidden_as(const char* path, bool* out);

/** Inspect the native system attribute where available.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean result; not a classification of operating-system-owned paths.
 * @return Operation status; unsupported attributes are not false results.
 */
clm_int clm_directory_is_system(clm_id id, bool* out);

/** Inspect a system attribute using a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result.
 * @return Operation status; return UNSUPPORTED where unavailable.
 */
clm_int clm_directory_is_system_as(const char* path, bool* out);

/** Inspect the native archive attribute, not whether a directory is a compressed archive.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unsupported platform attributes are explicit.
 */
clm_int clm_directory_is_archive(clm_id id, bool* out);

/** Inspect an archive attribute using a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result; unrelated to ZIP or other file formats.
 * @return Operation status; return UNSUPPORTED where unavailable.
 */
clm_int clm_directory_is_archive_as(const char* path, bool* out);

/** Inspect the native temporary attribute, not containment in a temporary base directory.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; return UNSUPPORTED where unavailable.
 */
clm_int clm_directory_is_temporary(clm_id id, bool* out);

/** Inspect a temporary attribute using a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result; location alone is not the attribute.
 * @return Operation status; unsupported semantics must not be inferred.
 */
clm_int clm_directory_is_temporary_as(const char* path, bool* out);

/** Inspect the native offline attribute, not network reachability.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; report unsupported attributes explicitly.
 */
clm_int clm_directory_is_offline(clm_id id, bool* out);

/** Inspect an offline attribute through a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result; not a server availability test.
 * @return Operation status; report UNSUPPORTED where unavailable.
 */
clm_int clm_directory_is_offline_as(const char* path, bool* out);

/** Inspect native entry encryption metadata, not transport or full-volume security.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean result.
 * @return Operation status; unsupported metadata is not interpreted as unencrypted.
 */
clm_int clm_directory_is_encrypted(clm_id id, bool* out);

/** Inspect native encryption metadata using a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; report UNSUPPORTED rather than inventing encryption status.
 */
clm_int clm_directory_is_encrypted_as(const char* path, bool* out);

/** Inspect native filesystem compression metadata, not archive contents.
 * @param id Full-width ID of the directory path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unavailable platform metadata returns UNSUPPORTED.
 */
clm_int clm_directory_is_compressed(clm_id id, bool* out);

/** Inspect native compression metadata through a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result; not a recursive compressed-size calculation.
 * @return Operation status; unsupported metadata is explicit.
 */
clm_int clm_directory_is_compressed_as(const char* path, bool* out);

/** Inspect Windows reparse-point identity without following the target.
 * @param id ID of the directory or directory-link path string.
 * @param[out] out Non-NULL Boolean result; reparse points include more than symbolic links.
 * @return Operation status; platforms lacking this concept return UNSUPPORTED.
 */
clm_int clm_directory_is_reparse_point(clm_id id, bool* out);

/** Inspect reparse-point identity through a raw UTF-8 path.
 * @param path NUL-terminated directory or directory-link path.
 * @param[out] out Non-NULL Boolean result, inspecting the entry itself.
 * @return Operation status; unsupported platform concepts remain distinct from false.
 */
clm_int clm_directory_is_reparse_point_as(const char* path, bool* out);

/** Inspect native mount-point identity under a platform policy still to be finalized.
 * @param id ID of the directory path string.
 * @param[out] out Non-NULL Boolean result; mount points are not synonyms for symlinks.
 * @return Operation status; unsupported mount classifications return UNSUPPORTED.
 */
clm_int clm_directory_is_mount_point(clm_id id, bool* out);

/** Inspect mount-point identity using a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param[out] out Non-NULL Boolean result.
 * @return Operation status; document root and bind-mount handling before implementation.
 */
clm_int clm_directory_is_mount_point_as(const char* path, bool* out);

/** Inspect a Windows directory junction entry without following its target.
 * @param id ID of the directory-link path string.
 * @param[out] out Non-NULL Boolean result; not every reparse point is a junction.
 * @return Operation status; other platforms return UNSUPPORTED.
 */
clm_int clm_directory_is_junction(clm_id id, bool* out);

/** Inspect junction identity using a raw UTF-8 path.
 * @param path NUL-terminated path of the entry to inspect.
 * @param[out] out Non-NULL Boolean result, independent of target accessibility.
 * @return Operation status; unsupported junction semantics are explicit.
 */
clm_int clm_directory_is_junction_as(const char* path, bool* out);

/** Inspect symbolic-link identity without requiring its target to exist.
 * @param id ID of the directory-link path string.
 * @param[out] out Non-NULL Boolean result for the link entry itself.
 * @return Operation status; reuse Path link classification where applicable.
 */
clm_int clm_directory_is_symbolic_link(clm_id id, bool* out);

/** Inspect a symbolic-link entry using a raw UTF-8 path.
 * @param path NUL-terminated entry path; dangling links remain link entries.
 * @param[out] out Non-NULL Boolean result, assigned only on success.
 * @return Operation status; not a followed-target directory existence test.
 */
clm_int clm_directory_is_symbolic_link_as(const char* path, bool* out);

/** Count immediate real directory children, excluding dot entries and links.
 * @param id ID of the parent directory path string.
 * @param[out] out Non-NULL 64-bit count, including hidden directories.
 * @return Operation status; use the same classification as find_directories.
 */
clm_int clm_directory_get_directories_count(clm_id id, clm_ulong* out);

/** Count immediate directory children using a raw UTF-8 path.
 * @param path NUL-terminated parent directory path.
 * @param[out] out Non-NULL 64-bit result, unchanged on scan failure or overflow.
 * @return Operation status; no recursion or hidden-entry exclusion.
 */
clm_int clm_directory_get_directories_count_as(const char* path, clm_ulong* out);

/** Count immediate regular files, excluding link entries and including hidden files.
 * @param id ID of the parent directory path string.
 * @param[out] out Non-NULL 64-bit count, assigned only after a complete successful scan.
 * @return Operation status; use the same classification as find_files.
 */
clm_int clm_directory_get_files_count(clm_id id, clm_ulong* out);

/** Count immediate regular files using a raw UTF-8 directory path.
 * @param path NUL-terminated parent directory path.
 * @param[out] out Non-NULL 64-bit count; overflow and query failures are errors.
 * @return Operation status; no recursive scan.
 */
clm_int clm_directory_get_files_count_as(const char* path, clm_ulong* out);

/** Create a directory and missing parents; an existing directory is success.
 * @param id ID of the target directory path string.
 * @return Operation status; an existing non-directory is an error.
 */
clm_int clm_directory_create_recursive(clm_id id);

/** Create missing directory parents from a raw UTF-8 path.
 * @param path NUL-terminated target path; the caller retains ownership.
 * @return Operation status; creation can leave successfully created parents on error.
 */
clm_int clm_directory_create_recursive_as(const char* path);

/** Copy a directory tree without traversing symbolic-link targets.
 * @param id ID of the source directory path string.
 * @param destination ID of the destination directory path string, not an output string.
 * @param overwrite Whether existing regular-file destinations may be replaced.
 * @return Operation status; reject copying into the source subtree.
 * @note Recreate links when supported; unsupported link types fail explicitly.
 */
clm_int clm_directory_copy(clm_id id, clm_id destination, bool overwrite);

/** Copy a directory tree using raw UTF-8 source and destination paths.
 * @param path NUL-terminated source directory path.
 * @param destination NUL-terminated destination path; not a writable output buffer.
 * @param overwrite Whether existing regular files may be replaced; type conflicts fail.
 * @return Operation status; filesystem mutations are not atomic.
 */
clm_int clm_directory_copy_as(const char* path, const char* destination, bool overwrite);

/** Delete a directory tree explicitly; never traverse link or junction targets.
 * @param id ID of the directory path to delete, including its root entry.
 * @return Operation status; reject filesystem roots and report partial errors.
 */
clm_int clm_directory_delete_recursive(clm_id id);

/** Delete a tree addressed by a raw UTF-8 path.
 * @param path NUL-terminated directory path; filesystem roots must be rejected.
 * @return Operation status; unsupported safe traversal must fail explicitly.
 */
clm_int clm_directory_delete_recursive_as(const char* path);

/** Remove all children recursively while retaining the directory itself.
 * @param id ID of the directory path string; roots and link roots are rejected.
 * @return Operation status; do not follow descendant links.
 */
clm_int clm_directory_clear(clm_id id);

/** Clear a directory using a raw UTF-8 path, retaining its root entry.
 * @param path NUL-terminated directory path; not a file or symbolic-link root.
 * @return Operation status; partial filesystem progress must be reported.
 */
clm_int clm_directory_clear_as(const char* path);

/** List immediate child entries of every type into an existing string array.
 * @param id ID of the parent directory path string.
 * @param results ID of an existing CLM string array, replaced only on success.
 * @return Operation status; exclude dot entries and include hidden entries.
 * @note Return full paths in native enumeration order, which is not guaranteed stable.
 */
clm_int clm_directory_find_entries(clm_id id, clm_id results);

/** List immediate child entries using a raw UTF-8 parent path.
 * @param path NUL-terminated parent directory path.
 * @param results ID of an existing output string array, replaced only on success.
 * @return Operation status; no recursion or implicit sorting.
 */
clm_int clm_directory_find_entries_as(const char* path, clm_id results);

/** Export child paths as NUL-separated UTF-8 strings with one extra final NUL.
 * @param path NUL-terminated parent directory path.
 * @param[out] buffer Writable result buffer; NULL with zero capacity queries size only.
 * @param buffer_size Capacity in bytes, including every terminator.
 * @param[out] required_size Required byte capacity; mandatory, including on insufficient capacity.
 * @param[out] count Number of complete child paths; mandatory.
 * @return Operation status; never truncate or partially write the buffer.
 * @note An empty list requires one NUL byte. Size-query results may become stale.
 */
clm_int clm_directory_find_entries_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size, clm_ulong* count);

/** Enumerate directory entries incrementally through a caller callback.
 * @param id ID of the root directory path string; the root itself is not visited.
 * @param options Optional recursion, depth, type, hidden-entry, and name-filter controls.
 * @param visit Callback receiving borrowed entry data; must not be NULL.
 * @param context Opaque caller-owned pointer passed to each callback.
 * @return SUCCESS, STOPPED for requested cancellation, or an error.
 * @note Errors after callbacks cannot undo previously delivered entries or callback effects.
 */
clm_int clm_directory_enumerate(clm_id id, const clm_directory_enumeration_options* options, clm_directory_visit_fn visit, void* context);

/** Enumerate entries from a raw UTF-8 root path.
 * @param path NUL-terminated root directory path, excluded from callbacks.
 * @param options Optional traversal controls; NULL selects immediate children of all types.
 * @param visit Non-NULL callback; borrowed strings are valid only during each invocation.
 * @param context Caller-owned callback state; may be NULL.
 * @return SUCCESS, STOPPED, or an error, without promising a filesystem snapshot.
 */
clm_int clm_directory_enumerate_as(const char* path, const clm_directory_enumeration_options* options, clm_directory_visit_fn visit, void* context);

/** Query directory metadata and capability masks without changing the path string.
 * @param id ID of the directory path string.
 * @param follow_symlinks Whether metadata describes the target rather than the entry.
 * @param[out] out Non-NULL caller-owned metadata record; unchanged on failure.
 * @return Operation status; unsupported optional fields have clear support-mask bits.
 */
clm_int clm_directory_get_info(clm_id id, bool follow_symlinks, clm_directory_info* out);

/** Query directory metadata through a raw UTF-8 path.
 * @param path NUL-terminated directory or directory-link path.
 * @param follow_symlinks Whether to inspect the target instead of the link entry.
 * @param[out] out Non-NULL result record; its support masks describe available fields.
 * @return Operation status; missing-entry handling must be specified before implementation.
 */
clm_int clm_directory_get_info_as(const char* path, bool follow_symlinks, clm_directory_info* out);

/** Set selected directory attributes without replacing unrelated attributes.
 * @param id ID of the target directory path string.
 * @param attributes Desired attribute bits; only mask-selected bits are changed.
 * @param mask Attribute bits to update; classification-only bits cannot be set.
 * @return Operation status; unsupported or immutable requested bits fail explicitly.
 * @note Read-only attributes are not effective access permissions; no ACL changes are implied.
 */
clm_int clm_directory_set_attributes(clm_id id, clm_ulong attributes, clm_ulong mask);

/** Set selected attributes through a raw UTF-8 directory path.
 * @param path NUL-terminated directory path.
 * @param attributes Desired attribute values for selected bits.
 * @param mask Bits to update; never interpret unsupported bits as successfully cleared.
 * @return Operation status; native partial-update guarantees require documentation.
 */
clm_int clm_directory_set_attributes_as(const char* path, clm_ulong attributes, clm_ulong mask);

/** Set selected timestamps; never substitute Unix metadata-change time for creation.
 * @param id ID of the target directory path string.
 * @param creation_time Desired creation timestamp, used only when selected by mask.
 * @param modification_time Desired modification timestamp, used only when selected.
 * @param access_time Desired access timestamp, used only when selected.
 * @param mask CLM_DIRECTORY_TIME_* fields to update; unselected fields remain unchanged.
 * @return Operation status; unavailable creation-time changes return UNSUPPORTED.
 */
clm_int clm_directory_set_times(clm_id id, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask);

/** Set selected timestamps through a raw UTF-8 directory path.
 * @param path NUL-terminated target directory path.
 * @param creation_time Desired creation timestamp when selected.
 * @param modification_time Desired modification timestamp when selected.
 * @param access_time Desired access timestamp when selected.
 * @param mask CLM_DIRECTORY_TIME_* selection bits; zero requests no changes.
 * @return Operation status; unsupported requested fields must be reported.
 */
clm_int clm_directory_set_times_as(const char* path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask);

/** Count all immediate child entries without filtering hidden names.
 * @param id ID of the parent directory path string.
 * @param[out] out Non-NULL 64-bit entry count; dot entries are excluded.
 * @return Operation status; overflow is an error and output remains unchanged.
 */
clm_int clm_directory_get_entries_count(clm_id id, clm_ulong* out);

/** Count all immediate child entries from a raw UTF-8 path.
 * @param path NUL-terminated parent directory path.
 * @param[out] out Non-NULL 64-bit result, assigned only on success.
 * @return Operation status; no recursion or implicit hidden-entry exclusions.
 */
clm_int clm_directory_get_entries_count_as(const char* path, clm_ulong* out);

/** Atomically create a unique temporary child directory inside an existing base.
 * @param base ID of the parent directory path; no implicit base creation.
 * @param[out] out ID of an existing string destination for the created path.
 * @return Operation status; the caller owns filesystem cleanup after success.
 * @note Do not generate a name then create it non-exclusively; clean up if publishing fails.
 */
clm_int clm_directory_create_temporary(clm_id base, clm_id out);

/** Create a unique temporary directory inside a raw UTF-8 base path.
 * @param path NUL-terminated existing parent directory path.
 * @param[out] out ID of an existing string receiving the new directory path.
 * @return Operation status; remove the new directory if output publication fails.
 */
clm_int clm_directory_create_temporary_as(const char* path, clm_id out);

/** Create a temporary directory and export its path without truncation.
 * @param path NUL-terminated existing parent directory path.
 * @param[out] buffer Caller-owned writable UTF-8 destination including space for NUL.
 * @param buffer_size Available byte capacity; insufficient capacity must not create a directory.
 * @param[out] required_size Required capacity; mandatory. NULL buffer with zero capacity only measures.
 * @return Operation status; size-query mode has no filesystem creation side effect.
 */
clm_int clm_directory_create_temporary_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size);
