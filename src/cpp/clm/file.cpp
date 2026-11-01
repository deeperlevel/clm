#pragma once

/*
 * =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
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

#include "base.hpp"
#include "path.hpp"
#include "bpa.hpp"
#include "string.hpp"
#include "gen.hpp"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <utime.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <linux/fs.h>
#endif
#endif

#define CLM_FILE_OPERATION_SUCCESS 0
#define CLM_FILE_OPERATION_FAILURE -1
#define CLM_FILE_OPERATION_UNSUPPORTED -2

/** @file
 * File API. Define CLM_FILE_IMPLEMENTATION in exactly one C translation unit
 * to provide the shared ID-to-native-resource registry.
 * Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native
 * resources. _as accepts raw UTF-8 paths, _to exports caller-owned data, and
 * _from consumes caller-owned data. Native handle representation remains private.
 * Existing BPA binary blocks have fixed capacity; text outputs are existing CLM
 * strings or string arrays. Whole-file outputs are prepared before publication.
 * Size/position queries use 64-bit values; callers synchronize registry access.
 * Status functions return SUCCESS/FAILURE/UNSUPPORTED; exists and is_open are
 * direct predicates returning 1, 0, or -1. Streaming transfers expose partial
 * progress and writes are not transactional. Closed IDs remain retired until
 * clm_file_shutdown; shutdown attempts every close before releasing registry state.
 * Text input/output in memory is UTF-8; encoding selects the file representation.
 * BOM variants require/emit the matching BOM; append never inserts a BOM into
 * existing content. UTF16 compatibility names mean explicit little-endian UTF-16.
 * Text conversion rejects malformed input and embedded NUL. Line helpers split
 * CRLF, LF, and CR, omit delimiters, and do not invent a final empty line; append
 * line helpers append exactly one LF, encoded for the selected file encoding.
 * Whole-file calls open and close internally; they do not register open-file IDs.
 * Native opens reject final-component links/reparse points rather than follow
 * them implicitly. Windows creation inherits parent ACLs; POSIX uses mode 0600.
 * Registry teardown is explicit; it is not automatically tied to BPA shutdown.
 */

#define CLM_FILE_TEXT_ENCODING_ASCII 0
#define CLM_FILE_TEXT_ENCODING_UTF8 1
#define CLM_FILE_TEXT_ENCODING_UTF8_BOM 2
#define CLM_FILE_TEXT_ENCODING_UTF16 3
#define CLM_FILE_TEXT_ENCODING_UTF16_BOM 4
#define CLM_FILE_TEXT_ENCODING_UTF16_LE CLM_FILE_TEXT_ENCODING_UTF16
#define CLM_FILE_TEXT_ENCODING_UTF16_LE_BOM CLM_FILE_TEXT_ENCODING_UTF16_BOM
#define CLM_FILE_TEXT_ENCODING_UTF16_BE 5
#define CLM_FILE_TEXT_ENCODING_UTF16_BE_BOM 6

#define CLM_FILE_OPEN_MODE_READ 1
#define CLM_FILE_OPEN_MODE_WRITE 2
#define CLM_FILE_OPEN_MODE_APPEND 4
#define CLM_FILE_OPEN_MODE_CREATE 8
#define CLM_FILE_OPEN_MODE_TRUNCATE 16
#define CLM_FILE_OPEN_MODE_EXCLUSIVE 32

#define CLM_FILE_SEEK_BEGIN 0
#define CLM_FILE_SEEK_CURRENT 1
#define CLM_FILE_SEEK_END 2

#define CLM_FILE_ATTRIBUTE_READ_ONLY     (UINT64_C(1) << 0)
#define CLM_FILE_ATTRIBUTE_HIDDEN        (UINT64_C(1) << 1)
#define CLM_FILE_ATTRIBUTE_SYSTEM        (UINT64_C(1) << 2)
#define CLM_FILE_ATTRIBUTE_ARCHIVE       (UINT64_C(1) << 3)
#define CLM_FILE_ATTRIBUTE_TEMPORARY     (UINT64_C(1) << 4)
#define CLM_FILE_ATTRIBUTE_OFFLINE       (UINT64_C(1) << 5)
#define CLM_FILE_ATTRIBUTE_ENCRYPTED     (UINT64_C(1) << 6)
#define CLM_FILE_ATTRIBUTE_COMPRESSED    (UINT64_C(1) << 7)
#define CLM_FILE_ATTRIBUTE_REPARSE_POINT (UINT64_C(1) << 8)
#define CLM_FILE_ATTRIBUTE_SYMBOLIC_LINK (UINT64_C(1) << 9)

#define CLM_FILE_TIME_CREATION 1u
#define CLM_FILE_TIME_MODIFICATION 2u
#define CLM_FILE_TIME_ACCESS 4u

/** Metadata for the selected file entry or link target.
 * Masks distinguish unsupported timestamps/attributes from zero or false values.
 * size is logical bytes, not allocated disk usage; timestamps have time_t precision.
 */
typedef struct {
    clm_path_info path;
    clm_ulong size;
    clm_ulong attributes;
    clm_ulong supported_attributes;
    time_t creation_time;
    time_t modification_time;
    time_t access_time;
    clm_uint supported_times;
} clm_file_info;

/** Opaque native file state; callers operate on open-file IDs, not this layout. */
struct clm_file;

#if defined(_WIN32)
typedef HANDLE clm_file_internal_handle;
#define CLM_FILE_INTERNAL_INVALID INVALID_HANDLE_VALUE
#else
typedef int clm_file_internal_handle;
#define CLM_FILE_INTERNAL_INVALID (-1)
#endif

typedef struct clm_file_internal_node {
    clm_id id;
    clm_int mode;
    bool eof;
    clm_file_internal_handle handle;
    struct clm_file_internal_node* next;
} clm_file_internal_node;

extern clm_file_internal_node* clm_file_internal_registry;
#ifdef CLM_FILE_IMPLEMENTATION
clm_file_internal_node* clm_file_internal_registry = NULL;
#endif

static inline int clm_file_internal_valid_path(const char* path)
{
    size_t characters;
    return path && *path && clm_string_measure(path, strlen(path), CLM_STRING_ENCODING_UTF8, &characters) == 0;
}

static inline clm_file_internal_node* clm_file_internal_find(clm_id id)
{
    for (clm_file_internal_node* node = clm_file_internal_registry; node; node = node->next)
        if (node->id == id) return node;
    return NULL;
}

static inline int clm_file_internal_close(clm_file_internal_handle handle)
{
#if defined(_WIN32)
    return CloseHandle(handle) ? 0 : -1;
#else
    return close(handle) == 0 ? 0 : -1;
#endif
}

static inline clm_file_internal_handle clm_file_internal_open(const char* path, int mode)
{
    const int allowed = 63;
    if (!clm_file_internal_valid_path(path) || (mode & ~allowed) || !(mode & 7) ||
        ((mode & CLM_FILE_OPEN_MODE_EXCLUSIVE) && !(mode & CLM_FILE_OPEN_MODE_CREATE)) ||
        ((mode & CLM_FILE_OPEN_MODE_TRUNCATE) && (!(mode & CLM_FILE_OPEN_MODE_WRITE) || (mode & CLM_FILE_OPEN_MODE_APPEND))))
        return CLM_FILE_INTERNAL_INVALID;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return CLM_FILE_INTERNAL_INVALID;
    DWORD access = (mode & CLM_FILE_OPEN_MODE_READ) ? GENERIC_READ : 0;
    if (mode & CLM_FILE_OPEN_MODE_APPEND) access |= FILE_APPEND_DATA | FILE_READ_ATTRIBUTES;
    else if (mode & CLM_FILE_OPEN_MODE_WRITE) access |= GENERIC_WRITE;
    DWORD disposition = OPEN_EXISTING;
    if (mode & CLM_FILE_OPEN_MODE_EXCLUSIVE) disposition = CREATE_NEW;
    else if (mode & CLM_FILE_OPEN_MODE_CREATE) disposition = OPEN_ALWAYS;
    HANDLE handle = CreateFileW(wide, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, disposition, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) return handle;
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(handle, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(handle); SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE;
    }
    if (mode & CLM_FILE_OPEN_MODE_TRUNCATE) {
        LARGE_INTEGER zero; zero.QuadPart = 0;
        if (!SetFilePointerEx(handle, zero, NULL, FILE_BEGIN) || !SetEndOfFile(handle)) {
            CloseHandle(handle); return INVALID_HANDLE_VALUE;
        }
    }
    return handle;
#else
    int flags = O_CLOEXEC | O_NOFOLLOW;
    if ((mode & CLM_FILE_OPEN_MODE_READ) && (mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) flags |= O_RDWR;
    else if (mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND)) flags |= O_WRONLY;
    else flags |= O_RDONLY;
    if (mode & CLM_FILE_OPEN_MODE_CREATE) flags |= O_CREAT;
    if (mode & CLM_FILE_OPEN_MODE_EXCLUSIVE) flags |= O_EXCL;
    if (mode & CLM_FILE_OPEN_MODE_APPEND) flags |= O_APPEND;
    const int handle = open(path, flags, 0600);
    if (handle < 0) return -1;
    struct stat info;
    if (fstat(handle, &info) || !S_ISREG(info.st_mode) || ((mode & CLM_FILE_OPEN_MODE_TRUNCATE) && ftruncate(handle, 0))) {
        close(handle); return -1;
    }
    return handle;
#endif
}

static inline int clm_file_internal_size(clm_file_internal_handle handle, clm_ulong* size)
{
    if (!size) return -1;
#if defined(_WIN32)
    LARGE_INTEGER result;
    if (!GetFileSizeEx(handle, &result) || result.QuadPart < 0) return -1;
    *size = (clm_ulong)result.QuadPart;
#else
    struct stat info;
    if (fstat(handle, &info) || info.st_size < 0) return -1;
    *size = (clm_ulong)info.st_size;
#endif
    return 0;
}

static inline int clm_file_internal_seek(clm_file_internal_handle handle, clm_long offset, int origin, clm_ulong* position)
{
    if (origin < 0 || origin > 2) return -1;
#if defined(_WIN32)
    LARGE_INTEGER displacement, result;
    displacement.QuadPart = offset;
    if (!SetFilePointerEx(handle, displacement, &result, (DWORD)origin) || result.QuadPart < 0) return -1;
    if (position) *position = (clm_ulong)result.QuadPart;
#else
    if ((clm_long)(off_t)offset != offset) return -2;
    const off_t result = lseek(handle, (off_t)offset, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END);
    if (result < 0) return -1;
    if (position) *position = (clm_ulong)result;
#endif
    return 0;
}

static inline int clm_file_internal_transfer(clm_file_internal_handle handle, void* buffer, size_t size,
                                             int writing, size_t* transferred)
{
    if (!transferred || (!buffer && size)) return -1;
    size_t total = 0;
    while (total < size) {
        const size_t chunk = size - total > 0x40000000u ? 0x40000000u : size - total;
#if defined(_WIN32)
        DWORD count = 0;
        const BOOL success = writing ? WriteFile(handle, (char*)buffer + total, (DWORD)chunk, &count, NULL)
                                     : ReadFile(handle, (char*)buffer + total, (DWORD)chunk, &count, NULL);
        if (!success) { *transferred = total; return -1; }
#else
        const ssize_t count = writing ? write(handle, (char*)buffer + total, chunk) : read(handle, (char*)buffer + total, chunk);
        if (count < 0) { if (errno == EINTR) continue; *transferred = total; return -1; }
#endif
        if (!count) { *transferred = total; return writing ? -1 : 0; }
        total += (size_t)count;
    }
    *transferred = total;
    return 0;
}

static inline int clm_file_internal_truncate(clm_file_internal_handle handle, clm_ulong size)
{
    if (size > (clm_ulong)LONG_MAX) return -2;
#if defined(_WIN32)
    clm_ulong original;
    if (clm_file_internal_seek(handle, 0, 1, &original) || clm_file_internal_seek(handle, (clm_long)size, 0, NULL)) return -1;
    const int status = SetEndOfFile(handle) ? 0 : -1;
    const int restored = clm_file_internal_seek(handle, (clm_long)original, 0, NULL);
    return status || restored ? -1 : 0;
#else
    if ((clm_ulong)(off_t)size != size) return -2;
    return ftruncate(handle, (off_t)size) == 0 ? 0 : -1;
#endif
}

static inline clm_bpa_block_node* clm_file_internal_binary(clm_id id, size_t size)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    return block && block->type != CLM_BPA_TYPE_STRING && size <= block->block_size ? block : NULL;
}

static inline unsigned char* clm_file_internal_read_all(const char* path, size_t* size)
{
    if (!size) return NULL;
    clm_file_internal_handle handle = clm_file_internal_open(path, CLM_FILE_OPEN_MODE_READ);
    if (handle == CLM_FILE_INTERNAL_INVALID) return NULL;
    size_t capacity = 4096, used = 0;
    unsigned char* bytes = (unsigned char*)malloc(capacity);
    if (!bytes) { clm_file_internal_close(handle); return NULL; }
    int status = 0;
    for (;;) {
        if (used == capacity) {
            if (capacity > (size_t)-1 / 2) { status = -1; break; }
            capacity *= 2;
            unsigned char* grown = (unsigned char*)realloc(bytes, capacity);
            if (!grown) { status = -1; break; }
            bytes = grown;
        }
        size_t count;
        status = clm_file_internal_transfer(handle, bytes + used, capacity - used, 0, &count);
        used += count;
        if (status || used < capacity) break;
    }
    if (clm_file_internal_close(handle)) status = -1;
    if (status) { free(bytes); return NULL; }
    *size = used;
    return bytes;
}

static inline int clm_file_internal_write_all(const char* path, const void* bytes, size_t size, int append)
{
    if (!bytes && size) return -1;
    const int mode = CLM_FILE_OPEN_MODE_CREATE | (append ? CLM_FILE_OPEN_MODE_APPEND : CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_TRUNCATE);
    clm_file_internal_handle handle = clm_file_internal_open(path, mode);
    if (handle == CLM_FILE_INTERNAL_INVALID) return -1;
    size_t count;
    int status = clm_file_internal_transfer(handle, (void*)bytes, size, 1, &count);
    if (clm_file_internal_close(handle)) status = -1;
    return status;
}

static inline int clm_file_internal_string(clm_id id, const char* text, size_t size)
{
    if (!clm_path_internal_text(id) || size > (size_t)INT_MAX) return -1;
    char* copy = (char*)malloc(size + 1);
    if (!copy) return -1;
    memcpy(copy, text, size); copy[size] = 0;
    const int status = clm_path_internal_store(id, copy) ? -1 : 0;
    free(copy);
    return status;
}

static inline int clm_file_internal_array(clm_id id, const char* packed, size_t bytes, clm_ulong count)
{
    clm_bpaa_array_node* array;
    if (!packed || count > INT_MAX || clm_string_array_get_node(id, &array) != 0 ||
        (array->sealed && count > array->capacity)) return -1;
    const clm_uint capacity = count > array->capacity ? (clm_uint)count : array->capacity;
    char** items = capacity ? (char**)calloc(capacity, sizeof(char*)) : NULL;
    if (capacity && !items) return -1;
    size_t offset = 0;
    clm_uint completed = 0;
    for (; completed < count; ++completed) {
        const char* end = offset < bytes ? (const char*)memchr(packed + offset, 0, bytes - offset) : NULL;
        if (!end) break;
        const size_t length = (size_t)(end - packed - offset);
        clm_uint copied;
        if (length >= INT_MAX || clm_bpa_copy_string(packed + offset, (clm_uint)length + 1, &items[completed], &copied) != 0) break;
        offset += length + 1;
    }
    if (completed != count) {
        for (clm_uint index = 0; index < completed; ++index) clm_bpa_pool_free(items[index], strlen(items[index]) + 1);
        free(items);
        return -1;
    }
    for (clm_uint index = 0; index < array->count; ++index) clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
    free(array->items.strings);
    array->items.strings = items; array->count = (clm_uint)count; array->capacity = capacity;
    array->index = 0; array->deck_size = (clm_uint)count;
    return 0;
}

static inline time_t clm_file_internal_time_from_native(
#if defined(_WIN32)
    FILETIME value
#else
    time_t value
#endif
)
{
#if defined(_WIN32)
    ULARGE_INTEGER native;
    native.LowPart = value.dwLowDateTime; native.HighPart = value.dwHighDateTime;
    return (time_t)((clm_long)(native.QuadPart / UINT64_C(10000000)) - INT64_C(11644473600));
#else
    return value;
#endif
}

static inline int clm_file_internal_info(const char* path, bool follow, clm_file_info* out)
{
    if (!clm_file_internal_valid_path(path) || !out) return -1;
    clm_file_info result = {0};
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    HANDLE handle = CreateFileW(wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | (follow ? 0 : FILE_FLAG_OPEN_REPARSE_POINT), NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return -1;
        *out = result; return 0;
    }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(handle, &info)) { CloseHandle(handle); return -1; }
    const DWORD flags = info.dwFileAttributes;
    result.path.exists = 1;
    result.path.is_directory = (flags & FILE_ATTRIBUTE_DIRECTORY) != 0;
    result.path.is_file = !result.path.is_directory;
    result.path.is_hidden = (flags & FILE_ATTRIBUTE_HIDDEN) != 0;
    const DWORD native_flags[] = {FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM,
        FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_ENCRYPTED,
        FILE_ATTRIBUTE_COMPRESSED, FILE_ATTRIBUTE_REPARSE_POINT};
    for (size_t index = 0; index < 9; ++index)
        if (flags & native_flags[index]) result.attributes |= UINT64_C(1) << index;
    result.supported_attributes = (UINT64_C(1) << 10) - 1;
    if (!follow && (flags & FILE_ATTRIBUTE_REPARSE_POINT)) {
        FILE_ATTRIBUTE_TAG_INFO tag;
        if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag))) { CloseHandle(handle); return -1; }
        result.path.is_symlink = tag.ReparseTag == IO_REPARSE_TAG_SYMLINK;
        if (result.path.is_symlink) {
            result.attributes |= CLM_FILE_ATTRIBUTE_SYMBOLIC_LINK;
            result.path.is_file = result.path.is_directory = 0;
        }
    }
    result.size = ((clm_ulong)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    result.creation_time = clm_file_internal_time_from_native(info.ftCreationTime);
    result.modification_time = clm_file_internal_time_from_native(info.ftLastWriteTime);
    result.access_time = clm_file_internal_time_from_native(info.ftLastAccessTime);
    result.supported_times = 7;
    CloseHandle(handle);
#else
    struct stat info;
    if ((follow ? stat(path, &info) : lstat(path, &info)) != 0) {
        if (errno != ENOENT && errno != ENOTDIR) return -1;
        *out = result; return 0;
    }
    result.path.exists = 1; result.path.is_file = S_ISREG(info.st_mode);
    result.path.is_directory = S_ISDIR(info.st_mode); result.path.is_symlink = S_ISLNK(info.st_mode);
    size_t end;
    const size_t first = clm_path_internal_filename(path, &end);
    result.path.is_hidden = end > first && path[first] == '.' && strcmp(path + first, ".") && strcmp(path + first, "..");
    if (result.path.is_hidden) result.attributes |= CLM_FILE_ATTRIBUTE_HIDDEN;
    if (result.path.is_symlink) result.attributes |= CLM_FILE_ATTRIBUTE_SYMBOLIC_LINK;
    result.supported_attributes = CLM_FILE_ATTRIBUTE_HIDDEN | CLM_FILE_ATTRIBUTE_SYMBOLIC_LINK;
    result.size = info.st_size >= 0 ? (clm_ulong)info.st_size : 0;
    result.modification_time = info.st_mtime; result.access_time = info.st_atime;
    result.supported_times = 6;
#endif
    *out = result;
    return 0;
}

static inline int clm_file_internal_attribute(const char* path, clm_ulong attribute, bool* out)
{
    clm_file_info info;
    if (!out || clm_file_internal_info(path, false, &info) || !info.path.exists) return -1;
    if (!(info.supported_attributes & attribute)) return -2;
    *out = (info.attributes & attribute) != 0;
    return 0;
}

static inline int clm_file_internal_set_attributes(const char* path, clm_ulong attributes, clm_ulong mask)
{
    clm_file_info info;
    if (clm_file_internal_info(path, false, &info) || !info.path.exists || info.path.is_symlink ||
        (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)) return -1;
    if (!mask) return 0;
#if defined(_WIN32)
    if (mask & ~UINT64_C(63)) return -2;
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    DWORD flags = GetFileAttributesW(wide);
    if (flags == INVALID_FILE_ATTRIBUTES) { free(wide); return -1; }
    const DWORD native_flags[] = {FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM,
        FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_OFFLINE};
    for (size_t index = 0; index < 6; ++index) {
        const clm_ulong bit = UINT64_C(1) << index;
        if (mask & bit) { flags &= ~native_flags[index]; if (attributes & bit) flags |= native_flags[index]; }
    }
    const int status = SetFileAttributesW(wide, flags ? flags : FILE_ATTRIBUTE_NORMAL) ? 0 : -1;
    free(wide);
    return status;
#else
    (void)attributes;
    return -2;
#endif
}

static inline int clm_file_internal_set_times(const char* path, time_t creation, time_t modification,
                                             time_t access, clm_uint mask)
{
    if (mask & ~7u) return -1;
    clm_file_info info;
    if (clm_file_internal_info(path, false, &info) || !info.path.exists || info.path.is_symlink ||
        (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)) return -1;
    if (!mask) return 0;
    if (mask & ~info.supported_times) return -2;
#if defined(_WIN32)
    const time_t values[] = {creation, modification, access};
    FILETIME times[3];
    for (size_t index = 0; index < 3; ++index) {
        if (!(mask & (1u << index))) continue;
        const clm_long seconds = (clm_long)values[index];
        if (seconds < -INT64_C(11644473600) || seconds > (clm_long)(ULONG_MAX / UINT64_C(10000000)) - INT64_C(11644473600)) return -1;
        ULARGE_INTEGER native;
        native.QuadPart = (clm_ulong)(seconds + INT64_C(11644473600)) * UINT64_C(10000000);
        times[index].dwLowDateTime = native.LowPart; times[index].dwHighDateTime = native.HighPart;
    }
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    HANDLE handle = CreateFileW(wide, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) return -1;
    const int status = SetFileTime(handle, mask & 1 ? &times[0] : NULL,
        mask & 4 ? &times[2] : NULL, mask & 2 ? &times[1] : NULL) ? 0 : -1;
    CloseHandle(handle);
    return status;
#else
    (void)creation;
    struct timespec times[2];
    times[0].tv_sec = access; times[0].tv_nsec = mask & 4 ? 0 : UTIME_OMIT;
    times[1].tv_sec = modification; times[1].tv_nsec = mask & 2 ? 0 : UTIME_OMIT;
    return utimensat(AT_FDCWD, path, times, AT_SYMLINK_NOFOLLOW) == 0 ? 0 : -1;
#endif
}

static inline char* clm_file_internal_decode(const unsigned char* bytes, size_t size, int encoding, size_t* length)
{
    if (!bytes || !length || encoding < 0 || encoding > 6) return NULL;
    size_t skip = 0;
    if (encoding == CLM_FILE_TEXT_ENCODING_UTF8_BOM) {
        if (size < 3 || memcmp(bytes, "\xef\xbb\xbf", 3)) return NULL;
        skip = 3;
    } else if (encoding == CLM_FILE_TEXT_ENCODING_UTF16_LE_BOM || encoding == CLM_FILE_TEXT_ENCODING_UTF16_BE_BOM) {
        const int big = encoding == CLM_FILE_TEXT_ENCODING_UTF16_BE_BOM;
        if (size < 2 || bytes[0] != (big ? 0xfe : 0xff) || bytes[1] != (big ? 0xff : 0xfe)) return NULL;
        skip = 2;
    } else if ((size >= 3 && !memcmp(bytes, "\xef\xbb\xbf", 3)) ||
               (size >= 2 && ((!memcmp(bytes, "\xff\xfe", 2)) || (!memcmp(bytes, "\xfe\xff", 2))))) return NULL;
    if (encoding == CLM_FILE_TEXT_ENCODING_ASCII)
        for (size_t index = 0; index < size; ++index) if (bytes[index] >= 128) return NULL;
    const bool utf16 = encoding >= 3;
    const bool big = encoding == 5 || encoding == 6;
    if (utf16 && ((size - skip) & 1)) return NULL;
    unsigned char* swapped = NULL;
    if (big) {
        swapped = (unsigned char*)malloc(size - skip + 1);
        if (!swapped) return NULL;
        for (size_t index = 0; index < size - skip; index += 2) { swapped[index] = bytes[skip + index + 1]; swapped[index + 1] = bytes[skip + index]; }
    }
    char* temporary = NULL;
    size_t result_length;
    const int status = clm_string_decode_bytes(utf16 ? CLM_STRING_ENCODING_UTF16LE : CLM_STRING_ENCODING_UTF8,
        big ? swapped : bytes + skip, size - skip, &temporary, &result_length);
    free(swapped);
    if (status != 0) return NULL;
    char* result = (char*)malloc(result_length + 1);
    if (result) { memcpy(result, temporary, result_length + 1); *length = result_length; }
    clm_string_temp_free(temporary, result_length + 1);
    return result;
}

static inline unsigned char* clm_file_internal_encode(const char* text, size_t length, int encoding, size_t* size)
{
    size_t characters;
    if ((!text && length) || !size || encoding < 0 || encoding > 6 ||
        clm_string_measure(text ? text : "", length, CLM_STRING_ENCODING_UTF8, &characters) != 0 ||
        length > ((size_t)-1 - 4) / 4) return NULL;
    unsigned char* output = (unsigned char*)malloc(length * 4 + 4);
    if (!output) return NULL;
    size_t used = 0;
    if (encoding == 2) { memcpy(output, "\xef\xbb\xbf", 3); used = 3; }
    if (encoding == 4) { output[used++] = 0xff; output[used++] = 0xfe; }
    if (encoding == 6) { output[used++] = 0xfe; output[used++] = 0xff; }
    size_t offset = 0;
    while (offset < length) {
        uint32_t point;
        const size_t first = offset;
        if (clm_string_utf8_decode_one((const unsigned char*)text, length, &offset, &point) != 0 || !point || (encoding == 0 && point > 127)) {
            free(output); return NULL;
        }
        if (encoding < 3) { memcpy(output + used, text + first, offset - first); used += offset - first; }
        else {
            uint16_t units[2];
            size_t count = 1;
            if (point < 0x10000) units[0] = (uint16_t)point;
            else { point -= 0x10000; units[0] = (uint16_t)(0xd800 | (point >> 10)); units[1] = (uint16_t)(0xdc00 | (point & 1023)); count = 2; }
            for (size_t index = 0; index < count; ++index) {
                output[used++] = (unsigned char)(encoding >= 5 ? units[index] >> 8 : units[index] & 255);
                output[used++] = (unsigned char)(encoding >= 5 ? units[index] & 255 : units[index] >> 8);
            }
        }
    }
    *size = used;
    return output;
}

static inline int clm_file_internal_text_write(const char* path, int encoding, const char* text, size_t size, int append, int line)
{
    if (!text && size) return -1;
    char* input = NULL;
    if (line) {
        if (size > (size_t)-1 - 2) return -1;
        input = (char*)malloc(size + 2);
        if (!input) return -1;
        if (size) memcpy(input, text, size);
        input[size] = '\n'; input[size + 1] = 0; text = input; ++size;
    }
    size_t encoded_size;
    unsigned char* encoded = clm_file_internal_encode(text, size, encoding, &encoded_size);
    free(input);
    if (!encoded) return -1;
    size_t skip = 0;
    if (append) {
        clm_file_info info;
        if (clm_file_internal_info(path, false, &info) || (info.path.exists && !info.path.is_file)) { free(encoded); return -1; }
        if (info.path.exists && info.size) {
            size_t previous_size, decoded_size;
            unsigned char* previous = clm_file_internal_read_all(path, &previous_size);
            char* decoded = previous ? clm_file_internal_decode(previous, previous_size, encoding, &decoded_size) : NULL;
            free(previous);
            if (!decoded) { free(encoded); return -1; }
            free(decoded);
            skip = encoding == 2 ? 3 : encoding == 4 || encoding == 6 ? 2 : 0;
        }
    }
    const int status = clm_file_internal_write_all(path, encoded + skip, encoded_size - skip, append);
    free(encoded);
    return status;
}

static inline char* clm_file_internal_lines(const char* text, size_t length, size_t* bytes, clm_ulong* count)
{
    char* packed = (char*)malloc(length + 2);
    if (!packed) return NULL;
    size_t used = 0, first = 0;
    clm_ulong total = 0;
    for (size_t cursor = 0; cursor < length; ++cursor) {
        if (text[cursor] != '\n' && text[cursor] != '\r') continue;
        memcpy(packed + used, text + first, cursor - first); used += cursor - first;
        packed[used++] = 0; ++total;
        if (text[cursor] == '\r' && cursor + 1 < length && text[cursor + 1] == '\n') ++cursor;
        first = cursor + 1;
    }
    if (first < length) { memcpy(packed + used, text + first, length - first); used += length - first; packed[used++] = 0; ++total; }
    packed[used++] = 0;
    *bytes = used; *count = total;
    return packed;
}

static inline int clm_file_internal_unlink(const char* path)
{
    if (!clm_file_internal_valid_path(path)) return -1;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    const int status = DeleteFileW(wide) ? 0 : -1;
    free(wide); return status;
#else
    struct stat info;
    if (lstat(path, &info) || S_ISDIR(info.st_mode)) return -1;
    return unlink(path) == 0 ? 0 : -1;
#endif
}

static inline char* clm_file_internal_temp_name(const char* base, const char* suffix)
{
    if (!clm_file_internal_valid_path(base)) return NULL;
    uint64_t random[2];
    if (!clm_gen_internal_bytes(random, sizeof(random))) return NULL;
    const size_t size = strlen(base) + strlen(suffix) + 48;
    char* path = (char*)malloc(size);
    if (path) snprintf(path, size, "%s%cclm-%016llx%016llx%s", base, clm_path_internal_native_separator(),
        (unsigned long long)random[0], (unsigned long long)random[1], suffix);
    return path;
}

static inline char* clm_file_internal_parent(const char* path)
{
    if (!clm_file_internal_valid_path(path)) return NULL;
    size_t end;
    size_t first = clm_path_internal_filename(path, &end);
    const size_t root = clm_path_internal_root(path);
    while (first > root && clm_path_internal_separator(path[first - 1])) --first;
    char* result = clm_path_internal_copy(first ? path : ".");
    if (result && first) result[first] = 0;
    return result;
}

static inline int clm_file_internal_register(clm_id id, const char* path, int mode)
{
    if (clm_file_internal_find(id)) return -1;
    clm_file_internal_node* node = (clm_file_internal_node*)calloc(1, sizeof(*node));
    if (!node) return -1;
    node->handle = clm_file_internal_open(path, mode);
    if (node->handle == CLM_FILE_INTERNAL_INVALID) { free(node); return -1; }
    node->id = id; node->mode = mode; node->next = clm_file_internal_registry;
    clm_file_internal_registry = node;
    return 0;
}

static inline int clm_file_internal_binary_read(const char* path, clm_id destination, void* buffer,
                                               size_t capacity, size_t* size, bool managed)
{
    clm_bpa_block_node* block = managed ? clm_file_internal_binary(destination, 0) : NULL;
    if (!size || (managed && !block) || (!managed && !buffer && capacity)) return -1;
    if (managed) { buffer = block->data; capacity = block->block_size; }
    size_t length;
    unsigned char* bytes = clm_file_internal_read_all(path, &length);
    if (!bytes) return -1;
    if ((!managed && !buffer) || capacity < length) {
        free(bytes); if (!managed) *size = length;
        return !managed && !buffer && !capacity ? 0 : -1;
    }
    if (length) memcpy(buffer, bytes, length);
    free(bytes); *size = length;
    return 0;
}

static inline int clm_file_internal_text_read(const char* path, int encoding, clm_id destination,
    char* buffer, size_t capacity, size_t* size, clm_ulong* count, int kind)
{
    const bool managed = kind == 0 || kind == 2;
    if ((kind < 2 && !size) || (kind >= 2 && !count) || (!managed && !size) || (!managed && !buffer && capacity)) return -1;
    clm_bpaa_array_node* array;
    if ((kind == 0 && !clm_path_internal_text(destination)) || (kind == 2 && clm_string_array_get_node(destination, &array) != 0)) return -1;
    size_t length, decoded_length;
    unsigned char* bytes = clm_file_internal_read_all(path, &length);
    char* text = bytes ? clm_file_internal_decode(bytes, length, encoding, &decoded_length) : NULL;
    free(bytes);
    if (!text) return -1;
    if (kind == 0) {
        const int status = clm_file_internal_string(destination, text, decoded_length);
        free(text); if (!status) *size = decoded_length;
        return status;
    }
    size_t required = decoded_length + 1;
    clm_ulong lines = 0;
    if (kind >= 2) {
        char* packed = clm_file_internal_lines(text, decoded_length, &required, &lines);
        free(text); text = packed;
        if (!text) return -1;
        if (kind == 2) {
            const int status = clm_file_internal_array(destination, text, required, lines);
            free(text); if (!status) *count = lines;
            return status;
        }
    }
    if (!buffer || capacity < required) {
        free(text); *size = required; if (kind == 3) *count = lines;
        return !buffer && !capacity ? 0 : -1;
    }
    memcpy(buffer, text, required); free(text); *size = required;
    if (kind == 3) *count = lines;
    return 0;
}

static inline int clm_file_internal_content_write(const char* path, clm_id content, size_t size, int encoding, int append, int line)
{
    if (encoding < 0) {
        clm_bpa_block_node* block = clm_file_internal_binary(content, size);
        return block ? clm_file_internal_write_all(path, block->data, size, append) : -1;
    }
    const char* text = clm_path_internal_text(content);
    return text && size <= strlen(text) ? clm_file_internal_text_write(path, encoding, text, size, append, line) : -1;
}

static inline int clm_file_internal_predicate(const char* path, int directory)
{
    clm_file_info info;
    if (clm_file_internal_info(path, true, &info)) return -1;
    return directory ? info.path.is_directory : info.path.is_file;
}

static inline int clm_file_internal_time(const char* path, time_t* out, unsigned field)
{
    clm_file_info info;
    if (!out || clm_file_internal_info(path, true, &info) || !info.path.exists) return -1;
    if (!(info.supported_times & field)) return -2;
    *out = field == 1 ? info.creation_time : field == 2 ? info.modification_time : info.access_time;
    return 0;
}

static inline int clm_file_internal_copy_file(const char* source, const char* destination, bool overwrite)
{
    clm_file_internal_handle input = clm_file_internal_open(source, CLM_FILE_OPEN_MODE_READ);
    if (input == CLM_FILE_INTERNAL_INVALID) return -1;
    clm_file_internal_handle output = clm_file_internal_open(destination, CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE |
        (overwrite ? 0 : CLM_FILE_OPEN_MODE_EXCLUSIVE));
    if (output == CLM_FILE_INTERNAL_INVALID) { clm_file_internal_close(input); return -1; }
    int status = 0;
#if defined(_WIN32)
    BY_HANDLE_FILE_INFORMATION first, second;
    if (!GetFileInformationByHandle(input, &first) || !GetFileInformationByHandle(output, &second) ||
        (first.dwVolumeSerialNumber == second.dwVolumeSerialNumber && first.nFileIndexHigh == second.nFileIndexHigh && first.nFileIndexLow == second.nFileIndexLow)) status = -1;
#else
    struct stat first, second;
    if (fstat(input, &first) || fstat(output, &second) || (first.st_dev == second.st_dev && first.st_ino == second.st_ino)) status = -1;
#endif
    if (!status) status = clm_file_internal_truncate(output, 0);
    unsigned char buffer[16384];
    while (!status) {
        size_t read_count, write_count;
        status = clm_file_internal_transfer(input, buffer, sizeof(buffer), 0, &read_count);
        if (!status && read_count) status = clm_file_internal_transfer(output, buffer, read_count, 1, &write_count);
        if (status || read_count < sizeof(buffer)) break;
    }
    if (clm_file_internal_close(input)) status = -1;
    if (clm_file_internal_close(output)) status = -1;
    return status;
}

static inline int clm_file_internal_move(const char* source, const char* destination, bool replace)
{
    if (!clm_file_internal_valid_path(source) || !clm_file_internal_valid_path(destination)) return -1;
#if defined(_WIN32)
    wchar_t* first = clm_path_internal_wide(source);
    wchar_t* second = clm_path_internal_wide(destination);
    const int status = first && second && MoveFileExW(first, second, replace ? MOVEFILE_REPLACE_EXISTING : 0) ? 0 : -1;
    free(first); free(second); return status;
#else
    if (replace) return rename(source, destination) == 0 ? 0 : -1;
#if defined(__linux__) && defined(RENAME_NOREPLACE)
    return syscall(SYS_renameat2, AT_FDCWD, source, AT_FDCWD, destination, RENAME_NOREPLACE) == 0 ? 0 : -1;
#else
    return -2;
#endif
#endif
}

static inline int clm_file_internal_positioned(clm_id id, clm_ulong offset, void* content, size_t size,
                                               size_t* transferred, bool writing)
{
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !transferred || (!content && size) ||
        offset > (clm_ulong)LONG_MAX || size > (clm_ulong)LONG_MAX - offset ||
        (writing ? !(node->mode & CLM_FILE_OPEN_MODE_WRITE) || (node->mode & CLM_FILE_OPEN_MODE_APPEND) : !(node->mode & CLM_FILE_OPEN_MODE_READ))) return -1;
#if defined(_WIN32)
    HANDLE independent = ReOpenFile(node->handle, writing ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (independent == INVALID_HANDLE_VALUE) return -1;
    int status = clm_file_internal_seek(independent, (clm_long)offset, 0, NULL);
    if (!status) status = clm_file_internal_transfer(independent, content, size, writing, transferred);
    if (clm_file_internal_close(independent)) status = -1;
    return status;
#else
    if ((clm_ulong)(off_t)offset != offset) return -2;
    size_t total = 0;
    while (total < size) {
        const size_t chunk = size - total > 0x40000000u ? 0x40000000u : size - total;
        const ssize_t count = writing ? pwrite(node->handle, (char*)content + total, chunk, (off_t)(offset + total))
                                     : pread(node->handle, (char*)content + total, chunk, (off_t)(offset + total));
        if (count < 0) { if (errno == EINTR) continue; *transferred = total; return -1; }
        if (!count) { *transferred = total; return writing ? -1 : 0; }
        total += (size_t)count;
    }
    *transferred = total;
    return 0;
#endif
}

static inline int clm_file_internal_temporary(clm_id id, const char* base, clm_id destination,
                                              char* buffer, size_t capacity, size_t* required, bool managed)
{
    clm_file_info info;
    if (clm_file_internal_find(id) || clm_file_internal_info(base, false, &info) || !info.path.is_directory ||
        (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT) || (managed ? !clm_path_internal_text(destination) : !required || (!buffer && capacity))) return -1;
    const size_t needed = strlen(base) + 42;
    if (!managed && (!buffer || capacity < needed)) { *required = needed; return !buffer && !capacity ? 0 : -1; }
    for (int attempt = 0; attempt < 64; ++attempt) {
        char* path = clm_file_internal_temp_name(base, ".tmp");
        if (!path) return -1;
        const int status = clm_file_internal_register(id, path, CLM_FILE_OPEN_MODE_READ | CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE | CLM_FILE_OPEN_MODE_EXCLUSIVE);
        if (!status) {
            if (managed && clm_path_internal_store(destination, path)) {
                clm_file_internal_node* node = clm_file_internal_registry;
                clm_file_internal_registry = node->next; clm_file_internal_close(node->handle); free(node);
                clm_file_internal_unlink(path); free(path); return -1;
            }
            if (!managed) { memcpy(buffer, path, strlen(path) + 1); *required = strlen(path) + 1; }
            free(path); return 0;
        }
        clm_file_info collision;
        const int retry = !clm_file_internal_info(path, false, &collision) && collision.path.exists;
        free(path);
        if (!retry) return -1;
    }
    return -1;
}

static inline int clm_file_internal_atomic(const char* path, const void* content, size_t size)
{
    if (!clm_file_internal_valid_path(path) || (!content && size)) return -1;
    clm_file_info info;
    if (clm_file_internal_info(path, false, &info) || (info.path.exists && (!info.path.is_file || (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)))) return -1;
    char* parent = clm_file_internal_parent(path);
    if (!parent) return -1;
    int status = -1;
    for (int attempt = 0; attempt < 64; ++attempt) {
        char* sibling = clm_file_internal_temp_name(parent, ".tmp");
        if (!sibling) break;
        clm_file_internal_handle handle = clm_file_internal_open(sibling, CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE | CLM_FILE_OPEN_MODE_EXCLUSIVE);
        if (handle != CLM_FILE_INTERNAL_INVALID) {
            size_t written;
            status = clm_file_internal_transfer(handle, (void*)content, size, 1, &written);
            if (clm_file_internal_close(handle)) status = -1;
            if (!status) status = clm_file_internal_move(sibling, path, true);
            if (status) clm_file_internal_unlink(sibling);
            free(sibling); break;
        }
        clm_file_info collision;
        const int retry = !clm_file_internal_info(sibling, false, &collision) && collision.path.exists;
        free(sibling); if (!retry) break;
    }
    free(parent);
    return status;
}

/** Test whether a path resolves to an existing regular file, following links.
 * @param path ID of the UTF-8 file path string, not an open-file ID.
 * @return 1 for a file, 0 for absent/non-file, -1 for query errors.
 */
static inline clm_int clm_file_exists(clm_id path) {
    return clm_file_internal_predicate(clm_path_internal_text(path), 0);
}

/** Test regular-file existence using a raw UTF-8 path.
 * @param path NUL-terminated path; a directory or dangling link is not an existing file.
 * @return 1 for a file, 0 for absent/non-file, -1 for query errors.
 */
static inline clm_int clm_file_exists_as(const char* path) {
    return clm_file_internal_predicate(path, 0);
}

/** Read whole-file binary bytes into an existing fixed-capacity BPA block.
 * @param path ID of the source file path string.
 * @param[out] content ID of a writable binary block; insufficient capacity is an error.
 * @param[out] size Non-NULL number of bytes read, assigned only on complete success.
 * @return Operation status; no text decoding or implicit block resizing.
 */
static inline clm_int clm_file_read_all(clm_id path, clm_id content, size_t* size) {
    return clm_file_internal_binary_read(clm_path_internal_text(path), content, NULL, 0, size, true);
}

/** Read an entire binary file using a raw UTF-8 path and BPA destination.
 * @param path NUL-terminated source file path.
 * @param[out] content ID of an existing writable binary block; output is not text.
 * @param[out] size Non-NULL complete byte count; outputs are unchanged on failure.
 * @return Operation status; internally opened resources must be closed.
 */
static inline clm_int clm_file_read_all_as(const char* path, clm_id content, size_t* size) {
    return clm_file_internal_binary_read(path, content, NULL, 0, size, true);
}

/** Read whole-file binary bytes into caller-owned memory without truncation.
 * @param path NUL-terminated UTF-8 source path.
 * @param[out] content Writable binary buffer; NULL with zero capacity measures only.
 * @param capacity Available buffer bytes; binary data has no added terminator.
 * @param[out] size Mandatory actual byte count, or required capacity on insufficient capacity/size query.
 * @return Operation status; an insufficient buffer must remain unchanged.
 * @note Files may change between size queries and reads; no filesystem snapshot is promised.
 */
static inline clm_int clm_file_read_all_to(const char* path, void* content, size_t capacity, size_t* size) {
    return clm_file_internal_binary_read(path, 0, content, capacity, size, false);
}

/** Decode a complete text file into an existing CLM UTF-8 string.
 * @param path ID of the source file path string.
 * @param encoding CLM_FILE_TEXT_ENCODING_* source representation and BOM policy.
 * @param[out] content ID of an existing string destination, not a binary block.
 * @param[out] size Non-NULL decoded UTF-8 byte length excluding NUL, not encoded file size.
 * @return Operation status; malformed input and embedded NUL are errors.
 */
static inline clm_int clm_file_read_all_text(clm_id path, clm_int encoding, clm_id content, size_t* size) {
    return clm_file_internal_text_read(clm_path_internal_text(path), encoding, content, NULL, 0, size, NULL, 0);
}

/** Decode whole-file text using a raw UTF-8 source path.
 * @param path NUL-terminated source path.
 * @param encoding Declared file encoding; BOM variants require the matching BOM.
 * @param[out] content ID of an existing CLM string receiving UTF-8 text without a BOM.
 * @param[out] size Non-NULL decoded UTF-8 byte count excluding NUL.
 * @return Operation status; publish outputs only after successful decoding.
 */
static inline clm_int clm_file_read_all_text_as(const char* path, clm_int encoding, clm_id content, size_t* size) {
    return clm_file_internal_text_read(path, encoding, content, NULL, 0, size, NULL, 0);
}

/** Decode a text file into caller-owned NUL-terminated UTF-8 memory.
 * @param path NUL-terminated UTF-8 source path.
 * @param encoding Source-file encoding; the output buffer is always UTF-8.
 * @param[out] content Writable text buffer; NULL with zero capacity measures only.
 * @param capacity Byte capacity including room for the UTF-8 NUL terminator.
 * @param[out] required_size Mandatory required byte capacity including NUL, also on insufficient capacity.
 * @return Operation status; no partial characters or truncated output.
 */
static inline clm_int clm_file_read_all_text_to(const char* path, clm_int encoding, char* content, size_t capacity, size_t* required_size) {
    return clm_file_internal_text_read(path, encoding, 0, content, capacity, required_size, NULL, 1);
}

/** Decode text lines into an existing CLM string array, excluding line delimiters.
 * @param path ID of the source file path string.
 * @param encoding Source-file encoding and BOM policy.
 * @param[out] content ID of an existing output string array, replaced only on success.
 * @param[out] line_count Non-NULL number of decoded lines; an empty file has zero lines.
 * @return Operation status; recognize CRLF, LF, and CR without a phantom final line.
 */
static inline clm_int clm_file_read_all_text_lines(clm_id path, clm_int encoding, clm_id content, clm_ulong* line_count) {
    return clm_file_internal_text_read(clm_path_internal_text(path), encoding, content, NULL, 0, NULL, line_count, 2);
}

/** Read decoded lines using a raw UTF-8 source path.
 * @param path NUL-terminated source file path.
 * @param encoding File encoding; decoded lines are UTF-8 strings without delimiters.
 * @param[out] content ID of an existing output string array.
 * @param[out] line_count Non-NULL successful line count; preserve blank interior lines.
 * @return Operation status; failures do not publish partial arrays.
 */
static inline clm_int clm_file_read_all_text_lines_as(const char* path, clm_int encoding, clm_id content, clm_ulong* line_count) {
    return clm_file_internal_text_read(path, encoding, content, NULL, 0, NULL, line_count, 2);
}

/** Export decoded lines as NUL-separated UTF-8 strings with one extra final NUL.
 * @param path NUL-terminated UTF-8 source path.
 * @param encoding Source-file encoding and BOM policy.
 * @param[out] content Writable packed buffer; NULL with zero capacity measures only.
 * @param capacity Byte capacity including every terminator.
 * @param[out] required_size Mandatory required capacity; report it on insufficient capacity.
 * @param[out] line_count Mandatory line count; disambiguates blank lines from the final sentinel.
 * @return Operation status; empty lists require one NUL byte, never truncate.
 */
static inline clm_int clm_file_read_all_text_lines_to(const char* path, clm_int encoding, char* content, size_t capacity, size_t* required_size, clm_ulong* line_count) {
    return clm_file_internal_text_read(path, encoding, 0, content, capacity, required_size, line_count, 3);
}

/** Create or truncate a regular file and write exactly the requested binary bytes.
 * @param path ID of the destination path string, not an open-file ID.
 * @param content ID of a binary BPA source block; size must fit its capacity.
 * @param size Number of bytes to write; no text encoding or terminator processing.
 * @return Operation status; filesystem failures may leave partial output.
 * @note This is not atomic replacement; use write_all_atomic when that contract is required.
 */
static inline clm_int clm_file_write_all(clm_id path, clm_id content, size_t size) {
    return clm_file_internal_content_write(clm_path_internal_text(path), content, size, -1, 0, 0);
}

/** Write whole-file binary contents using a raw UTF-8 destination and BPA source.
 * @param path NUL-terminated destination path; create or truncate the file.
 * @param content ID of an existing binary source block.
 * @param size Exact source byte count; zero creates/truncates an empty file.
 * @return Operation status; the whole-file helper owns and closes its native resource.
 */
static inline clm_int clm_file_write_all_as(const char* path, clm_id content, size_t size) {
    return clm_file_internal_content_write(path, content, size, -1, 0, 0);
}

/** Write a whole binary file from caller-owned bytes.
 * @param path NUL-terminated UTF-8 destination path; create or truncate.
 * @param content Read-only source memory; NULL permitted only when size is zero.
 * @param size Number of readable source bytes.
 * @return Operation status; no registered open-file ID is created.
 */
static inline clm_int clm_file_write_all_from(const char* path, const void* content, size_t size) {
    return clm_file_internal_write_all(path, content, size, 0);
}

/** Encode in-memory UTF-8 text into a newly created or truncated text file.
 * @param path ID of the destination path string.
 * @param encoding Target-file encoding and BOM policy.
 * @param content ID of a UTF-8 string source; not an already UTF-16 encoded block.
 * @param size Number of UTF-8 source bytes excluding NUL; must end on a code-point boundary.
 * @return Operation status; conversion errors must be checked before truncation.
 */
static inline clm_int clm_file_write_all_text(clm_id path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(clm_path_internal_text(path), content, size, encoding, 0, 0) : -1;
}

/** Write encoded whole-file text using a raw UTF-8 destination path.
 * @param path NUL-terminated destination path.
 * @param encoding Target file representation; BOM variants emit the matching BOM.
 * @param content ID of a CLM UTF-8 source string.
 * @param size UTF-8 source byte count, not the resulting encoded byte count.
 * @return Operation status; rejects invalid UTF-8, embedded NUL, and unsupported encodings.
 */
static inline clm_int clm_file_write_all_text_as(const char* path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(path, content, size, encoding, 0, 0) : -1;
}

/** Encode caller-owned UTF-8 bytes and write a complete text file.
 * @param path NUL-terminated UTF-8 destination path; create or truncate.
 * @param encoding Target-file encoding and BOM policy.
 * @param content Read-only UTF-8 source; NUL termination is not required within size.
 * @param size Exact UTF-8 source byte count excluding a terminator.
 * @return Operation status; source remains caller-owned and is not modified.
 */
static inline clm_int clm_file_write_all_text_from(const char* path, clm_int encoding, const char* content, size_t size) {
    return clm_file_internal_text_write(path, encoding, content, size, 0, 0);
}

/** Append binary bytes by path, opening and closing internally.
 * @param path ID of the destination file path string; create if absent.
 * @param content ID of a binary source BPA block.
 * @param size Exact source byte count; no encoding or newline processing.
 * @return Operation status; does not use or alter a registered open-file cursor.
 */
static inline clm_int clm_file_append(clm_id path, clm_id content, size_t size) {
    return clm_file_internal_content_write(clm_path_internal_text(path), content, size, -1, 1, 0);
}

/** Append whole-call binary bytes using a raw UTF-8 path and BPA source.
 * @param path NUL-terminated destination path; create if absent.
 * @param content ID of an existing binary source block.
 * @param size Number of readable bytes bounded by source capacity.
 * @return Operation status; no registered handle is created.
 */
static inline clm_int clm_file_append_as(const char* path, clm_id content, size_t size) {
    return clm_file_internal_content_write(path, content, size, -1, 1, 0);
}

/** Append caller-owned binary data using a raw UTF-8 path.
 * @param path NUL-terminated destination path.
 * @param content Read-only binary input; NULL only for a zero-byte transfer.
 * @param size Exact source byte count.
 * @return Operation status; concurrent whole-record atomicity is not promised.
 */
static inline clm_int clm_file_append_from(const char* path, const void* content, size_t size) {
    return clm_file_internal_write_all(path, content, size, 1);
}

/** Encode UTF-8 text and append it by path without inserting a mid-file BOM.
 * @param path ID of the destination text file path string.
 * @param encoding Target-file representation; existing BOM/encoding must be compatible.
 * @param content ID of the UTF-8 string source.
 * @param size Exact UTF-8 source byte count ending at a code-point boundary.
 * @return Operation status; create absent files using the selected BOM policy.
 */
static inline clm_int clm_file_append_text(clm_id path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(clm_path_internal_text(path), content, size, encoding, 1, 0) : -1;
}

/** Append encoded text using a raw UTF-8 destination and a CLM string source.
 * @param path NUL-terminated destination file path.
 * @param encoding Target-file encoding and compatible BOM policy.
 * @param content ID of a UTF-8 source string; not already encoded file bytes.
 * @param size UTF-8 source byte count excluding NUL.
 * @return Operation status; does not append an implicit newline.
 */
static inline clm_int clm_file_append_text_as(const char* path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(path, content, size, encoding, 1, 0) : -1;
}

/** Append encoded text from caller-owned UTF-8 memory.
 * @param path NUL-terminated UTF-8 destination path.
 * @param encoding Target file encoding; incompatible existing BOMs are errors.
 * @param content Read-only UTF-8 source buffer.
 * @param size Source bytes excluding NUL; no terminator is required within the bound.
 * @return Operation status; invalid text must be rejected before writing.
 */
static inline clm_int clm_file_append_text_from(const char* path, clm_int encoding, const char* content, size_t size) {
    return clm_file_internal_text_write(path, encoding, content, size, 1, 0);
}

/** Append encoded UTF-8 text followed by exactly one LF line terminator.
 * @param path ID of the destination file path string.
 * @param encoding Target-file encoding; LF is encoded in the same representation.
 * @param content ID of the UTF-8 text source.
 * @param size Exact UTF-8 source bytes; an existing trailing newline is not removed.
 * @return Operation status; append processing never inserts a BOM mid-file.
 */
static inline clm_int clm_file_append_line_text(clm_id path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(clm_path_internal_text(path), content, size, encoding, 1, 1) : -1;
}

/** Append one LF-terminated text record using a raw UTF-8 path.
 * @param path NUL-terminated destination path.
 * @param encoding Target-file encoding and BOM policy.
 * @param content ID of the UTF-8 text source.
 * @param size Number of UTF-8 source bytes excluding NUL.
 * @return Operation status; multi-write record atomicity is not promised.
 */
static inline clm_int clm_file_append_line_text_as(const char* path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(path, content, size, encoding, 1, 1) : -1;
}

/** Append caller-owned UTF-8 text and exactly one encoded LF.
 * @param path NUL-terminated UTF-8 destination path.
 * @param encoding Target-file representation.
 * @param content Read-only UTF-8 source buffer.
 * @param size Exact source byte count; excludes the LF added by this helper.
 * @return Operation status; validates source text and existing encoding first.
 */
static inline clm_int clm_file_append_line_text_from(const char* path, clm_int encoding, const char* content, size_t size) {
    return clm_file_internal_text_write(path, encoding, content, size, 1, 1);
}

/** Open a file and register its resource under a caller-selected unused ID.
 * @param id Unused open-file ID; existing registrations must not be overwritten.
 * @param path ID of the UTF-8 file path string; separate from the open-file identity.
 * @param mode Access flags plus optional CREATE/TRUNCATE/EXCLUSIVE disposition bits.
 * @return Operation status; exclusive requires create, truncation requires write.
 * @note No access flags is invalid; append implies write and excludes truncation.
 */
static inline clm_int clm_file_open(clm_id id, clm_id path, clm_int mode) {
    return clm_file_internal_register(id, clm_path_internal_text(path), mode);
}

/** Open a raw UTF-8 path under an explicit caller-selected open-file ID.
 * @param id Unused open-file ID to register only after native open succeeds.
 * @param path NUL-terminated file path; retained path text is not the handle identity.
 * @param mode CLM_FILE_OPEN_MODE_* access/disposition flags.
 * @return Operation status; do not leak a native resource if registration fails.
 */
static inline clm_int clm_file_open_as(clm_id id, const char* path, clm_int mode) {
    return clm_file_internal_register(id, path, mode);
}

/** Read up to a bounded number of bytes through an open-file cursor.
 * @param id Readable open-file ID, not a file path ID.
 * @param[out] content ID of an existing writable binary block with at least size bytes.
 * @param size Maximum number of bytes to read; no terminator is added.
 * @param[out] bytes_read Non-NULL actual byte count; zero at EOF is a successful read.
 * @return Operation status; advance by bytes transferred, not by the requested count.
 * @note Partial I/O error output/count semantics must be defined before implementation.
 */
static inline clm_int clm_file_read(clm_id id, clm_id content, size_t size, size_t* bytes_read) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    if (!node || !block || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_READ)) return -1;
    const int status = clm_file_internal_transfer(node->handle, block->data, size, 0, bytes_read);
    if (!status && size) node->eof = *bytes_read < size;
    return status;
}

/** Read through an open-file cursor into caller-owned binary memory.
 * @param id Readable open-file ID.
 * @param[out] content Writable memory with at least size bytes; NULL only for zero size.
 * @param size Maximum transfer size and destination capacity in bytes.
 * @param[out] bytes_read Non-NULL actual transfer count; short reads are permitted.
 * @return Operation status; distinguish EOF from I/O failure.
 */
static inline clm_int clm_file_read_to(clm_id id, void* content, size_t size, size_t* bytes_read) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_READ)) return -1;
    const int status = clm_file_internal_transfer(node->handle, content, size, 0, bytes_read);
    if (!status && size) node->eof = *bytes_read < size;
    return status;
}

/** Write bytes at the current open-file cursor from a BPA binary block.
 * @param id Writable open-file ID; append mode uses native append positioning.
 * @param content ID of a binary source block with at least size readable bytes.
 * @param size Requested byte count; no encoding or newline conversion.
 * @param[out] bytes_written Non-NULL actual transfer count, including partial I/O progress.
 * @return Operation status; cursor follows bytes written and failures do not roll back.
 */
static inline clm_int clm_file_write(clm_id id, clm_id content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    if (!node || !block || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) return -1;
    return clm_file_internal_transfer(node->handle, block->data, size, 1, bytes_written);
}

/** Write caller-owned binary bytes through the current open-file cursor.
 * @param id Writable open-file ID.
 * @param content Read-only source memory; NULL permitted only for a zero-byte write.
 * @param size Number of readable source bytes.
 * @param[out] bytes_written Non-NULL actual count, including partial progress on error.
 * @return Operation status; validate access mode before modifying the file.
 */
static inline clm_int clm_file_write_from(clm_id id, const void* content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) return -1;
    return clm_file_internal_transfer(node->handle, (void*)content, size, 1, bytes_written);
}

/** Append binary bytes through an open resource registered in append mode.
 * @param id Append-mode open-file ID; not a path string reference.
 * @param content ID of a readable binary source block.
 * @param size Requested byte count, bounded by the source block capacity.
 * @param[out] bytes_written Non-NULL actual byte count; cursor follows the appended bytes.
 * @return Operation status; separate calls need not form an atomic multi-write record.
 */
static inline clm_int clm_file_append_open(clm_id id, clm_id content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    if (!node || !block || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_APPEND)) return -1;
    return clm_file_internal_transfer(node->handle, block->data, size, 1, bytes_written);
}

/** Append caller-owned binary bytes through an append-mode open-file ID.
 * @param id Append-mode open-file ID.
 * @param content Read-only source memory with at least size readable bytes.
 * @param size Requested byte count, independent of text encoding.
 * @param[out] bytes_written Non-NULL actual transfer count, including partial progress.
 * @return Operation status; appends occur at native EOF, not a stale cached size.
 */
static inline clm_int clm_file_append_open_from(clm_id id, const void* content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_APPEND)) return -1;
    return clm_file_internal_transfer(node->handle, (void*)content, size, 1, bytes_written);
}

/** Query a seekable open-file resource's current absolute byte position.
 * @param id Open-file ID, not the original path ID.
 * @param[out] index Non-NULL 64-bit position result, assigned only on success.
 * @return Operation status; non-seekable resources return an explicit error.
 */
static inline clm_int clm_file_tell(clm_id id, clm_ulong* index) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID && index ? clm_file_internal_seek(node->handle, 0, 1, index) : -1;
}

/** Close the resource owned by a registered open-file ID.
 * @param id Open-file ID; closing by path is intentionally not supported.
 * @return Operation status; report late flush/close errors.
 * @note The implemented close policy must retire a consumed native handle even when close reports an error.
 */
static inline clm_int clm_file_close(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID) return -1;
    const clm_file_internal_handle handle = node->handle;
    node->handle = CLM_FILE_INTERNAL_INVALID;
    return clm_file_internal_close(handle);
}

/** Query logical regular-file byte size by path, not through an open handle.
 * @param path ID of the file path string; use get_open_size for open-file IDs.
 * @param[out] size Non-NULL unsigned 64-bit result; not allocated disk usage.
 * @return Operation status; output is unchanged on failure.
 */
static inline clm_int clm_file_get_size(clm_id path, clm_ulong* size) {
    clm_file_info info;
    if (!size || clm_file_internal_info(clm_path_internal_text(path), true, &info) || !info.path.is_file) return -1;
    *size = info.size; return 0;
}

/** Query logical file size using a raw UTF-8 path.
 * @param path NUL-terminated regular-file path; no open-file ID is required.
 * @param[out] size Non-NULL 64-bit byte count, avoiding platform size_t truncation.
 * @return Operation status; missing or non-file inputs are errors.
 */
static inline clm_int clm_file_get_size_as(const char* path, clm_ulong* size) {
    clm_file_info info;
    if (!size || clm_file_internal_info(path, true, &info) || !info.path.is_file) return -1;
    *size = info.size; return 0;
}

/** Query actual file creation time when supported by the filesystem.
 * @param id ID of the file path string, not an open-file ID.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; unavailable birth time returns UNSUPPORTED, never Unix ctime.
 */
static inline clm_int clm_file_get_creation_time(clm_id id, time_t* out) {
    return clm_file_internal_time(clm_path_internal_text(id), out, 1);
}

/** Query creation time through a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL timestamp output with native time_t precision.
 * @return Operation status; unsupported metadata is distinct from a zero timestamp.
 */
static inline clm_int clm_file_get_creation_time_as(const char* path, time_t* out) {
    return clm_file_internal_time(path, out, 1);
}

/** Query the filesystem-reported last modification timestamp.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL native timestamp result, unchanged on failure.
 * @return Operation status; no assumption of nanosecond precision.
 */
static inline clm_int clm_file_get_modification_time(clm_id id, time_t* out) {
    return clm_file_internal_time(clm_path_internal_text(id), out, 2);
}

/** Query last modification time using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; missing entries and query errors must be distinguished.
 */
static inline clm_int clm_file_get_modification_time_as(const char* path, time_t* out) {
    return clm_file_internal_time(path, out, 2);
}

/** Query a filesystem-reported access timestamp, which may be deferred or disabled.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL native timestamp result.
 * @return Operation status; not a guarantee of recent application access.
 */
static inline clm_int clm_file_get_access_time(clm_id id, time_t* out) {
    return clm_file_internal_time(clm_path_internal_text(id), out, 4);
}

/** Query access time through a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL native timestamp result, assigned only on success.
 * @return Operation status; respect native mount/access-time policies.
 */
static inline clm_int clm_file_get_access_time_as(const char* path, time_t* out) {
    return clm_file_internal_time(path, out, 4);
}

/** Query hidden-file classification under the native Path metadata policy.
 * @param id ID of the file path string; inspect the entry without following a link.
 * @param[out] out Non-NULL Boolean result.
 * @return Operation status; native attributes or POSIX dot naming apply.
 */
static inline clm_int clm_file_is_hidden(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_HIDDEN, out);
}

/** Query native hidden classification using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result, unchanged on failure.
 * @return Operation status; missing entries are not silently false.
 */
static inline clm_int clm_file_is_hidden_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_HIDDEN, out);
}

/** Inspect the native read-only attribute, not effective write-access permissions.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unavailable native attributes return UNSUPPORTED.
 */
static inline clm_int clm_file_is_read_only(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_READ_ONLY, out);
}

/** Inspect a read-only attribute through a raw UTF-8 path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result; ACLs and native access checks are separate.
 * @return Operation status; unsupported concepts remain distinct from false.
 */
static inline clm_int clm_file_is_read_only_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_READ_ONLY, out);
}

/** Inspect native filesystem compression metadata, not a file format signature.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unsupported compression metadata returns UNSUPPORTED.
 */
static inline clm_int clm_file_is_compressed(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_COMPRESSED, out);
}

/** Inspect compression metadata using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result; unrelated to ZIP or other encoded file formats.
 * @return Operation status; native capability limitations are explicit.
 */
static inline clm_int clm_file_is_compressed_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_COMPRESSED, out);
}

/** Inspect the native archive attribute, not whether the file contains an archive.
 * @param id ID of the file path string.
 * @param[out] out Non-NULL Boolean attribute result.
 * @return Operation status; unsupported platform attributes are explicit.
 */
static inline clm_int clm_file_is_archive(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_ARCHIVE, out);
}

/** Inspect an archive attribute using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param[out] out Non-NULL Boolean result; not an archive-file decoder.
 * @return Operation status; unsupported metadata must not be reported as false.
 */
static inline clm_int clm_file_is_archive_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_ARCHIVE, out);
}

/** Exclusively create a new empty regular file, then close it.
 * @param path ID of the target path string; existing entries are errors.
 * @return Operation status; no parent creation or open-file registration.
 */
static inline clm_int clm_file_create(clm_id path) {
    clm_file_internal_handle handle = clm_file_internal_open(clm_path_internal_text(path), CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE | CLM_FILE_OPEN_MODE_EXCLUSIVE);
    return handle != CLM_FILE_INTERNAL_INVALID ? clm_file_internal_close(handle) : -1;
}

/** Exclusively create and close a new file using a raw UTF-8 path.
 * @param path NUL-terminated target path; the caller retains ownership.
 * @return Operation status; existing entries and missing parents fail.
 */
static inline clm_int clm_file_create_as(const char* path) {
    clm_file_internal_handle handle = clm_file_internal_open(path, CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE | CLM_FILE_OPEN_MODE_EXCLUSIVE);
    return handle != CLM_FILE_INTERNAL_INVALID ? clm_file_internal_close(handle) : -1;
}

/** Copy regular-file contents to another path without following an unexpected destination link.
 * @param source ID of the source file path string.
 * @param destination ID of the destination path string, not an output string.
 * @param overwrite Whether an existing regular-file destination may be replaced.
 * @return Operation status; identical file identities must be rejected.
 * @note Metadata preservation and cleanup of a partially copied destination require explicit policy.
 */
static inline clm_int clm_file_copy(clm_id source, clm_id destination, bool overwrite) {
    return clm_file_internal_copy_file(clm_path_internal_text(source), clm_path_internal_text(destination), overwrite);
}

/** Copy a regular file using raw UTF-8 source and destination paths.
 * @param source NUL-terminated source path.
 * @param destination NUL-terminated destination path; not a writable output buffer.
 * @param overwrite Whether an existing regular file may be replaced; directories are errors.
 * @return Operation status; never silently copy a file onto itself.
 */
static inline clm_int clm_file_copy_as(const char* source, const char* destination, bool overwrite) {
    return clm_file_internal_copy_file(source, destination, overwrite);
}

/** Move or rename a file without replacing an existing destination.
 * @param source ID of the existing source file path string.
 * @param destination ID of the destination path string; neither ID is remapped.
 * @return Operation status; cross-filesystem moves fail instead of implicit copy/delete.
 */
static inline clm_int clm_file_move(clm_id source, clm_id destination) {
    const char* path = clm_path_internal_text(source);
    clm_file_info info;
    return !clm_file_internal_info(path, false, &info) && (info.path.is_file || info.path.is_symlink)
        ? clm_file_internal_move(path, clm_path_internal_text(destination), false) : -1;
}

/** Move or rename a file using raw UTF-8 paths.
 * @param source NUL-terminated source file path.
 * @param destination NUL-terminated destination path; existing entries are errors.
 * @return Operation status; no implicit overwrite or parent creation.
 */
static inline clm_int clm_file_move_as(const char* source, const char* destination) {
    clm_file_info info;
    return !clm_file_internal_info(source, false, &info) && (info.path.is_file || info.path.is_symlink)
        ? clm_file_internal_move(source, destination, false) : -1;
}

/** Delete a file entry; remove a symbolic link itself rather than its target.
 * @param path ID of the file or file-link path string; directories are rejected.
 * @return Operation status; missing-entry policy must be explicit.
 */
static inline clm_int clm_file_delete(clm_id path) {
    return clm_file_internal_unlink(clm_path_internal_text(path));
}

/** Delete a file entry using a raw UTF-8 path.
 * @param path NUL-terminated file or file-link path; never a directory tree.
 * @return Operation status; do not close unrelated open-file IDs.
 */
static inline clm_int clm_file_delete_as(const char* path) {
    return clm_file_internal_unlink(path);
}

/** Test whether an open-file ID is registered and currently owns a native resource.
 * @param id Open-file ID, not a path-string ID.
 * @return 1 if open, 0 if unregistered/closed, -1 on registry errors.
 */
static inline clm_int clm_file_is_open(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID;
}

/** Move the open-file cursor using a signed 64-bit offset.
 * @param id Open-file ID with seek support.
 * @param offset Signed displacement relative to origin; overflow and negative results fail.
 * @param origin CLM_FILE_SEEK_BEGIN, CLM_FILE_SEEK_CURRENT, or CLM_FILE_SEEK_END.
 * @return Operation status; seeking beyond EOF does not itself extend the file.
 */
static inline clm_int clm_file_seek(clm_id id, clm_long offset, clm_int origin) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID) return -1;
    const int status = clm_file_internal_seek(node->handle, offset, origin, NULL);
    if (!status) node->eof = false;
    return status;
}

/** Inspect end-of-file status observed by the previous sequential read.
 * @param id Readable open-file ID.
 * @param[out] out Non-NULL Boolean EOF result; not a prediction of future file growth.
 * @return Operation status; I/O errors must not be reported as EOF.
 */
static inline clm_int clm_file_eof(clm_id id, bool* out) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !out || !(node->mode & CLM_FILE_OPEN_MODE_READ)) return -1;
    *out = node->eof; return 0;
}

/** Flush user-space write buffers to the operating system without promising durability.
 * @param id Writable open-file ID.
 * @return Operation status; late buffered-write failures must be reported.
 */
static inline clm_int clm_file_flush(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID && (node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND)) ? 0 : -1;
}

/** Request native durable synchronization of an open file after flushing buffers.
 * @param id Writable open-file ID; the handle remains open.
 * @return Operation status; native durability limitations or lack of support are explicit.
 * @note File synchronization alone does not guarantee directory-entry durability.
 */
static inline clm_int clm_file_sync(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) return -1;
#if defined(_WIN32)
    return FlushFileBuffers(node->handle) ? 0 : -1;
#else
    return fsync(node->handle) == 0 ? 0 : -1;
#endif
}

/** Query current logical size through an open resource rather than its original path.
 * @param id Open-file ID; renaming its path must not invalidate this query.
 * @param[out] size Non-NULL 64-bit byte count, assigned only on success.
 * @return Operation status; do not rely on a stale cached size.
 */
static inline clm_int clm_file_get_open_size(clm_id id, clm_ulong* size) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID ? clm_file_internal_size(node->handle, size) : -1;
}

/** Set logical length through a writable open-file resource.
 * @param id Writable open-file ID; append-only restrictions must be checked.
 * @param size New 64-bit logical byte length; native representability is validated.
 * @return Operation status; cursor position remains unchanged, even if beyond new EOF.
 * @note Extension zero-fill, sparse allocation, and partial-error rules require native documentation.
 */
static inline clm_int clm_file_truncate(clm_id id, clm_ulong size) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID && (node->mode & CLM_FILE_OPEN_MODE_WRITE) &&
        !(node->mode & CLM_FILE_OPEN_MODE_APPEND) ? clm_file_internal_truncate(node->handle, size) : -1;
}

/** Read bytes at an absolute offset without changing the sequential cursor.
 * @param id Readable open-file ID.
 * @param offset Unsigned 64-bit absolute file offset; validate native representability.
 * @param content ID of an existing writable binary BPA block with sufficient capacity.
 * @param size Maximum number of bytes to read, not a terminator-inclusive text size.
 * @param[out] bytes_read Non-NULL actual byte count; short reads and EOF are not automatic errors.
 * @return Operation status; partial reads on I/O failure need defined output/count behavior.
 */
static inline clm_int clm_file_read_at(clm_id id, clm_ulong offset, clm_id content, size_t size, size_t* bytes_read) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_positioned(id, offset, block->data, size, bytes_read, false) : -1;
}

/** Read at an absolute offset into caller-owned memory without moving the cursor.
 * @param id Readable open-file ID.
 * @param offset Unsigned 64-bit absolute offset.
 * @param[out] content Writable buffer containing at least size bytes; NULL only when size is zero.
 * @param size Maximum transfer size and destination capacity in bytes.
 * @param[out] bytes_read Non-NULL actual byte count, including a partial successful read.
 * @return Operation status; distinguish zero-byte EOF from query failure.
 */
static inline clm_int clm_file_read_at_to(clm_id id, clm_ulong offset, void* content, size_t size, size_t* bytes_read) {
    return clm_file_internal_positioned(id, offset, content, size, bytes_read, false);
}

/** Write bytes at an absolute offset without changing the sequential cursor.
 * @param id Writable open-file ID; append-mode handles must be rejected for positioned writes.
 * @param offset Unsigned 64-bit destination offset.
 * @param content ID of an existing binary source BPA block.
 * @param size Number of source bytes to write, bounded by the source block capacity.
 * @param[out] bytes_written Non-NULL actual byte count, including partial I/O progress.
 * @return Operation status; an error does not undo bytes already written.
 */
static inline clm_int clm_file_write_at(clm_id id, clm_ulong offset, clm_id content, size_t size, size_t* bytes_written) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_positioned(id, offset, block->data, size, bytes_written, true) : -1;
}

/** Write caller-owned bytes at an absolute offset without moving the cursor.
 * @param id Writable non-append open-file ID.
 * @param offset Unsigned 64-bit absolute file offset.
 * @param content Read-only source buffer with at least size readable bytes.
 * @param size Number of bytes to transfer.
 * @param[out] bytes_written Non-NULL actual transfer count, including partial progress.
 * @return Operation status; caller retains source-buffer ownership.
 */
static inline clm_int clm_file_write_at_from(clm_id id, clm_ulong offset, const void* content, size_t size, size_t* bytes_written) {
    return clm_file_internal_positioned(id, offset, (void*)content, size, bytes_written, true);
}

/** Lock a byte range of an open file under native advisory/mandatory lock semantics.
 * @param id Open-file ID with the access mode required for the selected lock.
 * @param offset Unsigned 64-bit start offset.
 * @param length Unsigned byte length; zero means from offset through future EOF.
 * @param exclusive True for exclusive locking; false for a shared lock.
 * @param wait Whether to block waiting for a conflicting lock to be released.
 * @return Operation status; unsupported or unrepresentable ranges fail explicitly.
 * @note Lock ownership, process/thread interaction, and close behavior are platform-specific.
 */
static inline clm_int clm_file_lock(clm_id id, clm_ulong offset, clm_ulong length, bool exclusive, bool wait) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || offset > (clm_ulong)LONG_MAX ||
        (length && length > (clm_ulong)LONG_MAX - offset)) return -1;
#if defined(_WIN32)
    OVERLAPPED range = {0}; range.Offset = (DWORD)offset; range.OffsetHigh = (DWORD)(offset >> 32);
    const clm_ulong native_length = length ? length : ULONG_MAX - offset;
    return LockFileEx(node->handle, (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) | (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY),
        0, (DWORD)native_length, (DWORD)(native_length >> 32), &range) ? 0 : -1;
#else
    if ((clm_ulong)(off_t)offset != offset || (clm_ulong)(off_t)length != length) return -2;
    struct flock range = {0}; range.l_type = exclusive ? F_WRLCK : F_RDLCK;
    range.l_whence = SEEK_SET; range.l_start = (off_t)offset; range.l_len = (off_t)length;
    return fcntl(node->handle, wait ? F_SETLKW : F_SETLK, &range) == 0 ? 0 : -1;
#endif
}

/** Release a previously acquired byte-range lock.
 * @param id Open-file ID that owns the lock under the platform's ownership rules.
 * @param offset Unsigned 64-bit lock start offset.
 * @param length Byte length, or zero for the to-EOF range convention.
 * @return Operation status; do not silently unlock unrelated ranges.
 */
static inline clm_int clm_file_unlock(clm_id id, clm_ulong offset, clm_ulong length) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || offset > (clm_ulong)LONG_MAX ||
        (length && length > (clm_ulong)LONG_MAX - offset)) return -1;
#if defined(_WIN32)
    OVERLAPPED range = {0}; range.Offset = (DWORD)offset; range.OffsetHigh = (DWORD)(offset >> 32);
    const clm_ulong native_length = length ? length : ULONG_MAX - offset;
    return UnlockFileEx(node->handle, 0, (DWORD)native_length, (DWORD)(native_length >> 32), &range) ? 0 : -1;
#else
    if ((clm_ulong)(off_t)offset != offset || (clm_ulong)(off_t)length != length) return -2;
    struct flock range = {0}; range.l_type = F_UNLCK; range.l_whence = SEEK_SET;
    range.l_start = (off_t)offset; range.l_len = (off_t)length;
    return fcntl(node->handle, F_SETLK, &range) == 0 ? 0 : -1;
#endif
}

/** Query file metadata and capability masks without modifying the source path text.
 * @param path ID of the file or file-link path string.
 * @param follow_symlinks Whether metadata describes the target rather than the entry itself.
 * @param[out] out Non-NULL metadata result, unchanged on query failure.
 * @return Operation status; optional fields have explicit support masks.
 */
static inline clm_int clm_file_get_info(clm_id path, bool follow_symlinks, clm_file_info* out) {
    return clm_file_internal_info(clm_path_internal_text(path), follow_symlinks, out);
}

/** Query file metadata using a raw UTF-8 path.
 * @param path NUL-terminated file or file-link path.
 * @param follow_symlinks Whether to inspect the target instead of a link entry.
 * @param[out] out Non-NULL metadata result; unsupported fields are marked in masks.
 * @return Operation status; missing-entry and size-field validity policies need definition.
 */
static inline clm_int clm_file_get_info_as(const char* path, bool follow_symlinks, clm_file_info* out) {
    return clm_file_internal_info(path, follow_symlinks, out);
}

/** Set supported mutable file attributes without replacing unrelated attributes.
 * @param path ID of the target file path string.
 * @param attributes Desired values for mask-selected attribute bits.
 * @param mask Bits to update; identity/classification bits are not mutable attributes.
 * @return Operation status; unsupported requested changes fail explicitly.
 * @note A read-only attribute is not an ACL or effective write-access decision.
 */
static inline clm_int clm_file_set_attributes(clm_id path, clm_ulong attributes, clm_ulong mask) {
    return clm_file_internal_set_attributes(clm_path_internal_text(path), attributes, mask);
}

/** Set selected mutable file attributes using a raw UTF-8 path.
 * @param path NUL-terminated file path.
 * @param attributes Desired selected attribute values.
 * @param mask Attribute bits to change; unsupported bits are errors.
 * @return Operation status; native partial-update behavior needs documentation.
 */
static inline clm_int clm_file_set_attributes_as(const char* path, clm_ulong attributes, clm_ulong mask) {
    return clm_file_internal_set_attributes(path, attributes, mask);
}

/** Set selected file timestamps without substituting metadata-change time for creation.
 * @param path ID of the target file path string.
 * @param creation_time Requested creation timestamp when selected.
 * @param modification_time Requested modification timestamp when selected.
 * @param access_time Requested access timestamp when selected.
 * @param mask CLM_FILE_TIME_* fields to update; other fields remain unchanged.
 * @return Operation status; unavailable creation-time changes return UNSUPPORTED.
 */
static inline clm_int clm_file_set_times(clm_id path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask) {
    return clm_file_internal_set_times(clm_path_internal_text(path), creation_time, modification_time, access_time, mask);
}

/** Set selected timestamps using a raw UTF-8 file path.
 * @param path NUL-terminated file path.
 * @param creation_time Requested creation time when selected.
 * @param modification_time Requested modification time when selected.
 * @param access_time Requested access time when selected.
 * @param mask CLM_FILE_TIME_* selection bits; zero requests no changes.
 * @return Operation status; native partial-update behavior must be documented.
 */
static inline clm_int clm_file_set_times_as(const char* path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask) {
    return clm_file_internal_set_times(path, creation_time, modification_time, access_time, mask);
}

/** Exclusively create a private temporary file and register its open read/write resource.
 * @param id Caller-selected unused open-file ID.
 * @param directory ID of an existing parent directory path string.
 * @param[out] path ID of an existing string destination for the created path.
 * @return Operation status; close and remove the new entry if publication fails.
 * @note Successful callers must close the open-file ID and remove the file explicitly.
 */
static inline clm_int clm_file_create_temporary(clm_id id, clm_id directory, clm_id path) {
    return clm_file_internal_temporary(id, clm_path_internal_text(directory), path, NULL, 0, NULL, true);
}

/** Create and open a temporary file inside a raw UTF-8 directory path.
 * @param id Caller-selected unused open-file ID for the new resource.
 * @param directory NUL-terminated existing parent directory path.
 * @param[out] path ID of an existing string receiving the new file path.
 * @return Operation status; no implicit parent creation.
 */
static inline clm_int clm_file_create_temporary_as(clm_id id, const char* directory, clm_id path) {
    return clm_file_internal_temporary(id, directory, path, NULL, 0, NULL, true);
}

/** Create and open a temporary file, exporting its path into caller-owned memory.
 * @param id Unused open-file ID; size-query mode must not register it.
 * @param directory NUL-terminated existing parent directory path.
 * @param[out] path Writable UTF-8 output; NULL with zero capacity measures only.
 * @param capacity Available byte capacity including the terminator.
 * @param[out] required_size Mandatory required byte capacity; no creation on insufficient capacity.
 * @return Operation status; size-query mode must have no creation side effects.
 */
static inline clm_int clm_file_create_temporary_to(clm_id id, const char* directory, char* path, size_t capacity, size_t* required_size) {
    return clm_file_internal_temporary(id, directory, 0, path, capacity, required_size, false);
}

/** Replace whole-file bytes through a temporary sibling and native atomic rename.
 * @param path ID of the destination file path string.
 * @param content ID of an existing binary source BPA block.
 * @param size Number of readable source bytes.
 * @return Operation status; lack of atomic replacement support is explicit.
 * @note Atomic visibility is not crash durability; preservation of metadata needs policy.
 */
static inline clm_int clm_file_write_all_atomic(clm_id path, clm_id content, size_t size) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_atomic(clm_path_internal_text(path), block->data, size) : -1;
}

/** Atomically replace file bytes using a raw UTF-8 destination and BPA source.
 * @param path NUL-terminated destination file path.
 * @param content ID of a binary source block containing at least size bytes.
 * @param size Exact source byte count; zero replaces the file with an empty file.
 * @return Operation status; native replacement and link policies must be specified.
 */
static inline clm_int clm_file_write_all_atomic_as(const char* path, clm_id content, size_t size) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_atomic(path, block->data, size) : -1;
}

/** Atomically replace a file from caller-owned binary data.
 * @param path NUL-terminated destination file path.
 * @param content Read-only source buffer; NULL only for a zero-byte replacement.
 * @param size Number of readable source bytes.
 * @return Operation status; clean up staging files on failure.
 */
static inline clm_int clm_file_write_all_atomic_from(const char* path, const void* content, size_t size) {
    return clm_file_internal_atomic(path, content, size);
}

/** Close every remaining native resource and release all active and retired IDs.
 * @return SUCCESS when all closes succeed, otherwise FAILURE after attempting every close.
 * @note Callers must synchronize access; IDs may be reused only after this global teardown.
 */
static inline clm_int clm_file_shutdown(void) {
    int status = 0;
    while (clm_file_internal_registry) {
        clm_file_internal_node* node = clm_file_internal_registry;
        clm_file_internal_registry = node->next;
        if (node->handle != CLM_FILE_INTERNAL_INVALID && clm_file_internal_close(node->handle)) status = -1;
        free(node);
    }
    return status;
}
