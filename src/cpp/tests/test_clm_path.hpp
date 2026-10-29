#pragma once

#include "test.hpp"
#include "../clm/path.hpp"

#include <cstring>

static int clm_path_test_set(clm_id id, const char* text)
{
    return clm_bpa_set_string(id, text, static_cast<clm_uint>(std::strlen(text)) + 1) == static_cast<int>(std::strlen(text));
}

static int clm_path_test_equals(clm_id id, const char* expected)
{
    char buffer[2048];
    return clm_bpa_get_string(id, buffer, sizeof(buffer)) == static_cast<int>(std::strlen(expected)) &&
           std::strcmp(buffer, expected) == 0;
}

static int test_clm_path()
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Path");
    CLM_ASSERT(clm_bpa_create(8101, clm::bpa::CLM_BPA_TYPE_STRING) == clm::bpa::CLM_BPA_SUCCESS,
               "create an empty path string");
    CLM_ASSERT(clm_path_is_empty(8101) == 1,
               "recognize an existing empty path string");
    CLM_ASSERT(clm_bpa_create(8102, clm::bpa::CLM_BPA_TYPE_STRING) == clm::bpa::CLM_BPA_SUCCESS &&
               clm_bpa_create(8103, clm::bpa::CLM_BPA_TYPE_STRING) == clm::bpa::CLM_BPA_SUCCESS &&
               clm_bpa_create(8104, clm::bpa::CLM_BPA_TYPE_INT) == clm::bpa::CLM_BPA_SUCCESS,
               "create path destinations and a wrong-type fixture");
    CLM_ASSERT(clm_path_is_empty(8199) == -1 && clm_path_is_empty(8104) == -1,
               "distinguish invalid path references from false predicates");
#if defined(_WIN32)
    const char* absolute = "C:\\work\\reports\\annual.txt";
    const char* parent = "C:\\work\\reports";
    const char* normalized = "C:\\work\\annual.txt";
#else
    const char* absolute = "/work/reports/annual.txt";
    const char* parent = "/work/reports";
    const char* normalized = "/work/annual.txt";
#endif
    CLM_ASSERT(clm_path_test_set(8101, absolute) && clm_path_is_absolute(8101) == 1 &&
               clm_path_is_relative(8101) == 0 && clm_path_is_rooted(8101) == 1 &&
               clm_path_is_fully_qualified(8101) == 1 && clm_path_is_valid(8101) == 1,
               "classify an absolute native path");
    CLM_ASSERT(clm_path_has_extension(8101) == 1 && clm_path_get_filename(8101) == 0 &&
               clm_path_test_equals(8101, "annual.txt"), "extract a filename in place");
    CLM_ASSERT(clm_path_test_set(8101, absolute) && clm_path_get_parent(8101) == 0 &&
               clm_path_test_equals(8101, parent), "extract a native parent directory");
    CLM_ASSERT(clm_path_test_set(8101, absolute) && clm_path_get_filename_without_extension(8101) == 0 &&
               clm_path_test_equals(8101, "annual"), "extract a filename stem");
    CLM_ASSERT(clm_path_test_set(8101, absolute) && clm_path_get_extension(8101) == 0 &&
               clm_path_test_equals(8101, ".txt"), "include the leading dot in an extension");
    CLM_ASSERT(clm_path_test_set(8101, ".gitignore") && clm_path_has_extension(8101) == 0 &&
               clm_path_get_extension(8101) == 0 && clm_path_test_equals(8101, ""),
               "do not treat a dotfile name as an extension");
    CLM_ASSERT(clm_path_test_set(8101, "archive.tar.gz") && clm_path_remove_extension(8101) == 0 &&
               clm_path_test_equals(8101, "archive.tar"), "remove only the final extension");
    CLM_ASSERT(clm_path_test_set(8102, "json") && clm_path_change_extension(8101, 8102) == 0 &&
               clm_path_test_equals(8101, "archive.json"), "replace an extension without requiring a dot");
    CLM_ASSERT(clm_path_test_set(8101, "a") && clm_path_join(8101, 8101) == 0,
               "join safely when both input IDs alias");
#if defined(_WIN32)
    CLM_ASSERT(clm_path_test_equals(8101, "a\\a"), "use the native separator when joining");
    CLM_ASSERT(clm_path_test_set(8101, "C:\\work\\reports\\..\\annual.txt") &&
               clm_path_normalize(8101) == 0 && clm_path_test_equals(8101, normalized),
               "normalize native path dot segments");
    CLM_ASSERT(clm_path_test_set(8101, "C:relative") && clm_path_is_rooted(8101) == 1 &&
               clm_path_is_fully_qualified(8101) == 0 && clm_path_is_relative(8101) == 1,
               "distinguish drive-relative from fully qualified paths");
    CLM_ASSERT(clm_path_test_set(8101, "CON.txt") && clm_path_is_valid(8101) == 0 &&
               clm_path_test_set(8101, "bad?.txt") && clm_path_is_valid(8101) == 0,
               "reject reserved Windows names and forbidden filename characters");
#else
    CLM_ASSERT(clm_path_test_equals(8101, "a/a"), "use the native separator when joining");
    CLM_ASSERT(clm_path_test_set(8101, "/work/reports/../annual.txt") &&
               clm_path_normalize(8101) == 0 && clm_path_test_equals(8101, normalized),
               "normalize native path dot segments");
#endif
    CLM_ASSERT(clm_path_test_set(8101, "a/../b") && clm_path_sanitize(8101) == 0 &&
               clm_path_test_equals(8101, "b"), "sanitize through lexical normalization");
    CLM_ASSERT(clm_path_test_set(8101, absolute) && clm_path_test_set(8102, parent) &&
               clm_path_get_relative_path(8101, 8102) == 0 &&
               clm_path_test_equals(8101, "annual.txt"), "derive a relative path from an absolute base");
    CLM_ASSERT(clm_path_test_set(8101, absolute) && clm_path_is_within(8101, 8102) == 1 &&
               clm_path_is_within(8102, 8102) == 1, "check component containment including equality");
    CLM_ASSERT(clm_path_test_set(8101, "app2/data") && clm_path_test_set(8102, "app") &&
               clm_path_is_within(8101, 8102) == 0, "do not confuse text prefixes with path containment");
    CLM_ASSERT(clm_path_test_set(8101, "relative") && clm_path_ensure_ending_separator(8101) == 0 &&
               clm_path_ends_with_separator(8101) == 1 && clm_path_trim_ending_separator(8101) == 0 &&
               clm_path_test_equals(8101, "relative"), "ensure and trim a trailing separator");
    CLM_ASSERT(clm_path_test_set(8101, "///relative") && clm_path_trim_starting_separator(8101) == 0 &&
               clm_path_test_equals(8101, "relative"), "explicitly remove leading separators");
    CLM_ASSERT(clm_path_test_set(8101, "report-old.txt") && clm_path_test_set(8102, "old") &&
               clm_path_test_set(8103, "new") && clm_path_replace(8101, 8102, 8103) == 0 &&
               clm_path_test_equals(8101, "report-new.txt"), "replace non-overlapping literal path text");
    CLM_ASSERT(clm_path_test_set(8101, "\\\\server\\share\\dir\\file.txt") &&
               clm_path_is_unc(8101) == 1 && clm_path_is_extended_unc(8101) == 0 &&
               clm_path_get_unc_server(8101, 8102) == 0 && clm_path_test_equals(8102, "server") &&
               clm_path_get_unc_share(8101, 8103) == 0 && clm_path_test_equals(8103, "share"),
               "recognize and split a complete UNC path");
    CLM_ASSERT(clm_path_to_extended_unc(8101) == 0 && clm_path_is_extended_unc(8101) == 1 &&
               clm_path_from_extended_unc(8101) == 0 &&
               clm_path_test_equals(8101, "\\\\server\\share\\dir\\file.txt"), "round-trip safe extended UNC syntax");
    CLM_ASSERT(clm_path_build_unc(8102, 8103, 8101) == 0 &&
               clm_path_test_equals(8101, "\\\\server\\share\\"), "build a UNC share root");
    CLM_ASSERT(clm_path_test_set(8101, "\\\\server") && clm_path_is_unc(8101) == 0 &&
               clm_path_get_unc_server(8101, 8102) == 1 && clm_path_test_equals(8102, "server"),
               "reject incomplete UNC extraction without changing its destination");
    CLM_ASSERT(clm_path_get_separator(8102) == 0 && clm_path_get_invalid_characters(8103) == 0,
               "export native separator and printable invalid-character guidance");
    CLM_ASSERT(clm_path_get_current_directory(8102) == 0 &&
               clm_path_test_set(8101, ".") && clm_path_get_full_path(8101) == 0 &&
               clm_path_is_fully_qualified(8101) == 1 &&
               clm_path_set_current_directory(8102) == 0, "obtain absolute paths and restore the current directory");
    clm_path_info info = {0, 0, 0, 0, 0};
    CLM_ASSERT(clm_path_get_info(8102, 1, &info) == 0 && info.exists && info.is_directory &&
               clm_path_exists(8102) == 1 && clm_path_is_directory(8102) == 1 &&
               clm_path_is_file(8102) == 0 && clm_path_is_symlink(8102) == 0,
               "query current-directory metadata with explicit link following");
    CLM_ASSERT(clm_path_is_hidden(8102) >= 0 && clm_path_canonicalize(8102) == 0,
               "query hidden metadata and canonicalize an existing directory");
    char* original_directory = clm_path_internal_copy(clm_path_internal_text(8102));
#if defined(_WIN32)
    wchar_t temporary_directory[32768], temporary_file[32768];
    const DWORD temporary_length = GetTempPathW(32768, temporary_directory);
    const int named_fixture = temporary_length && temporary_length < 32768 &&
        GetTempFileNameW(temporary_directory, L"clm", 0, temporary_file) != 0;
    char* fixture_path = named_fixture ? clm_path_internal_utf8(temporary_file) : nullptr;
#else
    char temporary_file[] = "/tmp/clm-path-XXXXXX";
    const int descriptor = mkstemp(temporary_file);
    const int named_fixture = descriptor >= 0;
    if (descriptor >= 0) close(descriptor);
    char* fixture_path = named_fixture ? clm_path_internal_copy(temporary_file) : nullptr;
#endif
    CLM_ASSERT(named_fixture && fixture_path, "create an exclusive named temporary file");
    if (fixture_path) {
        CLM_ASSERT(clm_path_test_set(8101, fixture_path) && clm_path_is_file(8101) == 1 &&
                   clm_path_is_directory(8101) == 0 && clm_path_get_info(8101, 0, &info) == 0 && info.is_file,
                   "classify an actual regular file through its UTF-8 path");
        const std::size_t fixture_size = std::strlen(fixture_path) + 10;
        char* directory_path = static_cast<char*>(std::malloc(fixture_size));
        char* link_path = static_cast<char*>(std::malloc(fixture_size));
        if (directory_path && link_path) {
            std::snprintf(directory_path, fixture_size, "%s.dir", fixture_path);
            std::snprintf(link_path, fixture_size, "%s.link", fixture_path);
#if defined(_WIN32)
            wchar_t* directory_wide = clm_path_internal_wide(directory_path);
            wchar_t* link_wide = clm_path_internal_wide(link_path);
            const int created_directory = directory_wide && CreateDirectoryW(directory_wide, nullptr);
            const int created_link = link_wide && CreateSymbolicLinkW(link_wide, temporary_file, 2);
            CLM_ASSERT(SetFileAttributesW(temporary_file, FILE_ATTRIBUTE_HIDDEN) && clm_path_is_hidden(8101) == 1,
                       "read Windows hidden-file attributes");
#else
            const int created_directory = mkdir(directory_path, 0700) == 0;
            const int created_link = symlink(fixture_path, link_path) == 0;
#endif
            CLM_ASSERT(created_directory && clm_path_test_set(8103, directory_path) &&
                       clm_path_set_current_directory(8103) == 0 && clm_path_get_current_directory(8103) == 0 &&
                       clm_path_test_set(8102, original_directory) && clm_path_set_current_directory(8102) == 0,
                       "change into a temporary directory and restore process state");
            if (created_link) {
                CLM_ASSERT(clm_path_test_set(8103, link_path) && clm_path_is_symlink(8103) == 1 &&
                           clm_path_is_file(8103) == 1 && clm_path_get_info(8103, 0, &info) == 0 &&
                           info.is_symlink && !info.is_file && clm_path_get_info(8103, 1, &info) == 0 && info.is_file,
                           "distinguish a symbolic-link entry from its target");
#if defined(_WIN32)
                DeleteFileW(temporary_file);
#else
                unlink(fixture_path);
#endif
                CLM_ASSERT(clm_path_exists(8103) == 0 && clm_path_is_symlink(8103) == 1 &&
                           clm_path_get_info(8103, 0, &info) == 0 && info.exists,
                           "report dangling links as entries but not existing targets");
            } else {
                clm_test_write_line("    SKIPPED: symbolic-link fixtures require OS permission");
            }
            if (original_directory && clm_path_test_set(8102, original_directory)) clm_path_set_current_directory(8102);
#if defined(_WIN32)
            if (created_link) DeleteFileW(link_wide);
            if (created_directory) RemoveDirectoryW(directory_wide);
            std::free(directory_wide); std::free(link_wide);
#else
            if (created_link) unlink(link_path);
            if (created_directory) rmdir(directory_path);
#endif
        }
        std::free(directory_path); std::free(link_path);
#if defined(_WIN32)
        DeleteFileW(temporary_file);
#else
        unlink(fixture_path);
#endif
    }
    std::free(fixture_path); std::free(original_directory);
    CLM_ASSERT(clm_path_test_set(8101, "../../a/../b") && clm_path_normalize(8101) == 0 &&
#if defined(_WIN32)
               clm_path_test_equals(8101, "..\\..\\b"),
#else
               clm_path_test_equals(8101, "../../b"),
#endif
               "retain unresolved leading parent components during lexical normalization");
    CLM_ASSERT(clm_path_test_set(8101, "\\\\server\\share\\..\\x") &&
               clm_path_to_extended_unc(8101) == 1 &&
               clm_path_test_equals(8101, "\\\\server\\share\\..\\x"),
               "reject UNC conversions that would change dot-segment meaning");
    CLM_ASSERT(clm_path_test_set(8101, "clm_missing_path_8341/no-entry") &&
               clm_path_get_info(8101, 1, &info) == 0 && !info.exists && clm_path_exists(8101) == 0 &&
               clm_path_canonicalize(8101) == 1, "distinguish absent paths from invalid queries");
    CLM_ASSERT(clm_path_get_root(8102) == 0 && clm_path_trim_ending_separator(8102) == 0 &&
               clm_path_is_fully_qualified(8102) == 1, "preserve native roots during trailing-separator trimming");
    CLM_ASSERT(clm_path_get_info(8104, 1, &info) == 1 && clm_path_get_info(8102, 1, nullptr) == 1 &&
               clm_path_get_unc_share(8102, 8104) == 1, "reject invalid references and null metadata outputs");
    CLM_ASSERT(clm_bpa_delete(8102) == 0 && clm_bpa_delete(8103) == 0 && clm_bpa_delete(8104) == 0,
               "release all additional path fixtures");
    CLM_ASSERT(clm_bpa_delete(8101) == clm::bpa::CLM_BPA_SUCCESS,
               "release the path fixture");
    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}
