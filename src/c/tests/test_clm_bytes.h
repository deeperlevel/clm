#pragma once

#include "test.h"
#include "../clm/bytes.h"
#include "../clm/file.h"
#include "../clm/gen.h"

static clm_bytes_view clm_bytes_test_view(const void* data, const size_t length)
{
    clm_bytes_view view = {(const unsigned char*)data, length};
    return view;
}

static int clm_bytes_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Bytes");
    const unsigned char raw[] = {0x00, 0xFF, 0x10, 0x00, 0x7F};
    clm_bytes_buffer buffer = {0};
    clm_bytes_view view;
    size_t index = 0;

    CLM_ASSERT(clm_bytes_buffer_length(&buffer) == 0 && clm_bytes_buffer_view(&buffer).length == 0 &&
                   clm_bytes_buffer_append(&buffer, raw, sizeof(raw)) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_length(&buffer) == 5 &&
                   memcmp(clm_bytes_buffer_view(&buffer).data, raw, sizeof(raw)) == 0,
               "append binary data containing NUL bytes to a zero-initialized buffer");
    CLM_ASSERT(clm_bytes_buffer_append_byte(&buffer, 0xAB) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_fill(&buffer, 0, 3) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_string(&buffer, "hi") == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_view(&buffer, clm_bytes_test_view(raw, 2)) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_length(&buffer) == 5 + 1 + 3 + 2 + 2 &&
                   clm_bytes_buffer_view(&buffer).data[5] == 0xAB && clm_bytes_buffer_view(&buffer).data[9] == 'h',
               "append bytes, fills, strings without NUL, and views");
    CLM_ASSERT(clm_bytes_buffer_append(&buffer, NULL, 1) == CLM_BYTES_INVALID_ARGUMENT &&
                   clm_bytes_buffer_append(&buffer, NULL, 0) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_string(&buffer, NULL) == CLM_BYTES_INVALID_ARGUMENT &&
                   clm_bytes_buffer_append(NULL, raw, 1) == CLM_BYTES_INVALID_ARGUMENT &&
                   clm_bytes_buffer_length(&buffer) == 13,
               "reject invalid append arguments without changing the buffer");

    clm_bytes_buffer_clear(&buffer);
    clm_bytes_buffer_append_string(&buffer, "abcdef");
    CLM_ASSERT(clm_bytes_buffer_insert(&buffer, 3, "XY", 2) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("abcXYdef")) &&
                   clm_bytes_buffer_insert(&buffer, 0, ">", 1) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_insert(&buffer, 9, "<", 1) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string(">abcXYdef<")) &&
                   clm_bytes_buffer_insert(&buffer, 11, "!", 1) == CLM_BYTES_OUT_OF_RANGE,
               "insert at the front, middle, and end and reject indexes past the end");
    CLM_ASSERT(clm_bytes_buffer_remove(&buffer, 4, 2) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string(">abcdef<")) &&
                   clm_bytes_buffer_remove(&buffer, 6, 3) == CLM_BYTES_OUT_OF_RANGE &&
                   clm_bytes_buffer_remove(&buffer, 9, 0) == CLM_BYTES_OUT_OF_RANGE &&
                   clm_bytes_buffer_remove(&buffer, 0, 1) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_remove(&buffer, 6, 1) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("abcdef")),
               "remove ranges and reject ranges past the end");
    CLM_ASSERT(clm_bytes_buffer_resize(&buffer, 7, '.') == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("abcdef.")) &&
                   clm_bytes_buffer_resize(&buffer, 2, 0) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("ab")),
               "grow with a fill byte and shrink");

    clm_bytes_buffer_clear(&buffer);
    clm_bytes_buffer_append_string(&buffer, "0123456789");
    CLM_ASSERT(clm_bytes_buffer_append_view(&buffer, clm_bytes_buffer_view(&buffer)) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer),
                       clm_bytes_view_from_string("01234567890123456789")) &&
                   clm_bytes_buffer_insert(&buffer, 0, clm_bytes_buffer_view(&buffer).data + 15, 5) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer),
                       clm_bytes_view_from_string("5678901234567890123456789")),
               "append and insert data that overlaps the buffer's own storage");

    clm_bytes_buffer_dispose(&buffer);
    char out[16];
    CLM_ASSERT(clm_bytes_buffer_init(&buffer, 8) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_string(&buffer, "headerBODY") == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_consume(&buffer, 6) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("BODY")) &&
                   clm_bytes_buffer_append_string(&buffer, "-more-data-that-forces-growth") == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer),
                       clm_bytes_view_from_string("BODY-more-data-that-forces-growth")) &&
                   clm_bytes_buffer_read(&buffer, out, 4) == CLM_BYTES_SUCCESS && memcmp(out, "BODY", 4) == 0 &&
                   clm_bytes_buffer_consume(&buffer, 1000) == CLM_BYTES_OUT_OF_RANGE &&
                   clm_bytes_buffer_read(&buffer, NULL, 1) == CLM_BYTES_INVALID_ARGUMENT,
               "consume from the front, keep data across compaction and growth, and read out bytes");
    CLM_ASSERT(clm_bytes_buffer_consume(&buffer, clm_bytes_buffer_length(&buffer)) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_length(&buffer) == 0 &&
                   clm_bytes_buffer_append_string(&buffer, "x") == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_view(&buffer).data[0] == 'x',
               "reuse a fully consumed buffer");

    unsigned char* tail = NULL;
    size_t free_bytes = 0;
    clm_bytes_buffer_clear(&buffer);
    CLM_ASSERT(clm_bytes_buffer_prepare(&buffer, 16, &tail, &free_bytes) == CLM_BYTES_SUCCESS &&
                   tail != NULL && free_bytes >= 16 && (memcpy(tail, "received", 8), 1) &&
                   clm_bytes_buffer_commit(&buffer, 8) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("received")) &&
                   clm_bytes_buffer_prepare(&buffer, 0, &tail, &free_bytes) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_commit(&buffer, free_bytes + 1) == CLM_BYTES_OUT_OF_RANGE &&
                   clm_bytes_buffer_prepare(&buffer, 1, NULL, &free_bytes) == CLM_BYTES_INVALID_ARGUMENT,
               "expose writable tail space and commit written bytes");

    clm_bytes_buffer_dispose(&buffer);
    CLM_ASSERT(clm_bytes_buffer_init(&buffer, 0) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_set_limit(&buffer, 8) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_string(&buffer, "12345678") == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_byte(&buffer, '9') == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_insert(&buffer, 0, "x", 1) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_resize(&buffer, 9, 0) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_reserve(&buffer, 1) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&buffer), clm_bytes_view_from_string("12345678")),
               "refuse growth past the limit and leave the buffer unchanged");
    clm_bytes_buffer_consume(&buffer, 6);
    CLM_ASSERT(clm_bytes_buffer_append_string(&buffer, "abcdef") == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_prepare(&buffer, 1, &tail, &free_bytes) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_consume(&buffer, 4) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_prepare(&buffer, 1, &tail, &free_bytes) == CLM_BYTES_SUCCESS &&
                   free_bytes <= 4 && free_bytes >= 1 &&
                   clm_bytes_buffer_prepare(&buffer, 5, &tail, &free_bytes) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_set_limit(&buffer, 3) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_set_limit(&buffer, 0) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_append_fill(&buffer, 'z', 100) == CLM_BYTES_SUCCESS,
               "enforce the limit on consumed buffers, tail space, and limit changes");

    clm_bytes_buffer_clear(&buffer);
    clm_bytes_buffer_append(&buffer, "secret", 6);
    const unsigned char* secret = clm_bytes_buffer_view(&buffer).data;
    int zeroed = 1;
    CLM_ASSERT(clm_bytes_buffer_wipe(&buffer) == CLM_BYTES_SUCCESS && clm_bytes_buffer_length(&buffer) == 0,
               "wipe clears the buffer");
    for (size_t i = 0; i < 6; ++i)
        zeroed &= buffer.storage[i] == 0;
    CLM_ASSERT(zeroed && secret == buffer.storage, "wipe overwrites stored bytes with zeros");
    clm_bytes_buffer_dispose(&buffer);
    CLM_ASSERT(clm_bytes_buffer_length(&buffer) == 0 && buffer.storage == NULL &&
                   clm_bytes_buffer_wipe(NULL) == CLM_BYTES_INVALID_ARGUMENT,
               "dispose resets the buffer and invalid wipes are rejected");

    const clm_bytes_view text = clm_bytes_view_from_string("hello world");
    CLM_ASSERT(clm_bytes_view_slice(text, 6, 5, &view) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_equals(view, clm_bytes_view_from_string("world")) &&
                   clm_bytes_view_slice(text, 11, 0, &view) == CLM_BYTES_SUCCESS && view.length == 0 &&
                   clm_bytes_view_slice(text, 6, 6, &view) == CLM_BYTES_OUT_OF_RANGE &&
                   clm_bytes_view_slice(text, 12, 0, &view) == CLM_BYTES_OUT_OF_RANGE &&
                   clm_bytes_view_slice(text, 0, 1, NULL) == CLM_BYTES_INVALID_ARGUMENT &&
                   clm_bytes_view_from_string(NULL).length == 0,
               "slice views with bounds checks");
    char short_dest[4];
    size_t copied = 0;
    CLM_ASSERT(clm_bytes_view_copy(text, out, sizeof(out), &copied) == CLM_BYTES_SUCCESS && copied == 11 &&
                   memcmp(out, "hello world", 11) == 0 &&
                   clm_bytes_view_copy(text, short_dest, sizeof(short_dest), &copied) == CLM_BYTES_BUFFER_TOO_SMALL &&
                   copied == 11 &&
                   clm_bytes_view_copy(text, NULL, 0, &copied) == CLM_BYTES_BUFFER_TOO_SMALL && copied == 11 &&
                   clm_bytes_view_copy(clm_bytes_test_view(NULL, 0), NULL, 0, &copied) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_copy(clm_bytes_test_view(NULL, 1), out, sizeof(out), NULL) ==
                       CLM_BYTES_INVALID_ARGUMENT,
               "copy views and report the needed size when the destination is too small");

    const clm_bytes_view a = clm_bytes_test_view(raw, 5);
    const clm_bytes_view b = clm_bytes_test_view(raw, 4);
    unsigned char differs[5] = {0x00, 0xFF, 0x10, 0x00, 0x80};
    CLM_ASSERT(clm_bytes_view_equals(a, a) && !clm_bytes_view_equals(a, b) &&
                   !clm_bytes_view_equals(a, clm_bytes_test_view(differs, 5)) &&
                   clm_bytes_view_compare(a, b) > 0 && clm_bytes_view_compare(b, a) < 0 &&
                   clm_bytes_view_compare(a, clm_bytes_test_view(differs, 5)) < 0 &&
                   clm_bytes_view_compare(a, a) == 0 &&
                   clm_bytes_view_equals(clm_bytes_test_view(NULL, 0), clm_bytes_test_view(NULL, 0)) &&
                   !clm_bytes_view_equals(clm_bytes_test_view(NULL, 1), clm_bytes_test_view(NULL, 1)),
               "compare views as unsigned bytes with embedded NULs");
    CLM_ASSERT(clm_bytes_view_equals_constant_time(a, clm_bytes_test_view(raw, 5)) &&
                   !clm_bytes_view_equals_constant_time(a, clm_bytes_test_view(differs, 5)) &&
                   !clm_bytes_view_equals_constant_time(a, b),
               "compare secrets in constant time");
    CLM_ASSERT(clm_bytes_view_starts_with(text, clm_bytes_view_from_string("hello")) &&
                   !clm_bytes_view_starts_with(text, clm_bytes_view_from_string("world")) &&
                   clm_bytes_view_ends_with(text, clm_bytes_view_from_string("world")) &&
                   !clm_bytes_view_ends_with(text, clm_bytes_view_from_string("hello")) &&
                   clm_bytes_view_starts_with(text, clm_bytes_view_from_string("")) &&
                   !clm_bytes_view_starts_with(b, a) && !clm_bytes_view_ends_with(b, a),
               "test prefixes and suffixes");
    const unsigned char needle[] = {0x10, 0x00};
    CLM_ASSERT(clm_bytes_view_find(text, clm_bytes_view_from_string("o w"), &index) == CLM_BYTES_SUCCESS &&
                   index == 4 &&
                   clm_bytes_view_find(text, clm_bytes_view_from_string("o"), &index) == CLM_BYTES_SUCCESS &&
                   index == 4 &&
                   clm_bytes_view_find(text, clm_bytes_view_from_string("orl"), NULL) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_find(text, clm_bytes_view_from_string("xyz"), &index) == CLM_BYTES_NOT_FOUND &&
                   clm_bytes_view_find(text, clm_bytes_view_from_string("hello world!"), &index) ==
                       CLM_BYTES_NOT_FOUND &&
                   clm_bytes_view_find(text, clm_bytes_view_from_string("ld"), &index) == CLM_BYTES_SUCCESS &&
                   index == 9 &&
                   clm_bytes_view_find(text, clm_bytes_view_from_string(""), &index) == CLM_BYTES_SUCCESS &&
                   index == 0 &&
                   clm_bytes_view_find(a, clm_bytes_test_view(needle, 2), &index) == CLM_BYTES_SUCCESS &&
                   index == 2 &&
                   clm_bytes_view_find(clm_bytes_test_view(NULL, 1), text, &index) == CLM_BYTES_INVALID_ARGUMENT,
               "find byte sequences including partial matches and embedded NULs");
    CLM_ASSERT(clm_bytes_view_find_byte(a, 0x7F, &index) == CLM_BYTES_SUCCESS && index == 4 &&
                   clm_bytes_view_find_byte(a, 0x00, &index) == CLM_BYTES_SUCCESS && index == 0 &&
                   clm_bytes_view_find_byte(a, 0x42, &index) == CLM_BYTES_NOT_FOUND &&
                   clm_bytes_view_find_byte(clm_bytes_test_view(NULL, 0), 0, &index) == CLM_BYTES_NOT_FOUND,
               "find single byte values");

    char path[128];
    snprintf(path, sizeof(path), "clm_bytes_test_%llu.tmp", (unsigned long long)clm_gen_random_u64(0, ULONG_MAX));
    clm_bytes_buffer_init(&buffer, 0);
    CLM_ASSERT(clm_bytes_view_save_file(a, path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_view_append_to_file(clm_bytes_test_view(raw, 2), path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_load_file(&buffer, path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_length(&buffer) == 7 &&
                   memcmp(clm_bytes_buffer_view(&buffer).data, raw, 5) == 0 &&
                   memcmp(clm_bytes_buffer_view(&buffer).data + 5, raw, 2) == 0,
               "save, append, and load binary files byte for byte");
    CLM_ASSERT(clm_bytes_buffer_load_file(&buffer, path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_length(&buffer) == 14 &&
                   clm_bytes_view_save_file(clm_bytes_test_view(NULL, 0), path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_load_file(&buffer, path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_length(&buffer) == 14,
               "append loaded files to existing data and accept empty files");
    CLM_ASSERT(clm_bytes_view_save_file(a, path) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_set_limit(&buffer, 16) == CLM_BYTES_SUCCESS &&
                   clm_bytes_buffer_load_file(&buffer, path) == CLM_BYTES_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_length(&buffer) == 14 &&
                   clm_bytes_buffer_load_file(&buffer, "clm_bytes_missing_file.tmp") == CLM_BYTES_FAILURE &&
                   clm_bytes_buffer_load_file(&buffer, NULL) == CLM_BYTES_INVALID_ARGUMENT &&
                   clm_bytes_view_save_file(a, NULL) == CLM_BYTES_INVALID_ARGUMENT,
               "reject oversized and missing files without changing the buffer");
    clm_bytes_buffer_dispose(&buffer);
    CLM_ASSERT(clm_file_delete_as(path) == 0, "remove the bytes test file");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
