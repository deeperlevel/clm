#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
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

static inline clm_int clm_path_get_info(clm_id path, clm_int follow_symlinks, clm_path_info* info);

static inline const char* clm_path_internal_text(clm_id id)
{
    clm_bpa_block_node* block;
    const char* text;
    size_t bytes, characters;
    return clm_string_get_block(id, &block, &text, &bytes, &characters) == CLM_BPA_SUCCESS ? text : NULL;
}

static inline char* clm_path_internal_copy(const char* text)
{
    if (!text) return NULL;
    const size_t size = strlen(text) + 1;
    char* copy = (char*)malloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}

static inline int clm_path_internal_store(clm_id destination, const char* text)
{
    if (!text || !clm_path_internal_text(destination)) return 1;
    const size_t length = strlen(text);
    size_t characters;
    if (length >= INT_MAX || clm_string_measure(text, length, CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS) return 1;
    if (clm_bpa_set_string(destination, text, (clm_uint)length + 1) != (int)length) return 1;
    clm_bpa_find_block(destination)->encoding = CLM_STRING_ENCODING_UTF8;
    return 0;
}

static inline int clm_path_internal_slice(clm_id destination, const char* text, size_t length)
{
    char* copy = (char*)malloc(length + 1);
    if (!copy) return 1;
    memcpy(copy, text, length); copy[length] = 0;
    const int status = clm_path_internal_store(destination, copy);
    free(copy);
    return status;
}

static inline int clm_path_internal_separator(char character)
{
#if defined(_WIN32)
    return character == '/' || character == '\\';
#else
    return character == '/';
#endif
}

static inline char clm_path_internal_native_separator(void)
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

static inline int clm_path_internal_unc(const char* text, size_t* server, size_t* share, size_t* end)
{
    if (!text || strncmp(text, "\\\\", 2)) return 0;
    size_t start = 2;
    if (!strncmp(text, "\\\\?\\UNC\\", 8)) start = 8;
    else if (text[2] == '?' || text[2] == '.') return 0;
    size_t cursor = start;
    while (text[cursor] && text[cursor] != '\\') ++cursor;
    if (cursor == start || !text[cursor]) return 0;
    const size_t share_start = ++cursor;
    while (text[cursor] && text[cursor] != '\\') ++cursor;
    if (cursor == share_start) return 0;
    for (size_t index = start; index < cursor; ++index)
        if (strchr("/:*?\"<>|", text[index]) || (unsigned char)text[index] < 32) return 0;
    if ((cursor - share_start == 1 && text[share_start] == '.') ||
        (cursor - share_start == 2 && !strncmp(text + share_start, "..", 2))) return 0;
    if (server) *server = start;
    if (share) *share = share_start;
    if (end) *end = cursor;
    return 1;
}

static inline size_t clm_path_internal_root(const char* text)
{
#if defined(_WIN32)
    size_t end;
    if (clm_path_internal_unc(text, NULL, NULL, &end)) return end + (text[end] == '\\');
    if (!strncmp(text, "\\\\?\\", 4) && strlen(text) >= 7 && isalpha((unsigned char)text[4]) && text[5] == ':' && text[6] == '\\') return 7;
    if (isalpha((unsigned char)text[0]) && text[1] == ':') return clm_path_internal_separator(text[2]) ? 3 : 2;
#endif
    return clm_path_internal_separator(text[0]) ? 1 : 0;
}

static inline int clm_path_internal_qualified(const char* text)
{
#if defined(_WIN32)
    return clm_path_internal_unc(text, NULL, NULL, NULL) ||
        (strlen(text) >= 3 && isalpha((unsigned char)text[0]) && text[1] == ':' && clm_path_internal_separator(text[2])) ||
        (strlen(text) >= 7 && !strncmp(text, "\\\\?\\", 4) && isalpha((unsigned char)text[4]) && text[5] == ':' && text[6] == '\\');
#else
    return text[0] == '/';
#endif
}

static inline int clm_path_internal_equal(const char* left, const char* right, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        unsigned char first = (unsigned char)left[index], second = (unsigned char)right[index];
#if defined(_WIN32)
        first = (unsigned char)tolower(first); second = (unsigned char)tolower(second);
#endif
        if (first != second) return 0;
    }
    return 1;
}

static inline char* clm_path_internal_normalize(const char* text)
{
    if (!text) return NULL;
#if defined(_WIN32)
    if (!strncmp(text, "\\\\?\\", 4)) return clm_path_internal_copy(text);
    if (!strncmp(text, "\\\\", 2) && !clm_path_internal_unc(text, NULL, NULL, NULL)) return NULL;
#endif
    const size_t length = strlen(text), root = clm_path_internal_root(text);
    char* output = (char*)malloc(length + 3);
    size_t* starts = (size_t*)calloc(length + 1, sizeof(size_t));
    if (!output || !starts) { free(output); free(starts); return NULL; }
    size_t used = 0, depth = 0, cursor = root;
    const char separator = clm_path_internal_native_separator();
    for (; used < root; ++used) output[used] = clm_path_internal_separator(text[used]) ? separator : text[used];
    const int anchored = root && clm_path_internal_separator(text[root - 1]);
    while (cursor < length) {
        while (clm_path_internal_separator(text[cursor])) ++cursor;
        const size_t first = cursor;
        while (text[cursor] && !clm_path_internal_separator(text[cursor])) ++cursor;
        const size_t count = cursor - first;
        if (!count || (count == 1 && text[first] == '.')) continue;
        if (count == 2 && !strncmp(text + first, "..", 2)) {
            if (depth && !(used - starts[depth - 1] == 2 && !strncmp(output + starts[depth - 1], "..", 2))) {
                used = starts[--depth];
                if (used > root && output[used - 1] == separator) --used;
                continue;
            }
            if (anchored) continue;
        }
        if (used && output[used - 1] != separator && !(used == 2 && output[1] == ':')) output[used++] = separator;
        starts[depth++] = used;
        memcpy(output + used, text + first, count); used += count;
    }
    if (!used && length) output[used++] = '.';
    output[used] = 0;
    free(starts);
    return output;
}

static inline size_t clm_path_internal_filename(const char* text, size_t* end)
{
    const size_t root = clm_path_internal_root(text);
    size_t last = strlen(text);
    while (last > root && clm_path_internal_separator(text[last - 1])) --last;
    size_t first = last;
    while (first > root && !clm_path_internal_separator(text[first - 1])) --first;
    *end = last;
    return first;
}

static inline size_t clm_path_internal_extension(const char* text, size_t* end)
{
    const size_t first = clm_path_internal_filename(text, end);
    for (size_t cursor = *end; cursor > first + 1; --cursor) if (text[cursor - 1] == '.') return cursor - 1;
    return *end;
}

#if defined(_WIN32)
static inline wchar_t* clm_path_internal_wide(const char* text)
{
    if (!text) return NULL;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!count) return NULL;
    wchar_t* result = (wchar_t*)malloc((size_t)count * sizeof(wchar_t));
    if (result && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, count)) { free(result); return NULL; }
    return result;
}

static inline char* clm_path_internal_utf8(const wchar_t* text)
{
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
    if (!count) return NULL;
    char* result = (char*)malloc((size_t)count);
    if (result && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result, count, NULL, NULL)) { free(result); return NULL; }
    return result;
}
#endif

static inline char* clm_path_internal_current(void)
{
#if defined(_WIN32)
    const DWORD count = GetCurrentDirectoryW(0, NULL);
    wchar_t* wide = count ? (wchar_t*)malloc((size_t)count * sizeof(wchar_t)) : NULL;
    if (!wide) return NULL;
    const DWORD written = GetCurrentDirectoryW(count, wide);
    char* result = written && written < count ? clm_path_internal_utf8(wide) : NULL;
    free(wide);
    return result;
#else
    for (size_t size = 256; size <= (size_t)INT_MAX; size *= 2) {
        char* result = (char*)malloc(size);
        if (!result) return NULL;
        if (getcwd(result, size)) return result;
        const int error = errno; free(result);
        if (error != ERANGE) return NULL;
    }
    return NULL;
#endif
}

static inline int clm_path_internal_valid(const char* text)
{
    if (!text || !*text) return 0;
#if defined(_WIN32)
    const int extended = !strncmp(text, "\\\\?\\", 4);
    const size_t root = clm_path_internal_root(text);
    if (!strncmp(text, "\\\\", 2) && !clm_path_internal_unc(text, NULL, NULL, NULL) && !extended) return 0;
    if (extended && root == 1) return 0;
    for (size_t cursor = root; text[cursor]; ) {
        while (clm_path_internal_separator(text[cursor])) ++cursor;
        const size_t first = cursor;
        while (text[cursor] && !clm_path_internal_separator(text[cursor])) {
            if ((unsigned char)text[cursor] < 32 || strchr("<>:\"|?*", text[cursor])) return 0;
            ++cursor;
        }
        const size_t count = cursor - first;
        if (!count || (count == 1 && text[first] == '.') || (count == 2 && !strncmp(text + first, "..", 2))) continue;
        if (!extended && (text[cursor - 1] == '.' || text[cursor - 1] == ' ')) return 0;
        if (!extended) {
            char stem[5] = {0};
            size_t index = 0;
            while (index < count && index < 4 && text[first + index] != '.') {
                stem[index] = (char)toupper((unsigned char)text[first + index]); ++index;
            }
            if ((index == 3 && (!strcmp(stem, "CON") || !strcmp(stem, "PRN") || !strcmp(stem, "AUX") || !strcmp(stem, "NUL"))) ||
                (index == 4 && (!strncmp(stem, "COM", 3) || !strncmp(stem, "LPT", 3)) && stem[3] >= '1' && stem[3] <= '9')) return 0;
        }
    }
#endif
    return 1;
}

/** Normalize a path in place, rejecting results invalid under native filename rules.
 * @param[in,out] path ID of the path string to sanitize.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @warning Sanitization alone must not be treated as traversal protection.
 */
static inline clm_int clm_path_sanitize(clm_id path) {
    char* output = clm_path_internal_normalize(clm_path_internal_text(path));
    const int status = output && clm_path_internal_valid(output) ? clm_path_internal_store(path, output) : 1;
    free(output);
    return status;
}

/** Test whether a path is absolute under the selected platform's path rules.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_absolute(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_qualified(text) : -1;
}

/** Join two paths, replacing the first path's contents with the result.
 * @param[in,out] path1 ID of the base path and result string.
 * @param path2 ID of the path to append.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note A rooted second argument replaces the first. Input IDs may alias.
 */
static inline clm_int clm_path_join(clm_id path1, clm_id path2) {
    const char* left = clm_path_internal_text(path1);
    const char* right = clm_path_internal_text(path2);
    if (!left || !right) return 1;
    if (clm_path_internal_root(right)) return clm_path_internal_store(path1, right);
    const size_t first = strlen(left), second = strlen(right);
    if (first > (size_t)INT_MAX - 2 || second > (size_t)INT_MAX - first - 2) return 1;
    char* output = (char*)malloc(first + second + 2);
    if (!output) return 1;
    memcpy(output, left, first);
    size_t used = first;
    if (first && second && !clm_path_internal_separator(left[first - 1]) && !(first == 2 && left[1] == ':'))
        output[used++] = clm_path_internal_native_separator();
    memcpy(output + used, right, second + 1);
    const int status = clm_path_internal_store(path1, output);
    free(output);
    return status;
}

/** Query whether a filesystem entry exists at a path.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 * @note Follows symbolic links; dangling targets return false, while query errors return -1.
 */
static inline clm_int clm_path_exists(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 1, &info) == 0 ? info.exists : -1;
}

/** Query whether a path identifies a directory.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_directory(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 1, &info) == 0 ? info.is_directory : -1;
}

/** Query whether a path identifies a regular file.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_file(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 1, &info) == 0 ? info.is_file : -1;
}

/** Inspect the filesystem entry itself for symbolic-link identity.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_symlink(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 0, &info) == 0 ? info.is_symlink : -1;
}

/** Query filesystem metadata with explicit symbolic-link following.
 * @param path ID of the filesystem path string.
 * @param follow_symlinks Nonzero to query the target; zero to inspect the link itself.
 * @param[out] info Caller-owned metadata result; must not be NULL.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_info(clm_id path, clm_int follow_symlinks, clm_path_info* info) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text || !info) return 1;
    clm_path_info result = {0};
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow_symlinks ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);
    HANDLE handle = CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, flags, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return 1;
        *info = result; return 0;
    }
    BY_HANDLE_FILE_INFORMATION native;
    if (!GetFileInformationByHandle(handle, &native)) { CloseHandle(handle); return 1; }
    result.exists = 1;
    result.is_directory = (native.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    result.is_file = !result.is_directory;
    result.is_hidden = (native.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    if (!follow_symlinks && (native.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        FILE_ATTRIBUTE_TAG_INFO tag;
        if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag))) { CloseHandle(handle); return 1; }
        result.is_symlink = tag.ReparseTag == IO_REPARSE_TAG_SYMLINK;
        if (result.is_symlink) result.is_file = result.is_directory = 0;
    }
    CloseHandle(handle);
#else
    struct stat native;
    if ((follow_symlinks ? stat(text, &native) : lstat(text, &native)) != 0) {
        if (errno != ENOENT && errno != ENOTDIR) return 1;
        *info = result; return 0;
    }
    result.exists = 1;
    result.is_directory = S_ISDIR(native.st_mode);
    result.is_file = S_ISREG(native.st_mode);
    result.is_symlink = S_ISLNK(native.st_mode);
    size_t end;
    const size_t first = clm_path_internal_filename(text, &end);
    result.is_hidden = first < end && text[first] == '.' && !(end - first == 1 || (end - first == 2 && text[first + 1] == '.'));
#endif
    *info = result;
    return 0;
}

/** Make a path absolute against the current directory without requiring existence.
 * @param[in,out] path ID of the input path and resulting absolute path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_full_path(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
    char* absolute = NULL;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    const DWORD count = GetFullPathNameW(wide, 0, NULL, NULL);
    wchar_t* output = count ? (wchar_t*)malloc((size_t)count * sizeof(wchar_t)) : NULL;
    if (output) {
        const DWORD written = GetFullPathNameW(wide, count, output, NULL);
        if (written && written < count) absolute = clm_path_internal_utf8(output);
    }
    free(output); free(wide);
#else
    if (text[0] == '/') absolute = clm_path_internal_normalize(text);
    else {
        char* current = clm_path_internal_current();
        if (!current) return 1;
        const size_t size = strlen(current) + strlen(text) + 2;
        char* joined = (char*)malloc(size);
        if (joined) { snprintf(joined, size, "%s/%s", current, text); absolute = clm_path_internal_normalize(joined); }
        free(joined); free(current);
    }
#endif
    const int status = absolute ? clm_path_internal_store(path, absolute) : 1;
    free(absolute);
    return status;
}

/** Compute a lexical relative path from a base to a target.
 * @param[in,out] path ID of the target path, replaced by the relative result.
 * @param base ID of the base directory path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note Inputs must be fully qualified and have matching normalized roots.
 */
static inline clm_int clm_path_get_relative_path(clm_id path, clm_id base) {
    char* target = clm_path_internal_normalize(clm_path_internal_text(path));
    char* origin = clm_path_internal_normalize(clm_path_internal_text(base));
    if (!target || !origin || !clm_path_internal_qualified(target) || !clm_path_internal_qualified(origin)) { free(target); free(origin); return 1; }
    const size_t root = clm_path_internal_root(target), other = clm_path_internal_root(origin);
    if (root != other || !clm_path_internal_equal(target, origin, root)) { free(target); free(origin); return 1; }
    size_t first = root, second = root;
    while (target[first] && origin[second]) {
        size_t end_first = first, end_second = second;
        while (target[end_first] && !clm_path_internal_separator(target[end_first])) ++end_first;
        while (origin[end_second] && !clm_path_internal_separator(origin[end_second])) ++end_second;
        if (end_first - first != end_second - second || !clm_path_internal_equal(target + first, origin + second, end_first - first)) break;
        first = end_first + (target[end_first] != 0); second = end_second + (origin[end_second] != 0);
    }
    char* output = (char*)malloc(strlen(origin) * 3 + strlen(target) + 3);
    if (!output) { free(target); free(origin); return 1; }
    size_t used = 0;
    while (origin[second]) {
        while (origin[second] && !clm_path_internal_separator(origin[second])) ++second;
        if (origin[second]) ++second;
        memcpy(output + used, "..", 2); used += 2; output[used++] = clm_path_internal_native_separator();
    }
    memcpy(output + used, target + first, strlen(target + first) + 1); used += strlen(target + first);
    if (used && clm_path_internal_separator(output[used - 1])) output[--used] = 0;
    if (!used) memcpy(output, ".", 2);
    const int status = clm_path_internal_store(path, output);
    free(output); free(target); free(origin);
    return status;
}

/** Extract a filesystem root, including complete UNC server/share roots.
 * @param[in,out] path ID of the path string, replaced by its root.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_root(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_slice(path, text, clm_path_internal_root(text)) : 1;
}

/** Normalize path components lexically without filesystem access.
 * @param[in,out] path ID of the path string to normalize.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note Preserve roots and extended-path semantics; do not resolve symbolic links.
 */
static inline clm_int clm_path_normalize(clm_id path) {
    char* output = clm_path_internal_normalize(clm_path_internal_text(path));
    const int status = output ? clm_path_internal_store(path, output) : 1;
    free(output);
    return status;
}

/** Resolve an existing filesystem path, including symbolic links.
 * @param[in,out] path ID of the input path and canonical result string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_canonicalize(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
    char* output = NULL;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    HANDLE handle = CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) return 1;
    const DWORD count = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED);
    wchar_t* result = count ? (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t)) : NULL;
    if (result) {
        const DWORD written = GetFinalPathNameByHandleW(handle, result, count + 1, FILE_NAME_NORMALIZED);
        if (written && written <= count) output = clm_path_internal_utf8(result);
    }
    free(result); CloseHandle(handle);
#else
    output = realpath(text, NULL);
#endif
    const int status = output ? clm_path_internal_store(path, output) : 1;
    free(output);
    return status;
}

/** Compare lexical path components for containment under a base directory.
 * @param path ID of the candidate path string.
 * @param base ID of the base directory path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 * @warning Does not resolve links or enforce a filesystem security boundary.
 */
static inline clm_int clm_path_is_within(clm_id path, clm_id base) {
    char* target = clm_path_internal_normalize(clm_path_internal_text(path));
    char* origin = clm_path_internal_normalize(clm_path_internal_text(base));
    if (!target || !origin) { free(target); free(origin); return -1; }
    const size_t length = strlen(origin), target_length = strlen(target);
    if (clm_path_internal_qualified(target) != clm_path_internal_qualified(origin)) {
        free(target); free(origin); return 0;
    }
    if (!strcmp(origin, ".")) {
        const int contained = !clm_path_internal_root(target) &&
            !(target[0] == '.' && target[1] == '.' && (!target[2] || clm_path_internal_separator(target[2])));
        free(target); free(origin); return contained;
    }
    const int result = length && target_length >= length && clm_path_internal_equal(target, origin, length) &&
        (target_length == length || clm_path_internal_separator(origin[length - 1]) || clm_path_internal_separator(target[length]));
    free(target); free(origin);
    return result;
}

/** Test whether a path string contains no characters.
 * @param path ID of the path string, not a directory whose contents are inspected.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_empty(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? *text == 0 : -1;
}

/** Query hidden-path status under platform-specific rules.
 * @param path ID of the filesystem path string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_hidden(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 0, &info) == 0 ? info.is_hidden : -1;
}

/** Extract the final filename component of a path.
 * @param[in,out] path ID of the path string, replaced by its filename.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_filename(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    size_t end;
    const size_t first = clm_path_internal_filename(text, &end);
    return clm_path_internal_slice(path, text + first, end - first);
}

/** Extract the filename component without its final extension.
 * @param[in,out] path ID of the path string, replaced by the filename stem.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_filename_without_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    size_t end;
    const size_t extension = clm_path_internal_extension(text, &end), first = clm_path_internal_filename(text, &end);
    return clm_path_internal_slice(path, text + first, extension - first);
}

/** Extract an extension from the final filename component only.
 * @param[in,out] path ID of the path string, replaced by its extension.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    size_t end;
    const size_t extension = clm_path_internal_extension(text, &end);
    return clm_path_internal_slice(path, text + extension, end - extension);
}

/** Replace or add the extension of a path's filename component.
 * @param[in,out] path ID of the path string to update.
 * @param new_extension ID of the new extension string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_change_extension(clm_id path, clm_id new_extension) {
    const char* text = clm_path_internal_text(path);
    const char* extension = clm_path_internal_text(new_extension);
    if (!text || !extension || strpbrk(extension, "/\\")) return 1;
    size_t end;
    const size_t offset = clm_path_internal_extension(text, &end);
    char* output = (char*)malloc(offset + strlen(extension) + 2);
    if (!output) return 1;
    memcpy(output, text, offset);
    size_t used = offset;
    if (*extension && *extension != '.') output[used++] = '.';
    memcpy(output + used, extension, strlen(extension) + 1);
    const int status = clm_path_internal_store(path, output);
    free(output);
    return status;
}

/** Test for a trailing separator recognized by the platform path grammar.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_ends_with_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return -1;
    const size_t length = strlen(text);
    return length && clm_path_internal_separator(text[length - 1]);
}

/** Append a native separator when the path does not already end in one.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_ensure_ending_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
    const size_t length = strlen(text);
    if (clm_path_internal_separator(text[length - 1])) return 0;
    char* output = (char*)malloc(length + 2);
    if (!output) return 1;
    memcpy(output, text, length); output[length] = clm_path_internal_native_separator(); output[length + 1] = 0;
    const int status = clm_path_internal_store(path, output);
    free(output);
    return status;
}

/** Remove every leading native separator, explicitly discarding root separators.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @warning Removing root separators can change an absolute path into a relative path.
 */
static inline clm_int clm_path_trim_starting_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    while (clm_path_internal_separator(*text)) ++text;
    return clm_path_internal_store(path, text);
}

/** Trim trailing separators without destroying filesystem roots.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_trim_ending_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    size_t length = strlen(text);
    const size_t root = clm_path_internal_root(text);
    while (length > root && clm_path_internal_separator(text[length - 1])) --length;
    return clm_path_internal_slice(path, text, length);
}

/** Test whether the final filename component has an extension.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_has_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return -1;
    size_t end;
    const size_t offset = clm_path_internal_extension(text, &end);
    return offset < end;
}

/** Remove the final filename extension while retaining its directory path.
 * @param[in,out] path ID of the path string to update.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_remove_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    size_t end;
    const size_t offset = clm_path_internal_extension(text, &end);
    return clm_path_internal_slice(path, text, offset);
}

/** Extract the lexical parent directory without filesystem access.
 * @param[in,out] path ID of the path string, replaced by its parent directory.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_parent(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    size_t end;
    size_t first = clm_path_internal_filename(text, &end);
    const size_t root = clm_path_internal_root(text);
    while (first > root && clm_path_internal_separator(text[first - 1])) --first;
    return first ? clm_path_internal_slice(path, text, first) : clm_path_internal_store(path, ".");
}

/** Obtain platform-specific characters disallowed by the chosen path policy.
 * @param[out] destination ID of the result string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @note This list alone cannot express reserved names or all component restrictions.
 */
static inline clm_int clm_path_get_invalid_characters(clm_id destination) {
    return clm_path_internal_store(destination,
#if defined(_WIN32)
        "<>:\"|?*"
#else
        ""
#endif
    );
}

/** Test for a platform-recognized root or drive prefix.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_rooted(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_root(text) != 0 : -1;
}

/** Test whether path resolution is independent of current-directory context.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_fully_qualified(clm_id path) {
    return clm_path_is_absolute(path);
}

/** Recognize a complete ordinary or extended Windows UNC path.
 * @param path ID of the path string containing both a server and share name.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_unc(text, NULL, NULL, NULL) : -1;
}

/** Recognize a complete UNC path in the Windows extended-length namespace.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_extended_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? !strncmp(text, "\\\\?\\UNC\\", 8) && clm_path_internal_unc(text, NULL, NULL, NULL) : -1;
}

/** Extract the server component from a complete ordinary or extended UNC path.
 * @param path ID of the source UNC path string; its contents are not modified.
 * @param[out] destination ID of the result string, without separator characters.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_unc_server(clm_id path, clm_id destination) {
    const char* text = clm_path_internal_text(path);
    size_t server, share;
    return clm_path_internal_unc(text, &server, &share, NULL) ? clm_path_internal_slice(destination, text + server, share - server - 1) : 1;
}

/** Extract the share component from a complete ordinary or extended UNC path.
 * @param path ID of the source UNC path string; its contents are not modified.
 * @param[out] destination ID of the result string, without separator characters.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_unc_share(clm_id path, clm_id destination) {
    const char* text = clm_path_internal_text(path);
    size_t share, end;
    return clm_path_internal_unc(text, NULL, &share, &end) ? clm_path_internal_slice(destination, text + share, end - share) : 1;
}

/** Construct an ordinary UNC share root from validated server and share names.
 * @param server ID of the server-name component string.
 * @param share ID of the share-name component string.
 * @param[out] destination ID of the result path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_build_unc(clm_id server, clm_id share, clm_id destination) {
    const char* first = clm_path_internal_text(server);
    const char* second = clm_path_internal_text(share);
    if (!first || !second || !*first || !*second || strpbrk(first, "\\/") || strpbrk(second, "\\/")) return 1;
    const size_t size = strlen(first) + strlen(second) + 5;
    char* output = (char*)malloc(size);
    if (!output) return 1;
    snprintf(output, size, "\\\\%s\\%s\\", first, second);
    const int status = clm_path_internal_unc(output, NULL, NULL, NULL) ? clm_path_internal_store(destination, output) : 1;
    free(output);
    return status;
}

/** Convert an ordinary UNC path to extended syntax without changing its meaning.
 * @param[in,out] path ID of the UNC path string to convert.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_to_extended_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!clm_path_internal_unc(text, NULL, NULL, NULL)) return 1;
    if (!strncmp(text, "\\\\?\\UNC\\", 8)) return 0;
    for (size_t cursor = 2; text[cursor]; ++cursor)
        if ((text[cursor] == '.' || text[cursor] == ' ') && (text[cursor + 1] == '\\' || !text[cursor + 1])) return 1;
    char* output = (char*)malloc(strlen(text) + 7);
    if (!output) return 1;
    memcpy(output, "\\\\?\\UNC\\", 8); memcpy(output + 8, text + 2, strlen(text + 2) + 1);
    const int status = clm_path_internal_store(path, output);
    free(output);
    return status;
}

/** Convert an extended UNC path to ordinary syntax when representable.
 * @param[in,out] path ID of the UNC path string to convert.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_from_extended_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || strncmp(text, "\\\\?\\UNC\\", 8) || !clm_path_internal_unc(text, NULL, NULL, NULL) || strlen(text) - 6 >= 260) return 1;
    for (size_t cursor = 8; text[cursor]; ++cursor)
        if ((text[cursor] == '.' || text[cursor] == ' ') && (text[cursor + 1] == '\\' || !text[cursor + 1])) return 1;
    char* output = (char*)malloc(strlen(text) - 5);
    if (!output) return 1;
    memcpy(output, "\\\\", 2); memcpy(output + 2, text + 8, strlen(text + 8) + 1);
    const int status = clm_path_internal_store(path, output);
    free(output);
    return status;
}

/** Test whether a path is relative under the platform's path grammar.
 * @param path ID of the path string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_relative(clm_id path) {
    const int absolute = clm_path_is_absolute(path);
    return absolute < 0 ? -1 : !absolute;
}

/** Validate path syntax under platform-specific filename and root rules.
 * @param path ID of the path string to validate.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
static inline clm_int clm_path_is_valid(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_valid(text) : -1;
}

/** Replace all non-overlapping literal text matches without component interpretation.
 * @param[in,out] path ID of the path string to update.
 * @param old_value ID of the text to find.
 * @param new_value ID of the replacement text.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_replace(clm_id path, clm_id old_value, clm_id new_value) {
    const char* text = clm_path_internal_text(path);
    const char* old_text = clm_path_internal_text(old_value);
    const char* replacement = clm_path_internal_text(new_value);
    if (!text || !old_text || !replacement || !*old_text) return 1;
    const size_t old_length = strlen(old_text), new_length = strlen(replacement), length = strlen(text);
    size_t count = 0;
    for (const char* cursor = text; (cursor = strstr(cursor, old_text)) != NULL; cursor += old_length) ++count;
    if (new_length > old_length && count > ((size_t)INT_MAX - length - 1) / (new_length - old_length)) return 1;
    char* output = (char*)malloc(length + count * new_length - count * old_length + 1);
    if (!output) return 1;
    const char* cursor = text;
    char* target = output;
    const char* match;
    while ((match = strstr(cursor, old_text)) != NULL) {
        memcpy(target, cursor, (size_t)(match - cursor)); target += match - cursor;
        memcpy(target, replacement, new_length); target += new_length; cursor = match + old_length;
    }
    memcpy(target, cursor, strlen(cursor) + 1);
    const int status = clm_path_internal_store(path, output);
    free(output);
    return status;
}

/** Obtain the native filesystem path separator as a string.
 * @param[out] destination ID of the result string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_separator(clm_id destination) {
    const char separator[2] = {clm_path_internal_native_separator(), 0};
    return clm_path_internal_store(destination, separator);
}

/** Read the process's current working directory.
 * @param[out] destination ID of the result path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 */
static inline clm_int clm_path_get_current_directory(clm_id destination) {
    char* current = clm_path_internal_current();
    const int status = current ? clm_path_internal_store(destination, current) : 1;
    free(current);
    return status;
}

/** Change the process-wide current working directory.
 * @param path ID of an existing directory path string.
 * @return CLM_PATH_OPERATION_SUCCESS on success; CLM_PATH_OPERATION_FAILURE on error.
 * @warning This changes shared process state and affects relative-path resolution.
 */
static inline clm_int clm_path_set_current_directory(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    const int status = SetCurrentDirectoryW(wide) ? 0 : 1;
    free(wide);
    return status;
#else
    return chdir(text) == 0 ? 0 : 1;
#endif
}
