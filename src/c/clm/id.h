#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "dictionary.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ID_STORAGE_ID UINT64_C(0x0F000001)
#define CLM_ID_INDEX_STORAGE_ID UINT64_C(0x0F000002)
#define CLM_ID_NAMES_STORAGE_ID UINT64_C(0x0F000004)
#define CLM_ID_VALUES_STORAGE_ID UINT64_C(0x0F000005)
#define CLM_ID_NAME_BUFFER_ID UINT64_C(0x0F000003)
#define CLM_ID_NOT_FOUND (~UINT64_C(0))

/** Global ID-generator and registry state. Define CLM_ID_IMPLEMENTATION once. */
extern clm_id clm_id_generator_value;
extern clm_id clm_id_next_entry_value;
extern clm_id clm_id_active_index;
extern bool clm_id_generator_exhausted;
extern bool clm_id_case_sensitive;

#ifdef CLM_ID_IMPLEMENTATION
clm_id clm_id_generator_value = 0;
clm_id clm_id_next_entry_value = 0;
clm_id clm_id_active_index = ID_STORAGE_ID;
bool clm_id_generator_exhausted = false;
bool clm_id_case_sensitive = true;
#endif

/** Hashes a name, applying the C binding's ASCII case-folding policy. */
static inline uint32_t clm_id_internal_hash_name(
    const char* name,
    const bool case_sensitive)
{
    uint32_t hash = UINT32_C(2166136261);
    const unsigned char* cursor = (const unsigned char*)name;
    while (*cursor != '\0') {
        unsigned char character = *cursor++;
        if (!case_sensitive && character >= 'A' && character <= 'Z')
            character = (unsigned char)(character + ('a' - 'A'));
        hash ^= character;
        hash *= UINT32_C(16777619);
    }
    return hash;
}

/** Validates the UTF-8 bytes in a NUL-terminated name. */
static inline bool clm_id_internal_valid_name(const char* name)
{
    const unsigned char* text = (const unsigned char*)name;
    while (*text != '\0') {
        if (*text <= 0x7f) {
            ++text;
        } else if (*text >= 0xc2 && *text <= 0xdf) {
            if (text[1] < 0x80 || text[1] > 0xbf)
                return false;
            text += 2;
        } else if (*text >= 0xe0 && *text <= 0xef) {
            if (text[1] == '\0' || text[2] == '\0' ||
                text[1] < 0x80 || text[1] > 0xbf ||
                text[2] < 0x80 || text[2] > 0xbf ||
                (*text == 0xe0 && text[1] < 0xa0) ||
                (*text == 0xed && text[1] >= 0xa0))
                return false;
            text += 3;
        } else if (*text >= 0xf0 && *text <= 0xf4) {
            if (text[1] == '\0' || text[2] == '\0' || text[3] == '\0' ||
                text[1] < 0x80 || text[1] > 0xbf ||
                text[2] < 0x80 || text[2] > 0xbf ||
                text[3] < 0x80 || text[3] > 0xbf ||
                (*text == 0xf0 && text[1] < 0x90) ||
                (*text == 0xf4 && text[1] >= 0x90))
                return false;
            text += 4;
        } else {
            return false;
        }
    }
    return true;
}

/** Compares names exactly, or with ASCII-only case folding when requested. */
static inline bool clm_id_internal_names_equal(
    const char* left,
    const char* right,
    const bool case_sensitive)
{
    while (*left != '\0' && *right != '\0') {
        unsigned char left_character = (unsigned char)*left++;
        unsigned char right_character = (unsigned char)*right++;
        if (!case_sensitive) {
            if (left_character >= 'A' && left_character <= 'Z')
                left_character = (unsigned char)(left_character + ('a' - 'A'));
            if (right_character >= 'A' && right_character <= 'Z')
                right_character = (unsigned char)(right_character + ('a' - 'A'));
        }
        if (left_character != right_character)
            return false;
    }
    return *left == '\0' && *right == '\0';
}

/** Initializes private dictionaries and the shared BPA string scratch block. */
static inline int clm_id_internal_ensure_storage(void)
{
    const clm_int index_a_count = clm_typed_dictionary_count(ID_STORAGE_ID);
    const clm_int index_b_count =
        clm_typed_dictionary_count(CLM_ID_INDEX_STORAGE_ID);
    const clm_int names_count =
        clm_typed_dictionary_count(CLM_ID_NAMES_STORAGE_ID);
    const clm_int values_count =
        clm_typed_dictionary_count(CLM_ID_VALUES_STORAGE_ID);
    const clm_bpa_block_node* buffer =
        clm_bpa_find_block(CLM_ID_NAME_BUFFER_ID);

    if (index_a_count >= 0 && index_b_count >= 0 && names_count >= 0 &&
        values_count >= 0 && buffer != NULL &&
        buffer->type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_SUCCESS;

    if (clm_bpa_is_existing(ID_STORAGE_ID) ||
        clm_bpa_is_existing(CLM_ID_INDEX_STORAGE_ID) ||
        clm_bpa_is_existing(CLM_ID_NAMES_STORAGE_ID) ||
        clm_bpa_is_existing(CLM_ID_VALUES_STORAGE_ID) ||
        clm_bpa_is_existing(CLM_ID_NAME_BUFFER_ID) ||
        clm_bpaa_is_existing(ID_STORAGE_ID) ||
        clm_bpaa_is_existing(CLM_ID_INDEX_STORAGE_ID) ||
        clm_bpaa_is_existing(CLM_ID_NAMES_STORAGE_ID) ||
        clm_bpaa_is_existing(CLM_ID_VALUES_STORAGE_ID) ||
        clm_bpaa_is_existing(CLM_ID_NAME_BUFFER_ID))
        return CLM_BPA_FAILURE;

    if (clm_typed_dictionary_new(ID_STORAGE_ID, CLM_BPA_TYPE_STRUCT,
                                 sizeof(clm_id)) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_typed_dictionary_new(CLM_ID_INDEX_STORAGE_ID, CLM_BPA_TYPE_STRUCT,
                                 sizeof(clm_id)) != CLM_BPA_SUCCESS) {
        (void)clm_typed_dictionary_delete(ID_STORAGE_ID);
        return CLM_BPA_FAILURE;
    }
    if (clm_typed_dictionary_new(CLM_ID_NAMES_STORAGE_ID, CLM_BPA_TYPE_STRING, 0) !=
        CLM_BPA_SUCCESS) {
        (void)clm_typed_dictionary_delete(CLM_ID_INDEX_STORAGE_ID);
        (void)clm_typed_dictionary_delete(ID_STORAGE_ID);
        return CLM_BPA_FAILURE;
    }
    if (clm_typed_dictionary_new(CLM_ID_VALUES_STORAGE_ID, CLM_BPA_TYPE_STRUCT,
                                 sizeof(clm_id)) != CLM_BPA_SUCCESS) {
        (void)clm_typed_dictionary_delete(CLM_ID_NAMES_STORAGE_ID);
        (void)clm_typed_dictionary_delete(CLM_ID_INDEX_STORAGE_ID);
        (void)clm_typed_dictionary_delete(ID_STORAGE_ID);
        return CLM_BPA_FAILURE;
    }
    if (clm_bpa_create(CLM_ID_NAME_BUFFER_ID, CLM_BPA_TYPE_STRING) !=
        CLM_BPA_SUCCESS) {
        (void)clm_typed_dictionary_delete(CLM_ID_VALUES_STORAGE_ID);
        (void)clm_typed_dictionary_delete(CLM_ID_NAMES_STORAGE_ID);
        (void)clm_typed_dictionary_delete(CLM_ID_INDEX_STORAGE_ID);
        (void)clm_typed_dictionary_delete(ID_STORAGE_ID);
        return CLM_BPA_FAILURE;
    }
    clm_id_active_index = ID_STORAGE_ID;
    clm_id_next_entry_value = 0;
    return CLM_BPA_SUCCESS;
}

/** Copies one internally stored name into caller-owned temporary memory. */
static inline int clm_id_internal_copy_name(
    const clm_id entry_id,
    char** out_name)
{
    const clm_bpa_block_node* buffer;
    char* copy;
    if (out_name == NULL ||
        clm_typed_dictionary_get(CLM_ID_NAMES_STORAGE_ID, entry_id,
                     CLM_ID_NAME_BUFFER_ID) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    buffer = clm_bpa_find_block(CLM_ID_NAME_BUFFER_ID);
    if (buffer == NULL || buffer->type != CLM_BPA_TYPE_STRING ||
        buffer->data == NULL || buffer->length == UINT_MAX)
        return CLM_BPA_FAILURE;
    copy = (char*)malloc((size_t)buffer->length + 1);
    if (copy == NULL)
        return CLM_BPA_FAILURE;
    memcpy(copy, buffer->data, (size_t)buffer->length + 1);
    *out_name = copy;
    return CLM_BPA_SUCCESS;
}

/** Finds a name in an index dictionary, including tombstone-aware probing. */
static inline int clm_id_internal_find_name(
    const clm_id index_id,
    const char* name,
    const bool case_sensitive,
    clm_id* out_entry_id,
    clm_id* out_slot_key,
    bool* out_found)
{
    const clm_int entry_count = clm_typed_dictionary_count(index_id);
    const uint32_t hash = clm_id_internal_hash_name(name, case_sensitive);
    clm_id first_tombstone = CLM_ID_NOT_FOUND;
    if (entry_count < 0 || out_slot_key == NULL || out_found == NULL)
        return CLM_BPA_FAILURE;

    *out_found = false;
    for (clm_uint probe = 0; probe <= (clm_uint)entry_count; ++probe) {
        const clm_id candidate = (clm_id)(hash + probe);
        if (!clm_typed_dictionary_contains(index_id, candidate)) {
            *out_slot_key = first_tombstone != CLM_ID_NOT_FOUND
                ? first_tombstone : candidate;
            return CLM_BPA_SUCCESS;
        }

        clm_id entry_id;
        if (clm_typed_dictionary_get_to(index_id, candidate, &entry_id,
                           sizeof(entry_id)) != CLM_BPA_GET_SUCCESS)
            return CLM_BPA_FAILURE;
        if (entry_id == CLM_ID_NOT_FOUND) {
            if (first_tombstone == CLM_ID_NOT_FOUND)
                first_tombstone = candidate;
            continue;
        }

        char* stored_name;
        if (clm_id_internal_copy_name(entry_id, &stored_name) !=
            CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        const bool matches = clm_id_internal_names_equal(
            stored_name, name, case_sensitive);
        free(stored_name);
        if (matches) {
            *out_found = true;
            *out_slot_key = candidate;
            if (out_entry_id != NULL)
                *out_entry_id = entry_id;
            return CLM_BPA_SUCCESS;
        }
    }

    if (first_tombstone != CLM_ID_NOT_FOUND) {
        *out_slot_key = first_tombstone;
        return CLM_BPA_SUCCESS;
    }
    return CLM_BPA_FAILURE;
}

/** Allocates an unused internal entry key without colliding with active entries. */
static inline int clm_id_internal_allocate_entry_id(clm_id* out_entry_id)
{
    const clm_int count = clm_typed_dictionary_count(CLM_ID_NAMES_STORAGE_ID);
    if (count < 0 || out_entry_id == NULL)
        return CLM_BPA_FAILURE;
    for (clm_uint attempt = 0; attempt <= (clm_uint)count; ++attempt) {
        const clm_id candidate = clm_id_next_entry_value + attempt;
        if (candidate != CLM_ID_NOT_FOUND &&
            !clm_typed_dictionary_contains(CLM_ID_NAMES_STORAGE_ID, candidate)) {
            *out_entry_id = candidate;
            clm_id_next_entry_value = candidate == CLM_ID_NOT_FOUND - 1
                ? 0 : candidate + 1;
            return CLM_BPA_SUCCESS;
        }
    }
    return CLM_BPA_FAILURE;
}

/** Adds or replaces a hash-index slot with an internal entry key. */
static inline int clm_id_internal_store_index(
    const clm_id index_id,
    const clm_id slot_key,
    const clm_id entry_id)
{
    return clm_typed_dictionary_contains(index_id, slot_key)
        ? clm_typed_dictionary_update_from(index_id, slot_key, &entry_id,
                                           sizeof(entry_id))
        : clm_typed_dictionary_add_from(index_id, slot_key, &entry_id,
                                        sizeof(entry_id));
}

/** Inserts a new, nonempty name mapping and rejects duplicate effective names.
 * Multiple names may map to the same numeric ID.
 * @param name Name to register, compared according to the active case policy.
 * @param id Numeric ID associated with the name.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid/duplicate input.
 */
static inline int clm_id_register(const char* name, const clm_id id)
{
    clm_id slot_key;
    clm_id entry_id;
    bool found;
    if (name == NULL || name[0] == '\0' ||
        !clm_id_internal_valid_name(name) || strlen(name) > UINT_MAX ||
        clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS ||
        clm_id_internal_find_name(clm_id_active_index, name,
                                  clm_id_case_sensitive, NULL, &slot_key,
                                  &found) != CLM_BPA_SUCCESS ||
        found ||
        clm_id_internal_allocate_entry_id(&entry_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    if (clm_typed_dictionary_set_from(CLM_ID_NAMES_STORAGE_ID, entry_id,
                                      name, strlen(name) + 1) !=
        CLM_BPA_SET_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_typed_dictionary_set_from(CLM_ID_VALUES_STORAGE_ID, entry_id,
                                      &id, sizeof(id)) != CLM_BPA_SET_SUCCESS) {
        (void)clm_typed_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry_id);
        return CLM_BPA_FAILURE;
    }
    if (clm_id_internal_store_index(clm_id_active_index, slot_key, entry_id) !=
        CLM_BPA_SET_SUCCESS) {
        (void)clm_typed_dictionary_remove(CLM_ID_VALUES_STORAGE_ID, entry_id);
        (void)clm_typed_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry_id);
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SET_SUCCESS;
}

/** Maps a name to an ID; alias for clm_id_register.
 * @param name Nonempty UTF-8 name to register.
 * @param id Numeric ID; multiple distinct names may share it.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for a duplicate/invalid name.
 */
static inline int clm_id_map(const char* name, const clm_id id)
{
    return clm_id_register(name, id);
}

/** Resolves a registered name to its numeric ID.
 * @param name Name to look up using the active case policy.
 * @return The mapped ID, or CLM_ID_NOT_FOUND when absent/invalid.
 */
static inline clm_id clm_id_get(const char* name)
{
    clm_id entry_id;
    clm_id slot_key;
    bool found;
    clm_id value;
    if (name == NULL || name[0] == '\0' ||
        !clm_id_internal_valid_name(name) ||
        clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS ||
        clm_id_internal_find_name(clm_id_active_index, name,
                                  clm_id_case_sensitive, &entry_id, &slot_key,
                                  &found) != CLM_BPA_SUCCESS ||
        !found ||
        clm_typed_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry_id, &value,
                        sizeof(value)) != CLM_BPA_GET_SUCCESS)
        return CLM_ID_NOT_FOUND;
    return value;
}

/** Resolves a name and reports success separately from the mapped ID.
 * This permits CLM_ID_NOT_FOUND to be used as an ordinary mapped ID.
 * @param name Name to look up.
 * @param out_id Receives the numeric ID; must not be NULL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
 */
static inline int clm_id_try_get(const char* name, clm_id* out_id)
{
    clm_id value;
    if (out_id == NULL || name == NULL || name[0] == '\0' ||
        !clm_id_internal_valid_name(name) ||
        clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    clm_id entry_id;
    clm_id slot_key;
    bool found;
    if (clm_id_internal_find_name(clm_id_active_index, name,
                                  clm_id_case_sensitive, &entry_id, &slot_key,
                                  &found) != CLM_BPA_SUCCESS ||
        !found ||
        clm_typed_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry_id, &value,
                        sizeof(value)) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    *out_id = value;
    return CLM_BPA_GET_SUCCESS;
}

/** Copies the first registered spelling associated with an ID.
 * Aliases are permitted, so reverse lookup returns the earliest remaining
 * registration in insertion order. Pass buffer=NULL and capacity=0 to query
 * the UTF-8 byte length excluding NUL.
 * @param id Numeric ID to resolve.
 * @param buffer Receives the NUL-terminated name, or NULL for a size query.
 * @param capacity Buffer size including space for NUL.
 * @param byte_length Receives the name byte length excluding NUL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if unmapped/too small.
 */
static inline int clm_id_get_name(
    const clm_id id,
    char* buffer,
    const size_t capacity,
    size_t* byte_length)
{
    clm_id entry_id;
    size_t found_length;
    char* name;
    if (byte_length == NULL || (buffer == NULL && capacity != 0) ||
        clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_iterate_reset(CLM_ID_VALUES_STORAGE_ID) !=
            CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    while (clm_typed_dictionary_iterate_next(CLM_ID_VALUES_STORAGE_ID,
                                              &entry_id) == CLM_BPA_GET_SUCCESS) {
        clm_id mapped_id;
        if (clm_typed_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry_id,
                                        &mapped_id, sizeof(mapped_id)) !=
            CLM_BPA_GET_SUCCESS)
            return CLM_BPA_FAILURE;
        if (mapped_id != id)
            continue;
        if (clm_id_internal_copy_name(entry_id, &name) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        found_length = strlen(name);
        *byte_length = found_length;
        if (buffer == NULL) {
            free(name);
            return CLM_BPA_GET_SUCCESS;
        }
        if (capacity <= found_length) {
            free(name);
            return CLM_BPA_FAILURE;
        }
        memcpy(buffer, name, found_length + 1);
        free(name);
        return CLM_BPA_GET_SUCCESS;
    }
    return CLM_BPA_FAILURE;
}

/** Inserts a mapping or replaces the ID for an existing effective name.
 * Updating retains the originally registered spelling and insertion order.
 * @param name Name to insert or update.
 * @param id New numeric ID.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
static inline int clm_id_set(const char* name, const clm_id id)
{
    clm_id entry_id;
    clm_id slot_key;
    bool found;
    if (name == NULL || name[0] == '\0' ||
        !clm_id_internal_valid_name(name) || strlen(name) > UINT_MAX ||
        clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS ||
        clm_id_internal_find_name(clm_id_active_index, name,
                                  clm_id_case_sensitive, &entry_id, &slot_key,
                                  &found) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (!found)
        return clm_id_register(name, id);
    return clm_typed_dictionary_update_from(CLM_ID_VALUES_STORAGE_ID, entry_id,
                                            &id, sizeof(id));
}

/** Removes one mapping by name, leaving mappings for aliases intact.
 * @param name Name to remove under the active case policy.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the name is absent/invalid.
 */
static inline int clm_id_remove(const char* name)
{
    clm_id entry_id;
    clm_id slot_key;
    bool found;
    if (name == NULL || name[0] == '\0' ||
        !clm_id_internal_valid_name(name) ||
        clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS ||
        clm_id_internal_find_name(clm_id_active_index, name,
                                  clm_id_case_sensitive, &entry_id, &slot_key,
                                  &found) != CLM_BPA_SUCCESS ||
        !found)
        return CLM_BPA_FAILURE;
    const clm_id tombstone = CLM_ID_NOT_FOUND;
    if (clm_id_internal_store_index(clm_id_active_index, slot_key, tombstone) !=
            CLM_BPA_SET_SUCCESS ||
        clm_typed_dictionary_remove(CLM_ID_VALUES_STORAGE_ID, entry_id) !=
            CLM_BPA_SUCCESS ||
        clm_typed_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry_id) !=
            CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

/** Removes every registered name that maps to the supplied numeric ID.
 * @param id Numeric ID whose aliases should all be removed.
 * @return CLM_BPA_SUCCESS if at least one mapping was removed; otherwise failure.
 */
static inline int clm_id_unmap(const clm_id id)
{
    bool removed = false;
    if (clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    for (;;) {
        clm_id entry_id;
        clm_id current_id;
        char* name;
        bool found_entry = false;
        if (clm_typed_dictionary_iterate_reset(CLM_ID_NAMES_STORAGE_ID) !=
            CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        while (clm_typed_dictionary_iterate_next(CLM_ID_NAMES_STORAGE_ID,
                                                 &entry_id) == CLM_BPA_GET_SUCCESS) {
            if (clm_typed_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry_id,
                                            &current_id, sizeof(current_id)) !=
                CLM_BPA_GET_SUCCESS)
                return CLM_BPA_FAILURE;
            if (current_id == id) {
                found_entry = true;
                break;
            }
        }
        if (!found_entry)
            break;
        if (clm_id_internal_copy_name(entry_id, &name) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        const int result = clm_id_remove(name);
        free(name);
        if (result != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        removed = true;
    }
    return removed ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

/** Removes one mapping by name; equivalent to clm_id_remove.
 * @param name Name to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
 */
static inline int clm_id_unmap_name(const char* name)
{
    return clm_id_remove(name);
}

/** Returns whether exact-case matching is enabled. */
static inline bool clm_id_is_case_sensitive(void)
{
    return clm_id_case_sensitive;
}

/** Changes name matching policy after validating all existing names.
 * Case-insensitive matching in this C binding folds ASCII letters; bytes outside
 * ASCII are compared exactly. If folding creates a collision, state is unchanged.
 * @param enabled true for exact matching; false for ASCII case-insensitive.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if validation/allocation fails.
 */
static inline int clm_id_set_case_sensitive(const bool enabled)
{
    clm_id next_index;
    clm_id entry_id;
    if (clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (enabled == clm_id_case_sensitive)
        return CLM_BPA_SUCCESS;
    next_index = clm_id_active_index == ID_STORAGE_ID
        ? CLM_ID_INDEX_STORAGE_ID : ID_STORAGE_ID;
    if (clm_typed_dictionary_clear(next_index) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_iterate_reset(CLM_ID_NAMES_STORAGE_ID) !=
            CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    while (clm_typed_dictionary_iterate_next(CLM_ID_NAMES_STORAGE_ID, &entry_id) ==
           CLM_BPA_GET_SUCCESS) {
        char* name;
        clm_id slot_key;
        bool found;
        if (clm_id_internal_copy_name(entry_id, &name) != CLM_BPA_SUCCESS) {
            (void)clm_typed_dictionary_clear(next_index);
            return CLM_BPA_FAILURE;
        }
        const int search_result = clm_id_internal_find_name(
            next_index, name, enabled, NULL, &slot_key, &found);
        free(name);
        if (search_result != CLM_BPA_SUCCESS || found ||
            clm_typed_dictionary_add_from(next_index, slot_key, &entry_id,
                                          sizeof(entry_id)) != CLM_BPA_SET_SUCCESS) {
            (void)clm_typed_dictionary_clear(next_index);
            return CLM_BPA_FAILURE;
        }
    }

    clm_id_active_index = next_index;
    clm_id_case_sensitive = enabled;
    return CLM_BPA_SUCCESS;
}

/** Removes all mappings while preserving the active matching policy. */
static inline int clm_id_clear(void)
{
    if (clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_clear(ID_STORAGE_ID) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_clear(CLM_ID_INDEX_STORAGE_ID) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_clear(CLM_ID_NAMES_STORAGE_ID) != CLM_BPA_SUCCESS ||
        clm_typed_dictionary_clear(CLM_ID_VALUES_STORAGE_ID) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    clm_id_active_index = ID_STORAGE_ID;
    clm_id_next_entry_value = 0;
    return CLM_BPA_SUCCESS;
}

/** Releases the private dictionaries and scratch block.
 * The next mapping operation lazily recreates storage. Registry matching mode
 * and ID generator state are retained.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if cleanup cannot complete.
 */
static inline int clm_id_shutdown(void)
{
    int status = CLM_BPA_SUCCESS;
    if (clm_typed_dictionary_delete(CLM_ID_VALUES_STORAGE_ID) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_typed_dictionary_delete(CLM_ID_NAMES_STORAGE_ID) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_typed_dictionary_delete(CLM_ID_INDEX_STORAGE_ID) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_typed_dictionary_delete(ID_STORAGE_ID) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    if (clm_bpa_delete(CLM_ID_NAME_BUFFER_ID) != CLM_BPA_SUCCESS)
        status = CLM_BPA_FAILURE;
    clm_id_active_index = ID_STORAGE_ID;
    clm_id_next_entry_value = 0;
    return status;
}

/** Resets sequential ID generation so the next call returns zero. */
static inline void clm_id_gen_reset(void)
{
    clm_id_generator_value = 0;
    clm_id_generator_exhausted = false;
}

/** Returns the next sequential ID, beginning at zero.
 * Generated IDs are not checked against IDs supplied to registration functions.
 * After the available range is exhausted, this returns CLM_ID_NOT_FOUND.
 */
static inline clm_id clm_id_gen_next(void)
{
    if (clm_id_generator_exhausted)
        return CLM_ID_NOT_FOUND;
    const clm_id result = clm_id_generator_value;
    if (result == CLM_ID_NOT_FOUND - 1)
        clm_id_generator_exhausted = true;
    else
        ++clm_id_generator_value;
    return result;
}
