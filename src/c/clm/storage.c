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

#include "storage.h"

int clm_storage_internal_read(const clm_id id, clm_storage_internal_metadata* out) {
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

int clm_storage_internal_allocate_id(const clm_id forbidden, clm_id* result) {
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

int clm_storage_internal_new(
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

int clm_storage_new(
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

int clm_storage_new_2d(
    const clm_id id,
    const clm_byte type,
    const size_t struct_size,
    const clm_uint columns,
    const clm_uint rows) {
    return clm_storage_new(id, type, struct_size, columns, rows, 1);
}

int clm_storage_atlas_new(
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

int clm_storage_atlas_new_3d(
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

int clm_storage_get_info(const clm_id id, clm_storage_info* info) {
    clm_storage_internal_metadata metadata;
    if (!info || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    *info = metadata.info;
    return CLM_BPA_GET_SUCCESS;
}

int clm_storage_delete(const clm_id id) {
    clm_storage_internal_metadata metadata;
    if (clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    int a = clm_bpaa_delete(metadata.info.data_id);
    int b = clm_bpa_delete(id);
    return a == CLM_BPA_SUCCESS && b == CLM_BPA_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

int clm_storage_internal_index(
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

int clm_storage_get_to(
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

int clm_storage_set_from(
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

int clm_storage_get(
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

int clm_storage_set(
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

int clm_storage_fill_from(
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

int clm_storage_slice(
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

int clm_storage_atlas_internal_rect(
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

int clm_storage_atlas_get_frame_to(
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

int clm_storage_atlas_set_frame_from(
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

int clm_storage_atlas_slice_frame(
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
