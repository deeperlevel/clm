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
 
#include "bpa.h"

#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Two models share this header:
 * clm_typed_dictionary_*: homogeneous array-backed, value-copying dictionary.
 * clm_dictionary_*: heterogeneous key -> independently managed BPA block IDs.
 * Heterogeneous set()/add()/update() borrow existing BPA block references.
 * Heterogeneous set_from() makes an owned copy; owned blocks are released on
 * replacement, removal, clear and delete. Borrowed blocks remain caller-owned.
 * Keys are numeric clm_id values, not BPA string data.
 */

#define CLM_TYPED_DICTIONARY_INITIAL_CAPACITY 16u
#define CLM_TYPED_DICTIONARY_METADATA_MAGIC UINT64_C(0x434C4D444943544E)
#define CLM_TYPED_DICTIONARY_SLOT_EMPTY 0u
#define CLM_TYPED_DICTIONARY_SLOT_OCCUPIED 1u
#define CLM_TYPED_DICTIONARY_SLOT_DELETED 2u

/** Internal open-addressing slot holding one key and its value-array index. */
typedef struct {
    clm_id key;
    clm_uint value_index;
    clm_byte state;
} clm_typed_dictionary_slot;

/** Internal dictionary metadata stored in a BPA STRUCT block. */
typedef struct {
    uint64_t magic;
    clm_id slots_id;
    clm_id values_id;
    clm_id keys_id;
    clm_uint value_size;
    clm_uint count;
    clm_uint iterator;
    clm_byte value_type;
    clm_byte reserved[3];
} clm_typed_dictionary_metadata;

clm_bpa_block_node* clm_typed_dictionary_internal_metadata(
    const clm_id id);

int clm_typed_dictionary_internal_read(
    const clm_id id,
    clm_typed_dictionary_metadata* metadata);

clm_bpaa_array_node* clm_typed_dictionary_internal_array(
    const clm_id id,
    const clm_byte type,
    const clm_uint block_size);

clm_typed_dictionary_slot* clm_typed_dictionary_internal_slots(const clm_bpaa_array_node* array);

int clm_typed_dictionary_internal_allocate_array_id(
    const clm_id forbidden_a,
    const clm_id forbidden_b,
    clm_id* out_id);

int clm_typed_dictionary_internal_create_slots(
    const clm_id id,
    const clm_uint capacity);

clm_uint clm_typed_dictionary_internal_find_slot(
    const clm_typed_dictionary_slot* slots,
    const clm_uint capacity,
    const clm_id key,
    bool* found);

int clm_typed_dictionary_internal_rehash(
    const clm_id dictionary_id,
    clm_typed_dictionary_metadata* metadata,
    const clm_uint new_capacity);

int clm_typed_dictionary_internal_prepare_insert(
    const clm_id dictionary_id,
    clm_typed_dictionary_metadata* metadata,
    clm_bpaa_array_node** slots_array,
    clm_typed_dictionary_slot** slots);

int clm_typed_dictionary_internal_lookup(
    const clm_id dictionary_id,
    const clm_id key,
    clm_typed_dictionary_metadata* metadata,
    clm_bpaa_array_node** slots_array,
    clm_typed_dictionary_slot** slots,
    clm_uint* slot_index,
    bool* found);

/** Creates an empty dictionary for values of the specified BPA type.
 * @param id Unique dictionary ID; must not already identify a BPA block.
 * @param type BPA value type, including STRING or STRUCT.
 * @param size Required byte width for STRUCT; pass zero for other types.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
 */
int clm_typed_dictionary_new(
    const clm_id id,
    const clm_byte type,
    const size_t size);

/** Deletes a dictionary and its BPA/BPAA-owned storage.
 * @param id Dictionary ID to delete.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if it is missing/invalid.
 */
int clm_typed_dictionary_delete(const clm_id id);

/** Stores or replaces a value from a caller-owned buffer.
 * For STRING values, value must contain a terminating NUL within value_size.
 * @param id Dictionary ID.
 * @param key Integer key to create or replace.
 * @param value Source value bytes.
 * @param value_size Source byte count; fixed-width values require an exact match.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation failure.
 */
int clm_typed_dictionary_set_from(
    const clm_id id,
    const clm_id key,
    const void* value,
    const size_t value_size);

/** Stores or replaces a copy of a compatible BPA block's value.
 * @param id Dictionary ID.
 * @param key Integer key to create or replace.
 * @param source_block_id ID of the BPA block whose value to copy.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
 */
int clm_typed_dictionary_set(
    const clm_id id,
    const clm_id key,
    const clm_id source_block_id);

/** Retrieves a value into caller-owned memory.
 * @param id Dictionary ID.
 * @param key Key whose value to retrieve.
 * @param buffer Receives the value; strings include their NUL terminator.
 * @param buffer_size Destination capacity in bytes.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if missing/incompatible.
 */
int clm_typed_dictionary_get_to(
    const clm_id id,
    const clm_id key,
    void* buffer,
    const size_t buffer_size);

/** Retrieves a value into a compatible, pre-created BPA block.
 * @param id Dictionary ID.
 * @param key Key whose value to retrieve.
 * @param destination_block_id Compatible BPA destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if missing/incompatible.
 */
int clm_typed_dictionary_get(
    const clm_id id,
    const clm_id key,
    const clm_id destination_block_id);

/** Checks whether a key currently exists.
 * @param id Dictionary ID.
 * @param key Integer key to check.
 * @return true when present; false when absent or the dictionary is invalid.
 */
bool clm_typed_dictionary_contains(const clm_id id, const clm_id key);

/** Returns the number of key/value pairs, or CLM_BPA_FAILURE if invalid.
 * @param id Dictionary ID to query.
 * @return Entry count as clm_int, or CLM_BPA_FAILURE.
 */
clm_int clm_typed_dictionary_count(const clm_id id);

/** Removes a key and its value, preserving all other mappings.
 * @param id Dictionary ID.
 * @param key Integer key to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the key is absent/invalid.
 */
int clm_typed_dictionary_remove(const clm_id id, const clm_id key);

/** Resets ordered key iteration to the first currently stored key.
 * @param id Dictionary ID.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if invalid.
 */
int clm_typed_dictionary_iterate_reset(const clm_id id);

/** Returns the next key in insertion order.
 * The returned key can be passed to clm_typed_dictionary_get or
 * clm_typed_dictionary_get_to to retrieve its value. Adding/removing keys or clearing
 * the dictionary resets iteration; replacing a value does not.
 * @param id Dictionary ID.
 * @param key Receives the next key; must not be NULL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE when exhausted/invalid.
 */
int clm_typed_dictionary_iterate_next(
    const clm_id id,
    clm_id* key);

/** Removes every entry while retaining the dictionary ID and configured value type.
 * Reserved BPAA capacity is retained; the iteration cursor is reset.
 * @param id Dictionary ID.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the dictionary is invalid.
 */
int clm_typed_dictionary_clear(const clm_id id);

/** Inserts a new key/value pair, failing if the key already exists.
 * The value is copied from caller-owned memory and follows set_from's type and
 * size rules. Successful insertion places the key at the end of iteration order.
 * @param id Dictionary ID.
 * @param key New key; must not already exist.
 * @param value Source value bytes.
 * @param size Source byte count.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for duplicate/invalid input.
 */
int clm_typed_dictionary_add_from(
    const clm_id id, const clm_id key, const void* value, const size_t size);

/** Replaces the value for an existing key without changing its insertion order.
 * The value is copied from caller-owned memory and follows set_from's type and
 * size rules.
 * @param id Dictionary ID.
 * @param key Existing key to update.
 * @param value Replacement value bytes.
 * @param size Replacement byte count.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE if the key is missing/invalid.
 */
int clm_typed_dictionary_update_from(
    const clm_id id, const clm_id key, const void* value, const size_t size);

/** Copies all keys to caller-owned memory in insertion order.
 * The buffer must hold at least clm_typed_dictionary_count(id) keys. A NULL buffer is
 * allowed only when the dictionary is empty.
 * @param id Dictionary ID.
 * @param buffer Receives the ordered keys.
 * @param capacity Number of clm_id elements available in buffer.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for insufficient capacity or
 *         an invalid dictionary.
 */
int clm_typed_dictionary_keys_to(
    const clm_id id, clm_id* buffer, const size_t capacity);

/* ===== Heterogeneous dictionary (CLM_BPA_TYPE_ANY) ===== */

#define CLM_DICTIONARY_METADATA_MAGIC UINT64_C(0x434C4D414E593031)

typedef struct {
    clm_id value_id;
    clm_byte owned;
    clm_byte reserved[7];
} clm_dictionary_value_ref;

typedef struct {
    uint64_t magic;
    clm_id index_id; /* Private typed dictionary of clm_dictionary_value_ref */
} clm_dictionary_any_metadata;

int clm_dictionary_internal_read(
    clm_id id, clm_dictionary_any_metadata* out);

int clm_dictionary_internal_unused(clm_id avoid, clm_id* out);

int clm_dictionary_internal_ref(
    clm_id id, clm_id key, clm_dictionary_value_ref* value);

/** Create a heterogeneous dictionary; values are independently typed BPA blocks. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
int clm_dictionary_new(clm_id id);

int clm_dictionary_internal_assign(clm_id id, clm_id key,
    clm_id value_id, bool owned, int policy);

/** Map a key to an existing BPA block (borrowed; caller manages its lifetime). 
 * @param id CLM ID identifying the id.
 * @param key Input parameter described above.
 * @param source_block_id CLM ID identifying the source block.
 * @return Operation status or value described in the summary above.
 */
int clm_dictionary_set(clm_id id, clm_id key, clm_id source_block_id);
int clm_dictionary_add(clm_id id, clm_id key, clm_id source_block_id);
int clm_dictionary_update(clm_id id, clm_id key, clm_id source_block_id);

/** Copy bytes into a new, dictionary-owned BPA block of the declared type.
 * STRING source must include NUL and must be a valid CLM UTF-8 string.
 * @param id CLM ID identifying the id.
 * @param key Input parameter described above.
 * @param type Input parameter described above.
 * @param value Input data or buffer; not modified.
 * @param size Size in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
int clm_dictionary_set_from(clm_id id, clm_id key,
    clm_byte type, const void* value, size_t size);

/** Get the underlying BPA block identity; no copy and no ownership transfer. 
 * @param id CLM ID identifying the id.
 * @param key Input parameter described above.
 * @param out Writable output buffer.
 * @return Operation status or value described in the summary above.
 */
int clm_dictionary_get_id(clm_id id, clm_id key, clm_id* out);

int clm_dictionary_get_type(clm_id id, clm_id key, clm_byte* out);

/** Copy into caller memory; for strings include the NUL terminator. 
 * @param id CLM ID identifying the id.
 * @param key Input parameter described above.
 * @param buffer Writable output buffer.
 * @param capacity Available capacity.
 * @return Operation status or value described in the summary above.
 */
int clm_dictionary_get_to(clm_id id, clm_id key, void* buffer, size_t capacity);

/** Copy into pre-created, compatible BPA destination. 
 * @param id CLM ID identifying the id.
 * @param key Input parameter described above.
 * @param destination_id CLM ID identifying the destination.
 * @return Operation status or value described in the summary above.
 */
int clm_dictionary_get(clm_id id, clm_id key, clm_id destination_id);

bool clm_dictionary_contains(clm_id id, clm_id key);

clm_int clm_dictionary_count(clm_id id);

int clm_dictionary_remove(clm_id id, clm_id key);

int clm_dictionary_iterate_reset(clm_id id);

int clm_dictionary_iterate_next(clm_id id, clm_id* key);

int clm_dictionary_keys_to(clm_id id, clm_id* keys, size_t capacity);

int clm_dictionary_clear(clm_id id);

int clm_dictionary_delete(clm_id id);
