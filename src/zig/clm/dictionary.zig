//! BPA-backed keyed values with insertion-ordered iteration.

const std = @import("std");
const bpa = @import("clm_bpa");

pub const CLM_DICTIONARY_INITIAL_CAPACITY: u32 = 16;
pub const CLM_DICTIONARY_METADATA_MAGIC: u64 = 0x434C4D444943544E;
pub const CLM_DICTIONARY_SLOT_EMPTY: u8 = 0;
pub const CLM_DICTIONARY_SLOT_OCCUPIED: u8 = 1;
pub const CLM_DICTIONARY_SLOT_DELETED: u8 = 2;

pub const clm_id = u64;
pub const clm_uint = u32;
pub const clm_int = i32;
pub const clm_byte = u8;

/// Internal open-addressing slot holding one key and its value-array index.
pub const clm_dictionary_slot = extern struct {
    key: clm_id = 0,
    value_index: clm_uint = 0,
    state: clm_byte = CLM_DICTIONARY_SLOT_EMPTY,
};

/// Internal dictionary metadata stored in a BPA STRUCT block.
pub const clm_dictionary_metadata = extern struct {
    magic: u64 = 0,
    slots_id: clm_id = 0,
    values_id: clm_id = 0,
    keys_id: clm_id = 0,
    value_size: clm_uint = 0,
    count: clm_uint = 0,
    iterator: clm_uint = 0,
    value_type: clm_byte = 0,
    reserved: [3]clm_byte = .{ 0, 0, 0 },
};

fn metadataBlock(id: clm_id) ?*bpa.clm_bpa_block_node {
    const block = bpa.clm_bpa_find_block(id) orelse return null;
    if (block.type != bpa.CLM_BPA_TYPE_STRUCT or
        block.block_size != @sizeOf(clm_dictionary_metadata) or
        block.data.len < @sizeOf(clm_dictionary_metadata))
        return null;
    const metadata: *align(1) const clm_dictionary_metadata =
        @ptrCast(block.data.ptr);
    if (metadata.magic != CLM_DICTIONARY_METADATA_MAGIC) return null;
    return block;
}

fn readMetadata(id: clm_id, metadata: *clm_dictionary_metadata) i32 {
    if (metadataBlock(id)) |block| {
        const source: *align(1) const clm_dictionary_metadata =
            @ptrCast(block.data.ptr);
        metadata.* = source.*;
        return bpa.CLM_BPA_SUCCESS;
    }
    return bpa.CLM_BPA_FAILURE;
}

fn getArray(id: clm_id, value_type: clm_byte, block_size: clm_uint) ?*bpa.clm_bpaa_array_node {
    const array = bpa.clm_bpaa_find_array(id) orelse return null;
    if (array.type != value_type or
        (value_type == bpa.CLM_BPA_TYPE_STRUCT and array.block_size != block_size))
        return null;
    return array;
}

fn slotSlice(array: ?*bpa.clm_bpaa_array_node) ?[]clm_dictionary_slot {
    const slots_array = array orelse return null;
    if (slots_array.type != bpa.CLM_BPA_TYPE_STRUCT or
        slots_array.block_size != @sizeOf(clm_dictionary_slot) or
        slots_array.count != slots_array.capacity or
        slots_array.items.blocks.len < @as(usize, slots_array.capacity) * @sizeOf(clm_dictionary_slot))
        return null;
    const aligned: [*]clm_dictionary_slot =
        @ptrCast(@alignCast(slots_array.items.blocks.ptr));
    return aligned[0..slots_array.capacity];
}

fn allocateArrayId(forbidden_a: clm_id, forbidden_b: clm_id, out_id: *clm_id) i32 {
    var candidate: clm_id = std.math.maxInt(clm_id);
    var attempts: usize = 0;
    const limit = bpa.clm_bpa_global_arrays_count + 2;
    while (attempts <= limit) : (attempts += 1) {
        if (candidate != forbidden_a and candidate != forbidden_b and
            !bpa.clm_bpaa_is_existing(candidate))
        {
            out_id.* = candidate;
            return bpa.CLM_BPA_SUCCESS;
        }
        candidate -= 1;
    }
    return bpa.CLM_BPA_FAILURE;
}

fn createSlots(id: clm_id, capacity: clm_uint) i32 {
    if (capacity == 0 or capacity > std.math.maxInt(clm_int) or
        bpa.clm_bpaa_create_sized(
            id,
            bpa.CLM_BPA_TYPE_STRUCT,
            @intCast(capacity),
            @sizeOf(clm_dictionary_slot),
            0,
        ) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const empty = clm_dictionary_slot{};
    const bytes = std.mem.asBytes(&empty);
    for (0..capacity) |_| {
        if (bpa.clm_bpaa_append_raw(id, bytes) != bpa.CLM_BPA_SUCCESS) {
            _ = bpa.clm_bpaa_delete(id);
            return bpa.CLM_BPA_FAILURE;
        }
    }
    return bpa.CLM_BPA_SUCCESS;
}

fn findSlot(slots: []clm_dictionary_slot, key: clm_id, found: *bool) clm_uint {
    const capacity: clm_uint = @intCast(slots.len);
    var first_deleted = capacity;
    var index: clm_uint = @intCast(bpa.clm_bpa_hash_id(key) & (capacity - 1));
    found.* = false;
    for (0..capacity) |_| {
        if (slots[index].state == CLM_DICTIONARY_SLOT_EMPTY)
            return if (first_deleted != capacity) first_deleted else index;
        if (slots[index].state == CLM_DICTIONARY_SLOT_OCCUPIED and slots[index].key == key) {
            found.* = true;
            return index;
        }
        if (slots[index].state == CLM_DICTIONARY_SLOT_DELETED and first_deleted == capacity)
            first_deleted = index;
        index = (index + 1) & (capacity - 1);
    }
    return first_deleted;
}

fn rehash(dictionary_id: clm_id, metadata: *clm_dictionary_metadata, new_capacity: clm_uint) i32 {
    var new_slots_id: clm_id = 0;
    if (new_capacity < CLM_DICTIONARY_INITIAL_CAPACITY or
        new_capacity > std.math.maxInt(clm_int) or
        (new_capacity & (new_capacity - 1)) != 0 or
        allocateArrayId(dictionary_id, metadata.values_id, &new_slots_id) != bpa.CLM_BPA_SUCCESS or
        createSlots(new_slots_id, new_capacity) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;

    const old_array = getArray(metadata.slots_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_slot)) orelse {
        _ = bpa.clm_bpaa_delete(new_slots_id);
        return bpa.CLM_BPA_FAILURE;
    };
    const old_slots = slotSlice(old_array) orelse {
        _ = bpa.clm_bpaa_delete(new_slots_id);
        return bpa.CLM_BPA_FAILURE;
    };
    const new_array = getArray(new_slots_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_slot)) orelse {
        _ = bpa.clm_bpaa_delete(new_slots_id);
        return bpa.CLM_BPA_FAILURE;
    };
    const new_slots = slotSlice(new_array) orelse {
        _ = bpa.clm_bpaa_delete(new_slots_id);
        return bpa.CLM_BPA_FAILURE;
    };

    for (old_slots) |old_slot| {
        if (old_slot.state == CLM_DICTIONARY_SLOT_OCCUPIED) {
            var found = false;
            const target = findSlot(new_slots, old_slot.key, &found);
            if (found or target == new_slots.len) {
                _ = bpa.clm_bpaa_delete(new_slots_id);
                return bpa.CLM_BPA_FAILURE;
            }
            new_slots[target] = old_slot;
        }
    }

    const old_slots_id = metadata.slots_id;
    metadata.slots_id = new_slots_id;
    const block = metadataBlock(dictionary_id) orelse {
        metadata.slots_id = old_slots_id;
        _ = bpa.clm_bpaa_delete(new_slots_id);
        return bpa.CLM_BPA_FAILURE;
    };
    const target: *align(1) clm_dictionary_metadata = @ptrCast(block.data.ptr);
    target.* = metadata.*;
    if (bpa.clm_bpaa_delete(old_slots_id) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    return bpa.CLM_BPA_SUCCESS;
}

fn prepareInsert(
    dictionary_id: clm_id,
    metadata: *clm_dictionary_metadata,
    slots_array: *?*bpa.clm_bpaa_array_node,
    slots: *?[]clm_dictionary_slot,
) i32 {
    var array = getArray(metadata.slots_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_slot)) orelse return bpa.CLM_BPA_FAILURE;
    var entries = slotSlice(array) orelse return bpa.CLM_BPA_FAILURE;
    var deleted: clm_uint = 0;
    for (entries) |entry| {
        if (entry.state == CLM_DICTIONARY_SLOT_DELETED) deleted += 1;
    }
    if (metadata.count + deleted + 1 >= array.capacity - array.capacity / 4) {
        var new_capacity = array.capacity;
        if (metadata.count + 1 >= array.capacity - array.capacity / 4) {
            if (array.capacity > std.math.maxInt(clm_int) / 2) return bpa.CLM_BPA_FAILURE;
            new_capacity *= 2;
        }
        if (rehash(dictionary_id, metadata, new_capacity) != bpa.CLM_BPA_SUCCESS)
            return bpa.CLM_BPA_FAILURE;
        array = getArray(metadata.slots_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_slot)) orelse return bpa.CLM_BPA_FAILURE;
        entries = slotSlice(array) orelse return bpa.CLM_BPA_FAILURE;
    }
    slots_array.* = array;
    slots.* = entries;
    return bpa.CLM_BPA_SUCCESS;
}

fn lookup(
    id: clm_id,
    key: clm_id,
    metadata: *clm_dictionary_metadata,
    slots_array: *?*bpa.clm_bpaa_array_node,
    slots: *?[]clm_dictionary_slot,
    slot_index: *clm_uint,
    found: *bool,
) i32 {
    if (readMetadata(id, metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    const keys = getArray(metadata.keys_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_id)) orelse return bpa.CLM_BPA_FAILURE;
    const values = bpa.clm_bpaa_find_array(metadata.values_id) orelse return bpa.CLM_BPA_FAILURE;
    if (keys.count != values.count or keys.count != metadata.count) return bpa.CLM_BPA_FAILURE;
    const array = getArray(metadata.slots_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_slot)) orelse return bpa.CLM_BPA_FAILURE;
    const entries = slotSlice(array) orelse return bpa.CLM_BPA_FAILURE;
    slots_array.* = array;
    slots.* = entries;
    slot_index.* = findSlot(entries, key, found);
    return bpa.CLM_BPA_SUCCESS;
}

fn writeMetadata(id: clm_id, metadata: *const clm_dictionary_metadata) i32 {
    const block = bpa.clm_bpa_find_block(id) orelse return bpa.CLM_BPA_FAILURE;
    if (block.type != bpa.CLM_BPA_TYPE_STRUCT or
        block.block_size != @sizeOf(clm_dictionary_metadata) or
        block.data.len < @sizeOf(clm_dictionary_metadata))
        return bpa.CLM_BPA_FAILURE;
    const target: *align(1) clm_dictionary_metadata = @ptrCast(block.data.ptr);
    target.* = metadata.*;
    return bpa.CLM_BPA_SUCCESS;
}

/// Creates an empty dictionary for values of the specified BPA type.
/// STRUCT values require a nonzero byte width; STRING values use zero size.
pub fn clm_dictionary_new(id: clm_id, value_type: clm_byte, size: usize) i32 {
    var metadata: clm_dictionary_metadata = .{};
    var value_size: clm_uint = bpa.clm_bpa_type_size(value_type);
    if (bpa.clm_bpa_is_existing(id) or bpa.clm_bpaa_is_existing(id))
        return bpa.CLM_BPA_FAILURE;
    if (value_type == bpa.CLM_BPA_TYPE_STRUCT) {
        if (size == 0 or size > std.math.maxInt(clm_uint)) return bpa.CLM_BPA_FAILURE;
        value_size = @intCast(size);
    } else if (value_type == bpa.CLM_BPA_TYPE_STRING) {
        if (size != 0) return bpa.CLM_BPA_FAILURE;
        value_size = 0;
    } else if (value_size == 0 or (size != 0 and size != value_size)) {
        return bpa.CLM_BPA_FAILURE;
    }

    if (allocateArrayId(id, std.math.maxInt(clm_id), &metadata.slots_id) != bpa.CLM_BPA_SUCCESS or
        createSlots(metadata.slots_id, CLM_DICTIONARY_INITIAL_CAPACITY) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    if (allocateArrayId(id, metadata.slots_id, &metadata.values_id) != bpa.CLM_BPA_SUCCESS or
        bpa.clm_bpaa_create_sized(metadata.values_id, value_type, 0, value_size, 0) != bpa.CLM_BPA_SUCCESS)
    {
        _ = bpa.clm_bpaa_delete(metadata.slots_id);
        return bpa.CLM_BPA_FAILURE;
    }
    if (allocateArrayId(id, metadata.values_id, &metadata.keys_id) != bpa.CLM_BPA_SUCCESS or
        bpa.clm_bpaa_create_sized(metadata.keys_id, bpa.CLM_BPA_TYPE_STRUCT, 0, @sizeOf(clm_id), 0) != bpa.CLM_BPA_SUCCESS)
    {
        _ = bpa.clm_bpaa_delete(metadata.values_id);
        _ = bpa.clm_bpaa_delete(metadata.slots_id);
        return bpa.CLM_BPA_FAILURE;
    }
    metadata.value_type = value_type;
    metadata.value_size = value_size;
    metadata.magic = CLM_DICTIONARY_METADATA_MAGIC;
    if (bpa.clm_bpa_create_sized(id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_metadata)) != bpa.CLM_BPA_SUCCESS) {
        _ = bpa.clm_bpaa_delete(metadata.keys_id);
        _ = bpa.clm_bpaa_delete(metadata.values_id);
        _ = bpa.clm_bpaa_delete(metadata.slots_id);
        return bpa.CLM_BPA_FAILURE;
    }
    if (writeMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) {
        _ = bpa.clm_bpa_delete(metadata.keys_id);
        _ = bpa.clm_bpaa_delete(metadata.values_id);
        _ = bpa.clm_bpaa_delete(metadata.slots_id);
        return bpa.CLM_BPA_FAILURE;
    }
    return bpa.CLM_BPA_SUCCESS;
}

/// Deletes a dictionary and its BPA/BPAA-owned storage.
pub fn clm_dictionary_delete(id: clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    var status = bpa.CLM_BPA_SUCCESS;
    if (bpa.clm_bpaa_delete(metadata.keys_id) != bpa.CLM_BPA_SUCCESS) status = bpa.CLM_BPA_FAILURE;
    if (bpa.clm_bpaa_delete(metadata.values_id) != bpa.CLM_BPA_SUCCESS) status = bpa.CLM_BPA_FAILURE;
    if (bpa.clm_bpaa_delete(metadata.slots_id) != bpa.CLM_BPA_SUCCESS) status = bpa.CLM_BPA_FAILURE;
    if (bpa.clm_bpa_delete(id) != bpa.CLM_BPA_SUCCESS) status = bpa.CLM_BPA_FAILURE;
    return status;
}

/// Stores or replaces a value from a caller-owned slice.
/// STRING data must include a NUL terminator.
pub fn clm_dictionary_set_from(id: clm_id, key: clm_id, value: []const u8) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    var slots_array: ?*bpa.clm_bpaa_array_node = null;
    var slots_opt: ?[]clm_dictionary_slot = null;
    var slot_index: clm_uint = 0;
    var found = false;
    if (lookup(id, key, &metadata, &slots_array, &slots_opt, &slot_index, &found) != bpa.CLM_BPA_SUCCESS or
        (metadata.value_type != bpa.CLM_BPA_TYPE_STRING and value.len != metadata.value_size) or
        (metadata.value_type == bpa.CLM_BPA_TYPE_STRING and std.mem.indexOfScalar(u8, value, 0) == null) or
        value.len > std.math.maxInt(clm_uint))
        return bpa.CLM_BPA_FAILURE;
    const values = getArray(metadata.values_id, metadata.value_type, metadata.value_size) orelse return bpa.CLM_BPA_FAILURE;
    const slots = slots_opt orelse return bpa.CLM_BPA_FAILURE;
    if (found and slots[slot_index].value_index >= values.count) return bpa.CLM_BPA_FAILURE;
    if (found) {
        const result = if (metadata.value_type == bpa.CLM_BPA_TYPE_STRING)
            bpa.clm_bpaa_set_string_raw(metadata.values_id, @intCast(slots[slot_index].value_index), value)
        else
            bpa.clm_bpaa_set_raw(metadata.values_id, @intCast(slots[slot_index].value_index), value);
        return if (result < 0) bpa.CLM_BPA_FAILURE else bpa.CLM_BPA_SET_SUCCESS;
    }
    if (metadata.count >= std.math.maxInt(clm_int) or
        prepareInsert(id, &metadata, &slots_array, &slots_opt) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const updated_slots = slots_opt orelse return bpa.CLM_BPA_FAILURE;
    const updated_array = slots_array orelse return bpa.CLM_BPA_FAILURE;
    slot_index = findSlot(updated_slots, key, &found);
    if (found or slot_index >= updated_array.capacity) return bpa.CLM_BPA_FAILURE;
    const current_values = getArray(metadata.values_id, metadata.value_type, metadata.value_size) orelse return bpa.CLM_BPA_FAILURE;
    if (current_values.count >= std.math.maxInt(clm_int)) return bpa.CLM_BPA_FAILURE;
    const value_index = current_values.count;
    if (bpa.clm_bpaa_append_raw(metadata.keys_id, std.mem.asBytes(&key)) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    const append_result = bpa.clm_bpaa_append_raw(metadata.values_id, value);
    if (append_result != bpa.CLM_BPA_SUCCESS) {
        _ = bpa.clm_bpaa_remove(metadata.keys_id, @intCast(value_index));
        return bpa.CLM_BPA_FAILURE;
    }
    updated_slots[slot_index] = .{ .key = key, .value_index = value_index, .state = CLM_DICTIONARY_SLOT_OCCUPIED };
    metadata.count += 1;
    metadata.iterator = 0;
    if (writeMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) {
        _ = bpa.clm_bpaa_remove(metadata.values_id, @intCast(value_index));
        _ = bpa.clm_bpaa_remove(metadata.keys_id, @intCast(value_index));
        updated_slots[slot_index].state = CLM_DICTIONARY_SLOT_DELETED;
        return bpa.CLM_BPA_FAILURE;
    }
    return bpa.CLM_BPA_SET_SUCCESS;
}

/// Stores or replaces a copy of a compatible BPA block's value.
pub fn clm_dictionary_set(id: clm_id, key: clm_id, source_block_id: clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    const source = bpa.clm_bpa_find_block(source_block_id) orelse return bpa.CLM_BPA_FAILURE;
    if ((source.type != bpa.CLM_BPA_TYPE_STRING and source.data.len == 0) or
        readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or
        source.type != metadata.value_type or
        (source.type != bpa.CLM_BPA_TYPE_STRING and source.block_size != metadata.value_size))
        return bpa.CLM_BPA_FAILURE;
    const source_bytes = if (source.type == bpa.CLM_BPA_TYPE_STRING and source.data.len == 0)
        "\x00"
    else if (source.type == bpa.CLM_BPA_TYPE_STRING)
        source.data[0..@min(source.data.len, @as(usize, source.length) + 1)]
    else
        source.data[0..source.block_size];
    return clm_dictionary_set_from(id, key, source_bytes);
}

/// Retrieves a value into caller-owned memory.
pub fn clm_dictionary_get_to(id: clm_id, key: clm_id, buffer: []u8) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    var slots_array: ?*bpa.clm_bpaa_array_node = null;
    var slots_opt: ?[]clm_dictionary_slot = null;
    var slot_index: clm_uint = 0;
    var found = false;
    if (lookup(id, key, &metadata, &slots_array, &slots_opt, &slot_index, &found) != bpa.CLM_BPA_SUCCESS or !found)
        return bpa.CLM_BPA_FAILURE;
    const values = getArray(metadata.values_id, metadata.value_type, metadata.value_size) orelse return bpa.CLM_BPA_FAILURE;
    const slots = slots_opt orelse return bpa.CLM_BPA_FAILURE;
    if (slots[slot_index].value_index >= values.count) return bpa.CLM_BPA_FAILURE;
    const status = if (metadata.value_type == bpa.CLM_BPA_TYPE_STRING)
        bpa.clm_bpaa_get_string_raw(metadata.values_id, @intCast(slots[slot_index].value_index), buffer)
    else
        bpa.clm_bpaa_get_raw(metadata.values_id, @intCast(slots[slot_index].value_index), buffer);
    return if (status < 0) bpa.CLM_BPA_FAILURE else bpa.CLM_BPA_GET_SUCCESS;
}

/// Retrieves a value into a compatible, pre-created BPA block.
pub fn clm_dictionary_get(id: clm_id, key: clm_id, destination_block_id: clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    var slots_array: ?*bpa.clm_bpaa_array_node = null;
    var slots_opt: ?[]clm_dictionary_slot = null;
    var slot_index: clm_uint = 0;
    var found = false;
    const destination = bpa.clm_bpa_find_block(destination_block_id) orelse return bpa.CLM_BPA_FAILURE;
    if (lookup(id, key, &metadata, &slots_array, &slots_opt, &slot_index, &found) != bpa.CLM_BPA_SUCCESS or
        !found or destination.type != metadata.value_type or
        (metadata.value_type != bpa.CLM_BPA_TYPE_STRING and destination.block_size != metadata.value_size))
        return bpa.CLM_BPA_FAILURE;
    const values = getArray(metadata.values_id, metadata.value_type, metadata.value_size) orelse return bpa.CLM_BPA_FAILURE;
    const slots = slots_opt orelse return bpa.CLM_BPA_FAILURE;
    if (slots[slot_index].value_index >= values.count) return bpa.CLM_BPA_FAILURE;
    const status = if (metadata.value_type == bpa.CLM_BPA_TYPE_STRING)
        bpa.clm_bpaa_get_string(metadata.values_id, @intCast(slots[slot_index].value_index), destination_block_id)
    else
        bpa.clm_bpaa_get(metadata.values_id, @intCast(slots[slot_index].value_index), destination_block_id, metadata.value_size);
    return if (metadata.value_type == bpa.CLM_BPA_TYPE_STRING)
        (if (status < 0) bpa.CLM_BPA_FAILURE else bpa.CLM_BPA_GET_SUCCESS)
    else
        (if (status == bpa.CLM_BPA_GET_SUCCESS) status else bpa.CLM_BPA_FAILURE);
}

/// Checks whether a key currently exists.
pub fn clm_dictionary_contains(id: clm_id, key: clm_id) bool {
    var metadata: clm_dictionary_metadata = undefined;
    var slots_array: ?*bpa.clm_bpaa_array_node = null;
    var slots_opt: ?[]clm_dictionary_slot = null;
    var slot_index: clm_uint = 0;
    var found = false;
    return lookup(id, key, &metadata, &slots_array, &slots_opt, &slot_index, &found) == bpa.CLM_BPA_SUCCESS and found;
}

/// Returns the number of entries, or CLM_BPA_FAILURE if invalid.
pub fn clm_dictionary_count(id: clm_id) clm_int {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or metadata.count > std.math.maxInt(clm_int))
        return bpa.CLM_BPA_FAILURE;
    return @intCast(metadata.count);
}

/// Removes a key and its value, preserving all other mappings.
pub fn clm_dictionary_remove(id: clm_id, key: clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    var slots_array: ?*bpa.clm_bpaa_array_node = null;
    var slots_opt: ?[]clm_dictionary_slot = null;
    var slot_index: clm_uint = 0;
    var found = false;
    if (lookup(id, key, &metadata, &slots_array, &slots_opt, &slot_index, &found) != bpa.CLM_BPA_SUCCESS or !found)
        return bpa.CLM_BPA_FAILURE;
    const values = getArray(metadata.values_id, metadata.value_type, metadata.value_size) orelse return bpa.CLM_BPA_FAILURE;
    const slots = slots_opt orelse return bpa.CLM_BPA_FAILURE;
    const array = slots_array orelse return bpa.CLM_BPA_FAILURE;
    const removed_index = slots[slot_index].value_index;
    if (removed_index >= values.count or removed_index > std.math.maxInt(clm_int) or
        bpa.clm_bpaa_remove(metadata.values_id, @intCast(removed_index)) != bpa.CLM_BPA_SUCCESS or
        bpa.clm_bpaa_remove(metadata.keys_id, @intCast(removed_index)) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    slots[slot_index].state = CLM_DICTIONARY_SLOT_DELETED;
    metadata.count -= 1;
    metadata.iterator = 0;
    for (slots[0..array.capacity]) |*slot| {
        if (slot.state == CLM_DICTIONARY_SLOT_OCCUPIED and slot.value_index > removed_index)
            slot.value_index -= 1;
    }
    return writeMetadata(id, &metadata);
}

/// Resets ordered key iteration to the first currently stored key.
pub fn clm_dictionary_iterate_reset(id: clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    metadata.iterator = 0;
    return writeMetadata(id, &metadata);
}

/// Returns the next key in insertion order.
pub fn clm_dictionary_iterate_next(id: clm_id, key: *clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    const keys = getArray(metadata.keys_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_id)) orelse return bpa.CLM_BPA_FAILURE;
    if (keys.count != metadata.count or metadata.iterator >= metadata.count)
        return bpa.CLM_BPA_FAILURE;
    var key_bytes: [@sizeOf(clm_id)]u8 = undefined;
    const status = bpa.clm_bpaa_get_raw(metadata.keys_id, @intCast(metadata.iterator), &key_bytes);
    if (status != bpa.CLM_BPA_GET_SUCCESS) return bpa.CLM_BPA_FAILURE;
    key.* = std.mem.bytesToValue(clm_id, &key_bytes);
    metadata.iterator += 1;
    if (writeMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    return bpa.CLM_BPA_GET_SUCCESS;
}

/// Removes every entry while retaining the dictionary and its reserved capacity.
pub fn clm_dictionary_clear(id: clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS) return bpa.CLM_BPA_FAILURE;
    const slots_array = getArray(metadata.slots_id, bpa.CLM_BPA_TYPE_STRUCT, @sizeOf(clm_dictionary_slot)) orelse return bpa.CLM_BPA_FAILURE;
    const slots = slotSlice(slots_array) orelse return bpa.CLM_BPA_FAILURE;
    if (bpa.clm_bpaa_clear(metadata.values_id) != bpa.CLM_BPA_SUCCESS or
        bpa.clm_bpaa_clear(metadata.keys_id) != bpa.CLM_BPA_SUCCESS)
        return bpa.CLM_BPA_FAILURE;
    @memset(slots, clm_dictionary_slot{});
    metadata.count = 0;
    metadata.iterator = 0;
    return writeMetadata(id, &metadata);
}

/// Inserts a new key/value pair, failing if the key already exists.
pub fn clm_dictionary_add_from(id: clm_id, key: clm_id, value: []const u8) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or clm_dictionary_contains(id, key))
        return bpa.CLM_BPA_FAILURE;
    return clm_dictionary_set_from(id, key, value);
}

/// Replaces the value for an existing key without changing its insertion order.
pub fn clm_dictionary_update_from(id: clm_id, key: clm_id, value: []const u8) i32 {
    if (!clm_dictionary_contains(id, key)) return bpa.CLM_BPA_FAILURE;
    return clm_dictionary_set_from(id, key, value);
}

/// Copies all keys to caller-owned memory in insertion order.
pub fn clm_dictionary_keys_to(id: clm_id, buffer: []clm_id) i32 {
    var metadata: clm_dictionary_metadata = undefined;
    if (readMetadata(id, &metadata) != bpa.CLM_BPA_SUCCESS or buffer.len < metadata.count)
        return bpa.CLM_BPA_FAILURE;
    for (0..metadata.count) |index| {
        const key_bytes = std.mem.asBytes(&buffer[index]);
        if (bpa.clm_bpaa_get_raw(metadata.keys_id, @intCast(index), key_bytes) != bpa.CLM_BPA_GET_SUCCESS)
            return bpa.CLM_BPA_FAILURE;
    }
    return bpa.CLM_BPA_GET_SUCCESS;
}
