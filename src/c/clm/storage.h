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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file storage.h
 * @brief BPA-backed fixed-dimension grid and atlas storage.
 *
 * Coordinates are zero-based and row-major: columns vary fastest, then rows,
 * then layers. Payloads are fixed-width byte-copyable BPA values; strings and
 * owning object types are not supported. Operations are not thread-safe.
 */
#define CLM_STORAGE_MAGIC UINT64_C(0x434c4d53544f5247)

/** Dimensions and element metadata for an existing storage resource. */
typedef struct clm_storage_info {
    /** Number of elements along the column (x) axis. */
    clm_uint columns, rows, layers;
    /** Atlas frame dimensions in elements; zero for non-atlas storage. */
    clm_uint frame_columns, frame_rows;
    /** Byte width of every stored value. */
    clm_uint element_size;
    /** BPA value type used for each element. */
    clm_byte type;
    /** Nonzero when the storage is configured as an atlas. */
    clm_byte atlas;
    /** Internal BPAA array ID; managed by clm_storage. */
    clm_id data_id;
} clm_storage_info;

typedef struct clm_storage_internal_metadata {
    uint64_t magic;
    clm_storage_info info;
} clm_storage_internal_metadata;

int clm_storage_internal_read(const clm_id id, clm_storage_internal_metadata* out);

int clm_storage_internal_allocate_id(const clm_id forbidden, clm_id* result);

int clm_storage_internal_new(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint layers,
    const clm_uint frame_columns,
    const clm_uint frame_rows,
    const bool atlas);

/** Creates a three-dimensional row-major storage grid.
 * The values are zero-initialized. Use CLM_BPA_TYPE_STRUCT with a nonzero
 * struct_size for custom fixed-width elements; strings are unsupported.
 * @param id Unique storage ID across BPA blocks and BPAA arrays.
 * @param type BPA type for each element.
 * @param struct_size Byte width for STRUCT; pass zero for built-in types.
 * @param columns Number of columns; must be nonzero.
 * @param rows Number of rows; must be nonzero.
 * @param layers Number of layers; must be nonzero.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
 */
int clm_storage_new(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint layers);
/** Creates a two-dimensional row-major grid (one layer).
 * @param id Unique storage ID.
 * @param type BPA type for each element.
 * @param struct_size Byte width for STRUCT; pass zero for built-in types.
 * @param columns Number of columns; must be nonzero.
 * @param rows Number of rows; must be nonzero.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
 */
int clm_storage_new_2d(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows);
/** Creates a two-dimensional atlas whose grid dimensions are divisible by the frame size.
 * Frames are addressed by frame row and frame column, independently of element
 * row/column coordinates.
 * @param id Unique storage ID.
 * @param type BPA type for each element.
 * @param struct_size Byte width for STRUCT; pass zero for built-in types.
 * @param columns Total atlas width in elements.
 * @param rows Total atlas height in elements.
 * @param frame_columns Frame width in elements.
 * @param frame_rows Frame height in elements.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
 */
int clm_storage_atlas_new(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint frame_columns,
    const clm_uint frame_rows);
/** Creates a layered atlas; each layer contains the same frame grid.
 * @param id Unique storage ID.
 * @param type BPA type for each element.
 * @param struct_size Byte width for STRUCT; pass zero for built-in types.
 * @param columns Total atlas width in elements.
 * @param rows Total atlas height in elements.
 * @param layers Number of atlas layers.
 * @param frame_columns Frame width in elements.
 * @param frame_rows Frame height in elements.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
 */
int clm_storage_atlas_new_3d(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint layers,
    const clm_uint frame_columns,
    const clm_uint frame_rows);
/** Copies dimensions and element metadata to the caller.
 * @param id Storage ID.
 * @param info Receives metadata; must not be NULL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
 */
int clm_storage_get_info(const clm_id id, clm_storage_info* info);
/** Deletes the storage resource and its internally managed BPAA array.
 * @param id Storage ID.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the ID is invalid.
 */
int clm_storage_delete(const clm_id id);
int clm_storage_internal_index(
    const clm_storage_info* info,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    clm_int* index);
/** Copies one element into caller-owned memory.
 * Coordinates are zero-based in layer, row, column order.
 * @param id Storage ID.
 * @param layer Zero-based layer coordinate.
 * @param row Zero-based row coordinate.
 * @param column Zero-based column coordinate.
 * @param buffer Receives the element bytes.
 * @param buffer_size Destination capacity; must be at least the element width.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
 */
int clm_storage_get_to(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    void* buffer,
    const size_t buffer_size);
/** Replaces one element from caller-owned bytes.
 * Coordinates are zero-based in layer, row, column order.
 * @param id Storage ID.
 * @param layer Zero-based layer coordinate.
 * @param row Zero-based row coordinate.
 * @param column Zero-based column coordinate.
 * @param value Source element bytes.
 * @param value_size Source byte count; must exactly equal the element width.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
 */
int clm_storage_set_from(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const void* value,
    const size_t value_size);
/** Copies one element into a compatible BPA destination block.
 * @param id Storage ID.
 * @param layer Zero-based layer coordinate.
 * @param row Zero-based row coordinate.
 * @param column Zero-based column coordinate.
 * @param destination_block_id Existing BPA block with matching type and size.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
 */
int clm_storage_get(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const clm_id destination_block_id);
/** Copies a compatible BPA source block into one element.
 * @param id Storage ID.
 * @param layer Zero-based layer coordinate.
 * @param row Zero-based row coordinate.
 * @param column Zero-based column coordinate.
 * @param source_block_id Existing BPA block with matching type and size.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
 */
int clm_storage_set(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const clm_id source_block_id);
/** Replaces every element with a copy of the same value.
 * @param id Storage ID.
 * @param value Source element bytes.
 * @param value_size Source byte count; must exactly equal the element width.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
 */
int clm_storage_fill_from(
    const clm_id id,
    const void* value,
    const size_t value_size);
/** Copies a contiguous rectangular or volumetric region into new storage.
 * The destination preserves the source element type and width, but is a regular
 * grid rather than an atlas.
 * @param source_id Source storage ID.
 * @param layer Starting zero-based layer.
 * @param row Starting zero-based row.
 * @param column Starting zero-based column.
 * @param layers Number of layers to copy.
 * @param rows Number of rows to copy.
 * @param columns Number of columns to copy.
 * @param destination_id New unique ID for the copied storage.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid bounds/IDs/allocation.
 */
int clm_storage_slice(
    const clm_id source_id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const clm_uint layers,
    const clm_uint rows,
    const clm_uint columns,
    const clm_id destination_id);
/** Resolves and validates an atlas frame. Internal helper shared by frame APIs. */
int clm_storage_atlas_internal_rect(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    clm_storage_info* info,
    clm_bpaa_array_node** array);
/** Copies one atlas frame to a tightly packed row-major caller buffer.
 * @param id Atlas storage ID.
 * @param layer Zero-based layer.
 * @param frame_row Zero-based frame row within the atlas.
 * @param frame_column Zero-based frame column within the atlas.
 * @param buffer Receives the frame bytes, with no row padding.
 * @param buffer_size Destination capacity; must fit the entire frame.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds.
 */
int clm_storage_atlas_get_frame_to(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    void* buffer,
    const size_t buffer_size);
/** Copies a tightly packed row-major buffer into one atlas frame.
 * @param id Atlas storage ID.
 * @param layer Zero-based layer.
 * @param frame_row Zero-based frame row within the atlas.
 * @param frame_column Zero-based frame column within the atlas.
 * @param buffer Frame bytes in row-major order, with no row padding.
 * @param buffer_size Source byte count; must exactly equal the frame size.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds.
 */
int clm_storage_atlas_set_frame_from(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    const void* buffer,
    const size_t buffer_size);
/** Copies an atlas frame into a new regular 2D storage resource.
 * @param id Atlas storage ID.
 * @param layer Zero-based layer.
 * @param frame_row Zero-based frame row within the atlas.
 * @param frame_column Zero-based frame column within the atlas.
 * @param destination_id New unique ID for the frame storage.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds/ID.
 */
int clm_storage_atlas_slice_frame(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    const clm_id destination_id);
