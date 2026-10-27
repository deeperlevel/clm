/* CLM — Cross Language Modules | CC0 1.0 | Don't start from scratch, bro. */
#pragma once
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

static inline int clm_storage_internal_read(const clm_id id, clm_storage_internal_metadata* out) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (!out || !block || block->type != CLM_BPA_TYPE_STRUCT ||
        block->block_size != sizeof(*out) || !block->data) return CLM_BPA_FAILURE;
    memcpy(out, block->data, sizeof(*out));
    if (out->magic != CLM_STORAGE_MAGIC) return CLM_BPA_FAILURE;
    const clm_bpaa_array_node* data = clm_bpaa_find_array(out->info.data_id);
    if (!data || data->type != out->info.type ||
        data->block_size != out->info.element_size) return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

static inline int clm_storage_internal_allocate_id(const clm_id forbidden, clm_id* result) {
    if (!result) return CLM_BPA_FAILURE;
    const size_t count = clm_bpa_global_arrays_count;
    if (count > (size_t)-1 - 2) return CLM_BPA_FAILURE;
    for (size_t i = 0; i <= count + 1; ++i) {
        const clm_id candidate = ~(clm_id)0 - i;
        if (candidate != forbidden && !clm_bpaa_is_existing(candidate)) {
            *result = candidate;
            return CLM_BPA_SUCCESS;
        }
    }
    return CLM_BPA_FAILURE;
}

static inline int clm_storage_internal_new(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint layers,
    const clm_uint frame_columns,
    const clm_uint frame_rows,
    const bool atlas) {
    if (!columns || !rows || !layers || clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) ||
        (atlas && (!frame_columns || !frame_rows || columns % frame_columns || rows % frame_rows)) ||
        (!atlas && (frame_columns || frame_rows)) || type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    clm_uint width = clm_bpa_type_size(type);
    if (type == CLM_BPA_TYPE_STRUCT) {
        if (!struct_size || struct_size > (size_t)(~(uint32_t)0)) return CLM_BPA_FAILURE;
        width = (clm_uint)struct_size;
    } else if (!width || (struct_size && struct_size != width)) return CLM_BPA_FAILURE;
    /* Reject multiplication overflow before evaluating the full product. */
    if ((uint64_t)columns * rows > (uint64_t)INT_MAX / layers) return CLM_BPA_FAILURE;
    const uint64_t total = (uint64_t)columns * rows * layers;
    if (!total || total > INT_MAX || total > (uint64_t)(((size_t)-1) / width)) return CLM_BPA_FAILURE;
    clm_id data_id;
    if (clm_storage_internal_allocate_id(id, &data_id) != CLM_BPA_SUCCESS ||
        clm_bpaa_create_sized(data_id, type, (clm_int)total, width, 0) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    clm_bpaa_array_node* array = clm_bpaa_find_array(data_id);
    if (!array || !array->items.blocks) { (void)clm_bpaa_delete(data_id); return CLM_BPA_FAILURE; }
    /* BPA reserves zero-initialized capacity; commit all grid cells at once. */
    array->count = (clm_uint)total;
    array->deck_size = array->count;
    if (clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT, (clm_uint)sizeof(clm_storage_internal_metadata)) != CLM_BPA_SUCCESS) {
        (void)clm_bpaa_delete(data_id); return CLM_BPA_FAILURE;
    }
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (!block || !block->data) { (void)clm_bpa_delete(id); (void)clm_bpaa_delete(data_id); return CLM_BPA_FAILURE; }
    clm_storage_internal_metadata metadata = {0};
    metadata.magic = CLM_STORAGE_MAGIC;
    metadata.info.columns = columns;
    metadata.info.rows = rows;
    metadata.info.layers = layers;
    metadata.info.frame_columns = frame_columns;
    metadata.info.frame_rows = frame_rows;
    metadata.info.element_size = width;
    metadata.info.type = type;
    metadata.info.atlas = (clm_byte)atlas;
    metadata.info.data_id = data_id;
    memcpy(block->data, &metadata, sizeof(metadata));
    return CLM_BPA_SUCCESS;
}

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
static inline int clm_storage_new(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint layers) {
    return clm_storage_internal_new(
        id,
        type,
        struct_size,
        columns,
        rows,
        layers,
        0,
        0,
        false
    );
}
/** Creates a two-dimensional row-major grid (one layer).
 * @param id Unique storage ID.
 * @param type BPA type for each element.
 * @param struct_size Byte width for STRUCT; pass zero for built-in types.
 * @param columns Number of columns; must be nonzero.
 * @param rows Number of rows; must be nonzero.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
 */
static inline int clm_storage_new_2d(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows) {
    return clm_storage_new(id, type, struct_size, columns, rows, 1);
}
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
static inline int clm_storage_atlas_new(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint frame_columns,
    const clm_uint frame_rows) {
    return clm_storage_internal_new(
        id,
        type,
        struct_size,
        columns,
        rows,
        1,
        frame_columns,
        frame_rows,
        true
    );
}
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
static inline int clm_storage_atlas_new_3d(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows,
    const clm_uint layers,
    const clm_uint frame_columns,
    const clm_uint frame_rows) {
    return clm_storage_internal_new(
        id,
        type,
        struct_size,
        columns,
        rows,
        layers,
        frame_columns,
        frame_rows,
        true
    );
}
/** Copies dimensions and element metadata to the caller.
 * @param id Storage ID.
 * @param info Receives metadata; must not be NULL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
 */
static inline int clm_storage_get_info(const clm_id id, clm_storage_info* info) {
    clm_storage_internal_metadata metadata;
    if (!info || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    *info = metadata.info;
    return CLM_BPA_GET_SUCCESS;
}
/** Deletes the storage resource and its internally managed BPAA array.
 * @param id Storage ID.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the ID is invalid.
 */
static inline int clm_storage_delete(const clm_id id) {
    clm_storage_internal_metadata metadata;
    if (clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    int a = clm_bpaa_delete(metadata.info.data_id);
    int b = clm_bpa_delete(id);
    return a == CLM_BPA_SUCCESS && b == CLM_BPA_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}
static inline int clm_storage_internal_index(
    const clm_storage_info* info,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    clm_int* index) {
    if (!info || !index || layer >= info->layers || row >= info->rows || column >= info->columns)
        return CLM_BPA_FAILURE;
    *index = (clm_int)(((uint64_t)layer * info->rows + row) * info->columns + column);
    return CLM_BPA_SUCCESS;
}
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
static inline int clm_storage_get_to(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    void* buffer,
    const size_t buffer_size) {
    clm_storage_internal_metadata metadata;
    clm_int index;
    if (!buffer || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS ||
        buffer_size < metadata.info.element_size) return CLM_BPA_FAILURE;
    return clm_bpaa_get_raw(metadata.info.data_id, index, buffer, metadata.info.element_size);
}
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
static inline int clm_storage_set_from(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const void* value,
    const size_t value_size) {
    clm_storage_internal_metadata metadata;
    clm_int index;
    if (!value || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS ||
        value_size != metadata.info.element_size) return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(metadata.info.data_id, index, value, metadata.info.element_size);
}
/** Copies one element into a compatible BPA destination block.
 * @param id Storage ID.
 * @param layer Zero-based layer coordinate.
 * @param row Zero-based row coordinate.
 * @param column Zero-based column coordinate.
 * @param destination_block_id Existing BPA block with matching type and size.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
 */
static inline int clm_storage_get(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const clm_id destination_block_id) {
    clm_storage_internal_metadata metadata;
    clm_int index;
    if (clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_get(metadata.info.data_id, index, destination_block_id, metadata.info.element_size);
}
/** Copies a compatible BPA source block into one element.
 * @param id Storage ID.
 * @param layer Zero-based layer coordinate.
 * @param row Zero-based row coordinate.
 * @param column Zero-based column coordinate.
 * @param source_block_id Existing BPA block with matching type and size.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
 */
static inline int clm_storage_set(
    const clm_id id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const clm_id source_block_id) {
    clm_storage_internal_metadata metadata;
    clm_int index;
    if (clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set(metadata.info.data_id, index, source_block_id, metadata.info.element_size);
}
/** Replaces every element with a copy of the same value.
 * @param id Storage ID.
 * @param value Source element bytes.
 * @param value_size Source byte count; must exactly equal the element width.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
 */
static inline int clm_storage_fill_from(
    const clm_id id,
    const void* value,
    const size_t value_size) {
    clm_storage_internal_metadata metadata;
    if (!value || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        value_size != metadata.info.element_size) return CLM_BPA_FAILURE;
    clm_bpaa_array_node* array = clm_bpaa_find_array(metadata.info.data_id);
    if (!array || !array->items.blocks) return CLM_BPA_FAILURE;
    unsigned char* snapshot = (unsigned char*)malloc(value_size);
    if (!snapshot) return CLM_BPA_FAILURE;
    memcpy(snapshot, value, value_size);
    for (clm_uint i = 0; i < array->count; ++i)
        memcpy(array->items.blocks + (size_t)i * value_size, snapshot, value_size);
    free(snapshot);
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}
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
static inline int clm_storage_slice(
    const clm_id source_id,
    const clm_uint layer,
    const clm_uint row,
    const clm_uint column,
    const clm_uint layers,
    const clm_uint rows,
    const clm_uint columns,
    const clm_id destination_id) {
    clm_storage_internal_metadata source;
    if (clm_storage_internal_read(source_id, &source) != CLM_BPA_SUCCESS ||
        !layers || !rows || !columns || layer >= source.info.layers || row >= source.info.rows || column >= source.info.columns ||
        layers > source.info.layers - layer || rows > source.info.rows - row || columns > source.info.columns - column ||
        clm_storage_new(destination_id, source.info.type, source.info.element_size, columns, rows, layers) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const clm_bpaa_array_node* src = clm_bpaa_find_array(source.info.data_id);
    clm_storage_internal_metadata dst;
    if (!src || clm_storage_internal_read(destination_id, &dst) != CLM_BPA_SUCCESS) {
        (void)clm_storage_delete(destination_id); return CLM_BPA_FAILURE;
    }
    clm_bpaa_array_node* dest = clm_bpaa_find_array(dst.info.data_id);
    if (!dest) { (void)clm_storage_delete(destination_id); return CLM_BPA_FAILURE; }
    const size_t bytes = (size_t)columns * source.info.element_size;
    for (clm_uint z = 0; z < layers; ++z)
        for (clm_uint y = 0; y < rows; ++y) {
            const size_t from = ((size_t)(layer + z) * source.info.rows + row + y) * source.info.columns + column;
            const size_t to = ((size_t)z * rows + y) * columns;
            memcpy(dest->items.blocks + to * source.info.element_size,
                   src->items.blocks + from * source.info.element_size, bytes);
        }
    return CLM_BPA_SUCCESS;
}
/** Resolves and validates an atlas frame. Internal helper shared by frame APIs. */
static inline int clm_storage_atlas_internal_rect(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    clm_storage_info* info,
    clm_bpaa_array_node** array) {
    clm_storage_internal_metadata metadata;
    if (!info || !array || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
        !metadata.info.atlas || layer >= metadata.info.layers ||
        frame_row >= metadata.info.rows / metadata.info.frame_rows ||
        frame_column >= metadata.info.columns / metadata.info.frame_columns) return CLM_BPA_FAILURE;
    *info = metadata.info;
    *array = clm_bpaa_find_array(metadata.info.data_id);
    return *array && (*array)->items.blocks ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}
/** Copies one atlas frame to a tightly packed row-major caller buffer.
 * @param id Atlas storage ID.
 * @param layer Zero-based layer.
 * @param frame_row Zero-based frame row within the atlas.
 * @param frame_column Zero-based frame column within the atlas.
 * @param buffer Receives the frame bytes, with no row padding.
 * @param buffer_size Destination capacity; must fit the entire frame.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds.
 */
static inline int clm_storage_atlas_get_frame_to(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    void* buffer,
    const size_t buffer_size) {
    clm_storage_info info;
    clm_bpaa_array_node* array;
    if (!buffer || clm_storage_atlas_internal_rect(id, layer, frame_row, frame_column,
                                                   &info, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const size_t stride = (size_t)info.frame_columns * info.element_size;
    const size_t needed = stride * info.frame_rows;
    if (buffer_size < needed) return CLM_BPA_FAILURE;
    unsigned char* dest = (unsigned char*)buffer;
    /* memmove-safe even if caller's buffer overlaps internal storage. */
    unsigned char* temporary = (unsigned char*)malloc(needed);
    if (!temporary) return CLM_BPA_FAILURE;
    for (clm_uint y = 0; y < info.frame_rows; ++y) {
        const size_t offset = ((size_t)layer * info.rows + frame_row * info.frame_rows + y) * info.columns + frame_column * info.frame_columns;
        memcpy(temporary + (size_t)y * stride, array->items.blocks + offset * info.element_size, stride);
    }
    memmove(dest, temporary, needed);
    free(temporary);
    return CLM_BPA_GET_SUCCESS;
}
/** Copies a tightly packed row-major buffer into one atlas frame.
 * @param id Atlas storage ID.
 * @param layer Zero-based layer.
 * @param frame_row Zero-based frame row within the atlas.
 * @param frame_column Zero-based frame column within the atlas.
 * @param buffer Frame bytes in row-major order, with no row padding.
 * @param buffer_size Source byte count; must exactly equal the frame size.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds.
 */
static inline int clm_storage_atlas_set_frame_from(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    const void* buffer,
    const size_t buffer_size) {
    clm_storage_info info;
    clm_bpaa_array_node* array;
    if (!buffer || clm_storage_atlas_internal_rect(id, layer, frame_row, frame_column,
                                                   &info, &array) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    const size_t stride = (size_t)info.frame_columns * info.element_size;
    const size_t needed = stride * info.frame_rows;
    if (buffer_size != needed) return CLM_BPA_FAILURE;
    unsigned char* temporary = malloc(needed);
    if (!temporary) return CLM_BPA_FAILURE;
    memcpy(temporary, buffer, needed);
    for (clm_uint y = 0; y < info.frame_rows; ++y) {
        const size_t offset = ((size_t)layer * info.rows + frame_row * info.frame_rows + y) * info.columns + frame_column * info.frame_columns;
        memcpy(array->items.blocks + offset * info.element_size, temporary + (size_t)y * stride, stride);
    }
    free(temporary);
    array->deck_size = array->count;
    return CLM_BPA_SET_SUCCESS;
}
/** Copies an atlas frame into a new regular 2D storage resource.
 * @param id Atlas storage ID.
 * @param layer Zero-based layer.
 * @param frame_row Zero-based frame row within the atlas.
 * @param frame_column Zero-based frame column within the atlas.
 * @param destination_id New unique ID for the frame storage.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds/ID.
 */
static inline int clm_storage_atlas_slice_frame(
    const clm_id id,
    const clm_uint layer,
    const clm_uint frame_row,
    const clm_uint frame_column,
    const clm_id destination_id) {
    clm_storage_info info;
    clm_bpaa_array_node* array;
    if (clm_storage_atlas_internal_rect(id, layer, frame_row, frame_column,
                                        &info, &array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_storage_slice(id, layer, frame_row * info.frame_rows,
                             frame_column * info.frame_columns, 1,
                             info.frame_rows, info.frame_columns, destination_id);
}
