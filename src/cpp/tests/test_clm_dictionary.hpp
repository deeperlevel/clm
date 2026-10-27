/*
 * =============================================================================
 * CLM — Cross Language Modules
 * Public Domain — CC0 1.0 Universal
 * =============================================================================
 */

#pragma once

#include "test.hpp"
#include "../clm/dictionary.hpp"

#include <cstring>

static int test_clm_dictionary()
{
    clm_test_result result = {0};
    using namespace clm::dictionary;

    constexpr clm_id id = 71000;
    clm_int value = 0;
    CLM_ASSERT(create(id, CLM_BPA_TYPE_INT, 0) == CLM_BPA_SUCCESS,
               "create an integer dictionary");
    CLM_ASSERT(count(id) == 0 &&
               create(id, CLM_BPA_TYPE_INT, 0) == CLM_BPA_FAILURE,
               "start empty and reject a duplicate ID");
    clm_id empty_iterator_key = 99;
    CLM_ASSERT(iterate_reset(id) == CLM_BPA_SUCCESS &&
               iterate_next(id, &empty_iterator_key) == CLM_BPA_FAILURE &&
               empty_iterator_key == 99,
               "iterate an empty dictionary without changing the output key");
    CLM_ASSERT(count(71999) == CLM_BPA_FAILURE &&
               !contains(71999, 0) &&
               remove(71999, 0) == CLM_BPA_FAILURE,
               "report missing dictionary resources consistently");
    int inserted = 1;
    for (clm_id key = 1000; key < 1040; ++key) {
        const auto item = static_cast<clm_int>((key - 1000) * 10);
        if (set_from(id, key, &item, sizeof(item)) !=
            CLM_BPA_SET_SUCCESS)
            inserted = 0;
    }
    CLM_ASSERT(inserted && count(id) == 40,
               "insert entries and grow dictionary storage");
    constexpr clm_int replacement = 9876;
    CLM_ASSERT(set_from(id, 1005, &replacement, sizeof(replacement)) == CLM_BPA_SET_SUCCESS &&
               get_to(id, 1005, &value, sizeof(value)) == CLM_BPA_GET_SUCCESS &&
               value == replacement && count(id) == 40,
               "replace a value without adding a key");
    CLM_ASSERT(!contains(id, 0), "recognize an absent zero key");
    constexpr clm_int zero_value = 41;
    CLM_ASSERT(set_from(id, 0, &zero_value, sizeof(zero_value)) == CLM_BPA_SET_SUCCESS &&
               contains(id, 0),
               "store key zero as an ordinary key");
    CLM_ASSERT(get_to(id, 1005, &value, 1) == CLM_BPA_FAILURE &&
               get_to(id, 99999, &value, sizeof(value)) == CLM_BPA_FAILURE,
               "reject undersized buffers and missing keys");
    CLM_ASSERT(set_from(id, 1005, &replacement, 1) == CLM_BPA_FAILURE &&
               get_to(id, 1005, &value, sizeof(value)) == CLM_BPA_GET_SUCCESS &&
               value == replacement,
               "reject truncated updates without changing the stored value");

    CLM_ASSERT(remove(id, 1005) == CLM_BPA_SUCCESS &&
               !contains(id, 1005) &&
               count(id) == 40,
               "remove one entry");
    constexpr clm_int added = 12345;
    CLM_ASSERT(add_from(id, 1005, &added, sizeof(added)) == CLM_BPA_SET_SUCCESS &&
               add_from(id, 1005, &added, sizeof(added)) == CLM_BPA_FAILURE &&
               update_from(id, 99999, &added, sizeof(added)) == CLM_BPA_FAILURE &&
               update_from(id, 1005, &replacement, sizeof(replacement)) == CLM_BPA_SET_SUCCESS,
               "enforce add-only and update-only semantics");

    clm_id keys[41] = {};
    CLM_ASSERT(keys_to(id, keys, 41) == CLM_BPA_GET_SUCCESS &&
               keys[0] == 1000 && keys[38] == 1039 && keys[39] == 0 &&
               keys[40] == 1005 &&
               keys_to(id, keys, 40) == CLM_BPA_FAILURE &&
               keys_to(id, nullptr, 41) == CLM_BPA_FAILURE,
               "copy keys in insertion order after removal and reinsertion");
    clm_id next = 99;
    clm_int iterated_value = 0;
    int iteration_ok = iterate_reset(id) == CLM_BPA_SUCCESS;
    for (std::size_t index = 0; index < 41 && iteration_ok; ++index) {
        const clm_int expected_value = keys[index] == 0
            ? zero_value
            : keys[index] == 1005
                ? replacement
                : static_cast<clm_int>((keys[index] - 1000) * 10);
        if (iterate_next(id, &next) != CLM_BPA_GET_SUCCESS ||
            next != keys[index] || !contains(id, next) ||
            get_to(id, next, &iterated_value, sizeof(iterated_value)) !=
                CLM_BPA_GET_SUCCESS ||
            iterated_value != expected_value)
            iteration_ok = 0;
    }
    CLM_ASSERT(iteration_ok &&
               iterate_next(id, &next) == CLM_BPA_FAILURE &&
               iterate_reset(id) == CLM_BPA_SUCCESS,
               "iterate keys in insertion order and reset iteration");

    CLM_ASSERT(create(71010, CLM_BPA_TYPE_STRING, 0) == CLM_BPA_SUCCESS &&
               create(71011, CLM_BPA_TYPE_STRUCT, 0) == CLM_BPA_FAILURE &&
               create(71012, CLM_BPA_TYPE_STRING, 1) == CLM_BPA_FAILURE,
               "validate string and struct dictionary configurations");
    constexpr char text[] = "dictionary value";
    char copied_text[32] = {};
    constexpr char empty[] = "";
    CLM_ASSERT(set_from(71010, 5, text, sizeof(text)) == CLM_BPA_SET_SUCCESS &&
               get_to(71010, 5, copied_text, sizeof(copied_text)) == CLM_BPA_GET_SUCCESS &&
               std::strcmp(copied_text, text) == 0 &&
               set(71010, 6, empty, sizeof(empty)) == CLM_BPA_SET_SUCCESS &&
               get(71010, 6, copied_text, sizeof(copied_text)) == CLM_BPA_GET_SUCCESS &&
               copied_text[0] == '\0',
               "store and retrieve NUL-terminated strings including empty strings");
    constexpr char padded[] = {'o', 'k', '\0', 'x', 'y'};
    CLM_ASSERT(set_from(71010, 8, padded, sizeof(padded)) == CLM_BPA_SET_SUCCESS &&
               get_to(71010, 8, copied_text, 3) == CLM_BPA_GET_SUCCESS &&
               std::strcmp(copied_text, "ok") == 0,
               "store strings only through their first NUL terminator");
    constexpr char unterminated[] = {'n', 'o'};
    CLM_ASSERT(set_from(71010, 5, unterminated, sizeof(unterminated)) == CLM_BPA_FAILURE &&
               get_to(71010, 5, copied_text, sizeof(copied_text)) == CLM_BPA_GET_SUCCESS &&
               std::strcmp(copied_text, text) == 0,
               "reject unterminated strings without replacing the prior value");
    struct pair { clm_int first; clm_int second; };
    constexpr pair input{.first = 11, .second = 22};
    pair output{};
    CLM_ASSERT(create(71020, CLM_BPA_TYPE_STRUCT, sizeof(pair)) == CLM_BPA_SUCCESS &&
               set_from(71020, 7, &input, sizeof(input)) == CLM_BPA_SET_SUCCESS &&
               get_to(71020, 7, &output, sizeof(output)) == CLM_BPA_GET_SUCCESS &&
               std::memcmp(&input, &output, sizeof(input)) == 0,
               "copy fixed-width struct values");
    output = {};
    CLM_ASSERT(set_from(71020, 7, &input, sizeof(input) - 1) == CLM_BPA_FAILURE &&
               get_to(71020, 7, &output, sizeof(output)) == CLM_BPA_GET_SUCCESS &&
               std::memcmp(&input, &output, sizeof(input)) == 0,
               "reject truncated struct updates without changing the stored value");
    CLM_ASSERT(clear(id) == CLM_BPA_SUCCESS &&
               count(id) == 0 &&
               !contains(id, 1000) &&
               erase(71010) == CLM_BPA_SUCCESS &&
               erase(71020) == CLM_BPA_SUCCESS &&
               erase(id) == CLM_BPA_SUCCESS &&
               count(id) == CLM_BPA_FAILURE,
               "clear and delete dictionary resources");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
