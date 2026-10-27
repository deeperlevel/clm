#pragma once

#include "test.hpp"
#include "../clm/bpa.hpp"

#include <cstring>

static int clm_bpa_test_compare(const void* left, const void* right)
{
    clm_int lhs = 0;
    clm_int rhs = 0;
    std::memcpy(&lhs, left, sizeof(lhs));
    std::memcpy(&rhs, right, sizeof(rhs));
    return (lhs > rhs) - (lhs < rhs);
}

static int test_clm_bpa()
{
    clm_test_result result{};
    using namespace clm::bpa;

    clm_int value = 0;
    clm_int values[] = {4, 1, 3, 2};
    clm_uint count = 0;
    clm_uint capacity = 0;
    clm_int first = 0;
    clm_int last = 0;
    std::size_t active_before = 0;
    std::size_t reserved_before = 0;
    std::size_t active_after = 0;
    std::size_t reserved_after = 0;
    char text[16] = {};
    constexpr clm_id block_id = UINT64_C(0xB001);
    constexpr clm_id array_id = UINT64_C(0xA001);
    constexpr clm_id slice_id = UINT64_C(0xA002);
    constexpr clm_id string_id = UINT64_C(0xB002);
    constexpr clm_id struct_id = UINT64_C(0xB003);
    constexpr clm_id string_array_id = UINT64_C(0xA003);

    clm_test_begin("CLM - Block Pool Allocation (BPA)");

    CLM_ASSERT(pool_stats(sizeof(clm_int), &active_before,
                                  &reserved_before) == CLM_BPA_SUCCESS);
    CLM_ASSERT(!block_exists(block_id));
    CLM_ASSERT(create(block_id, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS);
    CLM_ASSERT(pool_stats(sizeof(clm_int), &active_after,
                                  &reserved_after) == CLM_BPA_SUCCESS &&
               active_after == active_before + 1 &&
               reserved_after >= active_after);
    CLM_ASSERT(block_exists(block_id));
    CLM_ASSERT(set(block_id, &values[0], sizeof(values[0])) ==
               CLM_BPA_SET_SUCCESS);
    CLM_ASSERT(get(block_id, &value, sizeof(value)) ==
                   CLM_BPA_GET_SUCCESS &&
               value == 4);
    CLM_ASSERT(get(block_id, &value, sizeof(clm_byte)) ==
               CLM_BPA_FAILURE);
    CLM_ASSERT(ref_block(block_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(unref_block(block_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(block_exists(block_id));
    CLM_ASSERT(unref_block(block_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(!block_exists(block_id));

    CLM_ASSERT(create(string_id, CLM_BPA_TYPE_STRING) ==
               CLM_BPA_SUCCESS);
    CLM_ASSERT(set_string(string_id, "hello", sizeof("hello")) == 5);
    CLM_ASSERT(get_string(string_id, text, sizeof(text)) == 5);
    CLM_ASSERT(std::strcmp(text, "hello") == 0);
    CLM_ASSERT(get_string(string_id, text, 5) == CLM_BPA_FAILURE);
    CLM_ASSERT(replace(string_id, "world!", sizeof("world!")) == 6);
    CLM_ASSERT(get_string(string_id, text, sizeof(text)) == 6 &&
               std::strcmp(text, "world!") == 0);
    CLM_ASSERT(erase_block(string_id) == CLM_BPA_SUCCESS);

    CLM_ASSERT(create_sized(struct_id, CLM_BPA_TYPE_STRUCT,
                                    sizeof(values)) == CLM_BPA_SUCCESS);
    CLM_ASSERT(set(struct_id, values, sizeof(values)) ==
               CLM_BPA_SET_SUCCESS);
    clm_int structure[4] = {};
    CLM_ASSERT(get(struct_id, structure, sizeof(structure)) ==
                   CLM_BPA_GET_SUCCESS &&
               structure[0] == 4 && structure[1] == 1 &&
               structure[2] == 3 && structure[3] == 2);
    CLM_ASSERT(erase_block(struct_id) == CLM_BPA_SUCCESS);

    CLM_ASSERT(create(array_id, CLM_BPA_TYPE_INT, 0, 0) ==
               CLM_BPA_SUCCESS);
    for (int & v : values)
        CLM_ASSERT(append_raw(array_id, &v, sizeof(v)) == CLM_BPA_SUCCESS);
    CLM_ASSERT(get_count_to(array_id, &count) == CLM_BPA_GET_SUCCESS &&
               count == 4);
    CLM_ASSERT(get_capacity_to(array_id, &capacity) ==
                   CLM_BPA_GET_SUCCESS &&
               capacity >= count);
    CLM_ASSERT(get_first_raw(array_id, &first, sizeof(first)) ==
                   CLM_BPA_GET_SUCCESS &&
               first == 4);
    CLM_ASSERT(get_last_raw(array_id, &last, sizeof(last)) ==
                   CLM_BPA_GET_SUCCESS &&
               last == 2);
    CLM_ASSERT(sort_with(array_id, clm_bpa_test_compare) ==
               CLM_BPA_SUCCESS);
    CLM_ASSERT(get_raw(array_id, 0, &first, sizeof(first)) ==
                   CLM_BPA_GET_SUCCESS &&
               first == 1);
    clm_int selected[2] = {};
    const void* borrowed_data = nullptr;
    std::size_t borrowed_size = 0;
    CLM_ASSERT(slice_to(array_id, 1, 2, selected, sizeof(selected)) ==
                   CLM_BPA_SUCCESS &&
               selected[0] == 2 && selected[1] == 3);
    CLM_ASSERT(slice_borrow(array_id, 1, 2, &borrowed_data,
                                     &borrowed_size) == CLM_BPA_SUCCESS &&
               borrowed_data != nullptr && borrowed_size == sizeof(selected));
    clm_bpaa_array_node* borrowed_array = nullptr;
    CLM_ASSERT(advanced_borrow(array_id, &borrowed_array) ==
                   CLM_BPA_SUCCESS &&
               advanced_data(borrowed_array) != nullptr);
    CLM_ASSERT(advanced_get_raw(borrowed_array, 0, &first,
                                         sizeof(first)) == CLM_BPA_GET_SUCCESS &&
               first == 1);
    CLM_ASSERT(advanced_release(&borrowed_array) == CLM_BPA_SUCCESS &&
               borrowed_array == nullptr);
    CLM_ASSERT(draw_at_raw(array_id, 1, &last, sizeof(last)) ==
               CLM_BPA_GET_SUCCESS);
    CLM_ASSERT(shuffle(array_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(draw_raw(array_id, &last, sizeof(last)) ==
               CLM_BPA_GET_SUCCESS);
    value = 9;
    CLM_ASSERT(insert_raw(array_id, 1, &value, sizeof(value)) ==
               CLM_BPA_SUCCESS);
    CLM_ASSERT(get_raw(array_id, 1, &first, sizeof(first)) ==
                   CLM_BPA_GET_SUCCESS &&
               first == 9);
    CLM_ASSERT(set_raw(array_id, 1, &values[0], sizeof(values[0])) ==
               CLM_BPA_SET_SUCCESS);
    CLM_ASSERT(remove(array_id, 1) == CLM_BPA_SUCCESS);
    CLM_ASSERT(move_last(array_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(get_current_index_to(array_id, &count) ==
                   CLM_BPA_GET_SUCCESS &&
               count == 3);
    CLM_ASSERT(seal(array_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(append_raw(array_id, &value, sizeof(value)) ==
               CLM_BPA_FAILURE);
    CLM_ASSERT(unseal(array_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(slice(array_id, 1, 2, slice_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(get_count_to(slice_id, &count) == CLM_BPA_GET_SUCCESS &&
               count == 2);
    CLM_ASSERT(erase_array(slice_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(erase_array(array_id) == CLM_BPA_SUCCESS);

    CLM_ASSERT(create(string_array_id, CLM_BPA_TYPE_STRING, 0, 0) ==
               CLM_BPA_SUCCESS);
    CLM_ASSERT(append_string_raw(string_array_id, "one", sizeof("one")) ==
               CLM_BPA_SUCCESS);
    CLM_ASSERT(append_string_raw(string_array_id, "two", sizeof("two")) ==
               CLM_BPA_SUCCESS);
    CLM_ASSERT(get_string_raw(string_array_id, 1, text, sizeof(text)) == 3 &&
               std::strcmp(text, "two") == 0);
    CLM_ASSERT(set_string_raw(string_array_id, 0, "updated",
                                       sizeof("updated")) == 7);
    CLM_ASSERT(remove_first(string_array_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(move_first(string_array_id) == CLM_BPA_SUCCESS);
    CLM_ASSERT(get_string_raw(string_array_id, 0, text, sizeof(text)) == 3 &&
               std::strcmp(text, "two") == 0);
    CLM_ASSERT(erase_array(string_array_id) == CLM_BPA_SUCCESS);

    {
        constexpr clm_id block_id = UINT64_C(0xB004);
        constexpr clm_id array_id = UINT64_C(0xA004);
        constexpr clm_id copied_block_id = UINT64_C(0xB005);
        clm_int initial = 17;
        clm_int replacement = 19;
        clm_int observed = 0;

        CLM_ASSERT(create(block_id, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS);
        CLM_ASSERT(create(block_id, CLM_BPA_TYPE_INT) == CLM_BPA_FAILURE);
        CLM_ASSERT(replace(block_id, &initial, sizeof(initial)) ==
                   CLM_BPA_SET_SUCCESS);
        CLM_ASSERT(get(block_id, &observed, sizeof(observed)) ==
                       CLM_BPA_GET_SUCCESS &&
                   observed == initial);
        CLM_ASSERT(replace(block_id, &replacement, sizeof(replacement)) ==
                   CLM_BPA_SET_SUCCESS);
        CLM_ASSERT(replace(UINT64_C(0xB099), &replacement,
                           sizeof(replacement)) == CLM_BPA_FAILURE);
        CLM_ASSERT(create( UINT64_C(0xB006), CLM_BPA_TYPE_STRUCT) ==
                   CLM_BPA_FAILURE);

        CLM_ASSERT(create_sized(array_id, CLM_BPA_TYPE_INT, 1,
                                sizeof(initial), 0) == CLM_BPA_SUCCESS);
        CLM_ASSERT(append(array_id, block_id, sizeof(initial)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(set(array_id, 0, block_id, sizeof(initial)) ==
                   CLM_BPA_SET_SUCCESS);
        CLM_ASSERT(insert(array_id, 0, block_id, sizeof(initial)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(get(array_id, 1, copied_block_id, sizeof(initial)) ==
                   CLM_BPA_GET_SUCCESS);
        CLM_ASSERT(get(copied_block_id, &observed, sizeof(observed)) ==
                       CLM_BPA_GET_SUCCESS &&
                   observed == replacement);
        CLM_ASSERT(get_raw(array_id, -1, &observed, sizeof(observed)) ==
                   CLM_BPA_FAILURE);
        CLM_ASSERT(get_raw(array_id, 2, &observed, sizeof(observed)) ==
                   CLM_BPA_FAILURE);
        CLM_ASSERT(set_raw(array_id, 0, &replacement,
                           sizeof(replacement) - 1) == CLM_BPA_FAILURE);
        CLM_ASSERT(clear(array_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(get_count_to(array_id, &count) == CLM_BPA_GET_SUCCESS &&
                   count == 0);
        CLM_ASSERT(draw_raw(array_id, &observed, sizeof(observed)) ==
                   CLM_BPA_FAILURE);
        CLM_ASSERT(extend(array_id, 1) == CLM_BPA_SUCCESS);
        CLM_ASSERT(remove(array_id, 0) == CLM_BPA_FAILURE);
        CLM_ASSERT(erase_block(copied_block_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_array(array_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_block(block_id) == CLM_BPA_SUCCESS);
    }

    {
        constexpr clm_id source_id = UINT64_C(0xB007);
        constexpr clm_id array_id = UINT64_C(0xA005);
        constexpr clm_id copied_id = UINT64_C(0xB008);
        constexpr clm_id empty_id = UINT64_C(0xA006);
        constexpr clm_id string_array_id = UINT64_C(0xA007);
        constexpr clm_id string_slice_id = UINT64_C(0xA008);
        constexpr clm_id string_source_array_id = UINT64_C(0xA011);
        const char hello[] = "hello";
        const char world[] = "world!";
        const char empty[] = "";
        char output[16] = {};
        clm_int items[] = {4, 1, 3};
        clm_int observed[2] = {};
        clm_uint string_length = 0;
        const void* borrowed = nullptr;
        std::size_t borrowed_size = 0;

        CLM_ASSERT(create(array_id, CLM_BPA_TYPE_INT, 0, 0) ==
                   CLM_BPA_SUCCESS);
        for (clm_int item : items)
            CLM_ASSERT(append_raw(array_id, &item, sizeof(item)) ==
                       CLM_BPA_SUCCESS);
        CLM_ASSERT(slice(array_id, 1, 2, copied_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(slice_to(array_id, 1, 2, observed, sizeof(observed)) ==
                       CLM_BPA_SUCCESS &&
                   observed[0] == 1 && observed[1] == 3);
        CLM_ASSERT(slice(array_id, 3, 0, empty_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(get_count_to(empty_id, &count) == CLM_BPA_GET_SUCCESS &&
                   count == 0);
        CLM_ASSERT(slice_to(array_id, 3, 0, nullptr, 0) == CLM_BPA_SUCCESS);
        CLM_ASSERT(slice_borrow(array_id, 3, 0, &borrowed, &borrowed_size) ==
                       CLM_BPA_SUCCESS &&
                   borrowed == nullptr && borrowed_size == 0);
        CLM_ASSERT(slice(array_id, 2, 2, UINT64_C(0xA099)) ==
                   CLM_BPA_FAILURE);
        CLM_ASSERT(erase_array(empty_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_array(copied_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_array(array_id) == CLM_BPA_SUCCESS);

        CLM_ASSERT(create(source_id, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS);
        CLM_ASSERT(set_string(source_id, hello, sizeof(hello)) == 5);
        CLM_ASSERT(create(string_array_id, CLM_BPA_TYPE_STRING, 0, 1) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(append_string(string_array_id, source_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(set_string(source_id, world, sizeof(world)) == 6);
        CLM_ASSERT(get_string(string_array_id, 0, copied_id, sizeof(output)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(get_string(copied_id, output, sizeof(output)) == 5 &&
                   std::strcmp(output, hello) == 0);
        CLM_ASSERT(set_string(string_array_id, 0, source_id, sizeof(world)) ==
                   6);
        CLM_ASSERT(get_string_raw(string_array_id, 0, output, sizeof(output)) ==
                       6 &&
                   std::strcmp(output, world) == 0);
        CLM_ASSERT(append_string_raw(string_array_id, empty, sizeof(empty)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(move_to(string_array_id, 1) == CLM_BPA_SUCCESS);
        CLM_ASSERT(get_length_to(string_array_id, &string_length) ==
                       CLM_BPA_GET_SUCCESS &&
                   string_length == 0);
        CLM_ASSERT(remove(string_array_id, 0) == CLM_BPA_SUCCESS);
        CLM_ASSERT(get_string_raw(string_array_id, 0, output, sizeof(output)) ==
                       0 &&
                   std::strcmp(output, "") == 0);
        CLM_ASSERT(create(string_source_array_id, CLM_BPA_TYPE_STRING, 0, 0) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(append_string_raw(string_source_array_id, hello, sizeof(hello)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(slice(string_source_array_id, 0, 1, string_slice_id) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(set_string_raw(string_source_array_id, 0, world, sizeof(world)) ==
                   6);
        CLM_ASSERT(get_string_raw(string_slice_id, 0, output, sizeof(output)) ==
                       5 &&
                   std::strcmp(output, hello) == 0);
        CLM_ASSERT(slice_borrow(string_source_array_id, 0, 1, &borrowed,
                                &borrowed_size) == CLM_BPA_FAILURE);
        CLM_ASSERT(erase_array(string_slice_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_array(string_source_array_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_array(string_array_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_block(copied_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_block(source_id) == CLM_BPA_SUCCESS);
    }

    {
        constexpr clm_id id = UINT64_C(0xA009);
        clm_bpaa_array_node* borrowed = nullptr;
        clm_int first = 2;
        clm_int second = 4;
        clm_int third = 6;
        clm_int observed = 0;
        clm_int* data = nullptr;

        CLM_ASSERT(create(id, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS);
        CLM_ASSERT(append_raw(id, &first, sizeof(first)) == CLM_BPA_SUCCESS);
        CLM_ASSERT(advanced_borrow(id, &borrowed) == CLM_BPA_SUCCESS &&
                   borrowed != nullptr);
        CLM_ASSERT(borrowed->count == 1 && borrowed->capacity == 1 &&
                   borrowed->block_size == sizeof(clm_int));
        data = static_cast<clm_int*>(advanced_data(borrowed));
        CLM_ASSERT(data != nullptr);
        data[0] = 3;
        CLM_ASSERT(advanced_get_raw(borrowed, 0, &observed, sizeof(observed)) ==
                       CLM_BPA_GET_SUCCESS &&
                   observed == 3);
        CLM_ASSERT(advanced_set_raw(borrowed, 0, &second, sizeof(second)) ==
                   CLM_BPA_SET_SUCCESS);
        CLM_ASSERT(advanced_extend(borrowed, 1) == CLM_BPA_SUCCESS);
        CLM_ASSERT(advanced_append_raw(borrowed, &third, sizeof(third)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(advanced_insert_raw(borrowed, 1, &first, sizeof(first)) ==
                   CLM_BPA_SUCCESS);
        CLM_ASSERT(borrowed->count == 3 &&
                   advanced_get_raw(borrowed, 1, &observed,
                                    sizeof(observed)) == CLM_BPA_GET_SUCCESS &&
                   observed == first);
        CLM_ASSERT(advanced_remove(borrowed, 0) == CLM_BPA_SUCCESS &&
                   borrowed->count == 2);
        CLM_ASSERT(advanced_shrink(
                       borrowed, borrowed->capacity - borrowed->count) ==
                       CLM_BPA_SUCCESS &&
                   borrowed->capacity == borrowed->count);
        CLM_ASSERT(advanced_clear(borrowed) == CLM_BPA_SUCCESS &&
                   borrowed->count == 0);
        CLM_ASSERT(unref_array(id) == CLM_BPA_SUCCESS && array_exists(id));
        CLM_ASSERT(advanced_release(&borrowed) == CLM_BPA_SUCCESS &&
                   borrowed == nullptr);
        CLM_ASSERT(!array_exists(id));
        CLM_ASSERT(advanced_release(&borrowed) == CLM_BPA_FAILURE);
        CLM_ASSERT(create(id, CLM_BPA_TYPE_STRING, 0, 0) == CLM_BPA_SUCCESS);
        CLM_ASSERT(advanced_borrow(id, &borrowed) == CLM_BPA_SUCCESS &&
                   borrowed != nullptr);
        CLM_ASSERT(advanced_data(borrowed) == nullptr);
        CLM_ASSERT(advanced_get_raw(borrowed, 0, &observed, sizeof(observed)) ==
                   CLM_BPA_FAILURE);
        CLM_ASSERT(advanced_release(&borrowed) == CLM_BPA_SUCCESS &&
                   borrowed == nullptr);
        CLM_ASSERT(erase_array(id) == CLM_BPA_SUCCESS);
    }

    {
        constexpr clm_id block_id = UINT64_C(0xB009);
        constexpr clm_id array_id = UINT64_C(0xA010);
        std::size_t active_before = 0;
        std::size_t capacity_before = 0;
        std::size_t active_after = 0;
        std::size_t capacity_after = 0;
        clm_int value = 777;
        clm_int observed = 0;

        CLM_ASSERT(pool_stats(sizeof(clm_int), &active_before,
                              &capacity_before) == CLM_BPA_SUCCESS);
        CLM_ASSERT(create(block_id, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS);
        CLM_ASSERT(set(block_id, &value, sizeof(value)) == CLM_BPA_SET_SUCCESS);
        CLM_ASSERT(pool_stats(sizeof(clm_int), &active_after,
                              &capacity_after) == CLM_BPA_SUCCESS &&
                   active_after == active_before + 1);
        CLM_ASSERT(create_sized(array_id, CLM_BPA_TYPE_INT, 0,
                                sizeof(value), 0) == CLM_BPA_SUCCESS);
        CLM_ASSERT(append(array_id, block_id, sizeof(value)) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_array(array_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(erase_block(block_id) == CLM_BPA_SUCCESS);
        CLM_ASSERT(pool_stats(sizeof(clm_int), &active_after,
                              &capacity_before) == CLM_BPA_SUCCESS &&
                   active_after == active_before);
        CLM_ASSERT(create(block_id, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS);
        CLM_ASSERT(get(block_id, &observed, sizeof(observed)) ==
                       CLM_BPA_GET_SUCCESS &&
                   observed == 0);
        CLM_ASSERT(erase_block(block_id) == CLM_BPA_SUCCESS);
    }

    for (clm_uint i = 0; i < 32; ++i)
        CLM_ASSERT(create(UINT64_C(0x10000) + i, CLM_BPA_TYPE_INT) ==
                   CLM_BPA_SUCCESS);
    for (clm_uint i = 0; i < 32; i += 2)
        CLM_ASSERT(erase_block(UINT64_C(0x10000) + i) == CLM_BPA_SUCCESS);
    for (clm_uint i = 1; i < 32; i += 2)
        CLM_ASSERT(unref_block(UINT64_C(0x10000) + i) == CLM_BPA_SUCCESS);
    for (clm_uint i = 0; i < 16; ++i)
        CLM_ASSERT(create(UINT64_C(0x11000) + i, CLM_BPA_TYPE_INT) ==
                   CLM_BPA_SUCCESS);
    for (clm_uint i = 0; i < 16; ++i)
        CLM_ASSERT(erase_block(UINT64_C(0x11000) + i) == CLM_BPA_SUCCESS);

    for (clm_uint i = 0; i < 16; ++i)
        CLM_ASSERT(create(UINT64_C(0x12000) + i, CLM_BPA_TYPE_INT, 0, 0) ==
                   CLM_BPA_SUCCESS);
    for (clm_uint i = 0; i < 16; i += 2)
        CLM_ASSERT(erase_array(UINT64_C(0x12000) + i) == CLM_BPA_SUCCESS);
    for (clm_uint i = 0; i < 8; ++i)
        CLM_ASSERT(create(UINT64_C(0x13000) + i, CLM_BPA_TYPE_INT, 0, 0) ==
                   CLM_BPA_SUCCESS);
    for (clm_uint i = 1; i < 16; i += 2)
        CLM_ASSERT(erase_array(UINT64_C(0x12000) + i) == CLM_BPA_SUCCESS);
    for (clm_uint i = 0; i < 8; ++i)
        CLM_ASSERT(erase_array(UINT64_C(0x13000) + i) == CLM_BPA_SUCCESS);

    CLM_ASSERT(create(UINT64_C(0x14000), CLM_BPA_TYPE_STRING, 0, 0) == CLM_BPA_SUCCESS &&
               append_string_raw(UINT64_C(0x14000), "z", 2) == CLM_BPA_SUCCESS &&
               append_string_raw(UINT64_C(0x14000), "a", 2) == CLM_BPA_SUCCESS &&
               shuffle(UINT64_C(0x14000)) == CLM_BPA_SUCCESS &&
               erase_array(UINT64_C(0x14000)) == CLM_BPA_SUCCESS,
               "shuffle supports string arrays as well as numeric arrays");
    CLM_ASSERT(pool_cleanup() == CLM_BPA_SUCCESS);
    CLM_ASSERT(shutdown() == CLM_BPA_SUCCESS);

    clm_test_summary(result.passed, result.failed);
    return result.failed;
}
