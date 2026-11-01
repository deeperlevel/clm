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

#include "directory.h"

char* clm_directory_internal_join(const char* base, const char* name) {
    if (!clm_file_internal_valid_path(base) || !name) return NULL;
    const size_t first = strlen(base), second = strlen(name);
    if (first > (size_t)-1 - second - 2) return NULL;
    char* path = (char*)malloc(first + second + 2);
    if (!path) return NULL;
    memcpy(path, base, first);
    const size_t separator = first && clm_path_internal_separator(base[first - 1]) ? 0 : 1;
    if (separator) path[first] = clm_path_internal_native_separator();
    memcpy(path + first + separator, name, second + 1);
    return path;
}

void clm_directory_internal_snapshot_free(clm_directory_internal_snapshot* snapshot) {
    for (size_t index = 0; index < snapshot->count; ++index) {
        free(snapshot->children[index].path); free(snapshot->children[index].name);
    }
    free(snapshot->children);
#if defined(_WIN32)
    if (snapshot->guard != INVALID_HANDLE_VALUE) CloseHandle(snapshot->guard);
#else
    if (snapshot->guard) closedir(snapshot->guard);
#endif
    memset(snapshot, 0, sizeof(*snapshot));
#if defined(_WIN32)
    snapshot->guard = INVALID_HANDLE_VALUE;
#endif
}

int clm_directory_internal_snapshot_add(clm_directory_internal_snapshot* snapshot, const char* base, const char* name) {
    if (!strcmp(name, ".") || !strcmp(name, "..")) return 0;
    clm_directory_internal_child child = {0};
    child.path = clm_directory_internal_join(base, name); child.name = clm_path_internal_copy(name);
    clm_file_info info;
    if (!child.path || !child.name || clm_file_internal_info(child.path, false, &info) || !info.path.exists) {
        free(child.path); free(child.name); return -1;
    }
    child.hidden = info.path.is_hidden != 0; child.reparse = info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT;
    child.type = info.path.is_symlink || child.reparse ? CLM_DIRECTORY_ENTRY_LINK : info.path.is_directory
        ? CLM_DIRECTORY_ENTRY_DIRECTORY : info.path.is_file ? CLM_DIRECTORY_ENTRY_FILE : CLM_DIRECTORY_ENTRY_OTHER;
    if (snapshot->count >= (size_t)-1 / sizeof(child) - 1) { free(child.path); free(child.name); return -1; }
    clm_directory_internal_child* grown = (clm_directory_internal_child*)realloc(snapshot->children,
        (snapshot->count + 1) * sizeof(child));
    if (!grown) { free(child.path); free(child.name); return -1; }
    snapshot->children = grown; snapshot->children[snapshot->count++] = child;
    return 0;
}

int clm_directory_internal_snapshot_open(const char* path, bool follow, clm_directory_internal_snapshot* snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
#if defined(_WIN32)
    snapshot->guard = INVALID_HANDLE_VALUE;
#endif
    if (!clm_file_internal_valid_path(path)) return -1;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    snapshot->guard = CreateFileW(wide, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | (follow ? 0 : FILE_FLAG_OPEN_REPARSE_POINT), NULL);
    free(wide);
    if (snapshot->guard == INVALID_HANDLE_VALUE) return -1;
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(snapshot->guard, &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (!follow && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))) { clm_directory_internal_snapshot_free(snapshot); return -1; }
    snapshot->device = info.dwVolumeSerialNumber;
    snapshot->identity = ((clm_ulong)info.nFileIndexHigh << 32) | info.nFileIndexLow;
    char* pattern = clm_directory_internal_join(path, "*");
    wchar_t* wide_pattern = pattern ? clm_path_internal_wide(pattern) : NULL;
    free(pattern);
    if (!wide_pattern) { clm_directory_internal_snapshot_free(snapshot); return -1; }
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(wide_pattern, &entry);
    free(wide_pattern);
    if (search == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return 0;
        clm_directory_internal_snapshot_free(snapshot); return -1;
    }
    int status = 0;
    do {
        char* name = clm_path_internal_utf8(entry.cFileName);
        if (!name || clm_directory_internal_snapshot_add(snapshot, path, name)) status = -1;
        free(name);
        if (status) break;
    } while (FindNextFileW(search, &entry));
    if (!status && GetLastError() != ERROR_NO_MORE_FILES) status = -1;
    FindClose(search);
#else
    const int descriptor = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | (follow ? 0 : O_NOFOLLOW));
    if (descriptor < 0) return -1;
    snapshot->guard = fdopendir(descriptor);
    if (!snapshot->guard) { close(descriptor); return -1; }
    struct stat info;
    if (fstat(descriptor, &info)) { clm_directory_internal_snapshot_free(snapshot); return -1; }
    snapshot->device = (clm_ulong)info.st_dev; snapshot->identity = (clm_ulong)info.st_ino;
    int status = 0;
    for (;;) {
        errno = 0;
        struct dirent* entry = readdir(snapshot->guard);
        if (!entry) { if (errno) status = -1; break; }
        if (clm_directory_internal_snapshot_add(snapshot, path, entry->d_name)) { status = -1; break; }
    }
#endif
    if (status) clm_directory_internal_snapshot_free(snapshot);
    return status;
}

int clm_directory_internal_create(const char* path) {
    if (!clm_file_internal_valid_path(path)) return -1;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    const int status = CreateDirectoryW(wide, NULL) ? 0 : -1;
    free(wide); return status;
#else
    return mkdir(path, 0700) == 0 ? 0 : -1;
#endif
}

int clm_directory_internal_remove(const char* path) {
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(path);
    if (!wide) return -1;
    const int status = RemoveDirectoryW(wide) ? 0 : -1;
    free(wide); return status;
#else
    return rmdir(path) == 0 ? 0 : -1;
#endif
}

int clm_directory_internal_guard_root(const char* path) {
    if (!clm_file_internal_valid_path(path)) return -1;
    char* normalized = clm_path_internal_normalize(path);
    if (!normalized) return -1;
    const size_t root = clm_path_internal_root(normalized), length = strlen(normalized);
    const int unsafe = !strcmp(normalized, ".") || !strcmp(normalized, "..") || !length || (root && length <= root);
    free(normalized);
    if (unsafe) return -1;
    size_t end;
    const size_t first = clm_path_internal_filename(path, &end);
    if ((end - first == 1 && path[first] == '.') || (end - first == 2 && !memcmp(path + first, "..", 2))) return -1;
    return 0;
}

int clm_directory_internal_create_recursive(const char* path, unsigned depth) {
    if (depth > 256 || !clm_file_internal_valid_path(path)) return -1;
    clm_file_info info;
    if (clm_file_internal_info(path, false, &info)) return -1;
    if (info.path.exists) return info.path.is_directory && !(info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT) ? 0 : -1;
    char* parent = clm_file_internal_parent(path);
    if (!parent || !strcmp(parent, path)) { free(parent); return -1; }
    const int status = clm_directory_internal_create_recursive(parent, depth + 1);
    free(parent);
    if (status) return status;
    if (!clm_directory_internal_create(path)) return 0;
    return !clm_file_internal_info(path, false, &info) && info.path.is_directory &&
        !(info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT) ? 0 : -1;
}

char* clm_directory_internal_temporary_base(void) {
#if defined(_WIN32)
    const DWORD count = GetTempPathW(0, NULL);
    wchar_t* wide = count ? (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t)) : NULL;
    if (!wide) return NULL;
    const DWORD written = GetTempPathW(count + 1, wide);
    char* result = written && written <= count ? clm_path_internal_utf8(wide) : NULL;
    free(wide); return result;
#else
    const char* configured = getenv("TMPDIR");
    return clm_path_internal_copy(configured && *configured ? configured : "/tmp");
#endif
}

int clm_directory_internal_buffer(char* buffer, size_t capacity, const char* text) {
    if (!buffer || !text || strlen(text) >= capacity) return -1;
    memcpy(buffer, text, strlen(text) + 1);
    return 0;
}

char* clm_directory_internal_packed(const char* path, clm_uint types, size_t* bytes, clm_ulong* count) {
    clm_directory_internal_snapshot snapshot;
    if (clm_directory_internal_snapshot_open(path, false, &snapshot)) return NULL;
    size_t size = 1;
    clm_ulong total = 0;
    for (size_t index = 0; index < snapshot.count; ++index) {
        const clm_directory_internal_child* child = &snapshot.children[index];
        if (!(types & child->type)) continue;
        const size_t length = strlen(child->path) + 1;
        if (length > (size_t)-1 - size) { clm_directory_internal_snapshot_free(&snapshot); return NULL; }
        size += length; ++total;
    }
    char* packed = (char*)malloc(size);
    if (!packed) { clm_directory_internal_snapshot_free(&snapshot); return NULL; }
    size_t used = 0;
    for (size_t index = 0; index < snapshot.count; ++index) {
        const clm_directory_internal_child* child = &snapshot.children[index];
        if (!(types & child->type)) continue;
        const size_t length = strlen(child->path) + 1;
        memcpy(packed + used, child->path, length); used += length;
    }
    packed[used] = 0;
    clm_directory_internal_snapshot_free(&snapshot); *bytes = size; *count = total;
    return packed;
}

int clm_directory_internal_collect(const char* path, clm_uint types, clm_id destination,
    char* buffer, size_t capacity, size_t* required, clm_ulong* count, bool managed) {
    clm_bpaa_array_node* array;
    if ((managed && clm_string_array_get_node(destination, &array) != 0) ||
        (!managed && (!required || !count || (!buffer && capacity)))) return -1;
    size_t bytes; clm_ulong total;
    char* packed = clm_directory_internal_packed(path, types, &bytes, &total);
    if (!packed) return -1;
    if (managed) { const int status = clm_file_internal_array(destination, packed, bytes, total); free(packed); return status; }
    if (!buffer || capacity < bytes) { free(packed); *required = bytes; *count = total; return !buffer && !capacity ? 0 : -1; }
    memcpy(buffer, packed, bytes); free(packed); *required = bytes; *count = total;
    return 0;
}

int clm_directory_internal_count(const char* path, clm_uint types, clm_ulong* count) {
    if (!count) return -1;
    size_t bytes; clm_ulong total;
    char* packed = clm_directory_internal_packed(path, types, &bytes, &total);
    if (!packed) return -1;
    free(packed); *count = total; return 0;
}

int clm_directory_internal_info(const char* path, bool follow, clm_directory_info* out) {
    clm_file_info info;
    if (!out || clm_file_internal_info(path, follow, &info) ||
        (info.path.exists && !info.path.is_directory && !info.path.is_symlink)) return -1;
    clm_directory_info result = {0};
    result.path = info.path;
    result.attributes = info.attributes & 255;
    result.supported_attributes = info.supported_attributes & 255;
    if (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT) result.attributes |= CLM_DIRECTORY_ATTRIBUTE_REPARSE_POINT;
    if (info.path.is_symlink) result.attributes |= CLM_DIRECTORY_ATTRIBUTE_SYMBOLIC_LINK;
    if (info.supported_attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)
        result.supported_attributes |= CLM_DIRECTORY_ATTRIBUTE_REPARSE_POINT | CLM_DIRECTORY_ATTRIBUTE_JUNCTION;
    if (info.supported_attributes & CLM_FILE_ATTRIBUTE_SYMBOLIC_LINK) result.supported_attributes |= CLM_DIRECTORY_ATTRIBUTE_SYMBOLIC_LINK;
#if defined(_WIN32)
    if (info.path.exists && !follow && (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)) {
        wchar_t* wide = clm_path_internal_wide(path);
        HANDLE handle = wide ? CreateFileW(wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL) : INVALID_HANDLE_VALUE;
        free(wide);
        FILE_ATTRIBUTE_TAG_INFO tag;
        if (handle == INVALID_HANDLE_VALUE) return -1;
        const BOOL success = GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, sizeof(tag));
        CloseHandle(handle);
        if (!success) return -1;
        if (tag.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) result.attributes |= CLM_DIRECTORY_ATTRIBUTE_JUNCTION;
    }
#else
    if (info.path.exists && info.path.is_directory) {
        char* parent = clm_directory_internal_join(path, "..");
        struct stat current, parent_info;
        if (!parent || stat(path, &current) || stat(parent, &parent_info)) { free(parent); return -1; }
        free(parent); result.supported_attributes |= CLM_DIRECTORY_ATTRIBUTE_MOUNT_POINT;
        if (current.st_dev != parent_info.st_dev || (current.st_dev == parent_info.st_dev && current.st_ino == parent_info.st_ino))
            result.attributes |= CLM_DIRECTORY_ATTRIBUTE_MOUNT_POINT;
    }
#endif
    result.creation_time = info.creation_time; result.modification_time = info.modification_time; result.access_time = info.access_time;
    result.supported_times = info.supported_times;
    *out = result; return 0;
}

int clm_directory_internal_attribute(const char* path, clm_ulong attribute, bool* out) {
    clm_directory_info info;
    if (!out || clm_directory_internal_info(path, false, &info) || !info.path.exists) return -1;
    if (!(info.supported_attributes & attribute)) return -2;
    *out = (info.attributes & attribute) != 0;
    return 0;
}

int clm_directory_internal_match(const char* pattern, const char* name) {
    if (!pattern) return 1;
    const char* star = NULL;
    const char* retry = NULL;
    while (*name) {
        if (*pattern == '*') { star = pattern++; retry = name; }
        else if (*pattern == '?') {
            ++pattern; ++name;
            while (((unsigned char)*name & 0xc0) == 0x80) ++name;
        }
        else if (*pattern == *name) { ++pattern; ++name; }
        else if (star) {
            pattern = star + 1; ++retry;
            while (((unsigned char)*retry & 0xc0) == 0x80) ++retry;
            name = retry;
        }
        else return 0;
    }
    while (*pattern == '*') ++pattern;
    return !*pattern;
}

int clm_directory_internal_walk(const char* path, const clm_directory_enumeration_options* options,
    clm_directory_visit_fn visit, void* context, clm_uint depth, const clm_directory_internal_ancestor* ancestors) {
    if (depth > 256) return -1;
    clm_directory_internal_snapshot snapshot;
    const bool follow = (options->flags & CLM_DIRECTORY_ENUM_FOLLOW_SYMLINKS) != 0;
    if (clm_directory_internal_snapshot_open(path, follow, &snapshot)) return -1;
    for (const clm_directory_internal_ancestor* previous = ancestors; previous; previous = previous->previous) {
        if (previous->device == snapshot.device && previous->identity == snapshot.identity) {
            clm_directory_internal_snapshot_free(&snapshot); return -1;
        }
    }
    if (ancestors && (options->flags & CLM_DIRECTORY_ENUM_STAY_ON_FILESYSTEM) && ancestors->device != snapshot.device) {
        clm_directory_internal_snapshot_free(&snapshot); return 0;
    }
    const clm_directory_internal_ancestor current = {snapshot.device, snapshot.identity, ancestors};
    int status = 0;
    for (size_t index = 0; index < snapshot.count; ++index) {
        clm_directory_internal_child* child = &snapshot.children[index];
        if (child->hidden && !(options->flags & CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN)) continue;
        if ((!options->types || (options->types & child->type)) && clm_directory_internal_match(options->pattern, child->name)) {
            const clm_directory_entry entry = {child->path, child->name, child->type, depth};
            const int action = visit(&entry, context);
            if (action) { status = action == CLM_DIRECTORY_VISIT_STOP ? CLM_DIRECTORY_OPERATION_STOPPED : -1; break; }
        }
        if (!(options->flags & CLM_DIRECTORY_ENUM_RECURSIVE) || (options->max_depth && depth >= options->max_depth)) continue;
        bool directory = child->type == CLM_DIRECTORY_ENTRY_DIRECTORY;
        if (follow && child->type == CLM_DIRECTORY_ENTRY_LINK) {
            clm_file_info info;
            if (clm_file_internal_info(child->path, true, &info)) { status = -1; break; }
            directory = info.path.is_directory != 0;
        }
        if (directory) status = clm_directory_internal_walk(child->path, options, visit, context, depth + 1, &current);
        if (status) break;
    }
    clm_directory_internal_snapshot_free(&snapshot);
    return status;
}

int clm_directory_internal_enumerate(const char* path, const clm_directory_enumeration_options* options,
    clm_directory_visit_fn visit, void* context) {
    const clm_directory_enumeration_options defaults = {CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN, 0, 0, NULL};
    if (!visit || (options && ((options->flags & ~15u) || (options->types & ~15u)))) return -1;
    size_t characters;
    if (options && options->pattern && clm_string_measure(options->pattern, strlen(options->pattern),
        CLM_STRING_ENCODING_UTF8, &characters) != 0) return -1;
    return clm_directory_internal_walk(path, options ? options : &defaults, visit, context, 1, NULL);
}

char* clm_directory_internal_resolved(const char* path, unsigned depth) {
    if (depth > 256 || !clm_file_internal_valid_path(path)) return NULL;
    char* normalized = clm_path_internal_normalize(path);
    if (!normalized) return NULL;
    clm_file_info info;
    if (clm_file_internal_info(normalized, false, &info) || info.path.is_symlink ||
        (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)) { free(normalized); return NULL; }
    char* result = NULL;
    if (info.path.exists) {
#if defined(_WIN32)
        wchar_t* wide = clm_path_internal_wide(normalized);
        HANDLE handle = wide ? CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL) : INVALID_HANDLE_VALUE;
        free(wide);
        if (handle != INVALID_HANDLE_VALUE) {
            const DWORD count = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED);
            wchar_t* output = count ? (wchar_t*)malloc(((size_t)count + 1) * sizeof(wchar_t)) : NULL;
            if (output) {
                const DWORD written = GetFinalPathNameByHandleW(handle, output, count + 1, FILE_NAME_NORMALIZED);
                if (written && written <= count) result = clm_path_internal_utf8(output);
            }
            free(output); CloseHandle(handle);
        }
#else
        result = realpath(normalized, NULL);
#endif
    } else {
        char* parent = clm_file_internal_parent(normalized);
        if (parent && strcmp(parent, normalized)) {
            char* resolved = clm_directory_internal_resolved(parent, depth + 1);
            size_t end;
            const size_t first = clm_path_internal_filename(normalized, &end);
            char* name = (char*)malloc(end - first + 1);
            if (name) { memcpy(name, normalized + first, end - first); name[end - first] = 0; }
            if (resolved && name) result = clm_directory_internal_join(resolved, name);
            free(resolved); free(name);
        }
        free(parent);
    }
    free(normalized);
    return result;
}

int clm_directory_internal_within(const char* target, const char* base) {
#if defined(_WIN32)
    wchar_t* first = clm_path_internal_wide(target);
    wchar_t* second = clm_path_internal_wide(base);
    if (!first || !second) { free(first); free(second); return -1; }
    const size_t length = wcslen(second), target_length = wcslen(first);
    const int result = length <= target_length && length <= INT_MAX &&
        CompareStringOrdinal(first, (int)length, second, (int)length, TRUE) == CSTR_EQUAL &&
        (length == target_length || second[length - 1] == L'\\' || first[length] == L'\\');
    free(first); free(second); return result;
#else
    const size_t length = strlen(base), target_length = strlen(target);
    return length <= target_length && !memcmp(target, base, length) &&
        (length == target_length || base[length - 1] == '/' || target[length] == '/');
#endif
}

int clm_directory_internal_copy_tree(const char* source, const char* destination,
                                                  bool overwrite, unsigned depth, clm_ulong device) {
    if (depth > 256) return -1;
    clm_directory_internal_snapshot snapshot;
    if (clm_directory_internal_snapshot_open(source, false, &snapshot)) return -1;
    if (depth && snapshot.device != device) { clm_directory_internal_snapshot_free(&snapshot); return -2; }
    if (!depth) device = snapshot.device;
    clm_file_info target;
    int status = clm_file_internal_info(destination, false, &target);
    if (!status && target.path.exists && (!target.path.is_directory || (target.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT))) status = -1;
    if (!status && !target.path.exists) status = clm_directory_internal_create(destination);
    for (size_t index = 0; !status && index < snapshot.count; ++index) {
        clm_directory_internal_child* child = &snapshot.children[index];
        char* output = clm_directory_internal_join(destination, child->name);
        if (!output) { status = -1; break; }
        if (child->type == CLM_DIRECTORY_ENTRY_DIRECTORY)
            status = clm_directory_internal_copy_tree(child->path, output, overwrite, depth + 1, device);
        else if (child->type == CLM_DIRECTORY_ENTRY_FILE)
            status = clm_file_internal_copy_file(child->path, output, overwrite);
        else status = -2;
        free(output);
    }
    clm_directory_internal_snapshot_free(&snapshot);
    return status;
}

int clm_directory_internal_copy(const char* source, const char* destination, bool overwrite) {
    if (clm_directory_internal_guard_root(source) || clm_directory_internal_guard_root(destination)) return -1;
    char* first = clm_directory_internal_resolved(source, 0);
    char* second = clm_directory_internal_resolved(destination, 0);
    if (!first || !second || clm_directory_internal_within(second, first) != 0) { free(first); free(second); return -1; }
    const int status = clm_directory_internal_copy_tree(first, second, overwrite, 0, 0);
    free(first); free(second);
    return status;
}

#if ((!defined(_WIN32)))
int clm_directory_internal_remove_fd(int descriptor, clm_ulong device, unsigned depth) {
    if (depth > 256) return -1;
    const int duplicate = dup(descriptor);
    if (duplicate < 0) return -1;
    DIR* stream = fdopendir(duplicate);
    if (!stream) { close(duplicate); return -1; }
    int status = 0;
    for (;;) {
        errno = 0;
        struct dirent* child = readdir(stream);
        if (!child) { if (errno) status = -1; break; }
        if (!strcmp(child->d_name, ".") || !strcmp(child->d_name, "..")) continue;
        struct stat info;
        if (fstatat(descriptor, child->d_name, &info, AT_SYMLINK_NOFOLLOW)) { status = -1; break; }
        if (S_ISDIR(info.st_mode)) {
            if ((clm_ulong)info.st_dev != device) { status = -2; break; }
            const int nested = openat(descriptor, child->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
            if (nested < 0) { status = -1; break; }
            status = clm_directory_internal_remove_fd(nested, device, depth + 1);
            close(nested);
            if (!status && unlinkat(descriptor, child->d_name, AT_REMOVEDIR)) status = -1;
        } else if (unlinkat(descriptor, child->d_name, 0)) status = -1;
        if (status) break;
    }
    closedir(stream);
    return status;
}
#endif

int clm_directory_internal_remove_tree(const char* path, bool keep, unsigned depth, clm_ulong device) {
    if (depth > 256 || clm_directory_internal_guard_root(path)) return -1;
    clm_directory_internal_snapshot snapshot;
    if (clm_directory_internal_snapshot_open(path, false, &snapshot)) return -1;
    if (depth && snapshot.device != device) { clm_directory_internal_snapshot_free(&snapshot); return -2; }
    if (!depth) device = snapshot.device;
    int status = 0;
#if defined(_WIN32)
    for (size_t index = 0; !status && index < snapshot.count; ++index) {
        clm_directory_internal_child* child = &snapshot.children[index];
        if (child->type == CLM_DIRECTORY_ENTRY_DIRECTORY)
            status = clm_directory_internal_remove_tree(child->path, false, depth + 1, device);
        else {
            wchar_t* wide = clm_path_internal_wide(child->path);
            const DWORD attributes = wide ? GetFileAttributesW(wide) : INVALID_FILE_ATTRIBUTES;
            if (attributes == INVALID_FILE_ATTRIBUTES) status = -1;
            else status = (attributes & FILE_ATTRIBUTE_DIRECTORY ? RemoveDirectoryW(wide) : DeleteFileW(wide)) ? 0 : -1;
            free(wide);
        }
    }
#else
    status = clm_directory_internal_remove_fd(dirfd(snapshot.guard), device, depth);
#endif
    clm_directory_internal_snapshot_free(&snapshot);
    if (!status && !keep) status = clm_directory_internal_remove(path);
    return status;
}

int clm_directory_internal_total_visit(const clm_directory_entry* entry, void* context) {
    if (entry->type != CLM_DIRECTORY_ENTRY_FILE) return 0;
    clm_file_info info;
    clm_ulong* total = (clm_ulong*)context;
    if (clm_file_internal_info(entry->path, false, &info) || !info.path.is_file || info.size > ULONG_MAX - *total) return -1;
    *total += info.size;
    return 0;
}

int clm_directory_internal_size(const char* path, clm_ulong* out) {
    if (!out) return -1;
    const clm_directory_enumeration_options options = {
        CLM_DIRECTORY_ENUM_RECURSIVE | CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN | CLM_DIRECTORY_ENUM_STAY_ON_FILESYSTEM,
        CLM_DIRECTORY_ENTRY_FILE, 0, NULL};
    clm_ulong total = 0;
    const int status = clm_directory_internal_enumerate(path, &options, clm_directory_internal_total_visit, &total);
    if (!status) *out = total;
    return status;
}

int clm_directory_internal_empty(const char* path, bool* out) {
    if (!out) return -1;
    clm_directory_internal_snapshot snapshot;
    if (clm_directory_internal_snapshot_open(path, false, &snapshot)) return -1;
    const bool empty = snapshot.count == 0;
    clm_directory_internal_snapshot_free(&snapshot); *out = empty;
    return 0;
}

int clm_directory_internal_temporary(const char* base, clm_id destination,
    char* buffer, size_t capacity, size_t* required, bool managed) {
    clm_file_info info;
    if (clm_file_internal_info(base, false, &info) || !info.path.is_directory ||
        (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT) || (managed ? !clm_path_internal_text(destination) : !required || (!buffer && capacity))) return -1;
    const size_t needed = strlen(base) + 38;
    if (!managed && (!buffer || capacity < needed)) { *required = needed; return !buffer && !capacity ? 0 : -1; }
    for (int attempt = 0; attempt < 64; ++attempt) {
        char* path = clm_file_internal_temp_name(base, "");
        if (!path) return -1;
        if (!clm_directory_internal_create(path)) {
            if (managed && clm_path_internal_store(destination, path)) {
                clm_directory_internal_remove(path); free(path); return -1;
            }
            if (!managed) { memcpy(buffer, path, strlen(path) + 1); *required = strlen(path) + 1; }
            free(path); return 0;
        }
        clm_file_info collision;
        const bool retry = !clm_file_internal_info(path, false, &collision) && collision.path.exists;
        free(path); if (!retry) return -1;
    }
    return -1;
}

int clm_directory_internal_delete_empty(const char* path) {
    if (clm_directory_internal_guard_root(path)) return -1;
    clm_directory_internal_snapshot snapshot;
    if (clm_directory_internal_snapshot_open(path, false, &snapshot)) return -1;
    const bool empty = snapshot.count == 0;
    clm_directory_internal_snapshot_free(&snapshot);
    return empty ? clm_directory_internal_remove(path) : -1;
}

int clm_directory_internal_set_attributes(const char* path, clm_ulong attributes, clm_ulong mask) {
    if (clm_file_internal_predicate(path, 1) != 1) return -1;
    if (mask & ~UINT64_C(63)) return -2;
    return clm_file_internal_set_attributes(path, attributes, mask);
}

int clm_directory_internal_set_times(const char* path, time_t creation, time_t modification, time_t access, clm_uint mask) {
    if (clm_file_internal_predicate(path, 1) != 1) return -1;
    return clm_file_internal_set_times(path, creation, modification, access, mask);
}

clm_int clm_directory_create(clm_id id) {
    return clm_directory_internal_create(clm_path_internal_text(id));
}

clm_int clm_directory_create_as(const char* path) {
    return clm_directory_internal_create(path);
}

clm_int clm_directory_exists(clm_id id) {
    return clm_file_internal_predicate(clm_path_internal_text(id), 1);
}

clm_int clm_directory_exists_as(const char* path) {
    return clm_file_internal_predicate(path, 1);
}

clm_int clm_directory_move(clm_id id, clm_id new_id) {
    const char* path = clm_path_internal_text(id);
    clm_file_info info;
    if (clm_directory_internal_guard_root(path) || clm_file_internal_info(path, false, &info) ||
        !info.path.is_directory || (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)) return -1;
    return clm_file_internal_move(path, clm_path_internal_text(new_id), false);
}

clm_int clm_directory_move_as(const char* path, const char* new_path) {
    clm_file_info info;
    if (clm_directory_internal_guard_root(path) || clm_file_internal_info(path, false, &info) ||
        !info.path.is_directory || (info.attributes & CLM_FILE_ATTRIBUTE_REPARSE_POINT)) return -1;
    return clm_file_internal_move(path, new_path, false);
}

clm_int clm_directory_delete(clm_id id) {
    return clm_directory_internal_delete_empty(clm_path_internal_text(id));
}

clm_int clm_directory_delete_as(const char* path) {
    return clm_directory_internal_delete_empty(path);
}

clm_int clm_directory_is_empty(clm_id id, bool* b) {
    return clm_directory_internal_empty(clm_path_internal_text(id), b);
}

clm_int clm_directory_is_empty_as(const char* path, bool* b) {
    return clm_directory_internal_empty(path, b);
}

clm_int clm_directory_get_current(clm_id dest) {
    return clm_path_get_current_directory(dest) ? -1 : 0;
}

clm_int clm_directory_get_current_to(char* dest, size_t dest_size) {
    char* current = clm_path_internal_current();
    const int status = clm_directory_internal_buffer(dest, dest_size, current);
    free(current); return status;
}

clm_int clm_directory_set_current(clm_id src) {
    return clm_path_set_current_directory(src) ? -1 : 0;
}

clm_int clm_directory_set_current_as(const char* src) {
    if (!clm_file_internal_valid_path(src)) return -1;
#if defined(_WIN32)
    wchar_t* wide = clm_path_internal_wide(src);
    const int status = wide && SetCurrentDirectoryW(wide) ? 0 : -1;
    free(wide); return status;
#else
    return chdir(src) == 0 ? 0 : -1;
#endif
}

clm_int clm_directory_find_directories(clm_id id, clm_id results) {
    // results is registered string array in string.h
    return clm_directory_internal_collect(clm_path_internal_text(id), CLM_DIRECTORY_ENTRY_DIRECTORY, results, NULL, 0, NULL, NULL, true);
}

clm_int clm_directory_find_directories_as(const char* path, clm_id results) {
    // results is registered string array in string.h
    return clm_directory_internal_collect(path, CLM_DIRECTORY_ENTRY_DIRECTORY, results, NULL, 0, NULL, NULL, true);
}

clm_int clm_directory_find_directories_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size, clm_ulong* count) {
    // buffer is allocated by the caller and will be filled with the found directories as paths
    return clm_directory_internal_collect(path, CLM_DIRECTORY_ENTRY_DIRECTORY, 0, buffer, buffer_size, required_size, count, false);
}

clm_int clm_directory_find_files(clm_id id, clm_id results) {
    // results is registered string array in string.h
    return clm_directory_internal_collect(clm_path_internal_text(id), CLM_DIRECTORY_ENTRY_FILE, results, NULL, 0, NULL, NULL, true);
}

clm_int clm_directory_find_files_as(const char* path, clm_id results) {
    // results is registered string array in string.h
    return clm_directory_internal_collect(path, CLM_DIRECTORY_ENTRY_FILE, results, NULL, 0, NULL, NULL, true);
}

clm_int clm_directory_find_files_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size, clm_ulong* count) {
    // buffer is allocated by the caller and will be filled with the found files as paths
    return clm_directory_internal_collect(path, CLM_DIRECTORY_ENTRY_FILE, 0, buffer, buffer_size, required_size, count, false);
}

clm_int clm_directory_get_creation_time(clm_id id, time_t* out) {
    const char* path = clm_path_internal_text(id);
    return clm_file_internal_predicate(path, 1) == 1 ? clm_file_internal_time(path, out, 1) : -1;
}

clm_int clm_directory_get_creation_time_as(const char* path, time_t* out) {
    return clm_file_internal_predicate(path, 1) == 1 ? clm_file_internal_time(path, out, 1) : -1;
}

clm_int clm_directory_get_modification_time(clm_id id, time_t* out) {
    const char* path = clm_path_internal_text(id);
    return clm_file_internal_predicate(path, 1) == 1 ? clm_file_internal_time(path, out, 2) : -1;
}

clm_int clm_directory_get_modification_time_as(const char* path, time_t* out) {
    return clm_file_internal_predicate(path, 1) == 1 ? clm_file_internal_time(path, out, 2) : -1;
}

clm_int clm_directory_get_access_time(clm_id id, time_t* out) {
    const char* path = clm_path_internal_text(id);
    return clm_file_internal_predicate(path, 1) == 1 ? clm_file_internal_time(path, out, 4) : -1;
}

clm_int clm_directory_get_access_time_as(const char* path, time_t* out) {
    return clm_file_internal_predicate(path, 1) == 1 ? clm_file_internal_time(path, out, 4) : -1;
}

clm_int clm_directory_get_size(clm_id id, clm_ulong* out) {
    return clm_directory_internal_size(clm_path_internal_text(id), out);
}

clm_int clm_directory_get_size_as(const char* path, clm_ulong* out) {
    return clm_directory_internal_size(path, out);
}

clm_int clm_directory_get_temporary(clm_id out) {
    char* base = clm_directory_internal_temporary_base();
    const int status = base && !clm_path_internal_store(out, base) ? 0 : -1;
    free(base); return status;
}

clm_int clm_directory_get_temporary_to(char* out, size_t out_size) {
    // out is a pointer where the temporary directory will be stored
    char* base = clm_directory_internal_temporary_base();
    const int status = clm_directory_internal_buffer(out, out_size, base);
    free(base); return status;
}

clm_int clm_directory_is_read_only(clm_id id, bool* out) {
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_READ_ONLY, out);
}

clm_int clm_directory_is_read_only_as(const char* path, bool* out) {
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_READ_ONLY, out);
}

clm_int clm_directory_is_hidden(clm_id id, bool* out) {
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_HIDDEN, out);
}

clm_int clm_directory_is_hidden_as(const char* path, bool* out) {
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_HIDDEN, out);
}

clm_int clm_directory_is_system(clm_id id, bool* out) {
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_SYSTEM, out);
}

clm_int clm_directory_is_system_as(const char* path, bool* out) {
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_SYSTEM, out);
}

clm_int clm_directory_is_archive(clm_id id, bool* out) {
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_ARCHIVE, out);
}

clm_int clm_directory_is_archive_as(const char* path, bool* out) {
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_ARCHIVE, out);
}

clm_int clm_directory_is_temporary(clm_id id, bool* out) {
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_TEMPORARY, out);
}

clm_int clm_directory_is_temporary_as(const char* path, bool* out) {
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_TEMPORARY, out);
}

clm_int clm_directory_is_offline(clm_id id, bool* out) {
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_OFFLINE, out);
}

clm_int clm_directory_is_offline_as(const char* path, bool* out) {
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_OFFLINE, out);
}

clm_int clm_directory_is_encrypted(clm_id id, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_ENCRYPTED, out);
}

clm_int clm_directory_is_encrypted_as(const char* path, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_ENCRYPTED, out);
}

clm_int clm_directory_is_compressed(clm_id id, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_COMPRESSED, out);
}

clm_int clm_directory_is_compressed_as(const char* path, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_COMPRESSED, out);
}

clm_int clm_directory_is_reparse_point(clm_id id, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_REPARSE_POINT, out);
}

clm_int clm_directory_is_reparse_point_as(const char* path, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_REPARSE_POINT, out);
}

clm_int clm_directory_is_mount_point(clm_id id, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_MOUNT_POINT, out);
}

clm_int clm_directory_is_mount_point_as(const char* path, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_MOUNT_POINT, out);
}

clm_int clm_directory_is_junction(clm_id id, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_JUNCTION, out);
}

clm_int clm_directory_is_junction_as(const char* path, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_JUNCTION, out);
}

clm_int clm_directory_is_symbolic_link(clm_id id, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(clm_path_internal_text(id), CLM_DIRECTORY_ATTRIBUTE_SYMBOLIC_LINK, out);
}

clm_int clm_directory_is_symbolic_link_as(const char* path, bool* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_attribute(path, CLM_DIRECTORY_ATTRIBUTE_SYMBOLIC_LINK, out);
}

clm_int clm_directory_get_directories_count(clm_id id, clm_ulong* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_count(clm_path_internal_text(id), CLM_DIRECTORY_ENTRY_DIRECTORY, out);
}

clm_int clm_directory_get_directories_count_as(const char* path, clm_ulong* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_count(path, CLM_DIRECTORY_ENTRY_DIRECTORY, out);
}

clm_int clm_directory_get_files_count(clm_id id, clm_ulong* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_count(clm_path_internal_text(id), CLM_DIRECTORY_ENTRY_FILE, out);
}

clm_int clm_directory_get_files_count_as(const char* path, clm_ulong* out) {
    // out is a pointer where the result will be stored
    return clm_directory_internal_count(path, CLM_DIRECTORY_ENTRY_FILE, out);
}

clm_int clm_directory_create_recursive(clm_id id) {
    return clm_directory_internal_create_recursive(clm_path_internal_text(id), 0);
}

clm_int clm_directory_create_recursive_as(const char* path) {
    return clm_directory_internal_create_recursive(path, 0);
}

clm_int clm_directory_copy(clm_id id, clm_id destination, bool overwrite) {
    return clm_directory_internal_copy(clm_path_internal_text(id), clm_path_internal_text(destination), overwrite);
}

clm_int clm_directory_copy_as(const char* path, const char* destination, bool overwrite) {
    return clm_directory_internal_copy(path, destination, overwrite);
}

clm_int clm_directory_delete_recursive(clm_id id) {
    return clm_directory_internal_remove_tree(clm_path_internal_text(id), false, 0, 0);
}

clm_int clm_directory_delete_recursive_as(const char* path) {
    return clm_directory_internal_remove_tree(path, false, 0, 0);
}

clm_int clm_directory_clear(clm_id id) {
    return clm_directory_internal_remove_tree(clm_path_internal_text(id), true, 0, 0);
}

clm_int clm_directory_clear_as(const char* path) {
    return clm_directory_internal_remove_tree(path, true, 0, 0);
}

clm_int clm_directory_find_entries(clm_id id, clm_id results) {
    return clm_directory_internal_collect(clm_path_internal_text(id), 15, results, NULL, 0, NULL, NULL, true);
}

clm_int clm_directory_find_entries_as(const char* path, clm_id results) {
    return clm_directory_internal_collect(path, 15, results, NULL, 0, NULL, NULL, true);
}

clm_int clm_directory_find_entries_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size, clm_ulong* count) {
    return clm_directory_internal_collect(path, 15, 0, buffer, buffer_size, required_size, count, false);
}

clm_int clm_directory_enumerate(clm_id id, const clm_directory_enumeration_options* options, clm_directory_visit_fn visit, void* context) {
    return clm_directory_internal_enumerate(clm_path_internal_text(id), options, visit, context);
}

clm_int clm_directory_enumerate_as(const char* path, const clm_directory_enumeration_options* options, clm_directory_visit_fn visit, void* context) {
    return clm_directory_internal_enumerate(path, options, visit, context);
}

clm_int clm_directory_get_info(clm_id id, bool follow_symlinks, clm_directory_info* out) {
    return clm_directory_internal_info(clm_path_internal_text(id), follow_symlinks, out);
}

clm_int clm_directory_get_info_as(const char* path, bool follow_symlinks, clm_directory_info* out) {
    return clm_directory_internal_info(path, follow_symlinks, out);
}

clm_int clm_directory_set_attributes(clm_id id, clm_ulong attributes, clm_ulong mask) {
    return clm_directory_internal_set_attributes(clm_path_internal_text(id), attributes, mask);
}

clm_int clm_directory_set_attributes_as(const char* path, clm_ulong attributes, clm_ulong mask) {
    return clm_directory_internal_set_attributes(path, attributes, mask);
}

clm_int clm_directory_set_times(clm_id id, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask) {
    return clm_directory_internal_set_times(clm_path_internal_text(id), creation_time, modification_time, access_time, mask);
}

clm_int clm_directory_set_times_as(const char* path, time_t creation_time, time_t modification_time, time_t access_time, clm_uint mask) {
    return clm_directory_internal_set_times(path, creation_time, modification_time, access_time, mask);
}

clm_int clm_directory_get_entries_count(clm_id id, clm_ulong* out) {
    return clm_directory_internal_count(clm_path_internal_text(id), 15, out);
}

clm_int clm_directory_get_entries_count_as(const char* path, clm_ulong* out) {
    return clm_directory_internal_count(path, 15, out);
}

clm_int clm_directory_create_temporary(clm_id base, clm_id out) {
    return clm_directory_internal_temporary(clm_path_internal_text(base), out, NULL, 0, NULL, true);
}

clm_int clm_directory_create_temporary_as(const char* path, clm_id out) {
    return clm_directory_internal_temporary(path, out, NULL, 0, NULL, true);
}

clm_int clm_directory_create_temporary_to(const char* path, char* buffer, size_t buffer_size, size_t* required_size) {
    return clm_directory_internal_temporary(path, 0, buffer, buffer_size, required_size, false);
}
