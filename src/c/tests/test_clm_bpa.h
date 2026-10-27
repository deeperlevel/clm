#pragma once

#include "test.h"
#include "../clm/bpa.h"

static int clm_bpa_tests_passed = 0;
static int clm_bpa_tests_failed = 0;

static int clm_bpa_compare_ints(const void* left, const void* right)
{
    clm_int left_value;
    clm_int right_value;
    memcpy(&left_value, left, sizeof(left_value));
    memcpy(&right_value, right, sizeof(right_value));
    return (left_value > right_value) - (left_value < right_value);
}

static clm_int clm_bpa_read_int(const clm_uint* value)
{
    clm_int result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static int clm_bpa_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Block Pool Allocation (BPA)");
    clm_int value;
    clm_int index;
    clm_int struct_value[2] = {123, 456};
    clm_int struct_result[2] = {0, 0};
    clm_uint count;
    clm_uint capacity;
    clm_uint length;
    clm_uint output[8] = {0};
    clm_byte sealed;
    clm_int values[] = {4, 1, 3, 2};
    clm_int drawn[5] = {0};
    clm_uint draw_count = 0;
    clm_uint i;
    clm_id wide_id = UINT64_C(0x100000001);
    const char greeting[] = "hello";
    const char replacement[] = "world!";
    const char expected_replaced[] = "world! updated";
    const char empty[] = "";
    clm_uint greeting_buffer[2] = {0};
    clm_uint replacement_buffer[2] = {0};
    clm_uint empty_buffer[1] = {0};
    clm_uint text_buffer[8] = {0};
    char manipulated_text[32];
    char* text = (char*)text_buffer;

    memcpy(greeting_buffer, greeting, sizeof(greeting));
    memcpy(replacement_buffer, replacement, sizeof(replacement));
    memcpy(empty_buffer, empty, sizeof(empty));

    CLM_ASSERT(!clm_bpa_is_existing(1), "!clm_bpa_is_existing(1)");
    CLM_ASSERT(!clm_bpaa_is_existing(10), "!clm_bpaa_is_existing(10)");
    CLM_ASSERT(clm_bpa_create(1, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS, "clm_bpa_create(1, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_is_existing(1), "clm_bpa_is_existing(1)");
    CLM_ASSERT(!clm_bpaa_is_existing(1), "!clm_bpaa_is_existing(1)");
    value = 9;
    CLM_ASSERT(clm_bpa_replace(1, &value, sizeof(value)) == CLM_BPA_SET_SUCCESS, "clm_bpa_replace(1, &value, sizeof(value)) == CLM_BPA_SET_SUCCESS");
    CLM_ASSERT(clm_bpa_get(1, &value, sizeof(value)) == CLM_BPA_GET_SUCCESS && value == 9, "clm_bpa_get(1, &value, sizeof(value)) == CLM_BPA_GET_SUCCESS && value == 9");
    CLM_ASSERT(clm_bpa_create(1, CLM_BPA_TYPE_INT) == CLM_BPA_FAILURE, "clm_bpa_create(1, CLM_BPA_TYPE_INT) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpa_set(1, (clm_uint*)&values[0], sizeof(values[0])) == CLM_BPA_SET_SUCCESS, "clm_bpa_set(1, (clm_uint*)&values[0], sizeof(values[0])) == CLM_BPA_SET_SUCCESS");
    CLM_ASSERT(clm_bpa_get(1, (clm_uint*)&value, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpa_get(1, (clm_uint*)&value, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(value == 4, "value == 4");
    CLM_ASSERT(clm_bpa_get(1, (clm_uint*)&value, 1) == CLM_BPA_FAILURE, "clm_bpa_get(1, (clm_uint*)&value, 1) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpa_ref(1) == CLM_BPA_SUCCESS, "clm_bpa_ref(1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_unref(1) == CLM_BPA_SUCCESS, "clm_bpa_unref(1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_get(1, (clm_uint*)&value, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpa_get(1, (clm_uint*)&value, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpa_unref(1) == CLM_BPA_SUCCESS, "clm_bpa_unref(1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_get(1, (clm_uint*)&value, sizeof(value)) == CLM_BPA_FAILURE, "clm_bpa_get(1, (clm_uint*)&value, sizeof(value)) == CLM_BPA_FAILURE");
    CLM_ASSERT(!clm_bpa_is_existing(1), "!clm_bpa_is_existing(1)");
    CLM_ASSERT(clm_bpa_create_sized(2, CLM_BPA_TYPE_STRUCT, sizeof(struct_value)) == CLM_BPA_SUCCESS, "clm_bpa_create_sized(2, CLM_BPA_TYPE_STRUCT, sizeof(struct_value)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_set(2, (clm_uint*)struct_value, sizeof(struct_value)) == CLM_BPA_SET_SUCCESS, "clm_bpa_set(2, (clm_uint*)struct_value, sizeof(struct_value)) == CLM_BPA_SET_SUCCESS");
    CLM_ASSERT(clm_bpa_get(2, (clm_uint*)struct_result, sizeof(struct_result)) == CLM_BPA_GET_SUCCESS, "clm_bpa_get(2, (clm_uint*)struct_result, sizeof(struct_result)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(struct_result[0] == 123 && struct_result[1] == 456, "struct_result[0] == 123 && struct_result[1] == 456");
    CLM_ASSERT(clm_bpa_delete(2) == CLM_BPA_SUCCESS, "clm_bpa_delete(2) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_create(3, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS, "clm_bpa_create(3, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_set_string(3, greeting_buffer, sizeof(greeting)) == 5, "clm_bpa_set_string(3, greeting_buffer, sizeof(greeting)) == 5");
    CLM_ASSERT(clm_bpa_get_string(3, text_buffer, sizeof(text_buffer)) == 5, "clm_bpa_get_string(3, text_buffer, sizeof(text_buffer)) == 5");
    CLM_ASSERT(strcmp(text, greeting) == 0, "strcmp(text, greeting) == 0");
    CLM_ASSERT(clm_bpa_get_string(3, text_buffer, 5) == CLM_BPA_FAILURE, "clm_bpa_get_string(3, text_buffer, 5) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpa_set_string(3, replacement_buffer, sizeof(replacement)) == 6, "clm_bpa_set_string(3, replacement_buffer, sizeof(replacement)) == 6");
    CLM_ASSERT(clm_bpa_get_string(3, text_buffer, sizeof(text_buffer)) == 6, "clm_bpa_get_string(3, text_buffer, sizeof(text_buffer)) == 6");
    CLM_ASSERT(strcmp(text, replacement) == 0, "strcmp(text, replacement) == 0");
    memcpy(manipulated_text, text, strlen(text) + 1);
    memcpy(manipulated_text + strlen(manipulated_text), " updated", sizeof(" updated"));
    CLM_ASSERT(clm_bpa_replace(3, manipulated_text, (clm_uint)(strlen(manipulated_text) + 1)) ==
                   (int)strlen(expected_replaced),
               "clm_bpa_replace(3, manipulated_text, strlen(manipulated_text) + 1) == strlen(expected_replaced)");
    CLM_ASSERT(clm_bpa_get_string(3, text_buffer, sizeof(text_buffer)) ==
                   (int)strlen(expected_replaced) && strcmp(text, expected_replaced) == 0,
               "clm_bpa_get_string(3, text_buffer, sizeof(text_buffer)) == strlen(expected_replaced) && strcmp(text, expected_replaced) == 0");
    CLM_ASSERT(clm_bpa_replace(999, manipulated_text, (clm_uint)(strlen(manipulated_text) + 1)) ==
                   CLM_BPA_FAILURE,
               "clm_bpa_replace(999, manipulated_text, strlen(manipulated_text) + 1) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpa_delete(3) == CLM_BPA_SUCCESS, "clm_bpa_delete(3) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpa_create(4, CLM_BPA_TYPE_STRUCT) == CLM_BPA_FAILURE, "clm_bpa_create(4, CLM_BPA_TYPE_STRUCT) == CLM_BPA_FAILURE");

    CLM_ASSERT(clm_bpaa_create(10, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS, "clm_bpaa_create(10, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_is_existing(10), "clm_bpaa_is_existing(10)");
    CLM_ASSERT(!clm_bpa_is_existing(10), "!clm_bpa_is_existing(10)");
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        CLM_ASSERT(clm_bpaa_append_raw(10, (clm_uint*)&values[i], sizeof(values[i])) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(10, (clm_uint*)&values[i], sizeof(values[i])) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_count_to(10, &count) == CLM_BPA_GET_SUCCESS && count == 4, "clm_bpaa_get_count_to(10, &count) == CLM_BPA_GET_SUCCESS && count == 4");
    CLM_ASSERT(clm_bpaa_get_capacity_to(10, &capacity) == CLM_BPA_GET_SUCCESS && capacity >= 4, "clm_bpaa_get_capacity_to(10, &capacity) == CLM_BPA_GET_SUCCESS && capacity >= 4");
    CLM_ASSERT(clm_bpaa_get_first_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_first_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpa_read_int(output) == 4, "clm_bpa_read_int(output) == 4");
    CLM_ASSERT(clm_bpaa_get_last_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_last_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpa_read_int(output) == 2, "clm_bpa_read_int(output) == 2");
    CLM_ASSERT(clm_bpaa_get_raw(10, -1, output, sizeof(value)) == CLM_BPA_FAILURE, "clm_bpaa_get_raw(10, -1, output, sizeof(value)) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpaa_get_raw(10, 4, output, sizeof(value)) == CLM_BPA_FAILURE, "clm_bpaa_get_raw(10, 4, output, sizeof(value)) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpaa_insert_raw(10, 1, (clm_uint*)&values[1], sizeof(value)) == CLM_BPA_SUCCESS, "clm_bpaa_insert_raw(10, 1, (clm_uint*)&values[1], sizeof(value)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_raw(10, 1, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_raw(10, 1, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpa_read_int(output) == 1, "clm_bpa_read_int(output) == 1");
    CLM_ASSERT(clm_bpaa_set_raw(10, 1, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SET_SUCCESS, "clm_bpaa_set_raw(10, 1, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SET_SUCCESS");
    CLM_ASSERT(clm_bpaa_move_to(10, 2) == CLM_BPA_SUCCESS, "clm_bpaa_move_to(10, 2) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_current_index_to(10, &index) == CLM_BPA_GET_SUCCESS && index == 2, "clm_bpaa_get_current_index_to(10, &index) == CLM_BPA_GET_SUCCESS && index == 2");
    CLM_ASSERT(clm_bpaa_get_current_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_current_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpaa_move_previous(10) == CLM_BPA_SUCCESS, "clm_bpaa_move_previous(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_move_next(10) == CLM_BPA_SUCCESS, "clm_bpaa_move_next(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_move_last(10) == CLM_BPA_SUCCESS, "clm_bpaa_move_last(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_move_next(10) == CLM_BPA_FAILURE, "clm_bpaa_move_next(10) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpaa_reset(10) == CLM_BPA_SUCCESS, "clm_bpaa_reset(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_extend(10, 2) == CLM_BPA_SUCCESS, "clm_bpaa_extend(10, 2) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_capacity_to(10, &capacity) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_capacity_to(10, &capacity) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpaa_shrink(10, capacity - 1) == CLM_BPA_SUCCESS, "clm_bpaa_shrink(10, capacity - 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_reverse(10) == CLM_BPA_SUCCESS, "clm_bpaa_reverse(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_sort_with(10, clm_bpa_compare_ints) == CLM_BPA_SUCCESS, "clm_bpaa_sort_with(10, clm_bpa_compare_ints) == CLM_BPA_SUCCESS");
    for (i = 1; i < 5; ++i) {
        clm_int current;
        CLM_ASSERT(clm_bpaa_get_raw(10, (clm_int)i, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_raw(10, (clm_int)i, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
        current = clm_bpa_read_int(output);
        CLM_ASSERT(clm_bpaa_get_raw(10, (clm_int)i - 1, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_raw(10, (clm_int)i - 1, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
        CLM_ASSERT(current >= clm_bpa_read_int(output), "current >= clm_bpa_read_int(output)");
    }
    CLM_ASSERT(clm_bpaa_shift_left(10, 1, 1) == CLM_BPA_SUCCESS, "clm_bpaa_shift_left(10, 1, 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_shift_right(10, 1, 1) == CLM_BPA_SUCCESS, "clm_bpaa_shift_right(10, 1, 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_swap(10, 0, 1) == CLM_BPA_SUCCESS, "clm_bpaa_swap(10, 0, 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_copy(10, 0, 1) == CLM_BPA_SUCCESS, "clm_bpaa_copy(10, 0, 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_sort(10, NULL) == CLM_BPA_FAILURE, "clm_bpaa_sort(10, NULL) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpaa_shuffle(10) == CLM_BPA_SUCCESS, "clm_bpaa_shuffle(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_draw_at_raw(10, 2, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_draw_at_raw(10, 2, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    drawn[0] = clm_bpa_read_int(output);
    for (i = 1; i < 5; ++i) {
        CLM_ASSERT(clm_bpaa_draw_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_draw_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
        drawn[i] = clm_bpa_read_int(output);
    }
    CLM_ASSERT(clm_bpaa_draw_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_draw_raw(10, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    for (i = 1; i <= 4; ++i) {
        clm_uint j;
        clm_uint occurrences = 0;
        for (j = 0; j < 5; ++j)
            if (drawn[j] == (clm_int)i)
                ++occurrences;
        CLM_ASSERT(occurrences == (i == 1 ? 0u : (i == 2 || i == 4 ? 2u : 1u)), "occurrences == (i == 1 ? 0u : (i == 2 || i == 4 ? 2u : 1u))");
        draw_count += occurrences;
    }
    CLM_ASSERT(draw_count == 5, "draw_count == 5");
    CLM_ASSERT(clm_bpaa_is_sealed_to(10, &sealed) == CLM_BPA_GET_SUCCESS && sealed == 0, "clm_bpaa_is_sealed_to(10, &sealed) == CLM_BPA_GET_SUCCESS && sealed == 0");
    CLM_ASSERT(clm_bpaa_seal(10) == CLM_BPA_SUCCESS, "clm_bpaa_seal(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_is_sealed_to(10, &sealed) == CLM_BPA_GET_SUCCESS && sealed == 1, "clm_bpaa_is_sealed_to(10, &sealed) == CLM_BPA_GET_SUCCESS && sealed == 1");
    CLM_ASSERT(clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_count_to(10, &count) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_count_to(10, &count) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_capacity_to(10, &capacity) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_capacity_to(10, &capacity) == CLM_BPA_GET_SUCCESS");
    while (count < capacity) {
        CLM_ASSERT(clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS");
        ++count;
    }
    CLM_ASSERT(clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_FAILURE, "clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpaa_clear(10) == CLM_BPA_SUCCESS, "clm_bpaa_clear(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(10, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_remove_first(10) == CLM_BPA_SUCCESS, "clm_bpaa_remove_first(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_clear(10) == CLM_BPA_SUCCESS, "clm_bpaa_clear(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_unseal(10) == CLM_BPA_SUCCESS, "clm_bpaa_unseal(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_extend(10, 1) == CLM_BPA_SUCCESS, "clm_bpaa_extend(10, 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_count_to(10, &count) == CLM_BPA_GET_SUCCESS && count == 0, "clm_bpaa_get_count_to(10, &count) == CLM_BPA_GET_SUCCESS && count == 0");
    CLM_ASSERT(clm_bpaa_clear(10) == CLM_BPA_SUCCESS, "clm_bpaa_clear(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_draw_raw(10, output, sizeof(value)) == CLM_BPA_FAILURE, "clm_bpaa_draw_raw(10, output, sizeof(value)) == CLM_BPA_FAILURE");
    CLM_ASSERT(clm_bpaa_ref(10) == CLM_BPA_SUCCESS, "clm_bpaa_ref(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_unref(10) == CLM_BPA_SUCCESS, "clm_bpaa_unref(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_unref(10) == CLM_BPA_SUCCESS, "clm_bpaa_unref(10) == CLM_BPA_SUCCESS");
    CLM_ASSERT(!clm_bpaa_is_existing(10), "!clm_bpaa_is_existing(10)");
    CLM_ASSERT(clm_bpaa_get_count_to(10, &count) == CLM_BPA_FAILURE, "clm_bpaa_get_count_to(10, &count) == CLM_BPA_FAILURE");

    CLM_ASSERT(clm_bpaa_create(11, CLM_BPA_TYPE_STRING, 0, 1) == CLM_BPA_SUCCESS, "clm_bpaa_create(11, CLM_BPA_TYPE_STRING, 0, 1) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_append_raw(11, greeting_buffer, sizeof(greeting)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(11, greeting_buffer, sizeof(greeting)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_append_raw(11, empty_buffer, sizeof(empty)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(11, empty_buffer, sizeof(empty)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_string_raw(11, 0, text_buffer, sizeof(text_buffer)) == 5, "clm_bpaa_get_string_raw(11, 0, text_buffer, sizeof(text_buffer)) == 5");
    CLM_ASSERT(strcmp(text, greeting) == 0, "strcmp(text, greeting) == 0");
    CLM_ASSERT(clm_bpaa_set_string_raw(11, 1, replacement_buffer, sizeof(replacement)) == 6, "clm_bpaa_set_string_raw(11, 1, replacement_buffer, sizeof(replacement)) == 6");
    CLM_ASSERT(clm_bpaa_move_first(11) == CLM_BPA_SUCCESS, "clm_bpaa_move_first(11) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_length_to(11, &length) == CLM_BPA_GET_SUCCESS && length == 5, "clm_bpaa_get_length_to(11, &length) == CLM_BPA_GET_SUCCESS && length == 5");
    CLM_ASSERT(clm_bpaa_remove(11, 0) == CLM_BPA_SUCCESS, "clm_bpaa_remove(11, 0) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_string_raw(11, 0, text_buffer, sizeof(text_buffer)) == 6, "clm_bpaa_get_string_raw(11, 0, text_buffer, sizeof(text_buffer)) == 6");
    CLM_ASSERT(strcmp(text, replacement) == 0, "strcmp(text, replacement) == 0");
    CLM_ASSERT(clm_bpaa_clear(11) == CLM_BPA_SUCCESS, "clm_bpaa_clear(11) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_delete(11) == CLM_BPA_SUCCESS, "clm_bpaa_delete(11) == CLM_BPA_SUCCESS");
    CLM_ASSERT(!clm_bpaa_is_existing(11), "!clm_bpaa_is_existing(11)");

    CLM_ASSERT(clm_bpaa_create_sized(12, CLM_BPA_TYPE_STRUCT, 1, sizeof(value), 0) ==
               CLM_BPA_SUCCESS, "clm_bpaa_create_sized(12, CLM_BPA_TYPE_STRUCT, 1, sizeof(value), 0) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_append_raw(12, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(12, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_raw(12, 0, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_raw(12, 0, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpa_read_int(output) == 4, "clm_bpa_read_int(output) == 4");
    CLM_ASSERT(clm_bpaa_delete(12) == CLM_BPA_SUCCESS, "clm_bpaa_delete(12) == CLM_BPA_SUCCESS");

    {
        const clm_id source_id = 13;
        const clm_id copied_id = 14;
        const clm_id string_array_id = 15;
        const clm_id string_copy_id = 16;
        const clm_id string_value_id = 17;
        const clm_id empty_copy_id = 19;
        clm_int copied_values[2] = {0, 0};
        const void* borrowed = NULL;
        size_t borrowed_size = 0;
        clm_int borrowed_first = 0;
        clm_int borrowed_second = 0;

        CLM_ASSERT(clm_bpaa_create(source_id, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS, "create source array for slicing");
        for (i = 0; i < 4; ++i)
            CLM_ASSERT(clm_bpaa_append_raw(source_id, &values[i], sizeof(values[i])) == CLM_BPA_SUCCESS, "append item to slice source");
        CLM_ASSERT(clm_bpaa_slice(source_id, 1, 2, copied_id) == CLM_BPA_SUCCESS, "copy BPAA element range into a new array");
        CLM_ASSERT(clm_bpaa_get_count_to(copied_id, &count) == CLM_BPA_GET_SUCCESS && count == 2, "copied BPAA slice has expected count");
        CLM_ASSERT(clm_bpaa_get_raw(copied_id, 0, &copied_values[0], sizeof(copied_values[0])) == CLM_BPA_GET_SUCCESS &&
                   clm_bpaa_get_raw(copied_id, 1, &copied_values[1], sizeof(copied_values[1])) == CLM_BPA_GET_SUCCESS &&
                   copied_values[0] == 1 && copied_values[1] == 3, "copied BPAA slice preserves selected values");
        CLM_ASSERT(clm_bpaa_slice_to(source_id, 1, 2, copied_values, sizeof(copied_values)) == CLM_BPA_SUCCESS &&
                   copied_values[0] == 1 && copied_values[1] == 3, "copy BPAA slice into caller buffer");
        CLM_ASSERT(clm_bpaa_slice(source_id, 4, 0, empty_copy_id) == CLM_BPA_SUCCESS &&
                   clm_bpaa_get_count_to(empty_copy_id, &count) == CLM_BPA_GET_SUCCESS && count == 0,
                   "empty BPAA copy slice at end creates an empty array");
        CLM_ASSERT(clm_bpaa_delete(empty_copy_id) == CLM_BPA_SUCCESS, "delete empty BPAA copy slice");
        CLM_ASSERT(clm_bpaa_slice_borrow(source_id, 1, 2, &borrowed, &borrowed_size) == CLM_BPA_SUCCESS &&
                   borrowed != NULL && borrowed_size == sizeof(copied_values), "borrow contiguous BPAA slice");
        memcpy(&borrowed_first, borrowed, sizeof(borrowed_first));
        memcpy(&borrowed_second, (const unsigned char*)borrowed + sizeof(borrowed_first), sizeof(borrowed_second));
        CLM_ASSERT(borrowed_first == 1 && borrowed_second == 3, "borrowed BPAA slice addresses expected values");
        CLM_ASSERT(clm_bpaa_slice_borrow(source_id, 4, 0, &borrowed, &borrowed_size) == CLM_BPA_SUCCESS &&
                   borrowed == NULL && borrowed_size == 0, "empty BPAA borrowed slice at end succeeds");
        CLM_ASSERT(clm_bpaa_slice_to(source_id, 4, 0, NULL, 0) == CLM_BPA_SUCCESS, "empty BPAA copied slice accepts null output");
        CLM_ASSERT(clm_bpaa_slice(source_id, 3, 2, 18) == CLM_BPA_FAILURE, "BPAA copy slice rejects out-of-range request");
        CLM_ASSERT(clm_bpaa_slice_to(source_id, -1, 1, copied_values, sizeof(copied_values)) == CLM_BPA_FAILURE, "BPAA buffer slice rejects negative start");
        CLM_ASSERT(clm_bpaa_create(string_array_id, CLM_BPA_TYPE_STRING, 0, 0) == CLM_BPA_SUCCESS, "create string array for slice rejection");
        CLM_ASSERT(clm_bpaa_slice_borrow(string_array_id, 0, 0, &borrowed, &borrowed_size) == CLM_BPA_FAILURE, "BPAA borrowed raw slice rejects string arrays");
        CLM_ASSERT(clm_bpa_create(string_value_id, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS &&
                   clm_bpa_set_string(string_value_id, "hello", sizeof("hello")) == 5, "create BPA string for deep-copy slice test");
        CLM_ASSERT(clm_bpaa_append_string(string_array_id, string_value_id) == CLM_BPA_SUCCESS, "append string into BPAA source");
        CLM_ASSERT(clm_bpaa_slice(string_array_id, 0, 1, string_copy_id) == CLM_BPA_SUCCESS, "deep-copy string BPAA slice");
        CLM_ASSERT(clm_bpaa_set_string_raw(string_array_id, 0, "changed", sizeof("changed")) == 7, "mutate string BPAA source after slicing");
        CLM_ASSERT(clm_bpaa_get_string_raw(string_copy_id, 0, text_buffer, sizeof(text_buffer)) == 5 &&
                   strcmp(text, "hello") == 0, "string BPAA slice owns an independent copy");
        CLM_ASSERT(clm_bpaa_delete(string_copy_id) == CLM_BPA_SUCCESS, "delete copied string BPAA slice");
        CLM_ASSERT(clm_bpa_delete(string_value_id) == CLM_BPA_SUCCESS, "delete source BPA string for slice test");
        CLM_ASSERT(clm_bpaa_delete(string_array_id) == CLM_BPA_SUCCESS, "delete string array used for slice rejection");
        CLM_ASSERT(clm_bpaa_delete(copied_id) == CLM_BPA_SUCCESS, "delete copied BPAA slice");
        CLM_ASSERT(clm_bpaa_delete(source_id) == CLM_BPA_SUCCESS, "delete BPAA slice source");
    }

    CLM_ASSERT(clm_bpaa_create(wide_id, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS, "clm_bpaa_create(wide_id, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_append_raw(wide_id, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(wide_id, (clm_uint*)&values[0], sizeof(value)) == CLM_BPA_SUCCESS");
    CLM_ASSERT(clm_bpaa_get_raw(wide_id, 0, output, sizeof(value)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_raw(wide_id, 0, output, sizeof(value)) == CLM_BPA_GET_SUCCESS");
    CLM_ASSERT(clm_bpa_read_int(output) == 4, "clm_bpa_read_int(output) == 4");
    CLM_ASSERT(clm_bpaa_delete(wide_id) == CLM_BPA_SUCCESS, "clm_bpaa_delete(wide_id) == CLM_BPA_SUCCESS");

    for (i = 0; i < 64; ++i)
        CLM_ASSERT(clm_bpa_create(UINT64_C(1000) + i, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS, "clm_bpa_create(UINT64_C(1000) + i, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS");
    for (i = 0; i < 64; i += 2)
        CLM_ASSERT(clm_bpa_delete(UINT64_C(1000) + i) == CLM_BPA_SUCCESS, "clm_bpa_delete(UINT64_C(1000) + i) == CLM_BPA_SUCCESS");
    for (i = 1; i < 64; i += 2)
        CLM_ASSERT(clm_bpa_unref(UINT64_C(1000) + i) == CLM_BPA_SUCCESS, "clm_bpa_unref(UINT64_C(1000) + i) == CLM_BPA_SUCCESS");
    for (i = 0; i < 32; ++i)
        CLM_ASSERT(clm_bpa_create(UINT64_C(2000) + i, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS, "clm_bpa_create(UINT64_C(2000) + i, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS");
    for (i = 0; i < 32; ++i)
        CLM_ASSERT(clm_bpa_delete(UINT64_C(2000) + i) == CLM_BPA_SUCCESS, "clm_bpa_delete(UINT64_C(2000) + i) == CLM_BPA_SUCCESS");

    for (i = 0; i < 32; ++i)
        CLM_ASSERT(clm_bpaa_create(UINT64_C(3000) + i, CLM_BPA_TYPE_INT, 0, 0) ==
                   CLM_BPA_SUCCESS, "clm_bpaa_create(UINT64_C(3000) + i, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS");
    for (i = 0; i < 32; i += 2)
        CLM_ASSERT(clm_bpaa_delete(UINT64_C(3000) + i) == CLM_BPA_SUCCESS, "clm_bpaa_delete(UINT64_C(3000) + i) == CLM_BPA_SUCCESS");
    for (i = 0; i < 16; ++i)
        CLM_ASSERT(clm_bpaa_create(UINT64_C(4000) + i, CLM_BPA_TYPE_INT, 0, 0) ==
                   CLM_BPA_SUCCESS, "clm_bpaa_create(UINT64_C(4000) + i, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS");
    for (i = 1; i < 32; i += 2)
        CLM_ASSERT(clm_bpaa_delete(UINT64_C(3000) + i) == CLM_BPA_SUCCESS, "clm_bpaa_delete(UINT64_C(3000) + i) == CLM_BPA_SUCCESS");
    for (i = 0; i < 16; ++i)
        CLM_ASSERT(clm_bpaa_delete(UINT64_C(4000) + i) == CLM_BPA_SUCCESS, "clm_bpaa_delete(UINT64_C(4000) + i) == CLM_BPA_SUCCESS");


    /* Pool reuse and bounded pooled allocation */
    {
        size_t active_before, reserved_before, active_after, reserved_after;
        clm_int initial = 777, observed = 0;
        clm_int another = 888;
        clm_id block_id = UINT64_C(50001);
        clm_id arr_id = UINT64_C(50002);
        clm_id text_id = UINT64_C(50003);
        clm_id text_array_id = UINT64_C(50004);
        size_t pool_capacity_after_delete;
        CLM_ASSERT(clm_bpa_pool_stats(sizeof(clm_int), &active_before, &reserved_before) == CLM_BPA_SUCCESS, "clm_bpa_pool_stats(sizeof(clm_int), &active_before, &reserved_before) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_create(50001, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS, "clm_bpa_create(50001, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_set(block_id, &initial, sizeof(initial)) == CLM_BPA_SET_SUCCESS, "clm_bpa_set(block_id, &initial, sizeof(initial)) == CLM_BPA_SET_SUCCESS");
        CLM_ASSERT(clm_bpa_pool_stats(sizeof(clm_int), &active_after, &reserved_after) == CLM_BPA_SUCCESS, "clm_bpa_pool_stats(sizeof(clm_int), &active_after, &reserved_after) == CLM_BPA_SUCCESS");
        CLM_ASSERT(active_after == active_before + 1 && reserved_after >= active_after, "active_after == active_before + 1 && reserved_after >= active_after");
        CLM_ASSERT(clm_bpaa_create_sized(arr_id, CLM_BPA_TYPE_STRUCT, 0, sizeof(clm_int), 0) == CLM_BPA_SUCCESS, "clm_bpaa_create_sized(arr_id, CLM_BPA_TYPE_STRUCT, 0, sizeof(clm_int), 0) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_append(arr_id, block_id, sizeof(initial)) == CLM_BPA_SUCCESS, "clm_bpaa_append(arr_id, block_id, sizeof(initial)) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_append(arr_id, block_id, sizeof(initial) + 1) == CLM_BPA_FAILURE, "clm_bpaa_append(arr_id, block_id, sizeof(initial) + 1) == CLM_BPA_FAILURE");
        CLM_ASSERT(clm_bpaa_get(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS");
        CLM_ASSERT(clm_bpa_get(block_id, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 777, "clm_bpa_get(block_id, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 777");
        CLM_ASSERT(clm_bpa_set(block_id, &another, sizeof(another)) == CLM_BPA_SET_SUCCESS, "clm_bpa_set(block_id, &another, sizeof(another)) == CLM_BPA_SET_SUCCESS");
        CLM_ASSERT(clm_bpaa_set(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_SET_SUCCESS, "clm_bpaa_set(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_SET_SUCCESS");
        CLM_ASSERT(clm_bpaa_insert(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_SUCCESS, "clm_bpaa_insert(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_get(arr_id, 1, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get(arr_id, 1, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS");
        CLM_ASSERT(clm_bpa_get(block_id, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 888, "clm_bpa_get(block_id, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 888");
        CLM_ASSERT(clm_bpaa_draw_at(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_draw_at(arr_id, 0, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS");
        CLM_ASSERT(clm_bpaa_draw(arr_id, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS, "clm_bpaa_draw(arr_id, block_id, sizeof(initial)) == CLM_BPA_GET_SUCCESS");
        CLM_ASSERT(clm_bpaa_get(arr_id, 0, UINT64_C(919191), sizeof(initial)) == CLM_BPA_FAILURE, "clm_bpaa_get(arr_id, 0, UINT64_C(919191), sizeof(initial)) == CLM_BPA_FAILURE");
        CLM_ASSERT(clm_bpaa_delete(arr_id) == CLM_BPA_SUCCESS, "clm_bpaa_delete(arr_id) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_delete(block_id) == CLM_BPA_SUCCESS, "clm_bpa_delete(block_id) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_pool_stats(sizeof(clm_int), &active_after, &pool_capacity_after_delete) == CLM_BPA_SUCCESS, "clm_bpa_pool_stats(sizeof(clm_int), &active_after, &pool_capacity_after_delete) == CLM_BPA_SUCCESS");
        CLM_ASSERT(active_after == active_before && pool_capacity_after_delete == reserved_after, "active_after == active_before && pool_capacity_after_delete == reserved_after");
        CLM_ASSERT(clm_bpa_create(50001, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS, "clm_bpa_create(50001, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_get(block_id, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 0, "clm_bpa_get(block_id, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 0");
        CLM_ASSERT(clm_bpa_delete(block_id) == CLM_BPA_SUCCESS, "clm_bpa_delete(block_id) == CLM_BPA_SUCCESS");

        /* AVO-style string block transfers, including preallocated destination. */
        CLM_ASSERT(clm_bpa_create(text_id, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS, "clm_bpa_create(text_id, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_set_string(text_id, greeting, sizeof(greeting)) == 5, "clm_bpa_set_string(text_id, greeting, sizeof(greeting)) == 5");
        CLM_ASSERT(clm_bpaa_create(text_array_id, CLM_BPA_TYPE_STRING, 0, 1) == CLM_BPA_SUCCESS, "clm_bpaa_create(text_array_id, CLM_BPA_TYPE_STRING, 0, 1) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_append_string(text_array_id, text_id) == CLM_BPA_SUCCESS, "clm_bpaa_append_string(text_array_id, text_id) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_set_string(text_id, replacement, sizeof(replacement)) == 6, "clm_bpa_set_string(text_id, replacement, sizeof(replacement)) == 6");
        CLM_ASSERT(clm_bpaa_get_string(text_array_id, 0, text_id) == CLM_BPA_GET_SUCCESS, "clm_bpaa_get_string(text_array_id, 0, text_id) == CLM_BPA_GET_SUCCESS");
        CLM_ASSERT(clm_bpa_get_string(text_id, text, sizeof(text_buffer)) == 5 && strcmp(text, greeting) == 0, "clm_bpa_get_string(text_id, text, sizeof(text_buffer)) == 5 && strcmp(text, greeting) == 0");
        CLM_ASSERT(clm_bpaa_set_string(text_array_id, 0, text_id) == CLM_BPA_SET_SUCCESS, "clm_bpaa_set_string(text_array_id, 0, text_id) == CLM_BPA_SET_SUCCESS");
        CLM_ASSERT(clm_bpaa_delete(text_array_id) == CLM_BPA_SUCCESS, "clm_bpaa_delete(text_array_id) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpa_delete(text_id) == CLM_BPA_SUCCESS, "clm_bpa_delete(text_id) == CLM_BPA_SUCCESS");
    }
    /* A BPAA element can come from an internal aliased source. */
    {
        clm_id id = UINT64_C(51000);
        void* borrowed = NULL;
        size_t borrowed_size = 0;
        clm_int x = 17, y = 19, check = 0;
        CLM_ASSERT(clm_bpaa_create(id, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS, "clm_bpaa_create(id, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_append_raw(id, &x, sizeof(x)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(id, &x, sizeof(x)) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_get_raw(id, 0, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 17, "clm_bpaa_get_raw(id, 0, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 17");
        CLM_ASSERT(clm_bpaa_append_raw(id, &y, sizeof(y)) == CLM_BPA_SUCCESS, "clm_bpaa_append_raw(id, &y, sizeof(y)) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_get_raw(id, 1, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 19, "clm_bpaa_get_raw(id, 1, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 19");
        /* Insert from the array's own payload while growth may relocate it. */
        CLM_ASSERT(clm_bpaa_insert_raw(id, 0, clm_bpaa_find_array(id)->items.blocks + sizeof(clm_int), sizeof(clm_int)) == CLM_BPA_SUCCESS, "clm_bpaa_insert_raw(id, 0, clm_bpaa_find_array(id)->items.blocks + sizeof(clm_int), sizeof(clm_int)) == CLM_BPA_SUCCESS");
        CLM_ASSERT(clm_bpaa_get_raw(id, 0, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 19, "clm_bpaa_get_raw(id, 0, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 19");
        CLM_ASSERT(clm_bpaa_get_raw(id, 2, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 19, "clm_bpaa_get_raw(id, 2, &check, sizeof(check)) == CLM_BPA_GET_SUCCESS && check == 19");
        CLM_ASSERT(clm_bpaa_delete(id) == CLM_BPA_SUCCESS, "clm_bpaa_delete(id) == CLM_BPA_SUCCESS");
        (void)borrowed;
        (void)borrowed_size;
    }
    {
        const clm_id id = UINT64_C(52000);
        clm_bpaa_array_node* array = NULL;
        clm_int initial[] = {2, 4};
        clm_int third = 6;
        clm_int observed = 0;
        clm_int* data;

        CLM_ASSERT(clm_bpaa_create(id, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS, "create fixed-width array for advanced borrow");
        CLM_ASSERT(clm_bpaa_append_raw(id, &initial[0], sizeof(initial[0])) == CLM_BPA_SUCCESS, "append initial advanced-array item");
        CLM_ASSERT(clm_bpaa_advanced_borrow(id, &array) == CLM_BPA_SUCCESS && array != NULL, "borrow array metadata");
        CLM_ASSERT(array->count == 1 && array->capacity == 1 && array->block_size == sizeof(clm_int), "borrow exposes array metadata");
        CLM_ASSERT(clm_bpaa_advanced_data(array) != NULL, "borrow exposes fixed-width payload");
        data = (clm_int*)clm_bpaa_advanced_data(array);
        data[0] = 3;
        CLM_ASSERT(clm_bpaa_advanced_get_raw(array, 0, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 3, "direct payload updates are visible through advanced API");
        CLM_ASSERT(clm_bpaa_advanced_set_raw(array, 0, &initial[1], sizeof(initial[1])) == CLM_BPA_SET_SUCCESS, "advanced set avoids ID lookup");
        CLM_ASSERT(clm_bpaa_advanced_extend(array, 1) == CLM_BPA_SUCCESS, "advanced extend reserves capacity");
        CLM_ASSERT(clm_bpaa_advanced_append_raw(array, &third, sizeof(third)) == CLM_BPA_SUCCESS, "advanced append uses borrowed metadata");
        CLM_ASSERT(clm_bpaa_advanced_insert_raw(array, 1, &initial[0], sizeof(initial[0])) == CLM_BPA_SUCCESS, "advanced insert uses borrowed metadata");
        CLM_ASSERT(array->count == 3 && clm_bpaa_advanced_get_raw(array, 1, &observed, sizeof(observed)) == CLM_BPA_GET_SUCCESS && observed == 2, "advanced insert preserves expected item order");
        CLM_ASSERT(clm_bpaa_advanced_remove(array, 0) == CLM_BPA_SUCCESS && array->count == 2, "advanced remove updates array metadata");
        CLM_ASSERT(clm_bpaa_advanced_shrink(array, array->count) == CLM_BPA_SUCCESS && array->capacity == array->count, "advanced shrink releases unused capacity");
        CLM_ASSERT(clm_bpaa_advanced_clear(array) == CLM_BPA_SUCCESS && array->count == 0, "advanced clear preserves borrowed array");
        CLM_ASSERT(clm_bpaa_unref(id) == CLM_BPA_SUCCESS && clm_bpaa_is_existing(id), "borrow retains array against ordinary unref");
        CLM_ASSERT(clm_bpaa_advanced_release(&array) == CLM_BPA_SUCCESS && array == NULL, "release advanced borrow and null pointer");
        CLM_ASSERT(!clm_bpaa_is_existing(id), "releasing final reference destroys advanced-borrowed array");
        CLM_ASSERT(clm_bpaa_advanced_release(&array) == CLM_BPA_FAILURE, "advanced release rejects already released pointer");
    }
    {
        const clm_id id = UINT64_C(52001);
        clm_bpaa_array_node* array = NULL;
        clm_int item = 0;
        CLM_ASSERT(clm_bpaa_advanced_borrow(id, &array) == CLM_BPA_FAILURE && array == NULL, "advanced borrow rejects missing array");
        CLM_ASSERT(clm_bpaa_create(id, CLM_BPA_TYPE_STRING, 0, 0) == CLM_BPA_SUCCESS, "create string array for advanced borrow");
        CLM_ASSERT(clm_bpaa_advanced_borrow(id, &array) == CLM_BPA_SUCCESS, "borrow string-array metadata");
        CLM_ASSERT(clm_bpaa_advanced_data(array) == NULL, "advanced fixed-width data accessor rejects string array");
        CLM_ASSERT(clm_bpaa_advanced_get_raw(array, 0, &item, sizeof(item)) == CLM_BPA_FAILURE, "advanced fixed-width operations reject string arrays");
        CLM_ASSERT(clm_bpaa_advanced_release(&array) == CLM_BPA_SUCCESS, "release string-array borrow");
        CLM_ASSERT(clm_bpaa_delete(id) == CLM_BPA_SUCCESS, "delete string array after releasing borrow");
    }
    CLM_ASSERT(clm_bpa_pool_cleanup() == CLM_BPA_SUCCESS, "clm_bpa_pool_cleanup() == CLM_BPA_SUCCESS");

    clm_bpa_tests_passed = result.passed;
    clm_bpa_tests_failed = result.failed;
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
