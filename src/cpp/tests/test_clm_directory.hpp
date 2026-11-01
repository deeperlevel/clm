#pragma once

#include "test.hpp"
#include "../clm/directory.hpp"
#include "../clm/gen.hpp"

typedef struct {
    clm_ulong count;
    clm_uint greatest_depth;
    bool stop;
} clm_directory_test_visits;

static clm_int clm_directory_test_visit(const clm_directory_entry* entry, void* context)
{
    clm_directory_test_visits* visits = (clm_directory_test_visits*)context;
    if (!entry || !entry->path || !entry->name || !entry->type || !entry->depth) return -1;
    ++visits->count;
    if (entry->depth > visits->greatest_depth) visits->greatest_depth = entry->depth;
    return visits->stop ? CLM_DIRECTORY_VISIT_STOP : CLM_DIRECTORY_VISIT_CONTINUE;
}

static int clm_directory_test_set(clm_id id, const char* text)
{
    return clm_bpa_set_string(id, text, (clm_uint)strlen(text) + 1) == (int)strlen(text);
}

static int clm_directory_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Directory");
    char path[128];
    snprintf(path, sizeof(path), "clm_directory_test_%llu", (unsigned long long)clm_gen_random_u64(0, ULONG_MAX));
    const int created = clm_directory_create_as(path) == 0;
    CLM_ASSERT(created && clm_directory_exists_as(path) == 1, "exclusively create and classify a directory");
    if (!created) { clm_test_summary(result.passed, result.failed); return 1; }
    char child[512], source_file[512], hidden_file[512], nested_file[512], copy[512], moved[512];
    snprintf(child, sizeof(child), "%s/child/nested", path);
    snprintf(source_file, sizeof(source_file), "%s/a.bin", path);
    snprintf(hidden_file, sizeof(hidden_file), "%s/.hidden", path);
    snprintf(nested_file, sizeof(nested_file), "%s/deep.bin", child);
    snprintf(copy, sizeof(copy), "%s-copy", path);
    snprintf(moved, sizeof(moved), "%s-moved", path);
    CLM_ASSERT(clm_string_new(9401, path) && clm_string_new(9402, child) && clm_string_new(9403, copy) &&
               clm_string_new(9405, "output") && clm_string_array_new(9404, 0) && clm_bpa_create(9406, CLM_BPA_TYPE_INT) == 0,
               "create directory string, array, and wrong-type fixtures");
    bool empty = false;
    CLM_ASSERT(clm_directory_exists(9401) == 1 && clm_directory_is_empty(9401, &empty) == 0 && empty &&
               clm_directory_is_empty_as(path, &empty) == 0 && empty && clm_directory_create(9401) == -1 &&
               clm_directory_exists(9499) == -1, "distinguish empty directories, create collisions, and invalid IDs");
    CLM_ASSERT(clm_directory_create_recursive(9402) == 0 && clm_directory_create_recursive_as(child) == 0 &&
               clm_file_write_all_from(source_file, "abc", 3) == 0 &&
               clm_file_write_all_from(hidden_file, "hi", 2) == 0 && clm_file_write_all_from(nested_file, "payload", 7) == 0,
               "create a nested tree with distinct regular files and hidden content");
#if defined(_WIN32)
    CLM_ASSERT(clm_file_set_attributes_as(hidden_file, CLM_FILE_ATTRIBUTE_HIDDEN, CLM_FILE_ATTRIBUTE_HIDDEN) == 0,
               "mark the hidden fixture using native Windows attributes");
#endif
    CLM_ASSERT(clm_directory_is_empty_as(path, &empty) == 0 && !empty && clm_directory_delete(9401) == -1,
               "refuse empty-only deletion of nonempty trees");
    clm_ulong count = 0, bytes = 0;
    CLM_ASSERT(clm_directory_get_directories_count(9401, &count) == 0 && count == 1 &&
               clm_directory_get_directories_count_as(path, &count) == 0 && count == 1 &&
               clm_directory_get_files_count(9401, &count) == 0 && count == 2 &&
               clm_directory_get_files_count_as(path, &count) == 0 && count == 2 &&
               clm_directory_get_entries_count(9401, &count) == 0 && count == 3 &&
               clm_directory_get_entries_count_as(path, &count) == 0 && count == 3,
               "align immediate directory, file, and mixed-entry count semantics");
    CLM_ASSERT(clm_directory_find_directories(9401, 9404) == 0 && clm_bpaa_find_array(9404)->count == 1 &&
               clm_directory_find_directories_as(path, 9404) == 0 &&
               clm_directory_find_files(9401, 9404) == 0 && clm_bpaa_find_array(9404)->count == 2 &&
               clm_directory_find_files_as(path, 9404) == 0 &&
               clm_directory_find_entries(9401, 9404) == 0 && clm_bpaa_find_array(9404)->count == 3 &&
               clm_directory_find_entries_as(path, 9404) == 0,
               "replace existing string arrays consistently across ID and raw-path listing adapters");
    char packed[4096]; memset(packed, '!', sizeof(packed));
    size_t required = 0;
    CLM_ASSERT(clm_directory_find_entries_to(path, NULL, 0, &required, &count) == 0 && count == 3 && required > 1 &&
               clm_directory_find_entries_to(path, packed, 1, &required, &count) == -1 && packed[0] == '!' &&
               clm_directory_find_entries_to(path, packed, sizeof(packed), &required, &count) == 0 &&
               packed[required - 1] == 0 && packed[required - 2] == 0 &&
               clm_directory_find_directories_to(path, packed, sizeof(packed), &required, &count) == 0 && count == 1 &&
               clm_directory_find_files_to(path, packed, sizeof(packed), &required, &count) == 0 && count == 2,
               "measure and export complete NUL-separated paths without insufficient-buffer writes");
    clm_directory_test_visits visits = {0};
    CLM_ASSERT(clm_directory_enumerate(9401, NULL, clm_directory_test_visit, &visits) == 0 && visits.count == 3 &&
               visits.greatest_depth == 1, "enumerate borrowed immediate entry data through callbacks");
    clm_directory_enumeration_options options = {CLM_DIRECTORY_ENUM_RECURSIVE | CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN, 0, 0, NULL};
    memset(&visits, 0, sizeof(visits));
    CLM_ASSERT(clm_directory_enumerate_as(path, &options, clm_directory_test_visit, &visits) == 0 &&
               visits.count == 5 && visits.greatest_depth == 3, "walk nested trees with explicit recursion and depth reporting");
    options.types = CLM_DIRECTORY_ENTRY_FILE; options.pattern = "*.bin";
    memset(&visits, 0, sizeof(visits));
    CLM_ASSERT(clm_directory_enumerate_as(path, &options, clm_directory_test_visit, &visits) == 0 && visits.count == 2,
               "filter callback results without pruning unmatched parent directories");
    options.pattern = NULL; options.types = 0; options.max_depth = 1; options.flags = CLM_DIRECTORY_ENUM_RECURSIVE;
    memset(&visits, 0, sizeof(visits));
    CLM_ASSERT(clm_directory_enumerate_as(path, &options, clm_directory_test_visit, &visits) == 0 && visits.count == 2 &&
               visits.greatest_depth == 1, "respect hidden exclusion and maximum traversal depth");
    memset(&visits, 0, sizeof(visits)); visits.stop = true;
    CLM_ASSERT(clm_directory_enumerate_as(path, NULL, clm_directory_test_visit, &visits) == CLM_DIRECTORY_OPERATION_STOPPED &&
               visits.count == 1, "report deliberate callback cancellation distinctly from failure");
    CLM_ASSERT(clm_directory_get_size(9401, &bytes) == 0 && bytes == 12 && clm_directory_get_size_as(path, &bytes) == 0 && bytes == 12,
               "sum recursive logical regular-file sizes including hidden files");
    CLM_ASSERT(clm_directory_copy(9401, 9403, false) == 0 && clm_directory_copy_as(path, copy, false) == -1 &&
               clm_directory_copy_as(path, copy, true) == 0 && clm_directory_copy(9401, 9401, true) == -1 &&
               clm_directory_copy_as(path, child, true) == -1,
               "copy trees with explicit overwrite policy and reject source/subtree destinations");
    CLM_ASSERT(clm_directory_clear(9403) == 0 && clm_directory_exists_as(copy) == 1 &&
               clm_directory_is_empty_as(copy, &empty) == 0 && empty &&
               clm_directory_test_set(9405, moved) && clm_directory_move(9403, 9405) == 0 &&
               clm_directory_move_as(moved, copy) == 0 && clm_directory_delete_as(copy) == 0,
               "clear while retaining roots, move without overwrite, and remove empty directories");
    CLM_ASSERT(clm_directory_copy_as(path, copy, false) == 0 && clm_directory_delete_recursive_as(copy) == 0,
               "remove copied trees through explicit recursive deletion");
    char current[4096];
    CLM_ASSERT(clm_directory_get_current_to(current, sizeof(current)) == 0 &&
               clm_directory_get_current(9405) == 0 && clm_directory_set_current(9401) == 0 &&
               clm_directory_set_current(9405) == 0 && clm_directory_set_current_as(current) == 0,
               "share working-directory integration and restore process-wide state");
    CLM_ASSERT(clm_directory_get_temporary(9405) == 0 && clm_directory_get_temporary_to(packed, sizeof(packed)) == 0 &&
               clm_directory_exists_as(packed) == 1, "obtain an existing temporary base without creating an entry");
    CLM_ASSERT(clm_directory_create_temporary_to(path, NULL, 0, &required) == 0 &&
               clm_directory_create_temporary_to(path, packed, 1, &required) == -1 &&
               clm_directory_get_entries_count_as(path, &count) == 0 && count == 3 &&
               clm_directory_create_temporary(9401, 9405) == 0 && clm_directory_delete(9405) == 0 &&
               clm_directory_create_temporary_as(path, 9405) == 0 && clm_directory_delete(9405) == 0 &&
               clm_directory_create_temporary_to(path, packed, sizeof(packed), &required) == 0 &&
               clm_directory_delete_as(packed) == 0,
               "create unique temporary directories exclusively without measuring or truncation side effects");
    clm_directory_info info;
    time_t timestamp = 0, chosen = time(NULL) - 30;
    CLM_ASSERT(clm_directory_get_info(9401, false, &info) == 0 && info.path.is_directory &&
               clm_directory_get_info_as(path, true, &info) == 0 &&
               clm_directory_set_times(9401, 0, chosen, chosen, 6) == 0 &&
               clm_directory_set_times_as(path, 0, chosen, chosen, 6) == 0 &&
               clm_directory_get_modification_time(9401, &timestamp) == 0 && timestamp == chosen &&
               clm_directory_get_modification_time_as(path, &timestamp) == 0 &&
               clm_directory_get_access_time(9401, &timestamp) == 0 && clm_directory_get_access_time_as(path, &timestamp) == 0,
               "query and update supported directory metadata with typed results");
    typedef clm_int (*id_attribute_fn)(clm_id, bool*);
    typedef clm_int (*raw_attribute_fn)(const char*, bool*);
    const id_attribute_fn id_queries[] = {clm_directory_is_read_only, clm_directory_is_hidden, clm_directory_is_system,
        clm_directory_is_archive, clm_directory_is_temporary, clm_directory_is_offline, clm_directory_is_encrypted,
        clm_directory_is_compressed, clm_directory_is_reparse_point, clm_directory_is_mount_point,
        clm_directory_is_junction, clm_directory_is_symbolic_link};
    const raw_attribute_fn raw_queries[] = {clm_directory_is_read_only_as, clm_directory_is_hidden_as, clm_directory_is_system_as,
        clm_directory_is_archive_as, clm_directory_is_temporary_as, clm_directory_is_offline_as, clm_directory_is_encrypted_as,
        clm_directory_is_compressed_as, clm_directory_is_reparse_point_as, clm_directory_is_mount_point_as,
        clm_directory_is_junction_as, clm_directory_is_symbolic_link_as};
    bool attribute = false;
    int attributes_valid = clm_directory_get_info_as(path, false, &info) == 0;
    for (size_t index = 0; index < 12; ++index) {
        const int expected = info.supported_attributes & (UINT64_C(1) << index) ? 0 : -2;
        attributes_valid &= id_queries[index](9401, &attribute) == expected && raw_queries[index](path, &attribute) == expected;
    }
    CLM_ASSERT(attributes_valid, "keep all attribute adapters consistent with metadata capability masks");
#if defined(_WIN32)
    CLM_ASSERT(clm_directory_get_creation_time(9401, &timestamp) == 0 && clm_directory_get_creation_time_as(path, &timestamp) == 0 &&
               clm_directory_set_attributes(9401, CLM_DIRECTORY_ATTRIBUTE_HIDDEN, CLM_DIRECTORY_ATTRIBUTE_HIDDEN) == 0 &&
               clm_directory_is_hidden_as(path, &attribute) == 0 && attribute &&
               clm_directory_set_attributes_as(path, 0, CLM_DIRECTORY_ATTRIBUTE_HIDDEN) == 0,
               "expose native directory creation time and supported mutable attributes");
#else
    CLM_ASSERT(clm_directory_get_creation_time(9401, &timestamp) == -2 &&
               clm_directory_set_attributes_as(path, CLM_DIRECTORY_ATTRIBUTE_SYSTEM, CLM_DIRECTORY_ATTRIBUTE_SYSTEM) == -2,
               "do not fabricate unsupported native directory attributes or birth time");
#endif
    char outside[512], sentinel[768], link[768];
    snprintf(outside, sizeof(outside), "%s-outside", path);
    snprintf(sentinel, sizeof(sentinel), "%s/keep", outside);
    snprintf(link, sizeof(link), "%s/link", path);
    CLM_ASSERT(clm_directory_create_as(outside) == 0 && clm_file_write_all_from(sentinel, "keep", 4) == 0,
               "prepare an outside sentinel for no-follow traversal tests");
#if defined(_WIN32)
    char* resolved_outside = clm_directory_internal_resolved(outside, 0);
    wchar_t* wide_target = resolved_outside ? clm_path_internal_wide(resolved_outside) : NULL;
    wchar_t* wide_link = clm_path_internal_wide(link);
    const bool linked = wide_target && wide_link && CreateSymbolicLinkW(wide_link, wide_target, 3);
    free(wide_target); free(wide_link); free(resolved_outside);
#else
    char* resolved_outside = realpath(outside, NULL);
    const bool linked = resolved_outside && symlink(resolved_outside, link) == 0;
    free(resolved_outside);
#endif
    if (linked) {
        CLM_ASSERT(clm_directory_internal_match("caf?.txt", "caf\xc3\xa9.txt") &&
               clm_directory_internal_match("*?.txt", "\xc3\xa9.txt"),
               "match wildcard question marks by UTF-8 code point rather than byte");
        char cycle[768]; snprintf(cycle, sizeof(cycle), "%s/cycle", child);
    #if defined(_WIN32)
        char* cycle_target = clm_directory_internal_resolved(path, 0);
        wchar_t* cycle_wide = clm_path_internal_wide(cycle);
        wchar_t* target_wide = cycle_target ? clm_path_internal_wide(cycle_target) : NULL;
        const bool cycled = cycle_wide && target_wide && CreateSymbolicLinkW(cycle_wide, target_wide, 3);
        free(cycle_target); free(cycle_wide); free(target_wide);
    #else
        char* cycle_target = realpath(path, NULL);
        const bool cycled = cycle_target && symlink(cycle_target, cycle) == 0;
        free(cycle_target);
    #endif
        if (cycled) {
            options.flags = CLM_DIRECTORY_ENUM_RECURSIVE | CLM_DIRECTORY_ENUM_INCLUDE_HIDDEN | CLM_DIRECTORY_ENUM_FOLLOW_SYMLINKS;
            options.max_depth = 0; options.types = 0; options.pattern = NULL;
            memset(&visits, 0, sizeof(visits));
            CLM_ASSERT(clm_directory_enumerate_as(path, &options, clm_directory_test_visit, &visits) == -1,
                   "detect recursive directory-link cycles by native identity");
        }
        CLM_ASSERT(clm_directory_is_symbolic_link_as(link, &attribute) == 0 && attribute &&
                   clm_directory_get_size_as(path, &bytes) == 0 && bytes == 12 &&
                   clm_directory_clear_as(path) == 0 && clm_file_exists_as(sentinel) == 1,
                   "clear link entries without traversing or deleting their outside targets");
    } else {
        clm_test_write_line("    SKIPPED: directory-link fixtures require native permission");
        CLM_ASSERT(clm_directory_clear_as(path) == 0, "clear regular descendants while keeping the directory root");
    }
    CLM_ASSERT(clm_directory_delete_recursive_as(outside) == 0 && clm_directory_is_empty_as(path, &empty) == 0 && empty,
               "clean up outside fixtures and verify the retained root is empty");
    CLM_ASSERT(clm_directory_delete_recursive_as("/") == -1 && clm_directory_clear_as(".") == -1 &&
               clm_directory_create_recursive(9499) == -1 && clm_directory_find_entries(9406, 9404) == -1 &&
               clm_directory_find_entries(9401, 9406) == -1 && clm_directory_get_info_as(path, true, NULL) == -1 &&
               clm_directory_enumerate_as(path, NULL, NULL, NULL) == -1,
               "reject roots, invalid IDs, wrong result types, and missing output callbacks");
    CLM_ASSERT(clm_directory_test_set(9405, copy) && clm_directory_create(9405) == 0 && clm_directory_delete_recursive(9405) == 0,
               "exercise ID-based creation and recursive teardown independently");
    CLM_ASSERT(clm_bpa_delete(9401) == 0 && clm_bpa_delete(9402) == 0 && clm_bpa_delete(9403) == 0 &&
               clm_bpaa_delete(9404) == 0 && clm_bpa_delete(9405) == 0 && clm_bpa_delete(9406) == 0,
               "release all directory fixture registrations");
    if (created) CLM_ASSERT(clm_directory_delete_as(path) == 0, "remove the isolated directory fixture");
    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}

static int test_clm_directory(void)
{
    return clm_directory_tests();
}

