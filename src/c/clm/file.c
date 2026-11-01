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

#include "file.h"

#include <stdatomic.h>

clm_file_internal_node* clm_file_internal_registry = NULL;

int clm_file_internal_valid_path(const char* path) {
    size_t characters;
    return path && *path && clm_string_measure(path, strlen(path), CLM_STRING_ENCODING_UTF8, &characters) == 0;
}

clm_file_internal_node* clm_file_internal_find(clm_id id) {
    for (clm_file_internal_node* node = clm_file_internal_registry; node; node = node->next)
        if (node->id == id) return node;
    return NULL;
}

int clm_file_internal_close(clm_file_internal_handle handle) {
#if defined(_WIN32)
    return CloseHandle(handle) ? 0 : -1;
#else
    return close(handle) == 0 ? 0 : -1;
#endif
}

clm_file_internal_handle clm_file_internal_open(const char* path, int mode) {
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
    const HANDLE handle = CreateFileW(wide, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
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

int clm_file_internal_size(clm_file_internal_handle handle, clm_ulong* size) {
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

int clm_file_internal_seek(clm_file_internal_handle handle, clm_long offset, int origin, clm_ulong* position) {
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

int clm_file_internal_transfer(clm_file_internal_handle handle, void* buffer, size_t size,
                                             int writing, size_t* transferred) {
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

int clm_file_internal_truncate(clm_file_internal_handle handle, clm_ulong size) {
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

clm_bpa_block_node* clm_file_internal_binary(clm_id id, size_t size) {
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    return block && block->type != CLM_BPA_TYPE_STRING && size <= block->block_size ? block : NULL;
}

unsigned char* clm_file_internal_read_all(const char* path, size_t* size) {
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

int clm_file_internal_write_all(const char* path, const void* bytes, size_t size, int append) {
    if (!bytes && size) return -1;
    const int mode = CLM_FILE_OPEN_MODE_CREATE | (append ? CLM_FILE_OPEN_MODE_APPEND : CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_TRUNCATE);
    clm_file_internal_handle handle = clm_file_internal_open(path, mode);
    if (handle == CLM_FILE_INTERNAL_INVALID) return -1;
    size_t count;
    int status = clm_file_internal_transfer(handle, (void*)bytes, size, 1, &count);
    if (clm_file_internal_close(handle)) status = -1;
    return status;
}

int clm_file_internal_string(clm_id id, const char* text, size_t size) {
    if (!clm_path_internal_text(id) || size > (size_t)INT_MAX) return -1;
    char* copy = (char*)malloc(size + 1);
    if (!copy) return -1;
    memcpy(copy, text, size); copy[size] = 0;
    const int status = clm_path_internal_store(id, copy) ? -1 : 0;
    free(copy);
    return status;
}

int clm_file_internal_array(clm_id id, const char* packed, size_t bytes, clm_ulong count) {
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

time_t clm_file_internal_time_from_native(
#if defined(_WIN32)
    FILETIME value
#else
    time_t value
#endif
) {
#if defined(_WIN32)
    ULARGE_INTEGER native;
    native.LowPart = value.dwLowDateTime; native.HighPart = value.dwHighDateTime;
    return (time_t)((clm_long)(native.QuadPart / UINT64_C(10000000)) - INT64_C(11644473600));
#else
    return value;
#endif
}

int clm_file_internal_info(const char* path, bool follow, clm_file_info* out) {
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

int clm_file_internal_attribute(const char* path, clm_ulong attribute, bool* out) {
    clm_file_info info;
    if (!out || clm_file_internal_info(path, false, &info) || !info.path.exists) return -1;
    if (!(info.supported_attributes & attribute)) return -2;
    *out = (info.attributes & attribute) != 0;
    return 0;
}

int clm_file_internal_set_attributes(const char* path, clm_ulong attributes, clm_ulong mask) {
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
    for (size_t index = 0; index < 6; ++index) {
        const clm_ulong bit = UINT64_C(1) << index;
        if (mask & bit) {
            const DWORD native_flags[] = {FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM,
                FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_OFFLINE};
            flags &= ~native_flags[index]; if (attributes & bit) flags |= native_flags[index]; }
    }
    const int status = SetFileAttributesW(wide, flags ? flags : FILE_ATTRIBUTE_NORMAL) ? 0 : -1;
    free(wide);
    return status;
#else
    (void)attributes;
    return -2;
#endif
}

int clm_file_internal_set_times(const char* path, time_t creation, time_t modification,
                                             time_t access, clm_uint mask) {
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
        if (seconds < -INT64_C(11644473600) || seconds > (clm_long)(UINT64_MAX / UINT64_C(10000000)) - INT64_C(11644473600)) return -1;
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
    struct stat current;
    if (stat(path, &current) != 0)
        return -1;
    struct timespec times[2];
    times[0].tv_sec = mask & 4 ? access : current.st_atim.tv_sec;
    times[0].tv_nsec = mask & 4 ? current.st_atim.tv_nsec : UTIME_OMIT;
    times[1].tv_sec = mask & 2 ? modification : current.st_mtim.tv_sec;
    times[1].tv_nsec = mask & 2 ? current.st_mtim.tv_nsec : UTIME_OMIT;
    return utimensat(AT_FDCWD, path, times, 0) == 0 ? 0 : -1;
#endif
}

char* clm_file_internal_decode(const unsigned char* bytes, size_t size, int encoding, size_t* length) {
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

unsigned char* clm_file_internal_encode(const char* text, size_t length, int encoding, size_t* size) {
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

int clm_file_internal_text_write(const char* path, int encoding, const char* text, size_t size, int append, int line) {
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

char* clm_file_internal_lines(const char* text, size_t length, size_t* bytes, clm_ulong* count) {
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

int clm_file_internal_unlink(const char* path) {
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

char* clm_file_internal_temp_name(const char* base, const char* suffix) {
    if (!clm_file_internal_valid_path(base)) return NULL;
    uint64_t random[2];
    if (!clm_gen_internal_bytes(random, sizeof(random))) return NULL;
    const size_t size = strlen(base) + strlen(suffix) + 48;
    char* path = (char*)malloc(size);
    if (path) snprintf(path, size, "%s%cclm-%016llx%016llx%s", base, clm_path_internal_native_separator(),
        (unsigned long long)random[0], (unsigned long long)random[1], suffix);
    return path;
}

char* clm_file_internal_parent(const char* path) {
    if (!clm_file_internal_valid_path(path)) return NULL;
    size_t end;
    size_t first = clm_path_internal_filename(path, &end);
    const size_t root = clm_path_internal_root(path);
    while (first > root && clm_path_internal_separator(path[first - 1])) --first;
    char* result = clm_path_internal_copy(first ? path : ".");
    if (result && first) result[first] = 0;
    return result;
}

int clm_file_internal_register(clm_id id, const char* path, int mode) {
    if (clm_file_internal_find(id)) return -1;
    clm_file_internal_node* node = (clm_file_internal_node*)calloc(1, sizeof(*node));
    if (!node) return -1;
    node->handle = clm_file_internal_open(path, mode);
    if (node->handle == CLM_FILE_INTERNAL_INVALID) { free(node); return -1; }
    node->id = id; node->mode = mode; node->next = clm_file_internal_registry;
    clm_file_internal_registry = node;
    return 0;
}

int clm_file_internal_binary_read(const char* path, clm_id destination, void* buffer,
                                               size_t capacity, size_t* size, bool managed) {
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

int clm_file_internal_text_read(const char* path, int encoding, clm_id destination,
    char* buffer, size_t capacity, size_t* size, clm_ulong* count, int kind) {
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

int clm_file_internal_content_write(const char* path, clm_id content, size_t size, int encoding, int append, int line) {
    if (encoding < 0) {
        clm_bpa_block_node* block = clm_file_internal_binary(content, size);
        return block ? clm_file_internal_write_all(path, block->data, size, append) : -1;
    }
    const char* text = clm_path_internal_text(content);
    return text && size <= strlen(text) ? clm_file_internal_text_write(path, encoding, text, size, append, line) : -1;
}

int clm_file_internal_predicate(const char* path, int directory) {
    clm_file_info info;
    if (clm_file_internal_info(path, true, &info)) return -1;
    return directory ? info.path.is_directory : info.path.is_file;
}

int clm_file_internal_time(const char* path, time_t* out, unsigned field) {
    clm_file_info info;
    if (!out || clm_file_internal_info(path, true, &info) || !info.path.exists) return -1;
    if (!(info.supported_times & field)) return -2;
    *out = field == 1 ? info.creation_time : field == 2 ? info.modification_time : info.access_time;
    return 0;
}

int clm_file_internal_copy_file(const char* source, const char* destination, bool overwrite) {
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

int clm_file_internal_move(const char* source, const char* destination, bool replace) {
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

int clm_file_internal_positioned(clm_id id, clm_ulong offset, void* content, size_t size,
                                               size_t* transferred, bool writing) {
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

int clm_file_internal_temporary(clm_id id, const char* base, clm_id destination,
                                              char* buffer, size_t capacity, size_t* required, bool managed) {
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

int clm_file_internal_atomic(const char* path, const void* content, size_t size) {
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

clm_int clm_file_exists(clm_id path) {
    return clm_file_internal_predicate(clm_path_internal_text(path), 0);
}

clm_int clm_file_exists_as(const char* path) {
    return clm_file_internal_predicate(path, 0);
}

clm_int clm_file_read_all(clm_id path, clm_id content, size_t* size) {
    return clm_file_internal_binary_read(clm_path_internal_text(path), content, NULL, 0, size, true);
}

clm_int clm_file_read_all_as(const char* path, clm_id content, size_t* size) {
    return clm_file_internal_binary_read(path, content, NULL, 0, size, true);
}

clm_int clm_file_read_all_to(const char* path, void* content, size_t capacity, size_t* size) {
    return clm_file_internal_binary_read(path, 0, content, capacity, size, false);
}

clm_int clm_file_read_all_text(clm_id path, clm_int encoding, clm_id content, size_t* size) {
    return clm_file_internal_text_read(clm_path_internal_text(path), encoding, content, NULL, 0, size, NULL, 0);
}

clm_int clm_file_read_all_text_as(const char* path, clm_int encoding, clm_id content, size_t* size) {
    return clm_file_internal_text_read(path, encoding, content, NULL, 0, size, NULL, 0);
}

clm_int clm_file_read_all_text_to(const char* path, clm_int encoding, char* content, size_t capacity, size_t* required_size) {
    return clm_file_internal_text_read(path, encoding, 0, content, capacity, required_size, NULL, 1);
}

clm_int clm_file_read_all_text_lines(clm_id path, clm_int encoding, clm_id content, clm_ulong* line_count) {
    return clm_file_internal_text_read(clm_path_internal_text(path), encoding, content, NULL, 0, NULL, line_count, 2);
}

clm_int clm_file_read_all_text_lines_as(const char* path, clm_int encoding, clm_id content, clm_ulong* line_count) {
    return clm_file_internal_text_read(path, encoding, content, NULL, 0, NULL, line_count, 2);
}

clm_int clm_file_read_all_text_lines_to(const char* path, clm_int encoding, char* content, size_t capacity, size_t* required_size, clm_ulong* line_count) {
    return clm_file_internal_text_read(path, encoding, 0, content, capacity, required_size, line_count, 3);
}

clm_int clm_file_write_all(clm_id path, clm_id content, size_t size) {
    return clm_file_internal_content_write(clm_path_internal_text(path), content, size, -1, 0, 0);
}

clm_int clm_file_write_all_as(const char* path, clm_id content, size_t size) {
    return clm_file_internal_content_write(path, content, size, -1, 0, 0);
}

clm_int clm_file_write_all_from(const char* path, const void* content, size_t size) {
    return clm_file_internal_write_all(path, content, size, 0);
}

clm_int clm_file_write_all_text(clm_id path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(clm_path_internal_text(path), content, size, encoding, 0, 0) : -1;
}

clm_int clm_file_write_all_text_as(const char* path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(path, content, size, encoding, 0, 0) : -1;
}

clm_int clm_file_write_all_text_from(const char* path, clm_int encoding, const char* content, size_t size) {
    return clm_file_internal_text_write(path, encoding, content, size, 0, 0);
}

clm_int clm_file_append(clm_id path, clm_id content, size_t size) {
    return clm_file_internal_content_write(clm_path_internal_text(path), content, size, -1, 1, 0);
}

clm_int clm_file_append_as(const char* path, clm_id content, size_t size) {
    return clm_file_internal_content_write(path, content, size, -1, 1, 0);
}

clm_int clm_file_append_from(const char* path, const void* content, size_t size) {
    return clm_file_internal_write_all(path, content, size, 1);
}

clm_int clm_file_append_text(clm_id path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(clm_path_internal_text(path), content, size, encoding, 1, 0) : -1;
}

clm_int clm_file_append_text_as(const char* path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(path, content, size, encoding, 1, 0) : -1;
}

clm_int clm_file_append_text_from(const char* path, clm_int encoding, const char* content, size_t size) {
    return clm_file_internal_text_write(path, encoding, content, size, 1, 0);
}

clm_int clm_file_append_line_text(clm_id path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(clm_path_internal_text(path), content, size, encoding, 1, 1) : -1;
}

clm_int clm_file_append_line_text_as(const char* path, clm_int encoding, clm_id content, size_t size) {
    return encoding >= 0 && encoding <= 6 ? clm_file_internal_content_write(path, content, size, encoding, 1, 1) : -1;
}

clm_int clm_file_append_line_text_from(const char* path, clm_int encoding, const char* content, size_t size) {
    return clm_file_internal_text_write(path, encoding, content, size, 1, 1);
}

static atomic_flag clm_file_internal_object_lock = ATOMIC_FLAG_INIT;
static clm_stringtify_interceptor_fn clm_file_internal_object_interceptor;
static void* clm_file_internal_object_interceptor_user;

static void clm_file_internal_object_lock_acquire(void) {
    while (atomic_flag_test_and_set_explicit(&clm_file_internal_object_lock, memory_order_acquire)) {}
}

static void clm_file_internal_object_lock_release(void) {
    atomic_flag_clear_explicit(&clm_file_internal_object_lock, memory_order_release);
}

void clm_file_intercept_object_writes(clm_stringtify_interceptor_fn interceptor, void* user) {
    clm_file_internal_object_lock_acquire();
    clm_file_internal_object_interceptor = interceptor;
    clm_file_internal_object_interceptor_user = interceptor ? user : NULL;
    clm_file_internal_object_lock_release();
}

void clm_file_do_not_intercept_object_writes(void) {
    clm_file_intercept_object_writes(NULL, NULL);
}

typedef clm_stringtify_status (*clm_file_internal_stringtify_fn)(
    clm_id, const clm_stringtify_options*, char*, size_t, size_t*);

static clm_int clm_file_internal_put_text(const char* path, clm_int encoding, const char* text, size_t size, bool append) {
    return append ? clm_file_append_text_from(path, encoding, text, size)
                  : clm_file_write_all_text_from(path, encoding, text, size);
}

static clm_int clm_file_internal_put_object(const char* path, clm_int encoding, clm_id id,
                                            clm_file_internal_stringtify_fn stringify, bool table, bool detailed,
                                            clm_stringtify_interceptor_fn format_interceptor, bool append) {
    if (!path || encoding < 0 || encoding > 6) return -1;
    clm_file_internal_object_lock_acquire();
    const clm_stringtify_interceptor_fn interceptor = clm_file_internal_object_interceptor;
    void* const interceptor_user = clm_file_internal_object_interceptor_user;
    clm_file_internal_object_lock_release();

    clm_stringtify_options options;
    clm_stringtify_options_init(&options);
    options.style = detailed ? CLM_STRINGTIFY_STYLE_DETAILED : CLM_STRINGTIFY_STYLE_PRETTY;
    options.interceptor = format_interceptor ? format_interceptor : interceptor;
    options.interceptor_user = format_interceptor ? NULL : interceptor_user;
    options.table_borders = table;
    options.align_columns = table;

    size_t required = 0;
    if (stringify(id, &options, NULL, 0, &required) != CLM_STRINGTIFY_SUCCESS || required == 0) return -1;
    char* text = (char*)malloc(required);
    if (!text) return -1;
    clm_int status = -1;
    if (stringify(id, &options, text, required, &required) == CLM_STRINGTIFY_SUCCESS)
        status = clm_file_internal_put_text(path, encoding, text, required - 1, append);
    free(text);
    return status;
}

static clm_int clm_file_internal_put_block(const char* path, clm_int encoding, clm_id id, bool inspect, bool append) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (!block) return -1;
    if (!inspect && block->type == CLM_BPA_TYPE_STRING) {
        if (block->length != 0 && !block->data) return -1;
        return clm_file_internal_put_text(path, encoding, block->data ? (const char*)block->data : "", block->length, append);
    }
    return clm_file_internal_put_object(path, encoding, id, clm_stringtify_block_to_string_with_options,
                                        false, true, NULL, append);
}

#define CLM_FILE_INTERNAL_OBJECT_WRITERS(kind, stringify, table, format) \
clm_int clm_file_write_##kind(clm_id path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_object(clm_path_internal_text(path), encoding, id, stringify, table, false, format, false); } \
clm_int clm_file_write_##kind##_as(const char* path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_object(path, encoding, id, stringify, table, false, format, false); } \
clm_int clm_file_append_##kind(clm_id path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_object(clm_path_internal_text(path), encoding, id, stringify, table, false, format, true); } \
clm_int clm_file_append_##kind##_as(const char* path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_object(path, encoding, id, stringify, table, false, format, true); }

#define CLM_FILE_INTERNAL_BLOCK_WRITERS(kind, inspect) \
clm_int clm_file_write_##kind(clm_id path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_block(clm_path_internal_text(path), encoding, id, inspect, false); } \
clm_int clm_file_write_##kind##_as(const char* path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_block(path, encoding, id, inspect, false); } \
clm_int clm_file_append_##kind(clm_id path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_block(clm_path_internal_text(path), encoding, id, inspect, true); } \
clm_int clm_file_append_##kind##_as(const char* path, clm_int encoding, clm_id id) { \
    return clm_file_internal_put_block(path, encoding, id, inspect, true); }

CLM_FILE_INTERNAL_BLOCK_WRITERS(block, false)
CLM_FILE_INTERNAL_BLOCK_WRITERS(inspect_block, true)
CLM_FILE_INTERNAL_OBJECT_WRITERS(dictionary, clm_stringtify_dictionary_to_string_with_options, false, NULL)
CLM_FILE_INTERNAL_OBJECT_WRITERS(storage, clm_stringtify_storage_to_string_with_options, false, NULL)
CLM_FILE_INTERNAL_OBJECT_WRITERS(table, clm_stringtify_table_to_string_with_options, true, NULL)
CLM_FILE_INTERNAL_OBJECT_WRITERS(tree, clm_stringtify_tree_to_string_with_options, false, NULL)
CLM_FILE_INTERNAL_OBJECT_WRITERS(tree_as_yaml, clm_stringtify_tree_to_string_with_options, false, clm_stringtify_tree_yaml_interceptor)
CLM_FILE_INTERNAL_OBJECT_WRITERS(tree_as_json, clm_stringtify_tree_to_string_with_options, false, clm_stringtify_tree_json_interceptor)

#undef CLM_FILE_INTERNAL_OBJECT_WRITERS
#undef CLM_FILE_INTERNAL_BLOCK_WRITERS

clm_int clm_file_open(clm_id id, clm_id path, clm_int mode) {
    return clm_file_internal_register(id, clm_path_internal_text(path), mode);
}

clm_int clm_file_open_as(clm_id id, const char* path, clm_int mode) {
    return clm_file_internal_register(id, path, mode);
}

clm_int clm_file_read(clm_id id, clm_id content, size_t size, size_t* bytes_read) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    if (!node || !block || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_READ)) return -1;
    const int status = clm_file_internal_transfer(node->handle, block->data, size, 0, bytes_read);
    if (!status && size) node->eof = *bytes_read < size;
    return status;
}

clm_int clm_file_read_to(clm_id id, void* content, size_t size, size_t* bytes_read) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_READ)) return -1;
    const int status = clm_file_internal_transfer(node->handle, content, size, 0, bytes_read);
    if (!status && size) node->eof = *bytes_read < size;
    return status;
}

clm_int clm_file_write(clm_id id, clm_id content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    if (!node || !block || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) return -1;
    return clm_file_internal_transfer(node->handle, block->data, size, 1, bytes_written);
}

clm_int clm_file_write_from(clm_id id, const void* content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) return -1;
    return clm_file_internal_transfer(node->handle, (void*)content, size, 1, bytes_written);
}

clm_int clm_file_append_open(clm_id id, clm_id content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    if (!node || !block || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_APPEND)) return -1;
    return clm_file_internal_transfer(node->handle, block->data, size, 1, bytes_written);
}

clm_int clm_file_append_open_from(clm_id id, const void* content, size_t size, size_t* bytes_written) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & CLM_FILE_OPEN_MODE_APPEND)) return -1;
    return clm_file_internal_transfer(node->handle, (void*)content, size, 1, bytes_written);
}

clm_int clm_file_tell(clm_id id, clm_ulong* index) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID && index ? clm_file_internal_seek(node->handle, 0, 1, index) : -1;
}

clm_int clm_file_close(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID) return -1;
    const clm_file_internal_handle handle = node->handle;
    node->handle = CLM_FILE_INTERNAL_INVALID;
    return clm_file_internal_close(handle);
}

clm_int clm_file_get_size(clm_id path, clm_ulong* size) {
    clm_file_info info;
    if (!size || clm_file_internal_info(clm_path_internal_text(path), true, &info) || !info.path.is_file) return -1;
    *size = info.size; return 0;
}

clm_int clm_file_get_size_as(const char* path, clm_ulong* size) {
    clm_file_info info;
    if (!size || clm_file_internal_info(path, true, &info) || !info.path.is_file) return -1;
    *size = info.size; return 0;
}

clm_int clm_file_get_creation_time(clm_id id, time_t* out) {
    return clm_file_internal_time(clm_path_internal_text(id), out, 1);
}

clm_int clm_file_get_creation_time_as(const char* path, time_t* out) {
    return clm_file_internal_time(path, out, 1);
}

clm_int clm_file_get_modification_time(clm_id id, time_t* out) {
    return clm_file_internal_time(clm_path_internal_text(id), out, 2);
}

clm_int clm_file_get_modification_time_as(const char* path, time_t* out) {
    return clm_file_internal_time(path, out, 2);
}

clm_int clm_file_get_access_time(clm_id id, time_t* out) {
    return clm_file_internal_time(clm_path_internal_text(id), out, 4);
}

clm_int clm_file_get_access_time_as(const char* path, time_t* out) {
    return clm_file_internal_time(path, out, 4);
}

clm_int clm_file_is_hidden(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_HIDDEN, out);
}

clm_int clm_file_is_hidden_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_HIDDEN, out);
}

clm_int clm_file_is_read_only(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_READ_ONLY, out);
}

clm_int clm_file_is_read_only_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_READ_ONLY, out);
}

clm_int clm_file_is_compressed(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_COMPRESSED, out);
}

clm_int clm_file_is_compressed_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_COMPRESSED, out);
}

clm_int clm_file_is_archive(clm_id id, bool* out) {
    return clm_file_internal_attribute(clm_path_internal_text(id), CLM_FILE_ATTRIBUTE_ARCHIVE, out);
}

clm_int clm_file_is_archive_as(const char* path, bool* out) {
    return clm_file_internal_attribute(path, CLM_FILE_ATTRIBUTE_ARCHIVE, out);
}

clm_int clm_file_create(clm_id path) {
    clm_file_internal_handle handle = clm_file_internal_open(clm_path_internal_text(path), CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE | CLM_FILE_OPEN_MODE_EXCLUSIVE);
    return handle != CLM_FILE_INTERNAL_INVALID ? clm_file_internal_close(handle) : -1;
}

clm_int clm_file_create_as(const char* path) {
    clm_file_internal_handle handle = clm_file_internal_open(path, CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE | CLM_FILE_OPEN_MODE_EXCLUSIVE);
    return handle != CLM_FILE_INTERNAL_INVALID ? clm_file_internal_close(handle) : -1;
}

clm_int clm_file_copy(clm_id source, clm_id destination, bool overwrite) {
    return clm_file_internal_copy_file(clm_path_internal_text(source), clm_path_internal_text(destination), overwrite);
}

clm_int clm_file_copy_as(const char* source, const char* destination, bool overwrite) {
    return clm_file_internal_copy_file(source, destination, overwrite);
}

clm_int clm_file_move(clm_id source, clm_id destination) {
    const char* path = clm_path_internal_text(source);
    clm_file_info info;
    return !clm_file_internal_info(path, false, &info) && (info.path.is_file || info.path.is_symlink)
        ? clm_file_internal_move(path, clm_path_internal_text(destination), false) : -1;
}

clm_int clm_file_move_as(const char* source, const char* destination) {
    clm_file_info info;
    return !clm_file_internal_info(source, false, &info) && (info.path.is_file || info.path.is_symlink)
        ? clm_file_internal_move(source, destination, false) : -1;
}

clm_int clm_file_delete(clm_id path) {
    return clm_file_internal_unlink(clm_path_internal_text(path));
}

clm_int clm_file_delete_as(const char* path) {
    return clm_file_internal_unlink(path);
}

clm_int clm_file_is_open(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID;
}

clm_int clm_file_seek(clm_id id, clm_long offset, clm_int origin) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID) return -1;
    const int status = clm_file_internal_seek(node->handle, offset, origin, NULL);
    if (!status) node->eof = false;
    return status;
}

clm_int clm_file_eof(clm_id id, bool* out) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !out || !(node->mode & CLM_FILE_OPEN_MODE_READ)) return -1;
    *out = node->eof; return 0;
}

clm_int clm_file_flush(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID && (node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND)) ? 0 : -1;
}

clm_int clm_file_sync(clm_id id) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    if (!node || node->handle == CLM_FILE_INTERNAL_INVALID || !(node->mode & (CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_APPEND))) return -1;
#if defined(_WIN32)
    return FlushFileBuffers(node->handle) ? 0 : -1;
#else
    return fsync(node->handle) == 0 ? 0 : -1;
#endif
}

clm_int clm_file_get_open_size(clm_id id, clm_ulong* size) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID ? clm_file_internal_size(node->handle, size) : -1;
}

clm_int clm_file_truncate(clm_id id, clm_ulong size) {
    clm_file_internal_node* node = clm_file_internal_find(id);
    return node && node->handle != CLM_FILE_INTERNAL_INVALID && (node->mode & CLM_FILE_OPEN_MODE_WRITE) &&
        !(node->mode & CLM_FILE_OPEN_MODE_APPEND) ? clm_file_internal_truncate(node->handle, size) : -1;
}

clm_int clm_file_read_at(clm_id id, clm_ulong offset, clm_id content, size_t size, size_t* bytes_read) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_positioned(id, offset, block->data, size, bytes_read, false) : -1;
}

clm_int clm_file_read_at_to(clm_id id, clm_ulong offset, void* content, size_t size, size_t* bytes_read) {
    return clm_file_internal_positioned(id, offset, content, size, bytes_read, false);
}

clm_int clm_file_write_at(clm_id id, clm_ulong offset, clm_id content, size_t size, size_t* bytes_written) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_positioned(id, offset, block->data, size, bytes_written, true) : -1;
}

clm_int clm_file_write_at_from(clm_id id, clm_ulong offset, const void* content, size_t size, size_t* bytes_written) {
    return clm_file_internal_positioned(id, offset, (void*)content, size, bytes_written, true);
}

clm_int clm_file_lock(clm_id id, clm_ulong offset, clm_ulong length, bool exclusive, bool wait) {
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

clm_int clm_file_unlock(clm_id id, clm_ulong offset, clm_ulong length) {
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

clm_int clm_file_get_info(clm_id path, bool follow_symlinks, clm_file_info* out) {
    return clm_file_internal_info(clm_path_internal_text(path), follow_symlinks, out);
}

clm_int clm_file_get_info_as(const char* path, bool follow_symlinks, clm_file_info* out) {
    return clm_file_internal_info(path, follow_symlinks, out);
}

clm_int clm_file_set_attributes(clm_id path, clm_ulong attributes, clm_ulong mask) {
    return clm_file_internal_set_attributes(clm_path_internal_text(path), attributes, mask);
}

clm_int clm_file_set_attributes_as(const char* path, clm_ulong attributes, clm_ulong mask) {
    return clm_file_internal_set_attributes(path, attributes, mask);
}

clm_int clm_file_set_times(clm_id path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask) {
    return clm_file_internal_set_times(clm_path_internal_text(path), creation_time, modification_time, access_time, mask);
}

clm_int clm_file_set_times_as(const char* path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask) {
    return clm_file_internal_set_times(path, creation_time, modification_time, access_time, mask);
}

clm_int clm_file_create_temporary(clm_id id, clm_id directory, clm_id path) {
    return clm_file_internal_temporary(id, clm_path_internal_text(directory), path, NULL, 0, NULL, true);
}

clm_int clm_file_create_temporary_as(clm_id id, const char* directory, clm_id path) {
    return clm_file_internal_temporary(id, directory, path, NULL, 0, NULL, true);
}

clm_int clm_file_create_temporary_to(clm_id id, const char* directory, char* path, size_t capacity, size_t* required_size) {
    return clm_file_internal_temporary(id, directory, 0, path, capacity, required_size, false);
}

clm_int clm_file_write_all_atomic(clm_id path, clm_id content, size_t size) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_atomic(clm_path_internal_text(path), block->data, size) : -1;
}

clm_int clm_file_write_all_atomic_as(const char* path, clm_id content, size_t size) {
    clm_bpa_block_node* block = clm_file_internal_binary(content, size);
    return block ? clm_file_internal_atomic(path, block->data, size) : -1;
}

clm_int clm_file_write_all_atomic_from(const char* path, const void* content, size_t size) {
    return clm_file_internal_atomic(path, content, size);
}

clm_int clm_file_shutdown(void) {
    int status = 0;
    while (clm_file_internal_registry) {
        clm_file_internal_node* node = clm_file_internal_registry;
        clm_file_internal_registry = node->next;
        if (node->handle != CLM_FILE_INTERNAL_INVALID && clm_file_internal_close(node->handle)) status = -1;
        free(node);
    }
    return status;
}
