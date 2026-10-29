#pragma once

#include "test.h"
#include "../clm/file.h"
#include "../clm/gen.h"

static int clm_file_test_set(clm_id id, const char* text)
{
    return clm_bpa_set_string(id, text, (clm_uint)strlen(text) + 1) == (int)strlen(text);
}

static int clm_file_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - File");
    char path[128];
    snprintf(path, sizeof(path), "clm_file_test_%llu.tmp", (unsigned long long)clm_gen_random_u64(0, ULONG_MAX));
    const int created = clm_file_create_as(path) == 0;
    CLM_ASSERT(created && clm_file_exists_as(path) == 1, "exclusively create and classify a regular file");
    if (!created) { clm_test_summary(result.passed, result.failed); return 1; }
    char copy[256], moved[256];
    snprintf(copy, sizeof(copy), "%s.copy", path);
    snprintf(moved, sizeof(moved), "%s.moved", path);
    CLM_ASSERT(clm_string_new(9101, path) && clm_string_new(9102, copy) && clm_string_new(9104, "text") &&
               clm_string_new(9107, "unchanged") && clm_string_array_new(9105, 0) &&
               clm_bpa_create_sized(9103, CLM_BPA_TYPE_STRUCT, 64) == 0 &&
               clm_bpa_create_sized(9106, CLM_BPA_TYPE_STRUCT, 64) == 0,
               "create isolated file path, binary, text, and line-array fixtures");
    CLM_ASSERT(clm_file_exists(9101) == 1 && clm_file_create(9101) == -1 && clm_file_create_as(path) == -1 &&
               clm_file_exists_as(NULL) == -1 && clm_file_exists(9199) == -1,
               "reject exclusive creation collisions and invalid path references");
    unsigned char binary[64] = {'A', 0, 'B'};
    unsigned char output[128]; memset(output, '!', sizeof(output));
    size_t count = 0;
    CLM_ASSERT(clm_bpa_set(9103, binary, sizeof(binary)) == CLM_BPA_SET_SUCCESS &&
               clm_file_write_all(9101, 9103, 3) == 0 && clm_file_read_all_to(path, NULL, 0, &count) == 0 && count == 3 &&
               clm_file_read_all_to(path, output, 2, &count) == -1 && count == 3 && output[0] == '!' &&
               clm_file_read_all_to(path, output, sizeof(output), &count) == 0 && count == 3 && !memcmp(output, binary, 3),
               "round-trip binary NUL bytes and measure capacity without truncation");
    CLM_ASSERT(clm_file_read_all(9101, 9106, &count) == 0 && count == 3 &&
               clm_file_read_all_as(path, 9106, &count) == 0 &&
               clm_file_write_all_as(path, 9103, 3) == 0 && clm_file_write_all_from(path, binary, 3) == 0 &&
               clm_file_append(9101, 9103, 3) == 0 && clm_file_append_as(path, 9103, 3) == 0 &&
               clm_file_append_from(path, binary, 3) == 0,
               "share raw-path and BPA adapters for whole-file binary transfers");
    CLM_ASSERT(clm_file_open(9201, 9101, CLM_FILE_OPEN_MODE_READ | CLM_FILE_OPEN_MODE_WRITE) == 0 &&
               clm_file_is_open(9201) == 1 && clm_file_open_as(9201, path, CLM_FILE_OPEN_MODE_READ) == -1 &&
               clm_file_open_as(9209, path, 0) == -1,
               "register internal native resources and reject duplicate IDs and invalid modes");
    bool eof = true;
    clm_ulong position = 99, file_size = 0;
    CLM_ASSERT(clm_file_read_to(9201, output, sizeof(output), &count) == 0 && count == 12 &&
               clm_file_eof(9201, &eof) == 0 && eof && clm_file_read_to(9201, output, 1, &count) == 0 && count == 0 &&
               clm_file_seek(9201, 0, CLM_FILE_SEEK_BEGIN) == 0 && clm_file_eof(9201, &eof) == 0 && !eof,
               "report short reads and EOF independently from operation failures");
    CLM_ASSERT(clm_file_read_at_to(9201, 1, output, 2, &count) == 0 && count == 2 &&
               output[0] == 0 && output[1] == 'B' && clm_file_tell(9201, &position) == 0 && position == 0 &&
               clm_file_write_at_from(9201, 1, "Z", 1, &count) == 0 &&
               clm_file_read_at(9201, 0, 9106, 3, &count) == 0 && clm_file_write_at(9201, 4, 9103, 2, &count) == 0 &&
               clm_file_tell(9201, &position) == 0 && position == 0,
               "preserve sequential cursor positions during native positioned reads and writes");
    CLM_ASSERT(clm_file_write(9201, 9103, 3, &count) == 0 && count == 3 &&
               clm_file_write_from(9201, "Q", 1, &count) == 0 && clm_file_seek(9201, 0, 0) == 0 &&
               clm_file_read(9201, 9106, 3, &count) == 0 && count == 3 && clm_file_get_open_size(9201, &file_size) == 0 && file_size == 12,
               "transfer bytes through registered IDs without exposing native handles");
    CLM_ASSERT(clm_file_seek(9201, INT64_C(5368709120), 0) == 0 && clm_file_tell(9201, &position) == 0 &&
               position == UINT64_C(5368709120) && clm_file_get_open_size(9201, &file_size) == 0 && file_size == 12 &&
               clm_file_truncate(9201, 2) == 0 && clm_file_tell(9201, &position) == 0 && position == UINT64_C(5368709120) &&
               clm_file_seek(9201, -1, 0) == -1 && clm_file_seek(9201, 0, 0) == 0,
               "support 64-bit cursors without extending on seek or narrowing offsets");
    CLM_ASSERT(clm_file_lock(9201, 0, 1, true, false) == 0 && clm_file_unlock(9201, 0, 1) == 0 &&
               clm_file_flush(9201) == 0 && clm_file_sync(9201) == 0 && clm_file_close(9201) == 0 &&
               clm_file_close(9201) == -1 && clm_file_is_open(9201) == 0 &&
               clm_file_open_as(9201, path, CLM_FILE_OPEN_MODE_READ) == -1,
               "lock, synchronize, close, and retire IDs without stale-ID handle reuse");
    CLM_ASSERT(clm_file_open_as(9202, path, CLM_FILE_OPEN_MODE_READ | CLM_FILE_OPEN_MODE_APPEND) == 0 &&
               clm_file_append_open(9202, 9103, 3, &count) == 0 &&
               clm_file_append_open_from(9202, "!", 1, &count) == 0 &&
               clm_file_write_at_from(9202, 0, "bad", 3, &count) == -1 && clm_file_close(9202) == 0,
               "append through append-only resources and reject positioned writes on them");
    int encoding_roundtrips = 1;
    for (int encoding = 0; encoding <= 6; ++encoding) {
        const char* text = encoding ? "caf\xc3\xa9\n" : "plain\n";
        size_t required = 0;
        encoding_roundtrips &= clm_file_write_all_text_from(path, encoding, text, strlen(text)) == 0 &&
            clm_file_read_all_text_to(path, encoding, NULL, 0, &required) == 0 && required == strlen(text) + 1 &&
            clm_file_read_all_text_to(path, encoding, (char*)output, sizeof(output), &required) == 0 &&
            !strcmp((char*)output, text);
    }
    CLM_ASSERT(encoding_roundtrips, "round-trip ASCII, UTF-8, and both UTF-16 endiannesses with explicit BOM policies");
    CLM_ASSERT(clm_file_test_set(9104, "alpha\r\n\r\nbeta\n") &&
               clm_file_write_all_text(9101, 2, 9104, 14) == 0 &&
               clm_file_read_all_text(9101, 2, 9107, &count) == 0 && count == 14 &&
               clm_file_read_all_text_as(path, 2, 9107, &count) == 0 &&
               clm_file_read_all_text_lines(9101, 2, 9105, &file_size) == 0 && file_size == 3 &&
               clm_file_read_all_text_lines_as(path, 2, 9105, &file_size) == 0 &&
               clm_file_read_all_text_lines_to(path, 2, (char*)output, sizeof(output), &count, &file_size) == 0 &&
               file_size == 3 && count == 13 && !memcmp(output, "alpha\0\0beta\0\0", 13),
               "decode whole text and line arrays without phantom trailing lines");
    CLM_ASSERT(clm_file_test_set(9104, "x") && clm_file_write_all_text_as(path, 4, 9104, 1) == 0 &&
               clm_file_append_text(9101, 4, 9104, 1) == 0 && clm_file_append_text_as(path, 4, 9104, 1) == 0 &&
               clm_file_append_text_from(path, 4, "y", 1) == 0 && clm_file_append_line_text(9101, 4, 9104, 1) == 0 &&
               clm_file_append_line_text_as(path, 4, 9104, 1) == 0 && clm_file_append_line_text_from(path, 4, "z", 1) == 0 &&
               clm_file_read_all_text_to(path, 4, (char*)output, sizeof(output), &count) == 0 &&
               !strcmp((char*)output, "xxxyx\nx\nz\n"), "append encoded text and LF records without mid-file BOMs");
    memset(output, '!', sizeof(output));
    const unsigned char invalid[] = {0xff, 0};
    CLM_ASSERT(clm_file_write_all_from(path, invalid, 1) == 0 &&
               clm_file_read_all_text_to(path, 1, (char*)output, sizeof(output), &count) == -1 && output[0] == '!' &&
               clm_file_write_all_text_from(path, 0, "\xc3\xa9", 2) == -1 &&
               clm_file_write_all_text_from(path, 1, "a\0b", 3) == -1,
               "reject malformed text and unrepresentable ASCII before publishing output or truncating");
    CLM_ASSERT(clm_file_write_all_atomic(9101, 9103, 3) == 0 &&
               clm_file_write_all_atomic_as(path, 9103, 3) == 0 &&
               clm_file_write_all_atomic_from(path, "atomic", 6) == 0 &&
               clm_file_copy(9101, 9102, false) == 0 && clm_file_copy_as(path, copy, false) == -1 &&
               clm_file_copy_as(path, copy, true) == 0 && clm_file_copy_as(path, path, true) == -1,
               "atomically replace contents and copy without clobbering collisions or self-copying");
    CLM_ASSERT(clm_file_move_as(copy, moved) == 0 && clm_file_test_set(9102, moved) &&
               clm_file_move(9102, 9101) == -1 && clm_file_get_size(9101, &file_size) == 0 && file_size == 6 &&
               clm_file_get_size_as(moved, &file_size) == 0 && file_size == 6 && clm_file_delete(9102) == 0,
               "move file entries without overwrite and query full-width logical sizes");
    clm_file_info info;
    time_t timestamp = 0, chosen = time(NULL) - 30;
    bool attribute = false;
    CLM_ASSERT(clm_file_get_info(9101, false, &info) == 0 && info.path.is_file &&
               clm_file_get_info_as(path, true, &info) == 0 && info.size == 6 &&
               clm_file_set_times(9101, 0, chosen, chosen, 6) == 0 &&
               clm_file_set_times_as(path, 0, chosen, chosen, 6) == 0 &&
               clm_file_get_modification_time(9101, &timestamp) == 0 && timestamp == chosen &&
               clm_file_get_modification_time_as(path, &timestamp) == 0 &&
               clm_file_get_access_time(9101, &timestamp) == 0 && clm_file_get_access_time_as(path, &timestamp) == 0,
               "query and update supported file timestamps through typed scalar outputs");
#if defined(_WIN32)
    CLM_ASSERT(clm_file_get_creation_time(9101, &timestamp) == 0 && clm_file_get_creation_time_as(path, &timestamp) == 0 &&
               clm_file_set_attributes(9101, CLM_FILE_ATTRIBUTE_HIDDEN, CLM_FILE_ATTRIBUTE_HIDDEN) == 0 &&
               clm_file_is_hidden(9101, &attribute) == 0 && attribute && clm_file_is_hidden_as(path, &attribute) == 0 &&
               clm_file_is_read_only(9101, &attribute) == 0 && clm_file_is_read_only_as(path, &attribute) == 0 &&
               clm_file_is_compressed(9101, &attribute) == 0 && clm_file_is_compressed_as(path, &attribute) == 0 &&
               clm_file_is_archive(9101, &attribute) == 0 && clm_file_is_archive_as(path, &attribute) == 0 &&
               clm_file_set_attributes_as(path, 0, CLM_FILE_ATTRIBUTE_HIDDEN) == 0,
               "expose supported native file attributes and creation time without string coercion");
#else
    CLM_ASSERT(clm_file_get_creation_time(9101, &timestamp) == -2 && clm_file_is_read_only(9101, &attribute) == -2,
               "report unsupported POSIX creation-time and attribute semantics explicitly");
#endif
    char* base = clm_path_internal_current();
    CLM_ASSERT(base && clm_file_test_set(9102, base) &&
               clm_file_create_temporary_to(9301, base, NULL, 0, &count) == 0 && clm_file_is_open(9301) == 0 &&
               clm_file_create_temporary_to(9301, base, (char*)output, 1, &count) == -1 &&
               clm_file_create_temporary(9301, 9102, 9107) == 0 && clm_file_is_open(9301) == 1 &&
               clm_file_close(9301) == 0 && clm_file_delete(9107) == 0 &&
               clm_file_create_temporary_as(9302, base, 9107) == 0 && clm_file_close(9302) == 0 && clm_file_delete(9107) == 0,
               "measure and exclusively create temporary files without leaking resources or insufficient-buffer entries");
    char temporary[2048];
    CLM_ASSERT(base && clm_file_create_temporary_to(9303, base, temporary, sizeof(temporary), &count) == 0 &&
               clm_file_shutdown() == 0 && clm_file_is_open(9303) == 0 && clm_file_delete_as(temporary) == 0 &&
               clm_file_shutdown() == 0, "shutdown closes live native resources and releases retired IDs idempotently");
    free(base);
    CLM_ASSERT(clm_file_read_to(9999, output, 1, &count) == -1 && clm_file_write_from(9999, "x", 1, &count) == -1 &&
               clm_file_get_info_as(NULL, false, &info) == -1 && clm_file_set_times_as(path, 0, 0, 0, 8) == -1,
               "reject unknown resources, invalid pointers, and unsupported timestamp masks");
    CLM_ASSERT(clm_bpa_create(9108, CLM_BPA_TYPE_BYTE) == 0 && (count = 123) &&
               clm_file_read_all_as(path, 9108, &count) == -1 && count == 123 &&
               clm_file_write_all_text(9101, -1, 9103, 3) == -1 &&
               clm_file_append_text_as(path, -1, 9103, 3) == -1 && clm_bpa_delete(9108) == 0,
               "preserve BPA-read outputs and reject invalid text encodings before writes");
    CLM_ASSERT(clm_bpa_delete(9101) == 0 && clm_bpa_delete(9102) == 0 && clm_bpa_delete(9103) == 0 &&
               clm_bpa_delete(9104) == 0 && clm_bpaa_delete(9105) == 0 && clm_bpa_delete(9106) == 0 && clm_bpa_delete(9107) == 0,
               "release all file test data registrations");
    if (created) CLM_ASSERT(clm_file_delete_as(path) == 0, "remove the isolated file fixture");
    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}