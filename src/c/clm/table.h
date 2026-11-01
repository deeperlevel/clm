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
#include "id.h"
#include "string.h"

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

int clm_table_internal_allocate_id(
	const clm_id table_id,
	clm_id* out_id);

int clm_table_internal_read(
	const clm_id id,
	clm_table_metadata* out,
	clm_bpaa_array_node** column_ids,
	clm_bpaa_array_node** column_names,
	clm_bpaa_array_node** row_ids);

clm_int clm_table_internal_column_index(
	const clm_bpaa_array_node* columns,
	const clm_id column_id);

int clm_table_internal_row_dictionary(
	const clm_bpaa_array_node* rows,
	const clm_int row,
	clm_id* dictionary_id);

int clm_table_internal_cell(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	clm_id* row_dictionary_id);

/** Creates an empty table with no columns or rows.
 * @param id Unique table ID across BPA blocks and BPAA arrays.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for duplicate ID or allocation failure.
 */
clm_int clm_table_new(const clm_id id);

/** Deletes a table and all column and cell data owned by it.
 * @param id Table ID.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the ID is not a table.
 */
clm_int clm_table_delete(const clm_id id);

/** Adds a column at the end of the schema using a previously mapped name.
 * The table retains the supplied spelling for serialization. Column IDs must
 * be unique within the table; aliases resolving to the same ID are duplicates.
 * @param id Table ID.
 * @param column_name Name already registered in clm_id.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for an unmapped name or duplicate.
 */
clm_int clm_table_add_column(
	const clm_id id,
	const char* column_name);

/** Removes a column and its cells; remaining columns keep their relative order.
 * @param id Table ID.
 * @param column_id Mapped ID of the column to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the table/column is missing.
 */
clm_int clm_table_remove_column(
	const clm_id id,
	const clm_id column_id);

/** Returns the number of columns, or CLM_BPA_FAILURE for an invalid table. 
 * @param id CLM ID identifying the id.
 * @return The number of columns, or CLM_BPA_FAILURE for an invalid table.
 */
clm_int clm_table_column_count(const clm_id id);

/** Returns a column's mapped ID by its zero-based schema position.
 * @param id Table ID.
 * @param index Zero-based column position.
 * @param column_id Receives the mapped ID.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/index.
 */
clm_int clm_table_column_id_at(
	const clm_id id,
	const clm_int index,
	clm_id* column_id);

/** Copies a column's retained name into a caller buffer.
 * Returns its UTF-8 byte length excluding NUL, or CLM_BPA_FAILURE if the
 * column is invalid or the buffer cannot hold the full name and terminator.
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param buffer Writable output buffer.
 * @param buffer_size Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_table_column_name_to(
	const clm_id id,
	const clm_int index,
	char* buffer,
	const size_t buffer_size);

/** Returns the number of rows, or CLM_BPA_FAILURE for an invalid table. 
 * @param id CLM ID identifying the id.
 * @return The number of rows, or CLM_BPA_FAILURE for an invalid table.
 */
clm_int clm_table_row_count(const clm_id id);

/** Appends a row whose cells are initially missing.
 * @param id Table ID.
 * @return The new zero-based row index, or CLM_BPA_FAILURE.
 */
clm_int clm_table_append_row(const clm_id id);

/** Inserts a missing-cell row before the given zero-based row index.
 * The index may equal the current row count to append.
 * @return The inserted row index, or CLM_BPA_FAILURE.
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 */
clm_int clm_table_insert_row(
	const clm_id id,
	const clm_int index);

/** Removes a row and shifts later row indices down by one.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid table/index.
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 */
clm_int clm_table_remove_row(
	const clm_id id,
	const clm_int index);

/** Removes all rows while preserving the table's column schema. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_table_clear_rows(const clm_id id);

/** Checks whether a cell is present; an empty string is still present. 
 * @param id CLM ID identifying the id.
 * @param row Input parameter described above.
 * @param column_id CLM ID identifying the column.
 * @return Whether the condition described above holds.
 */
bool clm_table_cell_contains(
	const clm_id id,
	const clm_int row,
	const clm_id column_id);

/** Sets a cell from valid NUL-terminated UTF-8 text; empty text is valid.
 * The row and column must already exist. Replaces an existing cell value.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation failure.
 * @param id CLM ID identifying the id.
 * @param row Input parameter described above.
 * @param column_id CLM ID identifying the column.
 * @param value Input data or buffer; not modified.
 */
clm_int clm_table_set_from(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	const char* value);

/** Sets a cell by copying a valid string BPA block's value.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
 * @param id CLM ID identifying the id.
 * @param row Input parameter described above.
 * @param column_id CLM ID identifying the column.
 * @param source_string_id CLM ID identifying the source string.
 */
clm_int clm_table_set(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	const clm_id source_string_id);

/** Copies a present cell into an existing string BPA destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for missing/incompatible input.
 * @param id CLM ID identifying the id.
 * @param row Input parameter described above.
 * @param column_id CLM ID identifying the column.
 * @param destination_string_id CLM ID identifying the destination string.
 */
clm_int clm_table_get(
	const clm_id id,
	const clm_int row,
	const clm_id column_id,
	const clm_id destination_string_id);

/** Removes a cell value, making that cell missing again.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid table/cell.
 * @param id CLM ID identifying the id.
 * @param row Input parameter described above.
 * @param column_id CLM ID identifying the column.
 */
clm_int clm_table_remove_cell(
	const clm_id id,
	const clm_int row,
	const clm_id column_id);
