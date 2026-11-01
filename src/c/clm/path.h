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
#include "bpa.h"
#include "string.h"
#include "platform.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/** @file
 * Filesystem-path API using UTF-8 strings referenced by CLM IDs.
 * Output and in-place parameters refer to string contents, not the ID value.
 * Lexical operations do not query the filesystem unless explicitly documented.
 * Operations return 0 on success and 1 on error; predicates return 0/1 or -1
 * on error. Output IDs must name existing string blocks. Aliasing is supported;
 * failed operations leave their outputs unchanged. Lexical grammar is native,
 * while explicit UNC helpers recognize Windows UNC syntax on every platform.
 */

#define CLM_PATH_OPERATION_SUCCESS 0
#define CLM_PATH_OPERATION_FAILURE 1

/** Metadata result; link following is selected by clm_path_get_info(). */
typedef struct {
    clm_int exists;
    clm_int is_directory;
    clm_int is_file;
    clm_int is_symlink;
    clm_int is_hidden;
} clm_path_info;

const char* clm_path_internal_text(clm_id id);

char* clm_path_internal_copy(const char* text);

int clm_path_internal_store(clm_id destination, const char* text);

int clm_path_internal_slice(clm_id destination, const char* text, size_t length);

int clm_path_internal_separator(char character);

char clm_path_internal_native_separator(void);

int clm_path_internal_unc(const char* text, size_t* server, size_t* share, size_t* end);

size_t clm_path_internal_root(const char* text);

int clm_path_internal_qualified(const char* text);

int clm_path_internal_equal(const char* left, const char* right, size_t length);

char* clm_path_internal_normalize(const char* text);

size_t clm_path_internal_filename(const char* text, size_t* end);

size_t clm_path_internal_extension(const char* text, size_t* end);

#if defined(_WIN32)
#if ((defined(_WIN32)))
wchar_t* clm_path_internal_wide(const char* text);
#endif

#if ((defined(_WIN32)))
char* clm_path_internal_utf8(const wchar_t* text);
#endif
#endif

char* clm_path_internal_current(void);

int clm_path_internal_valid(const char* text);

/** Normalize a path in place, rejecting results invalid under native filename rules.
 * @param[in,out] path ID of the path string to sanitize.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @warning Sanitization alone must not be treated as traversal protection.
 */
clm_int clm_path_sanitize(clm_id path);

/** Test whether a path is absolute under the selected platform's path rules.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_absolute(clm_id path);

/** Join two paths, replacing the first path's contents with the result.
 * @param[in,out] path1 ID of the base path and result string.
 * @param path2 ID of the path to append.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note A rooted second argument replaces the first. Input IDs may alias.
 */
clm_int clm_path_join(clm_id path1, clm_id path2);

/** Query filesystem metadata with explicit symbolic-link following.
 * @param path ID of the filesystem path string.
 * @param follow_symlinks Nonzero to query the target; zero to inspect the link itself.
 * @param[out] info Caller-owned metadata result; must not be NULL.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_info(clm_id path, clm_int follow_symlinks, clm_path_info* info);

/** Query whether a filesystem entry exists at a path.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 * @note Follows symbolic links; dangling targets return false, while query errors return -1.
 */
clm_int clm_path_exists(clm_id path);

/** Query whether a path identifies a directory.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_directory(clm_id path);

/** Query whether a path identifies a regular file.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_file(clm_id path);

/** Inspect the filesystem entry itself for symbolic-link identity.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_symlink(clm_id path);

/** Make a path absolute against the current directory without requiring existence.
 * @param[in,out] path ID of the input path and resulting absolute path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_full_path(clm_id path);

/** Compute a lexical relative path from a base to a target.
 * @param[in,out] path ID of the target path, replaced by the relative result.
 * @param base ID of the base directory path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note Inputs must be fully qualified and have matching normalized roots.
 */
clm_int clm_path_get_relative_path(clm_id path, clm_id base);

/** Extract a filesystem root, including complete UNC server/share roots.
 * @param[in,out] path ID of the path string, replaced by its root.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_root(clm_id path);

/** Normalize path components lexically without filesystem access.
 * @param[in,out] path ID of the path string to normalize.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note Preserve roots and extended-path semantics; do not resolve symbolic links.
 */
clm_int clm_path_normalize(clm_id path);

/** Resolve an existing filesystem path, including symbolic links.
 * @param[in,out] path ID of the input path and canonical result string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_canonicalize(clm_id path);

/** Compare lexical path components for containment under a base directory.
 * @param path ID of the candidate path string.
 * @param base ID of the base directory path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 * @warning Does not resolve links or enforce a filesystem security boundary.
 */
clm_int clm_path_is_within(clm_id path, clm_id base);

/** Test whether a path string contains no characters.
 * @param path ID of the path string, not a directory whose contents are inspected.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_empty(clm_id path);

/** Query hidden-path status under platform-specific rules.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_hidden(clm_id path);

/** Extract the final filename component of a path.
 * @param[in,out] path ID of the path string, replaced by its filename.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_filename(clm_id path);

/** Extract the filename component without its final extension.
 * @param[in,out] path ID of the path string, replaced by the filename stem.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_filename_without_extension(clm_id path);

/** Extract an extension from the final filename component only.
 * @param[in,out] path ID of the path string, replaced by its extension.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_extension(clm_id path);

/** Replace or add the extension of a path's filename component.
 * @param[in,out] path ID of the path string to update.
 * @param new_extension ID of the new extension string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_change_extension(clm_id path, clm_id new_extension);

/** Test for a trailing separator recognized by the platform path grammar.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_ends_with_separator(clm_id path);

/** Append a native separator when the path does not already end in one.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_ensure_ending_separator(clm_id path);

/** Remove every leading native separator, explicitly discarding root separators.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @warning Removing root separators can change an absolute path into a relative path.
 */
clm_int clm_path_trim_starting_separator(clm_id path);

/** Trim trailing separators without destroying filesystem roots.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_trim_ending_separator(clm_id path);

/** Test whether the final filename component has an extension.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_has_extension(clm_id path);

/** Remove the final filename extension while retaining its directory path.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_remove_extension(clm_id path);

/** Extract the lexical parent directory without filesystem access.
 * @param[in,out] path ID of the path string, replaced by its parent directory.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_parent(clm_id path);

/** Obtain platform-specific characters disallowed by the chosen path policy.
 * @param[out] destination ID of the result string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note This list alone cannot express reserved names or all component restrictions.
 */
clm_int clm_path_get_invalid_characters(clm_id destination);

/** Test for a platform-recognized root or drive prefix.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_rooted(clm_id path);

/** Test whether path resolution is independent of current-directory context.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_fully_qualified(clm_id path);

/** Recognize a complete ordinary or extended Windows UNC path.
 * @param path ID of the path string containing both a server and share name.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_unc(clm_id path);

/** Recognize a complete UNC path in the Windows extended-length namespace.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_extended_unc(clm_id path);

/** Extract the server component from a complete ordinary or extended UNC path.
 * @param path ID of the source UNC path string; its contents are not modified.
 * @param[out] destination ID of the result string, without separator characters.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_unc_server(clm_id path, clm_id destination);

/** Extract the share component from a complete ordinary or extended UNC path.
 * @param path ID of the source UNC path string; its contents are not modified.
 * @param[out] destination ID of the result string, without separator characters.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_unc_share(clm_id path, clm_id destination);

/** Construct an ordinary UNC share root from validated server and share names.
 * @param server ID of the server-name component string.
 * @param share ID of the share-name component string.
 * @param[out] destination ID of the result path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_build_unc(clm_id server, clm_id share, clm_id destination);

/** Convert an ordinary UNC path to extended syntax without changing its meaning.
 * @param[in,out] path ID of the UNC path string to convert.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_to_extended_unc(clm_id path);

/** Convert an extended UNC path to ordinary syntax when representable.
 * @param[in,out] path ID of the UNC path string to convert.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_from_extended_unc(clm_id path);

/** Test whether a path is relative under the platform's path grammar.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_relative(clm_id path);

/** Validate path syntax under platform-specific filename and root rules.
 * @param path ID of the path string to validate.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_path_is_valid(clm_id path);

/** Replace all non-overlapping literal text matches without component interpretation.
 * @param[in,out] path ID of the path string to update.
 * @param old_value ID of the text to find.
 * @param new_value ID of the replacement text.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_replace(clm_id path, clm_id old_value, clm_id new_value);

/** Obtain the native filesystem path separator as a string.
 * @param[out] destination ID of the result string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_separator(clm_id destination);

/** Read the process's current working directory.
 * @param[out] destination ID of the result path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
clm_int clm_path_get_current_directory(clm_id destination);

/** Change the process-wide current working directory.
 * @param path ID of an existing directory path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @warning This changes shared process state and affects relative-path resolution.
 */
clm_int clm_path_set_current_directory(clm_id path);
