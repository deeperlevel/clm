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

#pragma once

#include "base.hpp"
#include "bpa.hpp"
#include "string.hpp"
#include "platform.hpp"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define CLM_PATH_OPERATION_SUCCESS 0
#define CLM_PATH_OPERATION_FAILURE 1

typedef struct {
    clm_int exists;
    clm_int is_directory;
    clm_int is_file;
    clm_int is_symlink;
    clm_int is_hidden;
} clm_path_info;

static inline const char* clm_path_internal_text(clm_id id)
{
    auto* block = clm::bpa::detail::find_block(id);
    if (!block || block->type != clm::bpa::CLM_BPA_TYPE_STRING)
        return nullptr;
    return block->data.empty() ? "" : reinterpret_cast<const char*>(block->data.data());
}

static inline char* clm_path_internal_copy(const char* text)
{
    if (!text) return nullptr;
    const std::size_t size = std::strlen(text) + 1;
    char* copy = static_cast<char*>(std::malloc(size));
    if (copy) std::memcpy(copy, text, size);
    return copy;
}

static inline int clm_path_internal_store(clm_id destination, const char* text)
{
    if (!text || !clm_path_internal_text(destination)) return 1;
    const std::size_t length = std::strlen(text);
    if (length >= INT_MAX) return 1;
    if (clm_bpa_set_string(destination, text, static_cast<clm_uint>(length) + 1) != static_cast<int>(length)) return 1;
    auto* blk = clm::bpa::detail::find_block(destination);
    if (blk) blk->encoding = static_cast<clm_byte>(clm::string::encoding::utf8);
    return 0;
}

static inline int clm_path_internal_slice(clm_id destination, const char* text, std::size_t length)
{
    char* copy = static_cast<char*>(std::malloc(length + 1));
    if (!copy) return 1;
    std::memcpy(copy, text, length); copy[length] = 0;
    const int status = clm_path_internal_store(destination, copy);
    std::free(copy);
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

static inline char clm_path_internal_native_separator()
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

static inline int clm_path_internal_unc(const char* text, std::size_t* server, std::size_t* share, std::size_t* end)
{
    if (!text || std::strncmp(text, "\\\\", 2)) return 0;
    std::size_t start = 2;
    if (!std::strncmp(text, "\\\\?\\UNC\\", 8)) start = 8;
    else if (text[2] == '?' || text[2] == '.') return 0;
    std::size_t cursor = start;
    while (text[cursor] && text[cursor] != '\\') ++cursor;
    if (cursor == start || !text[cursor]) return 0;
    const std::size_t share_start = ++cursor;
    while (text[cursor] && text[cursor] != '\\') ++cursor;
    if (cursor == share_start) return 0;
    for (std::size_t index = start; index < cursor; ++index)
        if (std::strchr("/:*?\"<>|", text[index]) || static_cast<unsigned char>(text[index]) < 32) return 0;
    if ((cursor - share_start == 1 && text[share_start] == '.') ||
        (cursor - share_start == 2 && !std::strncmp(text + share_start, "..", 2))) return 0;
    if (server) *server = start;
    if (share) *share = share_start;
    if (end) *end = cursor;
    return 1;
}

static inline std::size_t clm_path_internal_root(const char* text)
{
#if defined(_WIN32)
    std::size_t end;
    if (clm_path_internal_unc(text, nullptr, nullptr, &end)) return end + (text[end] == '\\');
    if (!std::strncmp(text, "\\\\?\\", 4) && std::strlen(text) >= 7 && std::isalpha(static_cast<unsigned char>(text[4])) && text[5] == ':' && text[6] == '\\') return 7;
    if (std::isalpha(static_cast<unsigned char>(text[0])) && text[1] == ':') return clm_path_internal_separator(text[2]) ? 3 : 2;
#endif
    return clm_path_internal_separator(text[0]) ? 1 : 0;
}

static inline int clm_path_internal_qualified(const char* text)
{
#if defined(_WIN32)
    return clm_path_internal_unc(text, nullptr, nullptr, nullptr) ||
        (std::strlen(text) >= 3 && std::isalpha(static_cast<unsigned char>(text[0])) && text[1] == ':' && clm_path_internal_separator(text[2])) ||
        (std::strlen(text) >= 7 && !std::strncmp(text, "\\\\?\\", 4) && std::isalpha(static_cast<unsigned char>(text[4])) && text[5] == ':' && text[6] == '\\');
#else
    return text[0] == '/';
#endif
}

static inline int clm_path_internal_equal(const char* left, const char* right, std::size_t length)
{
    for (std::size_t index = 0; index < length; ++index) {
        unsigned char first = static_cast<unsigned char>(left[index]);
        unsigned char second = static_cast<unsigned char>(right[index]);
#if defined(_WIN32)
        first = static_cast<unsigned char>(std::tolower(first));
        second = static_cast<unsigned char>(std::tolower(second));
#endif
        if (first != second) return 0;
    }
    return 1;
}

static inline char* clm_path_internal_normalize(const char* text)
{
    if (!text) return nullptr;
#if defined(_WIN32)
    if (!std::strncmp(text, "\\\\?\\", 4)) return clm_path_internal_copy(text);
    if (!std::strncmp(text, "\\\\", 2) && !clm_path_internal_unc(text, nullptr, nullptr, nullptr)) return nullptr;
#endif
    const std::size_t length = std::strlen(text), root = clm_path_internal_root(text);
    char* output = static_cast<char*>(std::malloc(length + 3));
    std::size_t* starts = static_cast<std::size_t*>(std::calloc(length + 1, sizeof(std::size_t)));
    if (!output || !starts) { std::free(output); std::free(starts); return nullptr; }
    std::size_t used = 0, depth = 0, cursor = root;
    const char separator = clm_path_internal_native_separator();
    for (; used < root; ++used) output[used] = clm_path_internal_separator(text[used]) ? separator : text[used];
    const int anchored = root && clm_path_internal_separator(text[root - 1]);
    while (cursor < length) {
        while (clm_path_internal_separator(text[cursor])) ++cursor;
        const std::size_t first = cursor;
        while (text[cursor] && !clm_path_internal_separator(text[cursor])) ++cursor;
        const std::size_t count = cursor - first;
        if (!count || (count == 1 && text[first] == '.')) continue;
        if (count == 2 && !std::strncmp(text + first, "..", 2)) {
            if (depth && !(used - starts[depth - 1] == 2 && !std::strncmp(output + starts[depth - 1], "..", 2))) {
                used = starts[--depth];
                if (used > root && output[used - 1] == separator) --used;
                continue;
            }
            if (anchored) continue;
        }
        if (used && output[used - 1] != separator && !(used == 2 && output[1] == ':')) output[used++] = separator;
        starts[depth++] = used;
        std::memcpy(output + used, text + first, count); used += count;
    }
    if (!used && length) output[used++] = '.';
    output[used] = 0;
    std::free(starts);
    return output;
}

static inline std::size_t clm_path_internal_filename(const char* text, std::size_t* end)
{
    const std::size_t root = clm_path_internal_root(text);
    std::size_t last = std::strlen(text);
    while (last > root && clm_path_internal_separator(text[last - 1])) --last;
    std::size_t first = last;
    while (first > root && !clm_path_internal_separator(text[first - 1])) --first;
    *end = last;
    return first;
}

static inline std::size_t clm_path_internal_extension(const char* text, std::size_t* end)
{
    const std::size_t first = clm_path_internal_filename(text, end);
    for (std::size_t cursor = *end; cursor > first + 1; --cursor)
        if (text[cursor - 1] == '.') return cursor - 1;
    return *end;
}

#if defined(_WIN32)
static inline wchar_t* clm_path_internal_wide(const char* text)
{
    if (!text) return nullptr;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (!count) return nullptr;
    wchar_t* result = static_cast<wchar_t*>(std::malloc(static_cast<std::size_t>(count) * sizeof(wchar_t)));
    if (result && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, count)) {
        std::free(result);
        return nullptr;
    }
    return result;
}

static inline char* clm_path_internal_utf8(const wchar_t* text)
{
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    if (!count) return nullptr;
    char* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(count)));
    if (result && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result, count, nullptr, nullptr)) {
        std::free(result);
        return nullptr;
    }
    return result;
}
#endif

static inline char* clm_path_internal_current()
{
#if defined(_WIN32)
    const DWORD count = GetCurrentDirectoryW(0, nullptr);
    wchar_t* wide = count ? static_cast<wchar_t*>(std::malloc(static_cast<std::size_t>(count) * sizeof(wchar_t))) : nullptr;
    if (!wide) return nullptr;
    const DWORD written = GetCurrentDirectoryW(count, wide);
    char* result = written && written < count ? clm_path_internal_utf8(wide) : nullptr;
    std::free(wide);
    return result;
#else
    for (std::size_t size = 256; size <= static_cast<std::size_t>(INT_MAX); size *= 2) {
        char* result = static_cast<char*>(std::malloc(size));
        if (!result) return nullptr;
        if (getcwd(result, size)) return result;
        const int error = errno; std::free(result);
        if (error != ERANGE) return nullptr;
    }
    return nullptr;
#endif
}

static inline int clm_path_internal_valid(const char* text)
{
    if (!text || !*text) return 0;
#if defined(_WIN32)
    const int extended = !std::strncmp(text, "\\\\?\\", 4);
    const std::size_t root = clm_path_internal_root(text);
    if (!std::strncmp(text, "\\\\", 2) && !clm_path_internal_unc(text, nullptr, nullptr, nullptr) && !extended) return 0;
    if (extended && root == 1) return 0;
    for (std::size_t cursor = root; text[cursor]; ) {
        while (clm_path_internal_separator(text[cursor])) ++cursor;
        const std::size_t first = cursor;
        while (text[cursor] && !clm_path_internal_separator(text[cursor])) {
            if (static_cast<unsigned char>(text[cursor]) < 32 || std::strchr("<>:\"|?*", text[cursor])) return 0;
            ++cursor;
        }
        const std::size_t count = cursor - first;
        if (!count || (count == 1 && text[first] == '.') || (count == 2 && !std::strncmp(text + first, "..", 2))) continue;
        if (!extended && (text[cursor - 1] == '.' || text[cursor - 1] == ' ')) return 0;
        if (!extended) {
            char stem[5] = {0};
            std::size_t index = 0;
            while (index < count && index < 4 && text[first + index] != '.') {
                stem[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[first + index])));
                ++index;
            }
            if ((index == 3 && (!std::strcmp(stem, "CON") || !std::strcmp(stem, "PRN") || !std::strcmp(stem, "AUX") || !std::strcmp(stem, "NUL"))) ||
                (index == 4 && (!std::strncmp(stem, "COM", 3) || !std::strncmp(stem, "LPT", 3)) && stem[3] >= '1' && stem[3] <= '9')) return 0;
        }
    }
#endif
    return 1;
}

namespace clm::path {

inline constexpr clm_int SUCCESS = CLM_PATH_OPERATION_SUCCESS;
inline constexpr clm_int FAILURE = CLM_PATH_OPERATION_FAILURE;

using info = clm_path_info;

} // namespace clm::path

static inline clm_int clm_path_get_info(clm_id path, clm_int follow_symlinks, clm_path_info* info)
{
    const char* text = clm_path_internal_text(path);
    if (!text || !*text || !info) return 1;
    clm_path_info result = {0, 0, 0, 0, 0};
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (follow_symlinks ? 0 : FILE_FLAG_OPEN_REPARSE_POINT);
    HANDLE handle = CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, flags, nullptr);
    std::free(wide);
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
    std::size_t end;
    const std::size_t first = clm_path_internal_filename(text, &end);
    result.is_hidden = first < end && text[first] == '.' && !(end - first == 1 || (end - first == 2 && text[first + 1] == '.'));
#endif
    *info = result;
    return 0;
}

static inline clm_int clm_path_sanitize(clm_id path) {
    char* output = clm_path_internal_normalize(clm_path_internal_text(path));
    const int status = output && clm_path_internal_valid(output) ? clm_path_internal_store(path, output) : 1;
    std::free(output);
    return status;
}

static inline clm_int clm_path_is_absolute(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_qualified(text) : -1;
}

static inline clm_int clm_path_join(clm_id path1, clm_id path2) {
    const char* left = clm_path_internal_text(path1);
    const char* right = clm_path_internal_text(path2);
    if (!left || !right) return 1;
    if (clm_path_internal_root(right)) return clm_path_internal_store(path1, right);
    const std::size_t first = std::strlen(left), second = std::strlen(right);
    if (first > static_cast<std::size_t>(INT_MAX) - 2 || second > static_cast<std::size_t>(INT_MAX) - first - 2) return 1;
    char* output = static_cast<char*>(std::malloc(first + second + 2));
    if (!output) return 1;
    std::memcpy(output, left, first);
    std::size_t used = first;
    if (first && second && !clm_path_internal_separator(left[first - 1]) && !(first == 2 && left[1] == ':'))
        output[used++] = clm_path_internal_native_separator();
    std::memcpy(output + used, right, second + 1);
    const int status = clm_path_internal_store(path1, output);
    std::free(output);
    return status;
}

static inline clm_int clm_path_exists(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 1, &info) == 0 ? info.exists : -1;
}

static inline clm_int clm_path_is_directory(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 1, &info) == 0 ? info.is_directory : -1;
}

static inline clm_int clm_path_is_file(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 1, &info) == 0 ? info.is_file : -1;
}

static inline clm_int clm_path_is_symlink(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 0, &info) == 0 ? info.is_symlink : -1;
}

static inline clm_int clm_path_get_full_path(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
    char* absolute = nullptr;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    const DWORD count = GetFullPathNameW(wide, 0, nullptr, nullptr);
    wchar_t* output = count ? static_cast<wchar_t*>(std::malloc(static_cast<std::size_t>(count) * sizeof(wchar_t))) : nullptr;
    if (output) {
        const DWORD written = GetFullPathNameW(wide, count, output, nullptr);
        if (written && written < count) absolute = clm_path_internal_utf8(output);
    }
    std::free(output); std::free(wide);
#else
    if (text[0] == '/') absolute = clm_path_internal_normalize(text);
    else {
        char* current = clm_path_internal_current();
        if (!current) return 1;
        const std::size_t size = std::strlen(current) + std::strlen(text) + 2;
        char* joined = static_cast<char*>(std::malloc(size));
        if (joined) {
            std::snprintf(joined, size, "%s/%s", current, text);
            absolute = clm_path_internal_normalize(joined);
        }
        std::free(joined); std::free(current);
    }
#endif
    const int status = absolute ? clm_path_internal_store(path, absolute) : 1;
    std::free(absolute);
    return status;
}

static inline clm_int clm_path_get_relative_path(clm_id path, clm_id base) {
    char* target = clm_path_internal_normalize(clm_path_internal_text(path));
    char* origin = clm_path_internal_normalize(clm_path_internal_text(base));
    if (!target || !origin || !clm_path_internal_qualified(target) || !clm_path_internal_qualified(origin)) {
        std::free(target); std::free(origin); return 1;
    }
    const std::size_t root = clm_path_internal_root(target), other = clm_path_internal_root(origin);
    if (root != other || !clm_path_internal_equal(target, origin, root)) {
        std::free(target); std::free(origin); return 1;
    }
    std::size_t first = root, second = root;
    while (target[first] && origin[second]) {
        std::size_t end_first = first, end_second = second;
        while (target[end_first] && !clm_path_internal_separator(target[end_first])) ++end_first;
        while (origin[end_second] && !clm_path_internal_separator(origin[end_second])) ++end_second;
        if (end_first - first != end_second - second || !clm_path_internal_equal(target + first, origin + second, end_first - first)) break;
        first = end_first + (target[end_first] != 0); second = end_second + (origin[end_second] != 0);
    }
    char* output = static_cast<char*>(std::malloc(std::strlen(origin) * 3 + std::strlen(target) + 3));
    if (!output) { std::free(target); std::free(origin); return 1; }
    std::size_t used = 0;
    while (origin[second]) {
        while (origin[second] && !clm_path_internal_separator(origin[second])) ++second;
        if (origin[second]) ++second;
        std::memcpy(output + used, "..", 2); used += 2; output[used++] = clm_path_internal_native_separator();
    }
    std::memcpy(output + used, target + first, std::strlen(target + first) + 1); used += std::strlen(target + first);
    if (used && clm_path_internal_separator(output[used - 1])) output[--used] = 0;
    if (!used) std::memcpy(output, ".", 2);
    const int status = clm_path_internal_store(path, output);
    std::free(output); std::free(target); std::free(origin);
    return status;
}

static inline clm_int clm_path_get_root(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_slice(path, text, clm_path_internal_root(text)) : 1;
}

static inline clm_int clm_path_normalize(clm_id path) {
    char* output = clm_path_internal_normalize(clm_path_internal_text(path));
    const int status = output ? clm_path_internal_store(path, output) : 1;
    std::free(output);
    return status;
}

static inline clm_int clm_path_canonicalize(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
    char* output = nullptr;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    HANDLE handle = CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    std::free(wide);
    if (handle == INVALID_HANDLE_VALUE) return 1;
    const DWORD count = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED);
    wchar_t* result = count ? static_cast<wchar_t*>(std::malloc((static_cast<std::size_t>(count) + 1) * sizeof(wchar_t))) : nullptr;
    if (result) {
        const DWORD written = GetFinalPathNameByHandleW(handle, result, count + 1, FILE_NAME_NORMALIZED);
        if (written && written <= count) output = clm_path_internal_utf8(result);
    }
    std::free(result); CloseHandle(handle);
#else
    output = realpath(text, nullptr);
#endif
    const int status = output ? clm_path_internal_store(path, output) : 1;
    std::free(output);
    return status;
}

static inline clm_int clm_path_is_within(clm_id path, clm_id base) {
    char* target = clm_path_internal_normalize(clm_path_internal_text(path));
    char* origin = clm_path_internal_normalize(clm_path_internal_text(base));
    if (!target || !origin) { std::free(target); std::free(origin); return -1; }
    const std::size_t length = std::strlen(origin), target_length = std::strlen(target);
    if (clm_path_internal_qualified(target) != clm_path_internal_qualified(origin)) {
        std::free(target); std::free(origin); return 0;
    }
    if (!std::strcmp(origin, ".")) {
        const int contained = !clm_path_internal_root(target) &&
            !(target[0] == '.' && target[1] == '.' && (!target[2] || clm_path_internal_separator(target[2])));
        std::free(target); std::free(origin); return contained;
    }
    const int result = length && target_length >= length && clm_path_internal_equal(target, origin, length) &&
        (target_length == length || clm_path_internal_separator(origin[length - 1]) || clm_path_internal_separator(target[length]));
    std::free(target); std::free(origin);
    return result;
}

static inline clm_int clm_path_is_empty(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? (*text == 0) : -1;
}

static inline clm_int clm_path_is_hidden(clm_id path) {
    clm_path_info info;
    return clm_path_get_info(path, 0, &info) == 0 ? info.is_hidden : -1;
}

static inline clm_int clm_path_get_filename(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    std::size_t end;
    const std::size_t first = clm_path_internal_filename(text, &end);
    return clm_path_internal_slice(path, text + first, end - first);
}

static inline clm_int clm_path_get_filename_without_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    std::size_t end;
    const std::size_t extension = clm_path_internal_extension(text, &end), first = clm_path_internal_filename(text, &end);
    return clm_path_internal_slice(path, text + first, extension - first);
}

static inline clm_int clm_path_get_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    std::size_t end;
    const std::size_t extension = clm_path_internal_extension(text, &end);
    return clm_path_internal_slice(path, text + extension, end - extension);
}

static inline clm_int clm_path_change_extension(clm_id path, clm_id new_extension) {
    const char* text = clm_path_internal_text(path);
    const char* extension = clm_path_internal_text(new_extension);
    if (!text || !extension || std::strpbrk(extension, "/\\")) return 1;
    std::size_t end;
    const std::size_t offset = clm_path_internal_extension(text, &end);
    char* output = static_cast<char*>(std::malloc(offset + std::strlen(extension) + 2));
    if (!output) return 1;
    std::memcpy(output, text, offset);
    std::size_t used = offset;
    if (*extension && *extension != '.') output[used++] = '.';
    std::memcpy(output + used, extension, std::strlen(extension) + 1);
    const int status = clm_path_internal_store(path, output);
    std::free(output);
    return status;
}

static inline clm_int clm_path_ends_with_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return -1;
    const std::size_t length = std::strlen(text);
    return length && clm_path_internal_separator(text[length - 1]);
}

static inline clm_int clm_path_ensure_ending_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
    const std::size_t length = std::strlen(text);
    if (clm_path_internal_separator(text[length - 1])) return 0;
    char* output = static_cast<char*>(std::malloc(length + 2));
    if (!output) return 1;
    std::memcpy(output, text, length); output[length] = clm_path_internal_native_separator(); output[length + 1] = 0;
    const int status = clm_path_internal_store(path, output);
    std::free(output);
    return status;
}

static inline clm_int clm_path_trim_starting_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    while (clm_path_internal_separator(*text)) ++text;
    return clm_path_internal_store(path, text);
}

static inline clm_int clm_path_trim_ending_separator(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    std::size_t length = std::strlen(text);
    const std::size_t root = clm_path_internal_root(text);
    while (length > root && clm_path_internal_separator(text[length - 1])) --length;
    return clm_path_internal_slice(path, text, length);
}

static inline clm_int clm_path_has_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return -1;
    std::size_t end;
    const std::size_t offset = clm_path_internal_extension(text, &end);
    return offset < end;
}

static inline clm_int clm_path_remove_extension(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    std::size_t end;
    const std::size_t offset = clm_path_internal_extension(text, &end);
    return clm_path_internal_slice(path, text, offset);
}

static inline clm_int clm_path_get_parent(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text) return 1;
    std::size_t end;
    std::size_t first = clm_path_internal_filename(text, &end);
    const std::size_t root = clm_path_internal_root(text);
    while (first > root && clm_path_internal_separator(text[first - 1])) --first;
    return first ? clm_path_internal_slice(path, text, first) : clm_path_internal_store(path, ".");
}

static inline clm_int clm_path_get_invalid_characters(clm_id destination) {
    return clm_path_internal_store(destination,
#if defined(_WIN32)
        "<>:\"|?*"
#else
        ""
#endif
    );
}

static inline clm_int clm_path_is_rooted(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_root(text) != 0 : -1;
}

static inline clm_int clm_path_is_fully_qualified(clm_id path) {
    return clm_path_is_absolute(path);
}

static inline clm_int clm_path_is_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_unc(text, nullptr, nullptr, nullptr) : -1;
}

static inline clm_int clm_path_is_extended_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? !std::strncmp(text, "\\\\?\\UNC\\", 8) && clm_path_internal_unc(text, nullptr, nullptr, nullptr) : -1;
}

static inline clm_int clm_path_get_unc_server(clm_id path, clm_id destination) {
    const char* text = clm_path_internal_text(path);
    std::size_t server, share;
    return clm_path_internal_unc(text, &server, &share, nullptr) ? clm_path_internal_slice(destination, text + server, share - server - 1) : 1;
}

static inline clm_int clm_path_get_unc_share(clm_id path, clm_id destination) {
    const char* text = clm_path_internal_text(path);
    std::size_t share, end;
    return clm_path_internal_unc(text, nullptr, &share, &end) ? clm_path_internal_slice(destination, text + share, end - share) : 1;
}

static inline clm_int clm_path_build_unc(clm_id server, clm_id share, clm_id destination) {
    const char* first = clm_path_internal_text(server);
    const char* second = clm_path_internal_text(share);
    if (!first || !second || !*first || !*second || std::strpbrk(first, "\\/") || std::strpbrk(second, "\\/")) return 1;
    const std::size_t size = std::strlen(first) + std::strlen(second) + 5;
    char* output = static_cast<char*>(std::malloc(size));
    if (!output) return 1;
    std::snprintf(output, size, "\\\\%s\\%s\\", first, second);
    const int status = clm_path_internal_unc(output, nullptr, nullptr, nullptr) ? clm_path_internal_store(destination, output) : 1;
    std::free(output);
    return status;
}

static inline clm_int clm_path_to_extended_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!clm_path_internal_unc(text, nullptr, nullptr, nullptr)) return 1;
    if (!std::strncmp(text, "\\\\?\\UNC\\", 8)) return 0;
    for (std::size_t cursor = 2; text[cursor]; ++cursor)
        if ((text[cursor] == '.' || text[cursor] == ' ') && (text[cursor + 1] == '\\' || !text[cursor + 1])) return 1;
    char* output = static_cast<char*>(std::malloc(std::strlen(text) + 7));
    if (!output) return 1;
    std::memcpy(output, "\\\\?\\UNC\\", 8); std::memcpy(output + 8, text + 2, std::strlen(text + 2) + 1);
    const int status = clm_path_internal_store(path, output);
    std::free(output);
    return status;
}

static inline clm_int clm_path_from_extended_unc(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || std::strncmp(text, "\\\\?\\UNC\\", 8) || !clm_path_internal_unc(text, nullptr, nullptr, nullptr) || std::strlen(text) - 6 >= 260) return 1;
    for (std::size_t cursor = 8; text[cursor]; ++cursor)
        if ((text[cursor] == '.' || text[cursor] == ' ') && (text[cursor + 1] == '\\' || !text[cursor + 1])) return 1;
    char* output = static_cast<char*>(std::malloc(std::strlen(text) - 5));
    if (!output) return 1;
    std::memcpy(output, "\\\\", 2); std::memcpy(output + 2, text + 8, std::strlen(text + 8) + 1);
    const int status = clm_path_internal_store(path, output);
    std::free(output);
    return status;
}

static inline clm_int clm_path_is_relative(clm_id path) {
    const int absolute = clm_path_is_absolute(path);
    return absolute < 0 ? -1 : !absolute;
}

static inline clm_int clm_path_is_valid(clm_id path) {
    const char* text = clm_path_internal_text(path);
    return text ? clm_path_internal_valid(text) : -1;
}

static inline clm_int clm_path_replace(clm_id path, clm_id old_value, clm_id new_value) {
    const char* text = clm_path_internal_text(path);
    const char* old_text = clm_path_internal_text(old_value);
    const char* replacement = clm_path_internal_text(new_value);
    if (!text || !old_text || !replacement || !*old_text) return 1;
    const std::size_t old_length = std::strlen(old_text), new_length = std::strlen(replacement), length = std::strlen(text);
    std::size_t count = 0;
    for (const char* cursor = text; (cursor = std::strstr(cursor, old_text)) != nullptr; cursor += old_length) ++count;
    if (new_length > old_length && count > (static_cast<std::size_t>(INT_MAX) - length - 1) / (new_length - old_length)) return 1;
    char* output = static_cast<char*>(std::malloc(length + count * new_length - count * old_length + 1));
    if (!output) return 1;
    const char* cursor = text;
    char* target = output;
    const char* match;
    while ((match = std::strstr(cursor, old_text)) != nullptr) {
        std::memcpy(target, cursor, static_cast<std::size_t>(match - cursor)); target += match - cursor;
        std::memcpy(target, replacement, new_length); target += new_length; cursor = match + old_length;
    }
    std::memcpy(target, cursor, std::strlen(cursor) + 1);
    const int status = clm_path_internal_store(path, output);
    std::free(output);
    return status;
}

static inline clm_int clm_path_get_separator(clm_id destination) {
    const char separator[2] = {clm_path_internal_native_separator(), 0};
    return clm_path_internal_store(destination, separator);
}

static inline clm_int clm_path_get_current_directory(clm_id destination) {
    char* current = clm_path_internal_current();
    const int status = current ? clm_path_internal_store(destination, current) : 1;
    std::free(current);
    return status;
}

static inline clm_int clm_path_set_current_directory(clm_id path) {
    const char* text = clm_path_internal_text(path);
    if (!text || !*text) return 1;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(text);
    if (!wide) return 1;
    const int status = SetCurrentDirectoryW(wide) ? 0 : 1;
    std::free(wide);
    return status;
#else
    return chdir(text) == 0 ? 0 : 1;
#endif
}
