#pragma once

/*
* =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
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

#include "dictionary.hpp"

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

static inline int clm_tree_internal_allocate_unused_id(
	const clm_id avoid,
	clm_id* out_id);
static inline clm_int clm_tree_delete(const clm_id root_id);

typedef struct {
	clm_id id;
	size_t depth;
} clm_tree_internal_node;

typedef struct {
	clm_tree_internal_node* nodes;
	size_t count;
	size_t capacity;
} clm_tree_internal_id_list;

static inline void clm_tree_internal_id_list_dispose(
	clm_tree_internal_id_list* list);

/** Validates an ID as a dynamic dictionary object node. */
static inline bool clm_tree_internal_is_object(const clm_id id)
{
	clm_dictionary_any_metadata metadata;
	return clm_dictionary_internal_read(id, &metadata) == CLM_BPA_SUCCESS;
}

static inline bool clm_tree_internal_is_array(const clm_id id)
{
	const clm_bpa_block_node* block = clm_bpa_find_block(id);
	if (block == NULL || block->type != CLM_BPA_TYPE_STRUCT ||
		block->block_size != sizeof(clm_tree_array_metadata) || block->data == NULL)
		return false;
	clm_tree_array_metadata metadata;
	memcpy(&metadata, block->data, sizeof(metadata));
	return metadata.magic == CLM_TREE_ARRAY_MAGIC &&
		clm_dictionary_count(metadata.items_id) >= 0;
}

static inline bool clm_tree_internal_is_container(const clm_id id)
{
	return clm_tree_internal_is_object(id) || clm_tree_internal_is_array(id);
}

static inline int clm_tree_internal_array_read(
	const clm_id id,
	clm_tree_array_metadata* metadata)
{
	const clm_bpa_block_node* block = clm_bpa_find_block(id);
	if (metadata == NULL || block == NULL || block->type != CLM_BPA_TYPE_STRUCT ||
		block->block_size != sizeof(*metadata) || block->data == NULL)
		return CLM_BPA_FAILURE;
	memcpy(metadata, block->data, sizeof(*metadata));
	return metadata->magic == CLM_TREE_ARRAY_MAGIC &&
		clm_dictionary_count(metadata->items_id) >= 0
		? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

static inline bool clm_tree_internal_is_null(const clm_id id)
{
	const clm_bpa_block_node* block = clm_bpa_find_block(id);
	if (block == NULL || block->type != CLM_BPA_TYPE_STRUCT ||
		block->block_size != sizeof(clm_tree_null_metadata) || block->data == NULL)
		return false;
	clm_tree_null_metadata metadata;
	memcpy(&metadata, block->data, sizeof(metadata));
	return metadata.magic == CLM_TREE_NULL_MAGIC;
}

static inline bool clm_tree_internal_get_boolean(
	const clm_id id,
	bool* value)
{
	const clm_bpa_block_node* block = clm_bpa_find_block(id);
	clm_tree_bool_metadata metadata;
	if (value == NULL || block == NULL || block->type != CLM_BPA_TYPE_STRUCT ||
		block->block_size != sizeof(metadata) || block->data == NULL)
		return false;
	memcpy(&metadata, block->data, sizeof(metadata));
	if (metadata.magic != CLM_TREE_BOOL_MAGIC || metadata.value > 1)
		return false;
	*value = metadata.value != 0;
	return true;
}

/** Checks whether an object ID has already been collected. */
static inline bool clm_tree_internal_id_list_contains(
	const clm_tree_internal_id_list* list,
	const clm_id id)
{
	for (size_t index = 0; index < list->count; ++index)
		if (list->nodes[index].id == id)
			return true;
	return false;
}

/** Appends a unique object ID and enforces the maximum nesting depth. */
static inline int clm_tree_internal_id_list_append(
	clm_tree_internal_id_list* list,
	const clm_id id,
	const size_t depth)
{
	if (list == NULL || clm_tree_internal_id_list_contains(list, id) ||
		depth > CLM_TREE_MAX_DEPTH)
		return CLM_BPA_FAILURE;
	if (list->count == list->capacity) {
		if (list->capacity > ((size_t)-1) / 2)
			return CLM_BPA_FAILURE;
		const size_t next_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
		if (next_capacity > (size_t)-1 / sizeof(*list->nodes))
			return CLM_BPA_FAILURE;
		clm_tree_internal_node* grown =
			(clm_tree_internal_node*)realloc(list->nodes,
				next_capacity * sizeof(*list->nodes));
		if (grown == NULL)
			return CLM_BPA_FAILURE;
		list->nodes = grown;
		list->capacity = next_capacity;
	}
	list->nodes[list->count].id = id;
	list->nodes[list->count].depth = depth;
	++list->count;
	return CLM_BPA_SUCCESS;
}

/** Collects an exclusive acyclic object hierarchy before any deletion occurs. */
/** Walks child-object references and rejects cycles or shared subtrees. */
static inline int clm_tree_internal_collect(
	const clm_id root_id,
	clm_tree_internal_id_list* list)
{
	if (list == NULL)
		return CLM_BPA_FAILURE;
	list->nodes = NULL;
	list->count = 0;
	list->capacity = 0;
		if (!clm_tree_internal_is_container(root_id))
		return CLM_BPA_FAILURE;
	if (clm_tree_internal_id_list_append(list, root_id, 0) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;

	for (size_t object_index = 0; object_index < list->count; ++object_index) {
		const clm_id object_id = list->nodes[object_index].id;
			const bool is_array = clm_tree_internal_is_array(object_id);
			clm_tree_array_metadata array_metadata;
			if (is_array && clm_tree_internal_array_read(object_id, &array_metadata) !=
				CLM_BPA_SUCCESS)
				return CLM_BPA_FAILURE;
			const clm_id entries_id = is_array ? array_metadata.items_id : object_id;
			const clm_int count = clm_dictionary_count(entries_id);
		if (count < 0 || (size_t)count > (size_t)-1 / sizeof(clm_id))
			return CLM_BPA_FAILURE;
		clm_id* keys = count == 0 ? NULL
			: (clm_id*)malloc((size_t)count * sizeof(*keys));
		if ((count != 0 && keys == NULL) ||
			clm_dictionary_keys_to(entries_id, keys, (size_t)count) !=
				CLM_BPA_GET_SUCCESS) {
			free(keys);
			return CLM_BPA_FAILURE;
		}
		for (clm_int key_index = 0; key_index < count; ++key_index) {
			clm_id value_id;
			clm_byte type;
				if (clm_dictionary_get_id(entries_id, keys[key_index], &value_id) !=
					CLM_BPA_GET_SUCCESS ||
					clm_dictionary_get_type(entries_id, keys[key_index], &type) !=
					CLM_BPA_GET_SUCCESS) {
				free(keys);
				return CLM_BPA_FAILURE;
			}
				if (type == CLM_BPA_TYPE_STRUCT &&
					clm_tree_internal_is_container(value_id) &&
				clm_tree_internal_id_list_append(
					list, value_id, list->nodes[object_index].depth + 1) !=
					CLM_BPA_SUCCESS) {
				free(keys);
				return CLM_BPA_FAILURE;
			}
		}
		free(keys);
	}
	return CLM_BPA_SUCCESS;
}

/** Releases the temporary hierarchy list without deleting its objects. */
static inline void clm_tree_internal_id_list_dispose(
	clm_tree_internal_id_list* list)
{
	if (list != NULL) {
		free(list->nodes);
		list->nodes = NULL;
		list->count = 0;
		list->capacity = 0;
	}
}

/** Deletes a previously collected hierarchy in parent-before-child order. */
/** Deletes all collected object dictionaries and releases the temporary list. */
static inline int clm_tree_internal_delete_collected(
	clm_tree_internal_id_list* list)
{
	int status = CLM_BPA_SUCCESS;
		for (size_t index = 0; index < list->count; ++index) {
			const clm_id node_id = list->nodes[index].id;
			if (clm_tree_internal_is_array(node_id)) {
				clm_tree_array_metadata metadata;
				if (clm_tree_internal_array_read(node_id, &metadata) != CLM_BPA_SUCCESS ||
					clm_dictionary_delete(metadata.items_id) != CLM_BPA_SUCCESS ||
					clm_bpa_delete(node_id) != CLM_BPA_SUCCESS)
					status = CLM_BPA_FAILURE;
			} else if (clm_dictionary_delete(node_id) != CLM_BPA_SUCCESS) {
				status = CLM_BPA_FAILURE;
			}
		}
	clm_tree_internal_id_list_dispose(list);
	return status;
}

/** Creates an empty tree object, represented by a dynamic dictionary. */
static inline clm_int clm_tree_new(const clm_id id)
{
	return clm_dictionary_new(id);
}

/** Returns whether an ID denotes a dynamic dictionary object node. */
static inline bool clm_tree_is_object(const clm_id id)
{
	return clm_tree_internal_is_object(id);
}

/** Creates an empty ordered array node. */
static inline clm_int clm_tree_new_array(const clm_id id)
{
	clm_tree_array_metadata metadata = {0};
	clm_bpa_block_node* block;
	if (clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) ||
		clm_tree_internal_allocate_unused_id(id, &metadata.items_id) !=
			CLM_BPA_SUCCESS ||
		clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT,
							 (clm_uint)sizeof(metadata)) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	metadata.magic = CLM_TREE_ARRAY_MAGIC;
	block = clm_bpa_find_block(id);
	if (block == NULL || block->data == NULL) {
		(void)clm_bpa_delete(id);
		return CLM_BPA_FAILURE;
	}
	if (clm_dictionary_new(metadata.items_id) != CLM_BPA_SUCCESS) {
		(void)clm_bpa_delete(id);
		return CLM_BPA_FAILURE;
	}
	memcpy(block->data, &metadata, sizeof(metadata));
	return CLM_BPA_SUCCESS;
}

/** Returns whether an ID denotes a tree array node. */
static inline bool clm_tree_is_array(const clm_id id)
{
	return clm_tree_internal_is_array(id);
}

/** Returns whether an ID denotes an explicitly represented null leaf. */
static inline bool clm_tree_is_null(const clm_id id)
{
	return clm_tree_internal_is_null(id);
}

/** Returns whether an ID denotes a tree boolean leaf and optionally its value. */
static inline bool clm_tree_is_boolean(const clm_id id, bool* value)
{
	return clm_tree_internal_get_boolean(id, value);
}

static inline int clm_tree_internal_allocate_unused_id(
	const clm_id avoid,
	clm_id* out_id)
{
	if (out_id == NULL)
		return CLM_BPA_FAILURE;
	for (clm_id candidate = ~(clm_id)0; candidate != 0; --candidate)
		if (candidate != avoid && !clm_bpa_is_existing(candidate) &&
			!clm_bpaa_is_existing(candidate)) {
			*out_id = candidate;
			return CLM_BPA_SUCCESS;
		}
	return CLM_BPA_FAILURE;
}

/** Creates a fresh child object and inserts it under a key.
 * Child IDs must be unused. Only child objects created this way are owned by
 * the parent tree; do not share object IDs between parents.
 */
static inline clm_int clm_tree_add_object(
	const clm_id parent_id,
	const clm_id key,
	const clm_id child_id)
{
	if (!clm_tree_internal_is_object(parent_id) ||
		clm_dictionary_contains(parent_id, key) ||
		clm_dictionary_new(child_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_dictionary_add(parent_id, key, child_id) != CLM_BPA_SET_SUCCESS) {
		(void)clm_dictionary_delete(child_id);
		return CLM_BPA_FAILURE;
	}
	return CLM_BPA_SET_SUCCESS;
}

/** Adds a fresh array child to an object. The parent owns the child array. */
static inline clm_int clm_tree_add_array(
	const clm_id parent_id,
	const clm_id key,
	const clm_id array_id)
{
	if (!clm_tree_internal_is_object(parent_id) ||
		clm_dictionary_contains(parent_id, key) ||
		clm_tree_new_array(array_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_dictionary_add(parent_id, key, array_id) != CLM_BPA_SET_SUCCESS) {
		clm_tree_internal_id_list list;
		if (clm_tree_internal_collect(array_id, &list) == CLM_BPA_SUCCESS)
			(void)clm_tree_internal_delete_collected(&list);
		return CLM_BPA_FAILURE;
	}
	return CLM_BPA_SET_SUCCESS;
}

/** Validates and collects an existing child-object subtree before replacement. */
static inline int clm_tree_internal_collect_replaced_object(
	const clm_id object_id,
	const clm_id key,
	clm_tree_internal_id_list* list)
{
	memset(list, 0, sizeof(*list));
	if (!clm_dictionary_contains(object_id, key))
		return CLM_BPA_SUCCESS;
	clm_id value_id;
	clm_byte type;
	if (clm_dictionary_get_id(object_id, key, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_dictionary_get_type(object_id, key, &type) != CLM_BPA_GET_SUCCESS)
		return CLM_BPA_FAILURE;
	if (type != CLM_BPA_TYPE_STRUCT || !clm_tree_internal_is_container(value_id))
		return CLM_BPA_SUCCESS;
	if (clm_tree_internal_collect(value_id, list) != CLM_BPA_SUCCESS) {
		clm_tree_internal_id_list_dispose(list);
		return CLM_BPA_FAILURE;
	}
	return CLM_BPA_SUCCESS;
}

/** Sets a value by borrowing an existing BPA block; object nodes are rejected.
 * Replacing a child object releases its owned subtree after the new value is set.
 */
static inline clm_int clm_tree_set(
	const clm_id object_id,
	const clm_id key,
	const clm_id source_block_id)
{
	clm_tree_internal_id_list old_object;
	if (!clm_tree_internal_is_object(object_id) ||
		!clm_bpa_is_existing(source_block_id) ||
		clm_tree_internal_is_container(source_block_id) ||
		clm_tree_internal_collect_replaced_object(object_id, key, &old_object) !=
			CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const int result = clm_dictionary_set(object_id, key, source_block_id);
	if (result != CLM_BPA_SET_SUCCESS) {
		clm_tree_internal_id_list_dispose(&old_object);
		return CLM_BPA_FAILURE;
	}
	if (old_object.count != 0 &&
		clm_tree_internal_delete_collected(&old_object) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return CLM_BPA_SET_SUCCESS;
}

/** Copies a typed value into an owned leaf block at the given key. */
static inline clm_int clm_tree_set_from(
	const clm_id object_id,
	const clm_id key,
	const clm_byte type,
	const void* value,
	const size_t size)
{
	clm_tree_internal_id_list old_object;
	if (!clm_tree_internal_is_object(object_id) || type == CLM_BPA_TYPE_ANY ||
		clm_tree_internal_collect_replaced_object(object_id, key, &old_object) !=
			CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const int result = clm_dictionary_set_from(object_id, key, type, value, size);
	if (result != CLM_BPA_SET_SUCCESS) {
		clm_tree_internal_id_list_dispose(&old_object);
		return CLM_BPA_FAILURE;
	}
	if (old_object.count != 0 &&
		clm_tree_internal_delete_collected(&old_object) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return CLM_BPA_SET_SUCCESS;
}

/** Sets a null leaf using CLM's tree null marker. */
static inline clm_int clm_tree_set_null(
	const clm_id object_id,
	const clm_id key)
{
	const clm_tree_null_metadata null_value = {CLM_TREE_NULL_MAGIC};
	return clm_tree_set_from(object_id, key, CLM_BPA_TYPE_STRUCT,
							 &null_value, sizeof(null_value));
}

/** Sets a boolean leaf without conflating it with a numeric byte value. */
static inline clm_int clm_tree_set_boolean(
	const clm_id object_id,
	const clm_id key,
	const bool value)
{
	const clm_tree_bool_metadata boolean_value = {
		CLM_TREE_BOOL_MAGIC, (clm_byte)(value ? 1 : 0), {0}
	};
	return clm_tree_set_from(object_id, key, CLM_BPA_TYPE_STRUCT,
							 &boolean_value, sizeof(boolean_value));
}

/** Checks whether an object has a value for key. */
static inline bool clm_tree_contains(const clm_id object_id, const clm_id key)
{
	return clm_tree_internal_is_object(object_id) &&
		clm_dictionary_contains(object_id, key);
}

/** Returns the number of entries in an object, or CLM_BPA_FAILURE. */
static inline clm_int clm_tree_count(const clm_id object_id)
{
	return clm_tree_internal_is_object(object_id)
		? clm_dictionary_count(object_id) : CLM_BPA_FAILURE;
}

/** Retrieves a child BPA block ID without copying or transferring ownership. */
static inline clm_int clm_tree_get_id(
	const clm_id object_id,
	const clm_id key,
	clm_id* value_id)
{
	return clm_tree_internal_is_object(object_id)
		? clm_dictionary_get_id(object_id, key, value_id) : CLM_BPA_FAILURE;
}

/** Retrieves the BPA value type stored at key. Object children report STRUCT. */
static inline clm_int clm_tree_get_type(
	const clm_id object_id,
	const clm_id key,
	clm_byte* type)
{
	return clm_tree_internal_is_object(object_id)
		? clm_dictionary_get_type(object_id, key, type) : CLM_BPA_FAILURE;
}

/** Retrieves a child object ID; fails when key is not an object child. */
static inline clm_int clm_tree_get_object(
	const clm_id object_id,
	const clm_id key,
	clm_id* child_id)
{
	clm_id value_id;
	clm_byte type;
	if (child_id == NULL ||
		clm_tree_get_id(object_id, key, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_get_type(object_id, key, &type) != CLM_BPA_GET_SUCCESS ||
		type != CLM_BPA_TYPE_STRUCT || !clm_tree_internal_is_object(value_id))
		return CLM_BPA_FAILURE;
	*child_id = value_id;
	return CLM_BPA_GET_SUCCESS;
}

/** Returns the number of array elements, or CLM_BPA_FAILURE. */
static inline clm_int clm_tree_array_count(const clm_id array_id)
{
	clm_tree_array_metadata metadata;
	if (clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_count(metadata.items_id);
}

/** Appends a borrowed BPA leaf block to an array. */
static inline clm_int clm_tree_array_append(
	const clm_id array_id,
	const clm_id source_block_id)
{
	clm_tree_array_metadata metadata;
	if (clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS ||
		!clm_bpa_is_existing(source_block_id) ||
		clm_tree_internal_is_container(source_block_id))
		return CLM_BPA_FAILURE;
	const clm_int index = clm_dictionary_count(metadata.items_id);
	if (index < 0)
		return CLM_BPA_FAILURE;
	return clm_dictionary_add(metadata.items_id, (clm_id)index,
						  source_block_id);
}

/** Copies a typed value into a new owned array leaf. */
static inline clm_int clm_tree_array_append_from(
	const clm_id array_id,
	const clm_byte type,
	const void* value,
	const size_t size)
{
	clm_tree_array_metadata metadata;
	if (clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS ||
		type == CLM_BPA_TYPE_ANY)
		return CLM_BPA_FAILURE;
	const clm_int index = clm_dictionary_count(metadata.items_id);
	if (index < 0)
		return CLM_BPA_FAILURE;
	return clm_dictionary_set_from(metadata.items_id, (clm_id)index,
								   type, value, size);
}

/** Appends a typed null marker to an array. */
static inline clm_int clm_tree_array_append_null(const clm_id array_id)
{
	const clm_tree_null_metadata null_value = {CLM_TREE_NULL_MAGIC};
	return clm_tree_array_append_from(array_id, CLM_BPA_TYPE_STRUCT,
									  &null_value, sizeof(null_value));
}

/** Appends a boolean leaf without conflating it with a numeric byte value. */
static inline clm_int clm_tree_array_append_boolean(
	const clm_id array_id,
	const bool value)
{
	const clm_tree_bool_metadata boolean_value = {
		CLM_TREE_BOOL_MAGIC, (clm_byte)(value ? 1 : 0), {0}
	};
	return clm_tree_array_append_from(array_id, CLM_BPA_TYPE_STRUCT,
									  &boolean_value, sizeof(boolean_value));
}

/** Creates and appends a fresh object child to an array. */
static inline clm_int clm_tree_array_append_object(
	const clm_id array_id,
	const clm_id child_id)
{
	clm_tree_array_metadata metadata;
	if (clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS ||
		clm_tree_new(child_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const clm_int index = clm_dictionary_count(metadata.items_id);
	if (index < 0 || clm_dictionary_add(metadata.items_id, (clm_id)index,
										child_id) != CLM_BPA_SET_SUCCESS) {
		(void)clm_tree_delete(child_id);
		return CLM_BPA_FAILURE;
	}
	return CLM_BPA_SET_SUCCESS;
}

/** Creates and appends a fresh array child to an array. */
static inline clm_int clm_tree_array_append_array(
	const clm_id array_id,
	const clm_id child_id)
{
	clm_tree_array_metadata metadata;
	if (clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS ||
		clm_tree_new_array(child_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const clm_int index = clm_dictionary_count(metadata.items_id);
	if (index < 0 || clm_dictionary_add(metadata.items_id, (clm_id)index,
										child_id) != CLM_BPA_SET_SUCCESS) {
		clm_tree_array_metadata child_metadata;
		if (clm_tree_internal_array_read(child_id, &child_metadata) ==
			CLM_BPA_SUCCESS) {
			(void)clm_dictionary_delete(child_metadata.items_id);
			(void)clm_bpa_delete(child_id);
		}
		return CLM_BPA_FAILURE;
	}
	return CLM_BPA_SET_SUCCESS;
}

/** Gets an array element's BPA block ID without copying or transferring ownership. */
static inline clm_int clm_tree_array_get_id(
	const clm_id array_id,
	const clm_int index,
	clm_id* value_id)
{
	clm_tree_array_metadata metadata;
	if (value_id == NULL || index < 0 ||
		clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_get_id(metadata.items_id, (clm_id)index, value_id);
}

/** Returns an array element's BPA type. Child containers report STRUCT. */
static inline clm_int clm_tree_array_get_type(
	const clm_id array_id,
	const clm_int index,
	clm_byte* type)
{
	clm_tree_array_metadata metadata;
	if (index < 0 || type == NULL ||
		clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_get_type(metadata.items_id, (clm_id)index, type);
}

/** Retrieves an object element's ID; fails for leaves or array elements. */
static inline clm_int clm_tree_array_get_object(
	const clm_id array_id,
	const clm_int index,
	clm_id* child_id)
{
	clm_id value_id;
	clm_byte type;
	if (child_id == NULL ||
		clm_tree_array_get_id(array_id, index, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_array_get_type(array_id, index, &type) != CLM_BPA_GET_SUCCESS ||
		type != CLM_BPA_TYPE_STRUCT || !clm_tree_internal_is_object(value_id))
		return CLM_BPA_FAILURE;
	*child_id = value_id;
	return CLM_BPA_GET_SUCCESS;
}

/** Retrieves an array element's child array ID. */
static inline clm_int clm_tree_array_get_array(
	const clm_id array_id,
	const clm_int index,
	clm_id* child_id)
{
	clm_id value_id;
	clm_byte type;
	if (child_id == NULL ||
		clm_tree_array_get_id(array_id, index, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_array_get_type(array_id, index, &type) != CLM_BPA_GET_SUCCESS ||
		type != CLM_BPA_TYPE_STRUCT || !clm_tree_internal_is_array(value_id))
		return CLM_BPA_FAILURE;
	*child_id = value_id;
	return CLM_BPA_GET_SUCCESS;
}

/** Copies an array leaf to caller-owned memory; container elements are rejected. */
static inline clm_int clm_tree_array_get_to(
	const clm_id array_id,
	const clm_int index,
	void* buffer,
	const size_t capacity)
{
	clm_tree_array_metadata metadata;
	clm_id value_id;
	if (clm_tree_array_get_id(array_id, index, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_internal_is_container(value_id) || clm_tree_internal_is_null(value_id) ||
		clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_get_to(metadata.items_id, (clm_id)index, buffer, capacity);
}

/** Copies an array leaf into a compatible existing BPA block. */
static inline clm_int clm_tree_array_get(
	const clm_id array_id,
	const clm_int index,
	const clm_id destination_id)
{
	clm_tree_array_metadata metadata;
	clm_id value_id;
	if (clm_tree_array_get_id(array_id, index, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_internal_is_container(value_id) || clm_tree_internal_is_null(value_id) ||
	clm_tree_internal_array_read(array_id, &metadata) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_get(metadata.items_id, (clm_id)index, destination_id);
}

/** Copies a leaf value into caller-owned memory; object children are rejected. */
static inline clm_int clm_tree_get_to(
	const clm_id object_id,
	const clm_id key,
	void* buffer,
	const size_t capacity)
{
	clm_id value_id;
	if (clm_tree_get_id(object_id, key, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_internal_is_container(value_id) || clm_tree_internal_is_null(value_id))
		return CLM_BPA_FAILURE;
	return clm_dictionary_get_to(object_id, key, buffer, capacity);
}

/** Copies a leaf value into a compatible existing BPA block. */
static inline clm_int clm_tree_get(
	const clm_id object_id,
	const clm_id key,
	const clm_id destination_id)
{
	clm_id value_id;
	if (clm_tree_get_id(object_id, key, &value_id) != CLM_BPA_GET_SUCCESS ||
		clm_tree_internal_is_container(value_id) || clm_tree_internal_is_null(value_id))
		return CLM_BPA_FAILURE;
	return clm_dictionary_get(object_id, key, destination_id);
}

/** Removes a leaf or child object. Child-object subtrees are recursively deleted. */
static inline clm_int clm_tree_remove(
	const clm_id object_id,
	const clm_id key)
{
	clm_tree_internal_id_list child_objects;
	if (!clm_tree_internal_is_object(object_id) ||
		clm_tree_internal_collect_replaced_object(object_id, key,
											  &child_objects) != CLM_BPA_SUCCESS ||
		!clm_dictionary_contains(object_id, key))
		return CLM_BPA_FAILURE;
	if (clm_dictionary_remove(object_id, key) != CLM_BPA_SUCCESS) {
		clm_tree_internal_id_list_dispose(&child_objects);
		return CLM_BPA_FAILURE;
	}
	return child_objects.count != 0
		? clm_tree_internal_delete_collected(&child_objects)
		: CLM_BPA_SUCCESS;
}

/** Deletes an object and its exclusively owned nested object hierarchy.
 * Cycles, shared child objects, and hierarchies deeper than the safety limit
 * fail during validation before any node is deleted.
 */
static inline clm_int clm_tree_delete(const clm_id root_id)
{
	clm_tree_internal_id_list objects;
	if (clm_tree_internal_collect(root_id, &objects) != CLM_BPA_SUCCESS) {
		clm_tree_internal_id_list_dispose(&objects);
		return CLM_BPA_FAILURE;
	}
	return clm_tree_internal_delete_collected(&objects);
}

 

