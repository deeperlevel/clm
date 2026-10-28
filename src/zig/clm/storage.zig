//! BPA-backed fixed-dimension grid and atlas storage.
//!
//! Coordinates are zero-based and row-major: columns vary fastest, then rows,
//! then layers. Payloads are fixed-width byte-copyable BPA values; strings and
//! owning object types are not supported. Operations are not thread-safe.

const std = @import("std");
const bpa = @import("clm_bpa");

pub const clm_id = u64;
pub const clm_uint = u32;
pub const clm_int = i32;
pub const clm_byte = u8;
pub const CLM_STORAGE_MAGIC: u64 = 0x434c4d53544f5247;

/// Dimensions and element metadata for an existing storage resource.
pub const clm_storage_info = extern struct {
    /// Number of elements along the column (x) axis.
    columns: clm_uint = 0,
    rows: clm_uint = 0,
    layers: clm_uint = 0,
    /// Atlas frame dimensions in elements; zero for non-atlas storage.
    frame_columns: clm_uint = 0,
    frame_rows: clm_uint = 0,
    /// Byte width of every stored value.
    element_size: clm_uint = 0,
    /// BPA value type used for each element.
    type: clm_byte = 0,
    /// Nonzero when configured as an atlas.
    atlas: clm_byte = 0,
    /// Internal BPAA array ID, managed by clm_storage.
    data_id: clm_id = 0,
};

pub const clm_storage_internal_metadata = extern struct {
    magic: u64 = 0,
    info: clm_storage_info = .{},
};

fn metadataBlock(id: clm_id) ?*bpa.clm_bpa_block_node {
    const block = bpa.clm_bpa_find_block(id) orelse return null;
    if (block.type != bpa.CLM_BPA_TYPE_STRUCT or
        block.block_size != @sizeOf(clm_storage_internal_metadata) or
        block.data.len < @sizeOf(clm_storage_internal_metadata))
        return null;
    const metadata: *align(1) const clm_storage_internal_metadata =
        @ptrCast(block.data.ptr);
    if (metadata.magic != CLM_STORAGE_MAGIC) return null;
    return block;
}

fn readMetadata(id: clm_id, out: *clm_storage_internal_metadata) i32 {
    const block = metadataBlock(id) orelse return bpa.CLM_BPA_FAILURE;
    const source: *align(1) const clm_storage_internal_metadata =
        @ptrCast(block.data.ptr);
    out.* = source.*;
    const array = bpa.clm_bpaa_find_array(out.info.data_id) orelse return bpa.CLM_BPA_FAILURE;
    if (array.type != out.info.type or array.block_size != out.info.element_size)
        return bpa.CLM_BPA_FAILURE;
    return bpa.CLM_BPA_SUCCESS;
}

fn allocateArrayId(forbidden: clm_id, result: *clm_id) i32 {
    var candidate: clm_id = std.math.maxInt(clm_id);
    var attempts: usize = 0;
    const limit = bpa.clm_bpa_global_arrays_count + 1;
    while (attempts <= limit) : (attempts += 1) {
        if (candidate != forbidden and !bpa.clm_bpaa_is_existing(candidate)) {
            result.* = candidate;
            return bpa.CLM_BPA_SUCCESS;
        }
        candidate -= 1;
    }
    return bpa.CLM_BPA_FAILURE;
}

fn writeMetadata(id: clm_id, metadata: *const clm_storage_internal_metadata) i32 {
    const block = bpa.clm_bpa_find_block(id) orelse return bpa.CLM_BPA_FAILURE;
    if (block.type != bpa.CLM_BPA_TYPE_STRUCT or
        block.block_size != @sizeOf(clm_storage_internal_metadata) or
        block.data.len < @sizeOf(clm_storage_internal_metadata))
        return bpa.CLM_BPA_FAILURE;
    const target: *align(1) clm_storage_internal_metadata = @ptrCast(block.data.ptr);
    target.* = metadata.*;
    return bpa.CLM_BPA_SUCCESS;
}

fn internalNew(
    id: clm_id,
    value_type: clm_byte,
    struct_size: usize,
    columns: clm_uint,
    rows: clm_uint,
    layers: clm_uint,
    frame_columns: clm_uint,
    frame_rows: clm_uint,
    atlas: bool,
) i32 {
    if (columns == 0 or rows == 0 or layers == 0 or
        bpa.clm_bpa_is_existing(id) or bpa.clm_bpaa_is_existing(id) or
        (atlas and (frame_columns == 0 or frame_rows == 0 or
            columns % frame_columns != 0 or rows % frame_rows != 0)) or
        (!atlas and (frame_columns != 0 or frame_rows != 0)) or
        value_type == bpa.CLM_BPA_TYPE_STRING)
        return bpa.CLM_BPA_FAILURE;

    var width = bpa.clm_bpa_type_size(value_type);
    if (value_type == bpa.CLM_BPA_TYPE_STRUCT) {
        if (struct_size == 0 or struct_size > std.math.maxInt(clm_uint))
            return bpa.CLM_BPA_FAILURE;
        width = @intCast(struct_size);
    } else if (width == 0 or (struct_size != 0 and struct_size != width)) {
        return bpa.CLM_BPA_FAILURE;
    }
    const xy: u64 = @as(u64, columns) * rows;
    if (xy > @as(u64, std.math.maxInt(clm_int)) / layers) return bpa.CLM_BPA_FAILURE;
    const total = xy * layers;
    if (total == 0 or total > std.math.maxInt(clm_int) or
        total > @as(u64, @intCast(std.math.maxInt(usize))) / @as(u64, width))
        return bpa.CLM_BPA_FAILURE;

    var data_id: clm_id = 0;
    if (allocateArrayId(id, &data_id) != bpa.CLM_BPA_SUCCESS or
        bpa.clm_bpaa_create_sized(data_id, value_type, @intCast(total), width, 0) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const array = bpa.clm_bpaa_find_array(data_id) orelse {
        _ = bpa.clm_bpaa_delete(data_id);
        return bpa.CLM_BPA_FAILURE;
    };
    const bytes_needed: usize = @intCast(total * width);
    if (array.items.blocks.len < bytes_needed) {
        _ = bpa.clm_bpaa_delete(data_id);
        return bpa.CLM_BPA_FAILURE;
    }
    @memset(array.items.blocks[0..bytes_needed], 0);
    array.count = @intCast(total);
    array.deck_size = array.count;

    if (bpa.clm_bpa_create_sized(id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_storage_internal_metadata)) != bpa.CLM_BPA_SUCCESS) {
        _ = bpa.clm_bpaa_delete(data_id);
        return bpa.CLM_BPA_FAILURE;
    }
    var metadata = clm_storage_internal_metadata{
        .magic = CLM_STORAGE_MAGIC,
        .info = .{
            .columns = columns,
            .rows = rows,
            .layers = layers,
            .frame_columns = frame_columns,
            .frame_rows = frame_rows,
            .element_size = width,
            .type = value_type,
            .atlas = @intFromBool(atlas),
            .data_id = data_id,
        },
    };
    if (writeMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) {
        _ = bpa.clm_bpa_delete(id);
        _ = bpa.clm_bpaa_delete(data_id);
        return bpa.CLM_BPA_FAILURE;
    }
    return bpa.CLM_BPA_SUCCESS;
}

/// Creates a three-dimensional row-major storage grid. Values are zero-initialized.
/// STRUCT values require a nonzero byte width; STRING values are unsupported.
pub fn clm_storage_new(
    id: clm_id,
    value_type: clm_byte,
    struct_size: usize,
    columns: clm_uint,
    rows: clm_uint,
    layers: clm_uint,
) i32 {
    return internalNew(id, value_type, struct_size, columns, rows, layers, 0, 0, false);
}

/// Creates a two-dimensional row-major grid (one layer).
pub fn clm_storage_new_2d(
    id: clm_id,
    value_type: clm_byte,
    struct_size: usize,
    columns: clm_uint,
    rows: clm_uint,
) i32 {
    return clm_storage_new(id, value_type, struct_size, columns, rows, 1);
}

/// Creates a two-dimensional atlas whose grid dimensions divide evenly by the frame size.
pub fn clm_storage_atlas_new(
    id: clm_id,
    value_type: clm_byte,
    struct_size: usize,
    columns: clm_uint,
    rows: clm_uint,
    frame_columns: clm_uint,
    frame_rows: clm_uint,
) i32 {
    return internalNew(id, value_type, struct_size, columns, rows, 1, frame_columns, frame_rows, true);
}

/// Creates a layered atlas; each layer contains the same frame grid.
pub fn clm_storage_atlas_new_3d(
    id: clm_id,
    value_type: clm_byte,
    struct_size: usize,
    columns: clm_uint,
    rows: clm_uint,
    layers: clm_uint,
    frame_columns: clm_uint,
    frame_rows: clm_uint,
) i32 {
    return internalNew(id, value_type, struct_size, columns, rows, layers, frame_columns, frame_rows, true);
}

/// Copies dimensions and element metadata to the caller.
pub fn clm_storage_get_info(id: clm_id, info: *clm_storage_info) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    info.* = metadata.info;
    return bpa.CLM_BPA_GET_SUCCESS;
}

/// Deletes storage and its internally managed BPAA array.
pub fn clm_storage_delete(id: clm_id) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    const array_status = bpa.clm_bpaa_delete(metadata.info.data_id);
    const block_status = bpa.clm_bpa_delete(id);
    return if (array_status == bpa.CLM_BPA_SUCCESS and block_status == bpa.CLM_BPA_SUCCESS)
        bpa.CLM_BPA_SUCCESS
    else
        bpa.CLM_BPA_FAILURE;
}

fn internalIndex(info: *const clm_storage_info, layer: clm_uint, row: clm_uint, column: clm_uint) ?clm_int {
    if (layer >= info.layers or row >= info.rows or column >= info.columns) return null;
    const index = (@as(u64, layer) * info.rows + row) * info.columns + column;
    if (index > std.math.maxInt(clm_int)) return null;
    return @intCast(index);
}

/// Copies one element into caller-owned memory.
pub fn clm_storage_get_to(
    id: clm_id,
    layer: clm_uint,
    row: clm_uint,
    column: clm_uint,
    buffer: []u8,
) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or buffer.len < metadata.info.element_size)
        return bpa.CLM_BPA_FAILURE;
    const index = internalIndex(&metadata.info, layer, row, column) orelse return bpa.CLM_BPA_FAILURE;
    return bpa.clm_bpaa_get_raw(metadata.info.data_id, index, buffer[0..metadata.info.element_size]);
}

/// Replaces one element from caller-owned bytes. The source width must match exactly.
pub fn clm_storage_set_from(
    id: clm_id,
    layer: clm_uint,
    row: clm_uint,
    column: clm_uint,
    value: []const u8,
) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or value.len != metadata.info.element_size)
        return bpa.CLM_BPA_FAILURE;
    const index = internalIndex(&metadata.info, layer, row, column) orelse return bpa.CLM_BPA_FAILURE;
    return bpa.clm_bpaa_set_raw(metadata.info.data_id, index, value);
}

/// Copies one element into a compatible BPA destination block.
pub fn clm_storage_get(id: clm_id, layer: clm_uint, row: clm_uint, column: clm_uint, destination_block_id: clm_id) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    const index = internalIndex(&metadata.info, layer, row, column) orelse return bpa.CLM_BPA_FAILURE;
    return bpa.clm_bpaa_get(metadata.info.data_id, index, destination_block_id, metadata.info.element_size);
}

/// Copies a compatible BPA source block into one element.
pub fn clm_storage_set(id: clm_id, layer: clm_uint, row: clm_uint, column: clm_uint, source_block_id: clm_id) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    const index = internalIndex(&metadata.info, layer, row, column) orelse return bpa.CLM_BPA_FAILURE;
    return bpa.clm_bpaa_set(metadata.info.data_id, index, source_block_id, metadata.info.element_size);
}

/// Replaces every element with a copy of the same value.
pub fn clm_storage_fill_from(id: clm_id, value: []const u8) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or value.len != metadata.info.element_size)
        return bpa.CLM_BPA_FAILURE;
    const array = bpa.clm_bpaa_find_array(metadata.info.data_id) orelse return bpa.CLM_BPA_FAILURE;
    const bytes_needed = @as(usize, array.count) * metadata.info.element_size;
    if (array.items.blocks.len < bytes_needed) return bpa.CLM_BPA_FAILURE;
    const snapshot = std.heap.page_allocator.alloc(u8, value.len) catch return bpa.CLM_BPA_FAILURE;
    defer std.heap.page_allocator.free(snapshot);
    @memcpy(snapshot, value);
    for (0..array.count) |index| {
        const offset = index * metadata.info.element_size;
        @memcpy(array.items.blocks[offset .. offset + metadata.info.element_size], snapshot);
    }
    array.deck_size = array.count;
    return bpa.CLM_BPA_SUCCESS;
}

/// Copies a contiguous rectangular or volumetric region into new regular-grid storage.
pub fn clm_storage_slice(
    source_id: clm_id,
    layer: clm_uint,
    row: clm_uint,
    column: clm_uint,
    layers: clm_uint,
    rows: clm_uint,
    columns: clm_uint,
    destination_id: clm_id,
) i32 {
    var source: clm_storage_internal_metadata = undefined;
    if (readMetadata(source_id, &source) != bpa.CLM_BPA_SUCCESS or
        layers == 0 or rows == 0 or columns == 0 or
        layer >= source.info.layers or row >= source.info.rows or column >= source.info.columns or
        layers > source.info.layers - layer or rows > source.info.rows - row or
        columns > source.info.columns - column or
        clm_storage_new(destination_id, source.info.type, source.info.element_size, columns, rows, layers) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const src = bpa.clm_bpaa_find_array(source.info.data_id) orelse {
        _ = clm_storage_delete(destination_id);
        return bpa.CLM_BPA_FAILURE;
    };
    var destination: clm_storage_internal_metadata = undefined;
    if (readMetadata(destination_id, &destination) != bpa.CLM_BPA_SUCCESS) {
        _ = clm_storage_delete(destination_id);
        return bpa.CLM_BPA_FAILURE;
    }
    const dst = bpa.clm_bpaa_find_array(destination.info.data_id) orelse {
        _ = clm_storage_delete(destination_id);
        return bpa.CLM_BPA_FAILURE;
    };
    const bytes_per_row = @as(usize, columns) * source.info.element_size;
    for (0..layers) |z| {
        for (0..rows) |y| {
            const from = ((@as(usize, layer) + z) * source.info.rows + row + y) *
                source.info.columns + column;
            const to = (z * rows + y) * columns;
            const source_offset = from * source.info.element_size;
            const destination_offset = to * source.info.element_size;
            @memcpy(
                dst.items.blocks[destination_offset .. destination_offset + bytes_per_row],
                src.items.blocks[source_offset .. source_offset + bytes_per_row],
            );
        }
    }
    return bpa.CLM_BPA_SUCCESS;
}

fn atlasRect(
    id: clm_id,
    layer: clm_uint,
    frame_row: clm_uint,
    frame_column: clm_uint,
    info: *clm_storage_info,
    array_out: *?*bpa.clm_bpaa_array_node,
) i32 {
    var metadata: clm_storage_internal_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or metadata.info.atlas == 0 or
        layer >= metadata.info.layers or
        frame_row >= metadata.info.rows / metadata.info.frame_rows or
        frame_column >= metadata.info.columns / metadata.info.frame_columns)
        return bpa.CLM_BPA_FAILURE;
    const array = bpa.clm_bpaa_find_array(metadata.info.data_id) orelse return bpa.CLM_BPA_FAILURE;
    if (array.items.blocks.len == 0) return bpa.CLM_BPA_FAILURE;
    info.* = metadata.info;
    array_out.* = array;
    return bpa.CLM_BPA_SUCCESS;
}

/// Copies one atlas frame to a tightly packed row-major caller buffer.
pub fn clm_storage_atlas_get_frame_to(
    id: clm_id,
    layer: clm_uint,
    frame_row: clm_uint,
    frame_column: clm_uint,
    buffer: []u8,
) i32 {
    var info: clm_storage_info = undefined;
    var array: ?*bpa.clm_bpaa_array_node = null;
    if (atlasRect(id, layer, frame_row, frame_column, &info, &array) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const stride = @as(usize, info.frame_columns) * info.element_size;
    const needed = stride * info.frame_rows;
    if (buffer.len < needed) return bpa.CLM_BPA_FAILURE;
    const temporary = std.heap.page_allocator.alloc(u8, needed) catch return bpa.CLM_BPA_FAILURE;
    defer std.heap.page_allocator.free(temporary);
    const cells = array.?;
    for (0..info.frame_rows) |y| {
        const offset = ((@as(usize, layer) * info.rows +
            @as(usize, frame_row) * info.frame_rows + y) * info.columns +
            @as(usize, frame_column) * info.frame_columns) * info.element_size;
        @memcpy(temporary[y * stride .. (y + 1) * stride], cells.items.blocks[offset .. offset + stride]);
    }
    @memmove(buffer[0..needed], temporary);
    return bpa.CLM_BPA_GET_SUCCESS;
}

/// Copies a tightly packed row-major buffer into one atlas frame.
pub fn clm_storage_atlas_set_frame_from(
    id: clm_id,
    layer: clm_uint,
    frame_row: clm_uint,
    frame_column: clm_uint,
    buffer: []const u8,
) i32 {
    var info: clm_storage_info = undefined;
    var array: ?*bpa.clm_bpaa_array_node = null;
    if (atlasRect(id, layer, frame_row, frame_column, &info, &array) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const stride = @as(usize, info.frame_columns) * info.element_size;
    const needed = stride * info.frame_rows;
    if (buffer.len != needed) return bpa.CLM_BPA_FAILURE;
    const temporary = std.heap.page_allocator.alloc(u8, needed) catch return bpa.CLM_BPA_FAILURE;
    defer std.heap.page_allocator.free(temporary);
    @memcpy(temporary, buffer);
    const cells = array.?;
    for (0..info.frame_rows) |y| {
        const offset = ((@as(usize, layer) * info.rows +
            @as(usize, frame_row) * info.frame_rows + y) * info.columns +
            @as(usize, frame_column) * info.frame_columns) * info.element_size;
        @memcpy(cells.items.blocks[offset .. offset + stride], temporary[y * stride .. (y + 1) * stride]);
    }
    cells.deck_size = cells.count;
    return bpa.CLM_BPA_SET_SUCCESS;
}

/// Copies an atlas frame into a new regular 2D storage resource.
pub fn clm_storage_atlas_slice_frame(
    id: clm_id,
    layer: clm_uint,
    frame_row: clm_uint,
    frame_column: clm_uint,
    destination_id: clm_id,
) i32 {
    var info: clm_storage_info = undefined;
    var array: ?*bpa.clm_bpaa_array_node = null;
    if (atlasRect(id, layer, frame_row, frame_column, &info, &array) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    return clm_storage_slice(
        id,
        layer,
        frame_row * info.frame_rows,
        frame_column * info.frame_columns,
        1,
        info.frame_rows,
        info.frame_columns,
        destination_id,
    );
}
