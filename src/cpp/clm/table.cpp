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

#include "bpa.hpp"
#include "id.hpp"
#include "string.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CLM_TABLE_METADATA_MAGIC UINT64_C(0x434C4D5441424C45)

typedef struct {
	uint64_t magic;
	clm_id column_ids;
	clm_id column_names;
	clm_id row_ids;
} clm_table_metadata;

static inline int clm_table_internal_allocate_id(
	const clm_id table_id,
	clm_id* out_id)
{
	if (out_id == NULL)
		return CLM_BPA_FAILURE;
	for (clm_id candidate = ~(clm_id)0; candidate != 0; --candidate) {
		if (candidate != table_id && !clm_bpa_is_existing(candidate) &&
			!clm_bpaa_is_existing(candidate)) {
			*out_id = candidate;
			return CLM_BPA_SUCCESS;
		}
	}
	return CLM_BPA_FAILURE;
}

static inline int clm_table_internal_read(
	const clm_id id,
	clm_table_metadata* out,
	clm_bpaa_array_node** column_ids,
	clm_bpaa_array_node** column_names,
	clm_bpaa_array_node** row_ids)
{
	const clm_bpa_block_node* block = clm_bpa_find_block(id);
	if (out == NULL || block == NULL || block->type != CLM_BPA_TYPE_STRUCT ||
		block->block_size != sizeof(*out) || block->data == NULL)
		return CLM_BPA_FAILURE;
	memcpy(out, block->data, sizeof(*out));
	if (out->magic != CLM_TABLE_METADATA_MAGIC)
		return CLM_BPA_FAILURE;

	clm_bpaa_array_node* ids = clm_bpaa_find_array(out->column_ids);
	clm_bpaa_array_node* names = clm_bpaa_find_array(out->column_names);
	clm_bpaa_array_node* rows = clm_bpaa_find_array(out->row_ids);
	if (ids == NULL || ids->type != CLM_BPA_TYPE_STRUCT ||
		ids->block_size != sizeof(clm_id) || names == NULL ||
		names->type != CLM_BPA_TYPE_STRING ||
		names->encoding != CLM_STRING_ENCODING_UTF8 || rows == NULL ||
		rows->type != CLM_BPA_TYPE_STRUCT || rows->block_size != sizeof(clm_id) ||
		ids->count != names->count || ids->count > INT_MAX || rows->count > INT_MAX)
		return CLM_BPA_FAILURE;
	if (column_ids != NULL) *column_ids = ids;
	if (column_names != NULL) *column_names = names;
	if (row_ids != NULL) *row_ids = rows;
	return CLM_BPA_SUCCESS;
}

static inline clm_int clm_table_internal_column_index(
	const clm_bpaa_array_node* columns,
	const clm_id column_id)
{
	clm_id candidate;
	if (columns == NULL)
		return CLM_BPA_FAILURE;
	for (clm_uint index = 0; index < columns->count; ++index)
		if (clm_bpaa_get_raw(columns->id, (clm_int)index, &candidate,
							 sizeof(candidate)) == CLM_BPA_GET_SUCCESS &&
			candidate == column_id)
			return (clm_int)index;
	return CLM_BPA_FAILURE;
}

static inline int clm_table_internal_row_dictionary(
	const clm_bpaa_array_node* rows,
	const clm_int row,
	clm_id* dictionary_id)
{
	clm_id found;
	clm_dictionary_any_metadata metadata;
	if (rows == NULL || dictionary_id == NULL || row < 0 ||
		(clm_uint)row >= rows->count ||
		clm_bpaa_get_raw(rows->id, row, &found, sizeof(found)) !=
			CLM_BPA_GET_SUCCESS ||
		clm_dictionary_internal_read(found, &metadata) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	*dictionary_id = found;
	return CLM_BPA_SUCCESS;
}

static inline int clm_table_internal_cell(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	clm_id* row_dictionary_id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* columns;
	clm_bpaa_array_node* rows;
	if (clm_table_internal_read(id, &metadata, &columns, NULL, &rows) !=
			CLM_BPA_SUCCESS ||
		clm_table_internal_column_index(columns, column_id) < 0 ||
		clm_table_internal_row_dictionary(rows, row, row_dictionary_id) !=
			CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return CLM_BPA_SUCCESS;
}

/** Creates an empty table with no columns or rows.
 * @param id Unique table ID across BPA blocks and BPAA arrays.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for duplicate ID or allocation failure.
 */
static inline clm_int clm_table_new(const clm_id id)
{
	clm_table_metadata metadata = {0};
	if (clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) ||
		clm_table_internal_allocate_id(id, &metadata.column_ids) != CLM_BPA_SUCCESS ||
		clm_bpaa_create_sized(metadata.column_ids, CLM_BPA_TYPE_STRUCT, 0,
						  sizeof(clm_id), 0) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_table_internal_allocate_id(id, &metadata.column_names) !=
			CLM_BPA_SUCCESS ||
		clm_bpaa_create(metadata.column_names, CLM_BPA_TYPE_STRING, 0,
						CLM_STRING_ENCODING_UTF8) != CLM_BPA_SUCCESS) {
		(void)clm_bpaa_delete(metadata.column_ids);
		return CLM_BPA_FAILURE;
	}
	if (clm_table_internal_allocate_id(id, &metadata.row_ids) != CLM_BPA_SUCCESS ||
		clm_bpaa_create_sized(metadata.row_ids, CLM_BPA_TYPE_STRUCT, 0,
						  sizeof(clm_id), 0) != CLM_BPA_SUCCESS) {
		(void)clm_bpaa_delete(metadata.column_names);
		(void)clm_bpaa_delete(metadata.column_ids);
		return CLM_BPA_FAILURE;
	}
	metadata.magic = CLM_TABLE_METADATA_MAGIC;
	if (clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT,
						 (clm_uint)sizeof(metadata)) != CLM_BPA_SUCCESS) {
		(void)clm_bpaa_delete(metadata.row_ids);
		(void)clm_bpaa_delete(metadata.column_names);
		(void)clm_bpaa_delete(metadata.column_ids);
		return CLM_BPA_FAILURE;
	}
	clm_bpa_block_node* block = clm_bpa_find_block(id);
	if (block == NULL || block->data == NULL) {
		(void)clm_bpa_delete(id);
		(void)clm_bpaa_delete(metadata.row_ids);
		(void)clm_bpaa_delete(metadata.column_names);
		(void)clm_bpaa_delete(metadata.column_ids);
		return CLM_BPA_FAILURE;
	}
	memcpy(block->data, &metadata, sizeof(metadata));
	return CLM_BPA_SUCCESS;
}

/** Deletes a table and all column and cell data owned by it.
 * @param id Table ID.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the ID is not a table.
 */
static inline clm_int clm_table_delete(const clm_id id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* rows;
	if (clm_table_internal_read(id, &metadata, NULL, NULL, &rows) !=
		CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	for (clm_uint index = 0; index < rows->count; ++index) {
		clm_id row_id;
		clm_dictionary_any_metadata row_metadata;
		if (clm_bpaa_get_raw(rows->id, (clm_int)index, &row_id,
							 sizeof(row_id)) != CLM_BPA_GET_SUCCESS ||
			clm_dictionary_internal_read(row_id, &row_metadata) != CLM_BPA_SUCCESS)
			return CLM_BPA_FAILURE;
	}
	int status = CLM_BPA_SUCCESS;
	for (clm_uint index = 0; index < rows->count; ++index) {
		clm_id row_id;
		if (clm_bpaa_get_raw(rows->id, (clm_int)index, &row_id,
							 sizeof(row_id)) != CLM_BPA_GET_SUCCESS ||
			clm_dictionary_delete(row_id) != CLM_BPA_SUCCESS)
			status = CLM_BPA_FAILURE;
	}
	if (clm_bpaa_delete(metadata.row_ids) != CLM_BPA_SUCCESS)
		status = CLM_BPA_FAILURE;
	if (clm_bpaa_delete(metadata.column_names) != CLM_BPA_SUCCESS)
		status = CLM_BPA_FAILURE;
	if (clm_bpaa_delete(metadata.column_ids) != CLM_BPA_SUCCESS)
		status = CLM_BPA_FAILURE;
	if (clm_bpa_delete(id) != CLM_BPA_SUCCESS)
		status = CLM_BPA_FAILURE;
	return status;
}

/** Adds a column at the end of the schema using a previously mapped name.
 * The table retains the supplied spelling for serialization. Column IDs must
 * be unique within the table; aliases resolving to the same ID are duplicates.
 * @param id Table ID.
 * @param column_name Name already registered in clm_id.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for an unmapped name or duplicate.
 */
static inline clm_int clm_table_add_column(
	const clm_id id,
	const char* column_name)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* column_ids;
	clm_bpaa_array_node* column_names;
	clm_id column_id;
	size_t character_count;
	if (column_name == NULL || column_name[0] == '\0' ||
		strlen(column_name) >= UINT_MAX ||
		clm_string_measure(column_name, strlen(column_name),
						   CLM_STRING_ENCODING_UTF8, &character_count) != CLM_BPA_SUCCESS ||
		clm_id_try_get(column_name, &column_id) != CLM_BPA_GET_SUCCESS ||
		clm_table_internal_read(id, &metadata, &column_ids, &column_names, NULL) !=
			CLM_BPA_SUCCESS ||
		clm_table_internal_column_index(column_ids, column_id) >= 0 ||
		column_ids->count >= INT_MAX)
		return CLM_BPA_FAILURE;
	if (clm_bpaa_append_raw(column_ids->id, &column_id, sizeof(column_id)) !=
		CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_bpaa_append_raw(column_names->id, column_name,
							(clm_uint)(strlen(column_name) + 1)) != CLM_BPA_SUCCESS) {
		(void)clm_bpaa_remove(column_ids->id, (clm_int)(column_ids->count - 1));
		return CLM_BPA_FAILURE;
	}
	return CLM_BPA_SET_SUCCESS;
}

/** Removes a column and its cells; remaining columns keep their relative order.
 * @param id Table ID.
 * @param column_id Mapped ID of the column to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the table/column is missing.
 */
static inline clm_int clm_table_remove_column(
	const clm_id id,
	const clm_id column_id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* columns;
	clm_bpaa_array_node* rows;
	const clm_int column_index = clm_table_internal_read(
		id, &metadata, &columns, NULL, &rows) == CLM_BPA_SUCCESS
		? clm_table_internal_column_index(columns, column_id) : CLM_BPA_FAILURE;
	if (column_index < 0)
		return CLM_BPA_FAILURE;
	for (clm_uint row = 0; row < rows->count; ++row) {
		clm_id row_id;
		clm_dictionary_any_metadata row_metadata;
		if (clm_bpaa_get_raw(rows->id, (clm_int)row, &row_id,
							 sizeof(row_id)) != CLM_BPA_GET_SUCCESS ||
			clm_dictionary_internal_read(row_id, &row_metadata) != CLM_BPA_SUCCESS)
			return CLM_BPA_FAILURE;
	}
	for (clm_uint row = 0; row < rows->count; ++row) {
		clm_id row_id;
		if (clm_bpaa_get_raw(rows->id, (clm_int)row, &row_id,
							 sizeof(row_id)) != CLM_BPA_GET_SUCCESS)
			return CLM_BPA_FAILURE;
		if (clm_dictionary_contains(row_id, column_id) &&
			clm_dictionary_remove(row_id, column_id) != CLM_BPA_SUCCESS)
			return CLM_BPA_FAILURE;
	}
	if (clm_bpaa_remove(metadata.column_ids, column_index) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_bpaa_remove(metadata.column_names, column_index) == CLM_BPA_SUCCESS
		? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

/** Returns the number of columns, or CLM_BPA_FAILURE for an invalid table. */
static inline clm_int clm_table_column_count(const clm_id id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* columns;
	return clm_table_internal_read(id, &metadata, &columns, NULL, NULL) ==
			CLM_BPA_SUCCESS
		? (clm_int)columns->count : CLM_BPA_FAILURE;
}

/** Returns a column's mapped ID by its zero-based schema position.
 * @param id Table ID.
 * @param index Zero-based column position.
 * @param column_id Receives the mapped ID.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/index.
 */
static inline clm_int clm_table_column_id_at(
	const clm_id id,
	const clm_int index,
	clm_id* column_id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* columns;
	if (column_id == NULL || index < 0 ||
		clm_table_internal_read(id, &metadata, &columns, NULL, NULL) !=
			CLM_BPA_SUCCESS || (clm_uint)index >= columns->count ||
		clm_bpaa_get_raw(columns->id, index, column_id, sizeof(*column_id)) !=
			CLM_BPA_GET_SUCCESS)
		return CLM_BPA_FAILURE;
	return CLM_BPA_GET_SUCCESS;
}

/** Copies a column's retained name into a caller buffer.
 * Returns its UTF-8 byte length excluding NUL, or CLM_BPA_FAILURE if the
 * column is invalid or the buffer cannot hold the full name and terminator.
 */
static inline clm_int clm_table_column_name_to(
	const clm_id id,
	const clm_int index,
	char* buffer,
	const size_t buffer_size)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* names;
	if (buffer == NULL || buffer_size > UINT_MAX || index < 0 ||
		clm_table_internal_read(id, &metadata, NULL, &names, NULL) !=
			CLM_BPA_SUCCESS || (clm_uint)index >= names->count)
		return CLM_BPA_FAILURE;
	return clm_bpaa_get_string_raw(names->id, index, buffer,
								   (clm_uint)buffer_size);
}

/** Returns the number of rows, or CLM_BPA_FAILURE for an invalid table. */
static inline clm_int clm_table_row_count(const clm_id id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* rows;
	return clm_table_internal_read(id, &metadata, NULL, NULL, &rows) ==
			CLM_BPA_SUCCESS
		? (clm_int)rows->count : CLM_BPA_FAILURE;
}

/** Appends a row whose cells are initially missing.
 * @param id Table ID.
 * @return The new zero-based row index, or CLM_BPA_FAILURE.
 */
static inline clm_int clm_table_append_row(const clm_id id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* rows;
	clm_id row_id;
	if (clm_table_internal_read(id, &metadata, NULL, NULL, &rows) !=
			CLM_BPA_SUCCESS || rows->count >= INT_MAX ||
		clm_table_internal_allocate_id(id, &row_id) != CLM_BPA_SUCCESS ||
		clm_dictionary_new(row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_bpaa_append_raw(rows->id, &row_id, sizeof(row_id)) != CLM_BPA_SUCCESS) {
		(void)clm_dictionary_delete(row_id);
		return CLM_BPA_FAILURE;
	}
	return (clm_int)(rows->count - 1);
}

/** Inserts a missing-cell row before the given zero-based row index.
 * The index may equal the current row count to append.
 * @return The inserted row index, or CLM_BPA_FAILURE.
 */
static inline clm_int clm_table_insert_row(
	const clm_id id,
	const clm_int index)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* rows;
	clm_id row_id;
	if (index < 0 ||
		clm_table_internal_read(id, &metadata, NULL, NULL, &rows) !=
			CLM_BPA_SUCCESS || (clm_uint)index > rows->count ||
		rows->count >= INT_MAX ||
		clm_table_internal_allocate_id(id, &row_id) != CLM_BPA_SUCCESS ||
		clm_dictionary_new(row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_bpaa_insert_raw(rows->id, index, &row_id, sizeof(row_id)) !=
		CLM_BPA_SUCCESS) {
		(void)clm_dictionary_delete(row_id);
		return CLM_BPA_FAILURE;
	}
	return index;
}

/** Removes a row and shifts later row indices down by one.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid table/index.
 */
static inline clm_int clm_table_remove_row(
	const clm_id id,
	const clm_int index)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* rows;
	clm_id row_id;
	if (clm_table_internal_read(id, &metadata, NULL, NULL, &rows) !=
			CLM_BPA_SUCCESS ||
		clm_table_internal_row_dictionary(rows, index, &row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_bpaa_remove(rows->id, index) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_delete(row_id) == CLM_BPA_SUCCESS
		? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

/** Removes all rows while preserving the table's column schema. */
static inline clm_int clm_table_clear_rows(const clm_id id)
{
	clm_table_metadata metadata;
	clm_bpaa_array_node* rows;
	if (clm_table_internal_read(id, &metadata, NULL, NULL, &rows) !=
		CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	for (clm_uint index = 0; index < rows->count; ++index) {
		clm_id row_id;
		clm_dictionary_any_metadata row_metadata;
		if (clm_bpaa_get_raw(rows->id, (clm_int)index, &row_id,
							 sizeof(row_id)) != CLM_BPA_GET_SUCCESS ||
			clm_dictionary_internal_read(row_id, &row_metadata) != CLM_BPA_SUCCESS)
			return CLM_BPA_FAILURE;
	}
	for (clm_uint index = 0; index < rows->count; ++index) {
		clm_id row_id;
		if (clm_bpaa_get_raw(rows->id, (clm_int)index, &row_id,
							 sizeof(row_id)) != CLM_BPA_GET_SUCCESS ||
			clm_dictionary_delete(row_id) != CLM_BPA_SUCCESS)
			return CLM_BPA_FAILURE;
	}
	return clm_bpaa_clear(rows->id);
}

/** Checks whether a cell is present; an empty string is still present. */
static inline bool clm_table_cell_contains(
	const clm_id id,
	const clm_int row,
	const clm_id column_id)
{
	clm_id row_id;
	return clm_table_internal_cell(id, row, column_id, &row_id) ==
			CLM_BPA_SUCCESS && clm_dictionary_contains(row_id, column_id);
}

/** Sets a cell from valid NUL-terminated UTF-8 text; empty text is valid.
 * The row and column must already exist. Replaces an existing cell value.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation failure.
 */
static inline clm_int clm_table_set_from(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	const char* value)
{
	clm_id row_id;
	size_t character_count;
	if (value == NULL || strlen(value) >= UINT_MAX ||
		clm_string_measure(value, strlen(value), CLM_STRING_ENCODING_UTF8,
						   &character_count) != CLM_BPA_SUCCESS ||
		clm_table_internal_cell(id, row, column_id, &row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_set_from(row_id, column_id, CLM_BPA_TYPE_STRING,
								   value, strlen(value) + 1);
}

/** Sets a cell by copying a valid string BPA block's value.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
 */
static inline clm_int clm_table_set(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	const clm_id source_string_id)
{
	const clm_bpa_block_node* source = clm_bpa_find_block(source_string_id);
	size_t character_count;
	if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
		source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
		clm_string_measure(source->data != NULL ? (const char*)source->data : "",
						   source->length, CLM_STRING_ENCODING_UTF8,
						   &character_count) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	clm_id row_id;
	if (clm_table_internal_cell(id, row, column_id, &row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_set_from(row_id, column_id, CLM_BPA_TYPE_STRING,
								   source->data != NULL ? source->data : "",
								   (size_t)source->length + 1);
}

/** Copies a present cell into an existing string BPA destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for missing/incompatible input.
 */
static inline clm_int clm_table_get(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	const clm_id destination_string_id)
{
	const clm_bpa_block_node* destination =
		clm_bpa_find_block(destination_string_id);
	clm_id row_id;
	if (destination == NULL || destination->type != CLM_BPA_TYPE_STRING ||
		clm_table_internal_cell(id, row, column_id, &row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_get(row_id, column_id, destination_string_id);
}

/** Removes a cell value, making that cell missing again.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid table/cell.
 */
static inline clm_int clm_table_remove_cell(
	const clm_id id,
	const clm_int row,
	const clm_id column_id)
{
	clm_id row_id;
	if (clm_table_internal_cell(id, row, column_id, &row_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	return clm_dictionary_remove(row_id, column_id);
}

