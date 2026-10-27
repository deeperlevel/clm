module clm.storage;

import core.stdc.stdint : uint64_t;
import core.stdc.string : memcpy;
import clm.base : IdKey, clm_byte, clm_int, clm_uint;
public import clm.bpa;

enum ulong CLM_STORAGE_MAGIC = 0x434c4d53544f5247UL;

/** Dimensions and element metadata for an existing storage resource. */
struct StorageInfo
{
    clm_uint columns;
    clm_uint rows;
    clm_uint layers;
    clm_uint frame_columns;
    clm_uint frame_rows;
    clm_uint element_size;
    clm_byte type;
    clm_byte atlas;
    IdKey data_id;
}

alias clm_storage_info = StorageInfo;

private struct StorageInternalMetadata
{
    uint64_t magic;
    StorageInfo info;
}

private int clm_storage_internal_read(IdKey id, out StorageInternalMetadata metadata)
{
    auto block = clm_bpa_find_block(id);
    if (block is null || block.type != CLM_BPA_TYPE_STRUCT ||
        block.block_size != StorageInternalMetadata.sizeof ||
        block.data.length != StorageInternalMetadata.sizeof)
        return CLM_BPA_FAILURE;

    memcpy(&metadata, block.data.ptr, StorageInternalMetadata.sizeof);
    if (metadata.magic != CLM_STORAGE_MAGIC)
        return CLM_BPA_FAILURE;

    auto data = clm_bpaa_find_array(metadata.info.data_id);
    if (data is null || data.type != metadata.info.type ||
        data.block_size != metadata.info.element_size)
        return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

private int clm_storage_internal_allocate_id(IdKey forbidden, out IdKey result)
{
    auto count = clm_bpa_global_arrays_count;
    if (count > size_t.max - 2)
        return CLM_BPA_FAILURE;

    for (size_t i = 0; i <= count + 1; ++i)
    {
        auto candidate = IdKey.max - cast(IdKey)i;
        if (candidate != forbidden && !clm_bpaa_is_existing(candidate))
        {
            result = candidate;
            return CLM_BPA_SUCCESS;
        }
    }
    return CLM_BPA_FAILURE;
}

private int clm_storage_internal_new(
    IdKey id,
    clm_byte type,
    size_t struct_size,
    clm_uint columns,
    clm_uint rows,
    clm_uint layers,
    clm_uint frame_columns,
    clm_uint frame_rows,
    bool atlas)
{
    if (!columns || !rows || !layers || clm_bpa_is_existing(id) ||
        clm_bpaa_is_existing(id) ||
        (atlas && (!frame_columns || !frame_rows ||
                   columns % frame_columns || rows % frame_rows)) ||
        (!atlas && (frame_columns || frame_rows)) ||
        type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;

    clm_uint width = clm_bpa_type_size(type);
    if (type == CLM_BPA_TYPE_STRUCT)
    {
        if (!struct_size || struct_size > uint.max)
            return CLM_BPA_FAILURE;
        width = cast(clm_uint)struct_size;
    }
    else if (!width || (struct_size && struct_size != width))
        return CLM_BPA_FAILURE;

    if (cast(uint64_t)columns * rows > cast(uint64_t)int.max / layers)
        return CLM_BPA_FAILURE;
    auto total = cast(uint64_t)columns * rows * layers;
    if (!total || total > int.max || total > cast(uint64_t)(size_t.max / width))
        return CLM_BPA_FAILURE;

    IdKey data_id;
    if (clm_storage_internal_allocate_id(id, data_id) != CLM_BPA_SUCCESS ||
        clm_bpaa_create_sized(data_id, type, cast(clm_int)total, width, 0) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    auto array = clm_bpaa_find_array(data_id);
    if (array is null || array.blocks.length < cast(size_t)total * width)
    {
        clm_bpaa_delete(data_id);
        return CLM_BPA_FAILURE;
    }
    array.count = cast(clm_uint)total;
    array.deck_size = array.count;

    if (clm_bpa_create_sized(
            id, CLM_BPA_TYPE_STRUCT,
            cast(clm_uint)StorageInternalMetadata.sizeof) != CLM_BPA_SUCCESS)
    {
        clm_bpaa_delete(data_id);
        return CLM_BPA_FAILURE;
    }

    auto block = clm_bpa_find_block(id);
    if (block is null || block.data.length < StorageInternalMetadata.sizeof)
    {
        clm_bpa_delete(id);
        clm_bpaa_delete(data_id);
        return CLM_BPA_FAILURE;
    }

    StorageInternalMetadata metadata;
    metadata.magic = CLM_STORAGE_MAGIC;
    metadata.info.columns = columns;
    metadata.info.rows = rows;
    metadata.info.layers = layers;
    metadata.info.frame_columns = frame_columns;
    metadata.info.frame_rows = frame_rows;
    metadata.info.element_size = width;
    metadata.info.type = type;
    metadata.info.atlas = cast(clm_byte)atlas;
    metadata.info.data_id = data_id;
    memcpy(block.data.ptr, &metadata, StorageInternalMetadata.sizeof);
    return CLM_BPA_SUCCESS;
}

/** Creates a three-dimensional row-major storage grid of zero-initialized values. */
int clm_storage_new(
    IdKey id,
    clm_byte type,
    size_t struct_size,
    clm_uint columns,
    clm_uint rows,
    clm_uint layers)
{
    return clm_storage_internal_new(id, type, struct_size, columns, rows, layers, 0, 0, false);
}

/** Creates a two-dimensional row-major grid (one layer). */
int clm_storage_new_2d(
    IdKey id,
    clm_byte type,
    size_t struct_size,
    clm_uint columns,
    clm_uint rows)
{
    return clm_storage_new(id, type, struct_size, columns, rows, 1);
}

/** Creates a two-dimensional atlas whose dimensions are divisible by its frame size. */
int clm_storage_atlas_new(
    IdKey id,
    clm_byte type,
    size_t struct_size,
    clm_uint columns,
    clm_uint rows,
    clm_uint frame_columns,
    clm_uint frame_rows)
{
    return clm_storage_internal_new(
        id, type, struct_size, columns, rows, 1, frame_columns, frame_rows, true);
}

/** Creates a layered atlas; each layer contains the same frame grid. */
int clm_storage_atlas_new_3d(
    IdKey id,
    clm_byte type,
    size_t struct_size,
    clm_uint columns,
    clm_uint rows,
    clm_uint layers,
    clm_uint frame_columns,
    clm_uint frame_rows)
{
    return clm_storage_internal_new(
        id, type, struct_size, columns, rows, layers, frame_columns, frame_rows, true);
}

/** Copies dimensions and element metadata to the caller. */
int clm_storage_get_info(IdKey id, out StorageInfo info)
{
    StorageInternalMetadata metadata;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    info = metadata.info;
    return CLM_BPA_GET_SUCCESS;
}

/** Deletes the storage resource and its internally managed BPAA array. */
int clm_storage_delete(IdKey id)
{
    StorageInternalMetadata metadata;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto array_result = clm_bpaa_delete(metadata.info.data_id);
    auto block_result = clm_bpa_delete(id);
    return array_result == CLM_BPA_SUCCESS && block_result == CLM_BPA_SUCCESS
        ? CLM_BPA_SUCCESS
        : CLM_BPA_FAILURE;
}

private int clm_storage_internal_index(
    const ref StorageInfo info,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    out clm_int index)
{
    if (layer >= info.layers || row >= info.rows || column >= info.columns)
        return CLM_BPA_FAILURE;
    index = cast(clm_int)((cast(uint64_t)layer * info.rows + row) * info.columns + column);
    return CLM_BPA_SUCCESS;
}

private int clm_storage_internal_get_bytes(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    size_t capacity,
    out ubyte[] bytes,
    out StorageInternalMetadata metadata)
{
    clm_int index;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(metadata.info, layer, row, column, index) != CLM_BPA_SUCCESS ||
        capacity < metadata.info.element_size)
        return CLM_BPA_FAILURE;
    auto result = clm_bpaa_get_raw(
        metadata.info.data_id, index, bytes, metadata.info.element_size);
    return result == CLM_BPA_GET_SUCCESS ? result : CLM_BPA_FAILURE;
}

private int clm_storage_internal_set_bytes(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    const(ubyte)[] bytes,
    size_t value_size,
    out StorageInternalMetadata metadata)
{
    clm_int index;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(metadata.info, layer, row, column, index) != CLM_BPA_SUCCESS ||
        value_size != metadata.info.element_size || bytes.length < value_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(metadata.info.data_id, index, bytes[0 .. value_size]);
}

/** Copies one element into caller-owned D storage (a scalar or byte array). */
int clm_storage_get_to(T)(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    ref T value)
{
    static if (is(T == ubyte[]))
    {
        StorageInternalMetadata metadata;
        ubyte[] bytes;
        if (clm_storage_internal_get_bytes(
                id, layer, row, column, value.length, bytes, metadata) != CLM_BPA_GET_SUCCESS)
            return CLM_BPA_FAILURE;
        value[0 .. bytes.length] = bytes[];
        return CLM_BPA_GET_SUCCESS;
    }
    else
    {
        StorageInternalMetadata metadata;
        ubyte[] bytes;
        if (clm_storage_internal_get_bytes(
                id, layer, row, column, T.sizeof, bytes, metadata) != CLM_BPA_GET_SUCCESS ||
            metadata.info.element_size > T.sizeof)
            return CLM_BPA_FAILURE;
        memcpy(&value, bytes.ptr, metadata.info.element_size);
        return CLM_BPA_GET_SUCCESS;
    }
}

/** Replaces one element from caller-owned D storage (a scalar or byte array). */
int clm_storage_set_from(T)(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    T value)
{
    static if (is(T == ubyte[]))
    {
        StorageInternalMetadata metadata;
        return clm_storage_internal_set_bytes(
            id, layer, row, column, value, value.length, metadata);
    }
    else
    {
        auto bytes = (cast(const(ubyte)*)&value)[0 .. T.sizeof];
        StorageInternalMetadata metadata;
        return clm_storage_internal_set_bytes(
            id, layer, row, column, bytes, T.sizeof, metadata);
    }
}

/** Replaces one element with an explicit source byte count. */
int clm_storage_set_from(T)(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    T value,
    size_t value_size)
{
    static if (is(T == ubyte[]))
    {
        StorageInternalMetadata metadata;
        return clm_storage_internal_set_bytes(
            id, layer, row, column, value, value_size, metadata);
    }
    else
    {
        if (value_size > T.sizeof)
            return CLM_BPA_FAILURE;
        auto bytes = (cast(const(ubyte)*)&value)[0 .. value_size];
        StorageInternalMetadata metadata;
        return clm_storage_internal_set_bytes(
            id, layer, row, column, bytes, value_size, metadata);
    }
}

/** Copies one cell into a compatible BPA block. */
int clm_storage_get(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    IdKey destination_block_id)
{
    StorageInternalMetadata metadata;
    clm_int index;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(metadata.info, layer, row, column, index) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    auto destination = clm_bpa_find_block(destination_block_id);
    if (destination is null || destination.type != metadata.info.type ||
        destination.block_size != metadata.info.element_size)
        return CLM_BPA_FAILURE;
    ubyte[] value;
    if (clm_bpaa_get_raw(metadata.info.data_id, index, value, metadata.info.element_size) !=
        CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_bpa_set(destination_block_id, value) == CLM_BPA_SET_SUCCESS ? CLM_BPA_GET_SUCCESS : CLM_BPA_FAILURE;
}

/** Copies a compatible BPA block into one cell. */
int clm_storage_set(
    IdKey id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    IdKey source_block_id)
{
    StorageInternalMetadata metadata;
    clm_int index;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS ||
        clm_storage_internal_index(metadata.info, layer, row, column, index) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto source = clm_bpa_find_block(source_block_id);
    if (source is null || source.type != metadata.info.type ||
        source.block_size != metadata.info.element_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(metadata.info.data_id, index, source.data);
}

/** Replaces every element with a copy of the same scalar or byte-array value. */
int clm_storage_fill_from(T)(IdKey id, T value)
{
    static if (is(T == ubyte[]))
    {
        return clm_storage_fill_from_bytes(id, value);
    }
    else
    {
        return clm_storage_fill_from_bytes(
            id, (cast(const(ubyte)*)&value)[0 .. T.sizeof]);
    }
}

private int clm_storage_fill_from_bytes(IdKey id, const(ubyte)[] value)
{
    StorageInternalMetadata metadata;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS ||
        value.length != metadata.info.element_size)
        return CLM_BPA_FAILURE;
    auto array = clm_bpaa_find_array(metadata.info.data_id);
    if (array is null || array.blocks.length < cast(size_t)array.count * array.block_size)
        return CLM_BPA_FAILURE;

    auto snapshot = value.dup;
    foreach (i; 0 .. array.count)
    {
        auto offset = cast(size_t)i * metadata.info.element_size;
        array.blocks[offset .. offset + metadata.info.element_size] = snapshot[];
    }
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/** Copies a rectangular or volumetric region into a new regular grid. */
int clm_storage_slice(
    IdKey source_id,
    clm_uint layer,
    clm_uint row,
    clm_uint column,
    clm_uint layers,
    clm_uint rows,
    clm_uint columns,
    IdKey destination_id)
{
    StorageInternalMetadata source;
    if (clm_storage_internal_read(source_id, source) != CLM_BPA_SUCCESS ||
        !layers || !rows || !columns ||
        layer >= source.info.layers || row >= source.info.rows || column >= source.info.columns ||
        layers > source.info.layers - layer ||
        rows > source.info.rows - row ||
        columns > source.info.columns - column ||
        clm_storage_new(
            destination_id, source.info.type, source.info.element_size,
            columns, rows, layers) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    auto src = clm_bpaa_find_array(source.info.data_id);
    StorageInternalMetadata destination;
    if (src is null ||
        clm_storage_internal_read(destination_id, destination) != CLM_BPA_SUCCESS)
    {
        cast(void) clm_storage_delete(destination_id);
        return CLM_BPA_FAILURE;
    }
    auto dest = clm_bpaa_find_array(destination.info.data_id);
    if (dest is null)
    {
        cast(void) clm_storage_delete(destination_id);
        return CLM_BPA_FAILURE;
    }

    auto bytes = cast(size_t)columns * source.info.element_size;
    foreach (z; 0 .. layers)
    {
        foreach (y; 0 .. rows)
        {
            auto from = ((cast(size_t)(layer + z) * source.info.rows + row + y) *
                         source.info.columns + column) * source.info.element_size;
            auto to = (cast(size_t)z * rows + y) * bytes;
            dest.blocks[to .. to + bytes] = src.blocks[from .. from + bytes];
        }
    }
    return CLM_BPA_SUCCESS;
}

private int clm_storage_atlas_internal_rect(
    IdKey id,
    clm_uint layer,
    clm_uint frame_row,
    clm_uint frame_column,
    out StorageInfo info,
    out clm_bpaa_array_node array)
{
    StorageInternalMetadata metadata;
    if (clm_storage_internal_read(id, metadata) != CLM_BPA_SUCCESS ||
        !metadata.info.atlas || layer >= metadata.info.layers ||
        frame_row >= metadata.info.rows / metadata.info.frame_rows ||
        frame_column >= metadata.info.columns / metadata.info.frame_columns)
        return CLM_BPA_FAILURE;

    info = metadata.info;
    array = clm_bpaa_find_array(metadata.info.data_id);
    return array !is null && array.blocks.length != 0
        ? CLM_BPA_SUCCESS
        : CLM_BPA_FAILURE;
}

/** Copies one atlas frame to a tightly packed row-major byte array. */
int clm_storage_atlas_get_frame_to(
    IdKey id,
    clm_uint layer,
    clm_uint frame_row,
    clm_uint frame_column,
    ubyte[] buffer)
{
    StorageInfo info;
    clm_bpaa_array_node array;
    if (clm_storage_atlas_internal_rect(
            id, layer, frame_row, frame_column, info, array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    auto stride = cast(size_t)info.frame_columns * info.element_size;
    auto needed = stride * info.frame_rows;
    if (buffer.length < needed)
        return CLM_BPA_FAILURE;

    ubyte[] temporary;
    temporary.length = needed;
    foreach (y; 0 .. info.frame_rows)
    {
        auto offset = ((cast(size_t)layer * info.rows +
                       frame_row * info.frame_rows + y) *
                      info.columns + frame_column * info.frame_columns) *
                      info.element_size;
        auto target = cast(size_t)y * stride;
        temporary[target .. target + stride] = array.blocks[offset .. offset + stride];
    }
    buffer[0 .. needed] = temporary[];
    return CLM_BPA_GET_SUCCESS;
}

/** Copies a tightly packed row-major byte array into one atlas frame. */
int clm_storage_atlas_set_frame_from(
    IdKey id,
    clm_uint layer,
    clm_uint frame_row,
    clm_uint frame_column,
    const(ubyte)[] buffer)
{
    StorageInfo info;
    clm_bpaa_array_node array;
    if (clm_storage_atlas_internal_rect(
            id, layer, frame_row, frame_column, info, array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    auto stride = cast(size_t)info.frame_columns * info.element_size;
    auto needed = stride * info.frame_rows;
    if (buffer.length != needed)
        return CLM_BPA_FAILURE;

    auto temporary = buffer.dup;
    foreach (y; 0 .. info.frame_rows)
    {
        auto offset = ((cast(size_t)layer * info.rows +
                       frame_row * info.frame_rows + y) *
                      info.columns + frame_column * info.frame_columns) *
                      info.element_size;
        auto source = cast(size_t)y * stride;
        array.blocks[offset .. offset + stride] = temporary[source .. source + stride];
    }
    array.deck_size = array.count;
    return CLM_BPA_SET_SUCCESS;
}

/** Copies an atlas frame into a new regular two-dimensional storage resource. */
int clm_storage_atlas_slice_frame(
    IdKey id,
    clm_uint layer,
    clm_uint frame_row,
    clm_uint frame_column,
    IdKey destination_id)
{
    StorageInfo info;
    clm_bpaa_array_node array;
    if (clm_storage_atlas_internal_rect(
            id, layer, frame_row, frame_column, info, array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_storage_slice(
        id, layer, frame_row * info.frame_rows, frame_column * info.frame_columns,
        1, info.frame_rows, info.frame_columns, destination_id);
}
