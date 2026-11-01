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
#include <stdlib.h>

#define CLM_TREE_MAX_DEPTH 256u
#define CLM_TREE_ARRAY_MAGIC UINT64_C(0x434C4D5452454152)
#define CLM_TREE_NULL_MAGIC UINT64_C(0x434C4D5452454E55)
#define CLM_TREE_BOOL_MAGIC UINT64_C(0x434C4D545245424F)

typedef struct {
	uint64_t magic;
	clm_id items_id;
} clm_tree_array_metadata;

typedef struct {
	uint64_t magic;
} clm_tree_null_metadata;

typedef struct {
	uint64_t magic;
	clm_byte value;
	clm_byte reserved[7];
} clm_tree_bool_metadata;

typedef struct {
	clm_id id;
	size_t depth;
} clm_tree_internal_node;

typedef struct {
	clm_tree_internal_node* nodes;
	size_t count;
	size_t capacity;
} clm_tree_internal_id_list;

/** Validates an ID as a dynamic dictionary object node. */
bool clm_tree_internal_is_object(const clm_id id);

bool clm_tree_internal_is_array(const clm_id id);

bool clm_tree_internal_is_container(const clm_id id);

int clm_tree_internal_array_read(
	const clm_id id,
	clm_tree_array_metadata* metadata);

bool clm_tree_internal_is_null(const clm_id id);

bool clm_tree_internal_get_boolean(
	const clm_id id,
	bool* value);

/** Checks whether an object ID has already been collected. */
bool clm_tree_internal_id_list_contains(
	const clm_tree_internal_id_list* list,
	const clm_id id);

/** Appends a unique object ID and enforces the maximum nesting depth. */
int clm_tree_internal_id_list_append(
	clm_tree_internal_id_list* list,
	const clm_id id,
	const size_t depth);

/** Collects an exclusive acyclic object hierarchy before any deletion occurs. */
/** Walks child-object references and rejects cycles or shared subtrees. */
int clm_tree_internal_collect(
	const clm_id root_id,
	clm_tree_internal_id_list* list);

/** Releases the temporary hierarchy list without deleting its objects. */
void clm_tree_internal_id_list_dispose(
	clm_tree_internal_id_list* list);

/** Deletes a previously collected hierarchy in parent-before-child order. */
/** Deletes all collected object dictionaries and releases the temporary list. */
int clm_tree_internal_delete_collected(
	clm_tree_internal_id_list* list);

int clm_tree_internal_allocate_unused_id(
	const clm_id avoid,
	clm_id* out_id);

/** Deletes an object and its exclusively owned nested object hierarchy.
 * Cycles, shared child objects, and hierarchies deeper than the safety limit
 * fail during validation before any node is deleted.
 * @param root_id CLM ID identifying the root.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_delete(const clm_id root_id);

/** Creates an empty tree object, represented by a dynamic dictionary. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_new(const clm_id id);

/** Returns whether an ID denotes a dynamic dictionary object node. 
 * @param id CLM ID identifying the id.
 * @return Whether an ID denotes a dynamic dictionary object node.
 */
bool clm_tree_is_object(const clm_id id);

/** Creates an empty ordered array node. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_new_array(const clm_id id);

/** Returns whether an ID denotes a tree array node. 
 * @param id CLM ID identifying the id.
 * @return Whether an ID denotes a tree array node.
 */
bool clm_tree_is_array(const clm_id id);

/** Returns whether an ID denotes an explicitly represented null leaf. 
 * @param id CLM ID identifying the id.
 * @return Whether an ID denotes an explicitly represented null leaf.
 */
bool clm_tree_is_null(const clm_id id);

/** Returns whether an ID denotes a tree boolean leaf and optionally its value. 
 * @param id CLM ID identifying the id.
 * @param value Writable output or in/out pointer.
 * @return Whether an ID denotes a tree boolean leaf and optionally its value.
 */
bool clm_tree_is_boolean(const clm_id id, bool* value);

/** Creates a fresh child object and inserts it under a key.
 * Child IDs must be unused. Only child objects created this way are owned by
 * the parent tree; do not share object IDs between parents.
 * @param parent_id CLM ID identifying the parent.
 * @param key Input parameter described above.
 * @param child_id CLM ID identifying the child.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_add_object(
	const clm_id parent_id,
	const clm_id key,
	const clm_id child_id);

/** Adds a fresh array child to an object. The parent owns the child array. 
 * @param parent_id CLM ID identifying the parent.
 * @param key Input parameter described above.
 * @param array_id CLM ID identifying the array.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_add_array(
	const clm_id parent_id,
	const clm_id key,
	const clm_id array_id);

/** Validates and collects an existing child-object subtree before replacement. */
int clm_tree_internal_collect_replaced_object(
	const clm_id object_id,
	const clm_id key,
	clm_tree_internal_id_list* list);

/** Sets a value by borrowing an existing BPA block; object nodes are rejected.
 * Replacing a child object releases its owned subtree after the new value is set.
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param source_block_id CLM ID identifying the source block.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_set(
	const clm_id object_id,
	const clm_id key,
	const clm_id source_block_id);

/** Copies a typed value into an owned leaf block at the given key. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param type Input parameter described above.
 * @param value Input data or buffer; not modified.
 * @param size Size in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_set_from(
	const clm_id object_id,
	const clm_id key,
	const clm_byte type,
	const void* value,
	const size_t size);

/** Sets a null leaf using CLM's tree null marker. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_set_null(
	const clm_id object_id,
	const clm_id key);

/** Sets a boolean leaf without conflating it with a numeric byte value. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param value Value to process.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_set_boolean(
	const clm_id object_id,
	const clm_id key,
	const bool value);

/** Checks whether an object has a value for key. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @return Whether the condition described above holds.
 */
bool clm_tree_contains(const clm_id object_id, const clm_id key);

/** Returns the number of entries in an object, or CLM_BPA_FAILURE. 
 * @param object_id CLM ID identifying the object.
 * @return The number of entries in an object, or CLM_BPA_FAILURE.
 */
clm_int clm_tree_count(const clm_id object_id);

/** Retrieves a child BPA block ID without copying or transferring ownership. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param value_id Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_get_id(
	const clm_id object_id,
	const clm_id key,
	clm_id* value_id);

/** Retrieves the BPA value type stored at key. Object children report STRUCT. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param type Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_get_type(
	const clm_id object_id,
	const clm_id key,
	clm_byte* type);

/** Retrieves a child object ID; fails when key is not an object child. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param child_id Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_get_object(
	const clm_id object_id,
	const clm_id key,
	clm_id* child_id);

/** Returns the number of array elements, or CLM_BPA_FAILURE. 
 * @param array_id CLM ID identifying the array.
 * @return The number of array elements, or CLM_BPA_FAILURE.
 */
clm_int clm_tree_array_count(const clm_id array_id);

/** Appends a borrowed BPA leaf block to an array. 
 * @param array_id CLM ID identifying the array.
 * @param source_block_id CLM ID identifying the source block.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_append(
	const clm_id array_id,
	const clm_id source_block_id);

/** Copies a typed value into a new owned array leaf. 
 * @param array_id CLM ID identifying the array.
 * @param type Input parameter described above.
 * @param value Input data or buffer; not modified.
 * @param size Size in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_append_from(
	const clm_id array_id,
	const clm_byte type,
	const void* value,
	const size_t size);

/** Appends a typed null marker to an array. 
 * @param array_id CLM ID identifying the array.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_append_null(const clm_id array_id);

/** Appends a boolean leaf without conflating it with a numeric byte value. 
 * @param array_id CLM ID identifying the array.
 * @param value Value to process.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_append_boolean(
	const clm_id array_id,
	const bool value);

/** Creates and appends a fresh object child to an array. 
 * @param array_id CLM ID identifying the array.
 * @param child_id CLM ID identifying the child.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_append_object(
	const clm_id array_id,
	const clm_id child_id);

/** Creates and appends a fresh array child to an array. 
 * @param array_id CLM ID identifying the array.
 * @param child_id CLM ID identifying the child.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_append_array(
	const clm_id array_id,
	const clm_id child_id);

/** Gets an array element's BPA block ID without copying or transferring ownership. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param value_id Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_get_id(
	const clm_id array_id,
	const clm_int index,
	clm_id* value_id);

/** Returns an array element's BPA type. Child containers report STRUCT. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param type Writable output or in/out pointer.
 * @return An array element's BPA type. Child containers report STRUCT.
 */
clm_int clm_tree_array_get_type(
	const clm_id array_id,
	const clm_int index,
	clm_byte* type);

/** Retrieves an object element's ID; fails for leaves or array elements. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param child_id Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_get_object(
	const clm_id array_id,
	const clm_int index,
	clm_id* child_id);

/** Retrieves an array element's child array ID. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param child_id Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_get_array(
	const clm_id array_id,
	const clm_int index,
	clm_id* child_id);

/** Copies an array leaf to caller-owned memory; container elements are rejected. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param buffer Writable output buffer.
 * @param capacity Available capacity.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_get_to(
	const clm_id array_id,
	const clm_int index,
	void* buffer,
	const size_t capacity);

/** Copies an array leaf into a compatible existing BPA block. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param destination_id CLM ID identifying the destination.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_array_get(
	const clm_id array_id,
	const clm_int index,
	const clm_id destination_id);

/** Copies a leaf value into caller-owned memory; object children are rejected. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param buffer Writable output buffer.
 * @param capacity Available capacity.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_get_to(
	const clm_id object_id,
	const clm_id key,
	void* buffer,
	const size_t capacity);

/** Copies a leaf value into a compatible existing BPA block. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @param destination_id CLM ID identifying the destination.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_get(
	const clm_id object_id,
	const clm_id key,
	const clm_id destination_id);

/** Removes a leaf or child object. Child-object subtrees are recursively deleted. 
 * @param object_id CLM ID identifying the object.
 * @param key Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_tree_remove(
	const clm_id object_id,
	const clm_id key);

 
