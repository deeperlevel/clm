#pragma once

#include "test.h"
#include "../clm/dictionary.h"

#define clm_dictionary_metadata clm_typed_dictionary_metadata
#define clm_dictionary_new clm_typed_dictionary_new
#define clm_dictionary_delete clm_typed_dictionary_delete
#define clm_dictionary_set_from clm_typed_dictionary_set_from
#define clm_dictionary_set clm_typed_dictionary_set
#define clm_dictionary_get_to clm_typed_dictionary_get_to
#define clm_dictionary_get clm_typed_dictionary_get
#define clm_dictionary_contains clm_typed_dictionary_contains
#define clm_dictionary_count clm_typed_dictionary_count
#define clm_dictionary_remove clm_typed_dictionary_remove
#define clm_dictionary_iterate_reset clm_typed_dictionary_iterate_reset
#define clm_dictionary_iterate_next clm_typed_dictionary_iterate_next
#define clm_dictionary_add_from clm_typed_dictionary_add_from
#define clm_dictionary_update_from clm_typed_dictionary_update_from
#define clm_dictionary_keys_to clm_typed_dictionary_keys_to
#define clm_dictionary_clear clm_typed_dictionary_clear

static int clm_dictionary_typed_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Dictionary");

    const clm_id dictionary_id = 71000;
    const clm_id source_id = 71001;
    const clm_id destination_id = 71002;
    const clm_id string_source_id = 71004;
    clm_int value = 0;
    clm_int source_value = 9876;
    clm_id iterated_key = 0;
    clm_int iterated_count = 0;

    CLM_ASSERT(clm_dictionary_new(dictionary_id, CLM_BPA_TYPE_INT, 0) ==
                   CLM_BPA_SUCCESS,
               "create integer dictionary");
    CLM_ASSERT(clm_dictionary_count(dictionary_id) == 0,
               "new dictionary starts empty");
    CLM_ASSERT(clm_dictionary_new(dictionary_id, CLM_BPA_TYPE_INT, 0) ==
                   CLM_BPA_FAILURE,
               "reject duplicate dictionary ID");
    CLM_ASSERT(clm_dictionary_new(71003, CLM_BPA_TYPE_STRING, 1) ==
                   CLM_BPA_FAILURE,
               "reject invalid string dictionary size");
    CLM_ASSERT(clm_bpa_create_sized(71040, CLM_BPA_TYPE_STRUCT,
                                    sizeof(clm_dictionary_metadata)) ==
                       CLM_BPA_SUCCESS &&
                   clm_dictionary_count(71040) == CLM_BPA_FAILURE &&
                   clm_dictionary_delete(71040) == CLM_BPA_FAILURE &&
                   clm_bpa_delete(71040) == CLM_BPA_SUCCESS,
               "do not mistake an unrelated struct block for a dictionary");

    int inserted_all = 1;
    for (clm_int index = 0; index < 40; ++index) {
        const clm_id key = (clm_id)(1000 + index);
        const clm_int item = index * 10;
        if (clm_dictionary_set_from(dictionary_id, key, &item, sizeof(item)) !=
            CLM_BPA_SET_SUCCESS)
            inserted_all = 0;
    }
    CLM_ASSERT(inserted_all, "insert entries and grow hash table");
    CLM_ASSERT(clm_dictionary_count(dictionary_id) == 40,
               "count entries after insertion");
    CLM_ASSERT(clm_dictionary_contains(dictionary_id, 0) == false,
               "key zero is absent until inserted");
    CLM_ASSERT(clm_dictionary_set_from(dictionary_id, 1005, &source_value,
                                       sizeof(source_value)) ==
                   CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_to(dictionary_id, 1005, &value,
                                         sizeof(value)) == CLM_BPA_GET_SUCCESS &&
                   value == source_value &&
                   clm_dictionary_count(dictionary_id) == 40,
               "replace existing value without adding a key");

    CLM_ASSERT(clm_bpa_create(source_id, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS,
               "create BPA source block");
    CLM_ASSERT(clm_bpa_set(source_id, &source_value, sizeof(source_value)) ==
                   CLM_BPA_SET_SUCCESS,
               "write BPA source block");
    CLM_ASSERT(clm_dictionary_set(dictionary_id, 0, source_id) ==
                   CLM_BPA_SET_SUCCESS,
               "set from BPA block");
    CLM_ASSERT(
                   clm_dictionary_get_to(dictionary_id, 0, &value,
                                         sizeof(value)) == CLM_BPA_GET_SUCCESS &&
                   value == source_value,
               "retrieve value under key zero");
    CLM_ASSERT(clm_dictionary_count(dictionary_id) == 41,
               "count key zero as a distinct entry");
    CLM_ASSERT(clm_bpa_create(destination_id, CLM_BPA_TYPE_INT) ==
                   CLM_BPA_SUCCESS &&
                   clm_dictionary_get(dictionary_id, 0, destination_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_bpa_get(destination_id, &value, sizeof(value)) ==
                       CLM_BPA_GET_SUCCESS &&
                   value == source_value,
               "retrieve into compatible BPA block");

    CLM_ASSERT(clm_dictionary_remove(dictionary_id, 1005) == CLM_BPA_SUCCESS &&
                   !clm_dictionary_contains(dictionary_id, 1005) &&
                   clm_dictionary_count(dictionary_id) == 40,
               "remove key and compact value storage");
    CLM_ASSERT(clm_dictionary_get_to(dictionary_id, 1005, &value,
                                     sizeof(value)) == CLM_BPA_FAILURE,
               "missing key retrieval fails");
    const clm_int new_value = 12345;
    CLM_ASSERT(clm_dictionary_set_from(dictionary_id, 99999, &new_value,
                                       sizeof(new_value)) ==
                   CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_count(dictionary_id) == 41,
               "insert after removal into table with tombstones");

    int iteration_valid = clm_dictionary_iterate_reset(dictionary_id) ==
                          CLM_BPA_SUCCESS;
    clm_byte seen_keys[42] = {0};
    while (iteration_valid &&
           clm_dictionary_iterate_next(dictionary_id, &iterated_key) ==
               CLM_BPA_GET_SUCCESS) {
        clm_int expected_value;
        clm_int seen_index;
        if (iterated_key == 0) {
            expected_value = source_value;
            seen_index = 0;
        } else if (iterated_key == 99999) {
            expected_value = new_value;
            seen_index = 1;
        } else if (iterated_key >= 1000 && iterated_key < 1040 &&
                   iterated_key != 1005) {
            expected_value = (clm_int)(iterated_key - 1000) * 10;
            seen_index = (clm_int)(iterated_key - 1000) + 2;
        } else {
            iteration_valid = 0;
            break;
        }
        if (seen_keys[seen_index] ||
            !clm_dictionary_contains(dictionary_id, iterated_key) ||
            clm_dictionary_get_to(dictionary_id, iterated_key, &value,
                                  sizeof(value)) != CLM_BPA_GET_SUCCESS ||
            value != expected_value)
            iteration_valid = 0;
        seen_keys[seen_index] = 1;
        ++iterated_count;
    }
    CLM_ASSERT(iteration_valid && iterated_count == 41,
               "iterate all present keys and retrieve their values");
    CLM_ASSERT(clm_dictionary_iterate_reset(dictionary_id) == CLM_BPA_SUCCESS &&
                   clm_dictionary_iterate_next(dictionary_id, &iterated_key) ==
                       CLM_BPA_GET_SUCCESS,
               "reset iteration");

    CLM_ASSERT(clm_bpa_create(string_source_id, CLM_BPA_TYPE_STRING) ==
                   CLM_BPA_SUCCESS &&
                   clm_bpa_set_string(string_source_id, "dictionary value", 17) ==
                       16 &&
                   clm_dictionary_new(71010, CLM_BPA_TYPE_STRING, 0) ==
                       CLM_BPA_SUCCESS &&
                   clm_dictionary_set(71010, 5, string_source_id) ==
                       CLM_BPA_SET_SUCCESS,
               "create string dictionary and copy a BPA string");
    char text[32] = {0};
    CLM_ASSERT(clm_dictionary_get_to(71010, 5, text, sizeof(text)) ==
                   CLM_BPA_GET_SUCCESS &&
                   strcmp(text, "dictionary value") == 0,
               "retrieve string value to caller buffer");
    CLM_ASSERT(clm_dictionary_set_from(71010, 6, "", 1) ==
                   CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_to(71010, 6, text, sizeof(text)) ==
                       CLM_BPA_GET_SUCCESS &&
                   text[0] == '\0',
               "store and retrieve an empty string");

    struct clm_dictionary_test_value {
        clm_int first;
        clm_int second;
    } struct_value = {11, 22}, struct_result = {0, 0};
    CLM_ASSERT(clm_dictionary_new(71020, CLM_BPA_TYPE_STRUCT,
                                  sizeof(struct_value)) == CLM_BPA_SUCCESS &&
                   clm_dictionary_set_from(71020, 7, &struct_value,
                                           sizeof(struct_value)) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_to(71020, 7, &struct_result,
                                         sizeof(struct_result)) ==
                       CLM_BPA_GET_SUCCESS &&
                   memcmp(&struct_value, &struct_result, sizeof(struct_value)) ==
                       0,
               "store and retrieve fixed-size struct values");

    clm_id collision_keys[3] = {0, 0, 0};
    clm_uint collision_bucket = 0;
    clm_int collision_key_count = 0;
    for (clm_id candidate = 0;
         candidate < 100000 && collision_key_count < 3;
         ++candidate) {
        const clm_uint bucket = (clm_uint)(clm_bpa_hash_id(candidate) & 15);
        if (collision_key_count == 0)
            collision_bucket = bucket;
        if (bucket == collision_bucket)
            collision_keys[collision_key_count++] = candidate;
    }
    int collisions_work = collision_key_count == 3 &&
        clm_dictionary_new(71030, CLM_BPA_TYPE_INT, 0) == CLM_BPA_SUCCESS;
    for (clm_int index = 0; collisions_work && index < 3; ++index) {
        const clm_int item = index + 1;
        if (clm_dictionary_set_from(71030, collision_keys[index], &item,
                                    sizeof(item)) != CLM_BPA_SET_SUCCESS)
            collisions_work = 0;
    }
    const clm_int collision_value = 3;
    CLM_ASSERT(collisions_work &&
                   clm_dictionary_remove(71030, collision_keys[0]) ==
                       CLM_BPA_SUCCESS &&
                   clm_dictionary_get_to(71030, collision_keys[2], &value,
                                         sizeof(value)) == CLM_BPA_GET_SUCCESS &&
                   value == collision_value,
               "continue probing through a removed colliding key");

    /* Python-like keyed semantics and deterministic insertion-order iteration. */
    CLM_ASSERT(clm_dictionary_new(71050, CLM_BPA_TYPE_INT, 0) == CLM_BPA_SUCCESS,
               "create ordered dictionary");
    const clm_id ordered_keys[] = {900, 2, 77, 0};
    int ordered_valid = 1;
    for (int index = 0; index < 4; ++index) {
        clm_int item = index + 100;
        if (clm_dictionary_add_from(71050, ordered_keys[index], &item,
                                     sizeof(item)) != CLM_BPA_SET_SUCCESS)
            ordered_valid = 0;
    }
    CLM_ASSERT(ordered_valid && clm_dictionary_count(71050) == 4,
               "add distinct keys in insertion order");
    CLM_ASSERT(clm_dictionary_add_from(71050, 2, &source_value,
                                       sizeof(source_value)) == CLM_BPA_FAILURE,
               "insert-only rejects an existing key");
    CLM_ASSERT(clm_dictionary_update_from(71050, 333, &source_value,
                                          sizeof(source_value)) == CLM_BPA_FAILURE,
               "update-only rejects an absent key");
    CLM_ASSERT(clm_dictionary_update_from(71050, 2, &source_value,
                                          sizeof(source_value)) == CLM_BPA_SET_SUCCESS,
               "updating existing key preserves position");
    clm_id copied_keys[4] = {0};
    CLM_ASSERT(clm_dictionary_keys_to(71050, copied_keys, 4) == CLM_BPA_GET_SUCCESS &&
               memcmp(copied_keys, ordered_keys, sizeof ordered_keys) == 0,
               "keys copy retains insertion order");
    int order_matches = clm_dictionary_iterate_reset(71050) == CLM_BPA_SUCCESS;
    for (int index = 0; index < 4 && order_matches; ++index)
        if (clm_dictionary_iterate_next(71050, &iterated_key) != CLM_BPA_GET_SUCCESS ||
            iterated_key != ordered_keys[index])
            order_matches = 0;
    CLM_ASSERT(order_matches && clm_dictionary_iterate_next(71050, &iterated_key) == CLM_BPA_FAILURE,
               "iterator follows insertion order");
    CLM_ASSERT(clm_dictionary_remove(71050, 2) == CLM_BPA_SUCCESS &&
               clm_dictionary_add_from(71050, 2, &source_value,
                                       sizeof(source_value)) == CLM_BPA_SET_SUCCESS,
               "deleting and readding key appends it to end");
    const clm_id expected_after_readd[] = {900, 77, 0, 2};
    CLM_ASSERT(clm_dictionary_keys_to(71050, copied_keys, 4) == CLM_BPA_GET_SUCCESS &&
               memcmp(copied_keys, expected_after_readd,
                      sizeof expected_after_readd) == 0,
               "reinserted key comes last");
    CLM_ASSERT(clm_dictionary_clear(71050) == CLM_BPA_SUCCESS &&
               clm_dictionary_count(71050) == 0 &&
               !clm_dictionary_contains(71050, 900),
               "clear preserves empty dictionary resource");
    CLM_ASSERT(clm_dictionary_delete(71050) == CLM_BPA_SUCCESS,
               "delete ordered dictionary");

    CLM_ASSERT(clm_dictionary_delete(71030) == CLM_BPA_SUCCESS &&
                   clm_dictionary_delete(71020) == CLM_BPA_SUCCESS &&
                   clm_dictionary_delete(71010) == CLM_BPA_SUCCESS &&
                   clm_dictionary_delete(dictionary_id) == CLM_BPA_SUCCESS,
               "delete dictionaries and their backing arrays");
    CLM_ASSERT(clm_dictionary_count(dictionary_id) == CLM_BPA_FAILURE,
               "deleted dictionary is no longer accessible");
    CLM_ASSERT(clm_bpa_delete(source_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(destination_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(string_source_id) == CLM_BPA_SUCCESS,
               "delete BPA source and destination blocks");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}

#undef clm_dictionary_metadata
#undef clm_dictionary_new
#undef clm_dictionary_delete
#undef clm_dictionary_set_from
#undef clm_dictionary_set
#undef clm_dictionary_get_to
#undef clm_dictionary_get
#undef clm_dictionary_contains
#undef clm_dictionary_count
#undef clm_dictionary_remove
#undef clm_dictionary_iterate_reset
#undef clm_dictionary_iterate_next
#undef clm_dictionary_add_from
#undef clm_dictionary_update_from
#undef clm_dictionary_keys_to
#undef clm_dictionary_clear

static int clm_dictionary_dynamic_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Heterogeneous Dictionary");

    const clm_id dictionary_id = 71100;
    const clm_id integer_source_id = 71101;
    const clm_id string_source_id = 71102;
    const clm_id integer_destination_id = 71103;
    const clm_id string_destination_id = 71104;
    const clm_id wrong_destination_id = 71105;
    clm_int integer_value = 41;
    clm_int integer_result = 0;
    clm_byte type = 0;
    clm_id stored_id = 0;
    char string_result[32] = {0};

    CLM_ASSERT(clm_dictionary_new(dictionary_id) == CLM_BPA_SUCCESS &&
                   clm_dictionary_count(dictionary_id) == 0,
               "create an empty heterogeneous dictionary");
    CLM_ASSERT(clm_dictionary_new(dictionary_id) == CLM_BPA_FAILURE,
               "reject a duplicate heterogeneous dictionary ID");
    CLM_ASSERT(clm_bpa_create(integer_source_id, CLM_BPA_TYPE_INT) ==
                   CLM_BPA_SUCCESS &&
                   clm_bpa_set(integer_source_id, &integer_value,
                               sizeof(integer_value)) == CLM_BPA_SET_SUCCESS &&
                   clm_bpa_create(string_source_id, CLM_BPA_TYPE_STRING) ==
                   CLM_BPA_SUCCESS &&
                   clm_bpa_set_string(string_source_id, "borrowed text", 14) ==
                       13,
               "create borrowed integer and string values");

    CLM_ASSERT(clm_dictionary_add(dictionary_id, 10, integer_source_id) ==
                   CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_set(dictionary_id, 20, string_source_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_count(dictionary_id) == 2,
               "store differently typed borrowed values");
    CLM_ASSERT(clm_dictionary_get_type(dictionary_id, 10, &type) ==
                       CLM_BPA_GET_SUCCESS && type == CLM_BPA_TYPE_INT &&
                   clm_dictionary_get_type(dictionary_id, 20, &type) ==
                       CLM_BPA_GET_SUCCESS && type == CLM_BPA_TYPE_STRING,
               "report each heterogeneous value's BPA type");
    CLM_ASSERT(clm_dictionary_get_id(dictionary_id, 10, &stored_id) ==
                       CLM_BPA_GET_SUCCESS && stored_id == integer_source_id &&
                   clm_dictionary_get_to(dictionary_id, 10, &integer_result,
                                         sizeof(integer_result)) ==
                       CLM_BPA_GET_SUCCESS && integer_result == integer_value &&
                   clm_dictionary_get_to(dictionary_id, 20, string_result,
                                         sizeof(string_result)) ==
                       CLM_BPA_GET_SUCCESS &&
                   strcmp(string_result, "borrowed text") == 0,
               "retrieve borrowed values by ID and by copy");
    CLM_ASSERT(clm_dictionary_add(dictionary_id, 10, string_source_id) ==
                       CLM_BPA_FAILURE &&
                   clm_dictionary_update(dictionary_id, 30,
                                         integer_source_id) == CLM_BPA_FAILURE,
               "add rejects duplicates and update rejects missing keys");
    CLM_ASSERT(clm_dictionary_update(dictionary_id, 10, string_source_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_type(dictionary_id, 10, &type) ==
                       CLM_BPA_GET_SUCCESS && type == CLM_BPA_TYPE_STRING &&
                   clm_dictionary_set(dictionary_id, 10, integer_source_id) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_type(dictionary_id, 10, &type) ==
                       CLM_BPA_GET_SUCCESS && type == CLM_BPA_TYPE_INT,
               "replace a key with values of different types");

    CLM_ASSERT(clm_bpa_create(integer_destination_id, CLM_BPA_TYPE_INT) ==
                       CLM_BPA_SUCCESS &&
                   clm_dictionary_get(dictionary_id, 10,
                                      integer_destination_id) ==
                       CLM_BPA_GET_SUCCESS &&
                   clm_bpa_get(integer_destination_id, &integer_result,
                               sizeof(integer_result)) == CLM_BPA_GET_SUCCESS &&
                   integer_result == integer_value &&
                   clm_bpa_create(string_destination_id, CLM_BPA_TYPE_STRING) ==
                       CLM_BPA_SUCCESS &&
                   clm_dictionary_get(dictionary_id, 20,
                                      string_destination_id) ==
                       CLM_BPA_GET_SUCCESS,
               "copy heterogeneous values into compatible BPA destinations");
    CLM_ASSERT(clm_bpa_create(wrong_destination_id, CLM_BPA_TYPE_DOUBLE) ==
                       CLM_BPA_SUCCESS &&
                   clm_dictionary_get(dictionary_id, 10,
                                      wrong_destination_id) ==
                       CLM_BPA_FAILURE,
               "reject an incompatible typed destination");

    const clm_int copied_integer = 9876;
    CLM_ASSERT(clm_dictionary_set_from(dictionary_id, 40, CLM_BPA_TYPE_INT,
                                       &copied_integer,
                                       sizeof(copied_integer)) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_id(dictionary_id, 40, &stored_id) ==
                       CLM_BPA_GET_SUCCESS && stored_id != integer_source_id &&
                   clm_dictionary_get_to(dictionary_id, 40, &integer_result,
                                         sizeof(integer_result)) ==
                       CLM_BPA_GET_SUCCESS && integer_result == copied_integer,
               "copy caller-owned bytes into a dictionary-owned value block");
    const clm_id owned_integer_id = stored_id;

    const char copied_text[] = "owned text";
    CLM_ASSERT(clm_dictionary_set_from(dictionary_id, 50, CLM_BPA_TYPE_STRING,
                                       copied_text, sizeof(copied_text)) ==
                       CLM_BPA_SET_SUCCESS &&
                   clm_dictionary_get_id(dictionary_id, 50, &stored_id) ==
                       CLM_BPA_GET_SUCCESS && stored_id != string_source_id &&
                   clm_dictionary_get_to(dictionary_id, 50, string_result,
                                         sizeof(string_result)) ==
                       CLM_BPA_GET_SUCCESS &&
                   strcmp(string_result, copied_text) == 0,
               "copy a string into a dictionary-owned value block");
    const clm_id owned_string_id = stored_id;

    CLM_ASSERT(clm_dictionary_remove(dictionary_id, 40) == CLM_BPA_SUCCESS &&
                   clm_bpa_find_block(owned_integer_id) == NULL &&
                   clm_dictionary_contains(dictionary_id, 40) == false &&
                   clm_bpa_find_block(integer_source_id) != NULL,
               "remove owned values but preserve borrowed source blocks");
    CLM_ASSERT(clm_dictionary_clear(dictionary_id) == CLM_BPA_SUCCESS &&
                   clm_dictionary_count(dictionary_id) == 0 &&
                   clm_bpa_find_block(owned_string_id) == NULL &&
                   clm_bpa_find_block(string_source_id) != NULL,
               "clear releases owned values and preserves borrowed blocks");
    CLM_ASSERT(clm_dictionary_delete(dictionary_id) == CLM_BPA_SUCCESS &&
                   clm_dictionary_count(dictionary_id) == CLM_BPA_FAILURE,
               "delete heterogeneous dictionary and its index");
    CLM_ASSERT(clm_bpa_delete(integer_source_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(string_source_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(integer_destination_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(string_destination_id) == CLM_BPA_SUCCESS &&
                   clm_bpa_delete(wrong_destination_id) == CLM_BPA_SUCCESS,
               "caller releases borrowed values and destination blocks");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}

static int clm_dictionary_tests(void)
{
    const int typed_result = clm_dictionary_typed_tests();
    const int dynamic_result = clm_dictionary_dynamic_tests();
    return typed_result != 0 || dynamic_result != 0;
}
