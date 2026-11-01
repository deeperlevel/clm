#pragma once

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

#include "dictionary.h"

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


/** Hashes a name, applying the C binding's ASCII case-folding policy. */
uint32_t clm_id_internal_hash_name(
    const char* name,
    const bool case_sensitive);

/** Validates the UTF-8 bytes in a NUL-terminated name. */
bool clm_id_internal_valid_name(const char* name);

/** Compares names exactly, or with ASCII-only case folding when requested. */
bool clm_id_internal_names_equal(
    const char* left,
    const char* right,
    const bool case_sensitive);

/** Initializes private dictionaries and the shared BPA string scratch block. */
int clm_id_internal_ensure_storage(void);

/** Copies one internally stored name into caller-owned temporary memory. */
int clm_id_internal_copy_name(
    const clm_id entry_id,
    char** out_name);

/** Finds a name in an index dictionary, including tombstone-aware probing. */
int clm_id_internal_find_name(
    const clm_id index_id,
    const char* name,
    const bool case_sensitive,
    clm_id* out_entry_id,
    clm_id* out_slot_key,
    bool* out_found);

/** Allocates an unused internal entry key without colliding with active entries. */
int clm_id_internal_allocate_entry_id(clm_id* out_entry_id);

/** Adds or replaces a hash-index slot with an internal entry key. */
int clm_id_internal_store_index(
    const clm_id index_id,
    const clm_id slot_key,
    const clm_id entry_id);

/** Inserts a new, nonempty name mapping and rejects duplicate effective names.
 * Multiple names may map to the same numeric ID.
 * @param name Name to register, compared according to the active case policy.
 * @param id Numeric ID associated with the name.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid/duplicate input.
 */
int clm_id_register(const char* name, const clm_id id);

/** Maps a name to an ID; alias for clm_id_register.
 * @param name Nonempty UTF-8 name to register.
 * @param id Numeric ID; multiple distinct names may share it.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for a duplicate/invalid name.
 */
int clm_id_map(const char* name, const clm_id id);

/** Resolves a registered name to its numeric ID.
 * @param name Name to look up using the active case policy.
 * @return The mapped ID, or CLM_ID_NOT_FOUND when absent/invalid.
 */
clm_id clm_id_get(const char* name);

/** Resolves a name and reports success separately from the mapped ID.
 * This permits CLM_ID_NOT_FOUND to be used as an ordinary mapped ID.
 * @param name Name to look up.
 * @param out_id Receives the numeric ID; must not be NULL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
 */
int clm_id_try_get(const char* name, clm_id* out_id);

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
int clm_id_get_name(
    const clm_id id,
    char* buffer,
    const size_t capacity,
    size_t* byte_length);

/** Inserts a mapping or replaces the ID for an existing effective name.
 * Updating retains the originally registered spelling and insertion order.
 * @param name Name to insert or update.
 * @param id New numeric ID.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
int clm_id_set(const char* name, const clm_id id);

/** Removes one mapping by name, leaving mappings for aliases intact.
 * @param name Name to remove under the active case policy.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the name is absent/invalid.
 */
int clm_id_remove(const char* name);

/** Removes every registered name that maps to the supplied numeric ID.
 * @param id Numeric ID whose aliases should all be removed.
 * @return CLM_BPA_SUCCESS if at least one mapping was removed; otherwise failure.
 */
int clm_id_unmap(const clm_id id);

/** Removes one mapping by name; equivalent to clm_id_remove.
 * @param name Name to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
 */
int clm_id_unmap_name(const char* name);

/** Returns whether exact-case matching is enabled. 
 * @return Whether exact-case matching is enabled.
 */
bool clm_id_is_case_sensitive(void);

/** Changes name matching policy after validating all existing names.
 * Case-insensitive matching in this C binding folds ASCII letters; bytes outside
 * ASCII are compared exactly. If folding creates a collision, state is unchanged.
 * @param enabled true for exact matching; false for ASCII case-insensitive.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if validation/allocation fails.
 */
int clm_id_set_case_sensitive(const bool enabled);

/** Removes all mappings while preserving the active matching policy. 
 * @return Operation status or value described in the summary above.
 */
int clm_id_clear(void);

/** Releases the private dictionaries and scratch block.
 * The next mapping operation lazily recreates storage. Registry matching mode
 * and ID generator state are retained.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if cleanup cannot complete.
 */
int clm_id_shutdown(void);

/** Resets sequential ID generation so the next call returns zero. */
void clm_id_gen_reset(void);

/** Returns the next sequential ID, beginning at zero.
 * Generated IDs are not checked against IDs supplied to registration functions.
 * After the available range is exhausted, this returns CLM_ID_NOT_FOUND.
 * @return The next sequential ID, beginning at zero. Generated IDs are not checked against IDs supplied to registration functions. After the available range is exhausted, this returns CLM_ID_NOT_FOUND.
 */
clm_id clm_id_gen_next(void);
