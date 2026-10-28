//! Pooled block allocation (BPA) and block arrays (BPAA).
//!
//! Blocks and arrays are process-global resources identified by `clm_id`.
//! All stored values are independent copies. Borrowed node/data access remains
//! valid only until the corresponding resource is resized, replaced, deleted,
//! or the BPA subsystem is shut down.
//! Threading contract: BPA is externally synchronized. Concurrent callers must
//! hold clm_bpa_lock()/clm_bpa_unlock() across ALL BPA operations, including
//! the complete lifetime of raw borrowed pointers. The lock is non-reentrant.
//! Do not call the lock again from within a protected operation.

const std = @import("std");
const base = @import("clm_base");

pub const clm_bpa_status = i32;
pub const CLM_BPA_FAILURE: i32 = -1;
pub const CLM_BPA_SUCCESS: i32 = 0;
pub const CLM_BPA_GET_SUCCESS: i32 = 1;
pub const CLM_BPA_SET_SUCCESS: i32 = 2;

pub const CLM_BPA_TYPE_STRUCT: u8 = 0;
pub const CLM_BPA_TYPE_SIGNED_BYTE: u8 = 1;
pub const CLM_BPA_TYPE_BYTE: u8 = 2;
pub const CLM_BPA_TYPE_SHORT: u8 = 3;
pub const CLM_BPA_TYPE_UNSIGNED_SHORT: u8 = 4;
pub const CLM_BPA_TYPE_INT: u8 = 5;
pub const CLM_BPA_TYPE_UNSIGNED_INT: u8 = 6;
pub const CLM_BPA_TYPE_LONG: u8 = 7;
pub const CLM_BPA_TYPE_UNSIGNED_LONG: u8 = 8;
pub const CLM_BPA_TYPE_FLOAT: u8 = 9;
pub const CLM_BPA_TYPE_DOUBLE: u8 = 10;
pub const CLM_BPA_TYPE_STRING: u8 = 11;
pub const CLM_BPA_TYPE_UUID: u8 = 12;
pub const CLM_BPA_TYPE_DATETIME: u8 = 13;

pub const CLM_BPA_INITIAL_CAPACITY: usize = 16;
pub const CLM_BPA_POOL_SLAB_SLOTS: usize = 32;
pub const CLM_BPA_POOL_MAX_SLOT: usize = 4096;
pub const CLM_BPA_POOL_CLASSES: usize = 10;

const Allocator = std.mem.Allocator;
const allocator = std.heap.page_allocator;

const PoolSlot = struct {
    memory: []u8,
    free_next: ?*PoolSlot = null,
    all_next: ?*PoolSlot = null,
    in_use: bool = false,
};

const PoolSlab = struct {
    memory: []u8,
    next: ?*PoolSlab = null,
};

const PoolClass = struct {
    slot_size: usize = 0,
    active: usize = 0,
    capacity: usize = 0,
    free: ?*PoolSlot = null,
    all: ?*PoolSlot = null,
    slabs: ?*PoolSlab = null,
    by_address: std.AutoHashMapUnmanaged(usize, *PoolSlot) = .{},
};

pub const clm_bpa_block_node = struct {
    id: base.clm_id,
    type: u8,
    refcount: u16 = 1,
    block_size: u32,
    encoding: u8 = 0,
    length: u32 = 0,
    data: []u8 = &.{},
    registry_next: ?*clm_bpa_block_node = null,
};

pub const clm_bpaa_array_items = union(enum) {
    blocks: []u8,
    strings: [][]u8,
};

pub const clm_bpaa_array_node = struct {
    id: base.clm_id,
    type: u8,
    refcount: u16 = 1,
    count: u32 = 0,
    capacity: u32 = 0,
    block_size: u32,
    encoding: u8 = 0,
    index: u32 = 0,
    sealed: u8 = 0,
    deck_size: u32 = 0,
    items: clm_bpaa_array_items,
    registry_next: ?*clm_bpaa_array_node = null,
};

pub var clm_bpa_global_block_tombstone: clm_bpa_block_node = .{
    .id = 0,
    .type = CLM_BPA_TYPE_STRUCT,
    .block_size = 0,
};
pub const CLM_BPA_BLOCK_TOMBSTONE: *clm_bpa_block_node = &clm_bpa_global_block_tombstone;

pub var clm_bpa_global_array_tombstone: clm_bpaa_array_node = .{
    .id = 0,
    .type = CLM_BPA_TYPE_STRUCT,
    .block_size = 0,
    .items = .{ .blocks = &.{} },
};
pub const CLM_BPA_ARRAY_TOMBSTONE: *clm_bpaa_array_node = &clm_bpa_global_array_tombstone;

pub const clm_bpaa_compare_fn = *const fn ([]const u8, []const u8) i32;
pub const clm_bpaa_predicate_fn = *const fn ([]const u8, []const u8) bool;

var pools: [CLM_BPA_POOL_CLASSES]PoolClass = @splat(.{});
var blocks_head: ?*clm_bpa_block_node = null;
var arrays_head: ?*clm_bpaa_array_node = null;
var blocks_index: std.AutoHashMapUnmanaged(base.clm_id, *clm_bpa_block_node) = .{};
var arrays_index: std.AutoHashMapUnmanaged(base.clm_id, *clm_bpaa_array_node) = .{};
var bpa_mutex: std.atomic.Mutex = .unlocked;

/// Acquires the BPA-wide external synchronization lock. This lock is not
/// recursive: caller must release it once and must not acquire it twice.
/// All threads accessing BPA must follow this policy, including borrowers.
pub fn clm_bpa_lock() void {
    while (!bpa_mutex.tryLock()) std.atomic.spinLoopHint();
}

/// Releases the BPA-wide external synchronization lock.
pub fn clm_bpa_unlock() void {
    bpa_mutex.unlock();
}

pub var clm_bpa_global_blocks_count: usize = 0;
pub var clm_bpa_global_arrays_count: usize = 0;
pub var clm_bpa_global_blocks_capacity: usize = 0;
pub var clm_bpa_global_arrays_capacity: usize = 0;
pub var clm_bpa_global_blocks_tombstones: usize = 0;
pub var clm_bpa_global_arrays_tombstones: usize = 0;

fn poolClassFor(bytes: usize) usize {
    if (bytes == 0 or bytes > CLM_BPA_POOL_MAX_SLOT) return CLM_BPA_POOL_CLASSES;
    var size: usize = @max(@sizeOf(usize), 8);
    var index: usize = 0;
    while (size < bytes and index + 1 < CLM_BPA_POOL_CLASSES) : (index += 1) size *= 2;
    return if (size >= bytes) index else CLM_BPA_POOL_CLASSES;
}

/// Removes a block node from the registry and releases its owned payload.
pub fn clm_bpa_remove_block(id: base.clm_id) i32 {
    return clm_bpa_delete(id);
}

/// Selects the smallest pool class capable of holding `bytes`.
pub fn clm_bpa_pool_class_for(bytes: usize) usize {
    return poolClassFor(bytes);
}

/// Removes an array node from the registry and releases its owned payload.
pub fn clm_bpa_remove_array(id: base.clm_id) i32 {
    return clm_bpaa_delete(id);
}

fn addPoolSlab(pool: *PoolClass) ?*PoolSlot {
    const bytes = std.math.mul(usize, pool.slot_size, CLM_BPA_POOL_SLAB_SLOTS) catch return null;
    const slab = allocator.create(PoolSlab) catch return null;
    slab.memory = allocator.alloc(u8, bytes) catch {
        allocator.destroy(slab);
        return null;
    };
    @memset(slab.memory, 0);
    slab.next = pool.slabs;
    pool.slabs = slab;

    var first: ?*PoolSlot = null;
    for (0..CLM_BPA_POOL_SLAB_SLOTS) |i| {
        const slot = allocator.create(PoolSlot) catch break;
        const start = i * pool.slot_size;
        slot.* = .{
            .memory = slab.memory[start .. start + pool.slot_size],
            .free_next = first,
            .all_next = pool.all,
        };
        pool.by_address.put(allocator, @intFromPtr(slot.memory.ptr), slot) catch {
            allocator.destroy(slot);
            break;
        };
        pool.all = slot;
        first = slot;
        pool.capacity += 1;
    }
    return first;
}

/// Resizes a live array's reserved storage.
pub fn clm_bpaa_resize(array: *clm_bpaa_array_node, capacity: u32) i32 {
    if (capacity < array.count) return CLM_BPA_FAILURE;
    return resizeArray(array, capacity);
}

/// Ensures a live array has at least the requested capacity.
pub fn clm_bpaa_ensure_capacity(array: *clm_bpaa_array_node, required: u32) i32 {
    return ensureCapacity(array, required);
}

/// Allocates zeroed bytes from a reusable pool slot or a heap fallback.
pub fn clm_bpa_pool_alloc(bytes: usize) ?[]u8 {
    if (bytes == 0) return null;
    const index = poolClassFor(bytes);
    if (index == CLM_BPA_POOL_CLASSES) {
        const memory = allocator.alloc(u8, bytes) catch return null;
        @memset(memory, 0);
        return memory;
    }

    const pool = &pools[index];
    if (pool.slot_size == 0) {
        const minimum_slot: usize = @max(@sizeOf(usize), 8);
        pool.slot_size = minimum_slot << @as(u6, @intCast(index));
    }
    if (pool.free == null) {
        pool.free = addPoolSlab(pool);
        if (pool.free == null) return null;
    }

    const slot = pool.free.?;
    pool.free = slot.free_next;
    slot.in_use = true;
    pool.active += 1;
    @memset(slot.memory[0..bytes], 0);
    return slot.memory[0..bytes];
}

/// Returns a pooled or heap-fallback allocation.
pub fn clm_bpa_pool_free(memory: []u8, bytes: usize) void {
    if (memory.len == 0) return;
    const index = poolClassFor(bytes);
    if (index == CLM_BPA_POOL_CLASSES) {
        allocator.free(memory);
        return;
    }
    // Address index avoids the old linear scan of every slot in the class.
    // Unknown/mismatched pointers are programmer errors, NOT heap allocations:
    // freeing a sub-slice of a slab through page_allocator corrupts memory.
    const slot = pools[index].by_address.get(@intFromPtr(memory.ptr)) orelse return;
    if (!slot.in_use or bytes > slot.memory.len) return;
    slot.in_use = false;
    slot.free_next = pools[index].free;
    pools[index].free = slot;
    if (pools[index].active > 0) pools[index].active -= 1;
}

/// Releases all pool slots, but refuses to invalidate checked-out allocations.
pub fn clm_bpa_pool_cleanup() i32 {
    for (pools) |pool| {
        if (pool.active != 0) return CLM_BPA_FAILURE;
    }
    for (&pools) |*pool| {
        var slot = pool.all;
        while (slot) |current| {
            const next = current.all_next;
            allocator.destroy(current);
            slot = next;
        }
        var slab = pool.slabs;
        while (slab) |current| {
            const next = current.next;
            allocator.free(current.memory);
            allocator.destroy(current);
            slab = next;
        }
        pool.by_address.deinit(allocator);
        pool.* = .{};
    }
    return CLM_BPA_SUCCESS;
}

/// Reports active slots and reserved capacity for a pool class.
pub fn clm_bpa_pool_stats(bytes: usize, active: *usize, capacity: *usize) i32 {
    const index = poolClassFor(bytes);
    if (index == CLM_BPA_POOL_CLASSES) return CLM_BPA_FAILURE;
    active.* = pools[index].active;
    capacity.* = pools[index].capacity;
    return CLM_BPA_SUCCESS;
}

/// Mixes an identifier into the hash used by BPA registries.
pub fn clm_bpa_hash_id(value: base.clm_id) usize {
    var id = value;
    id ^= id >> 30;
    id *%= 0xbf58476d1ce4e5b9;
    id ^= id >> 27;
    id *%= 0x94d049bb133111eb;
    id ^= id >> 31;
    return @truncate(id);
}

/// Returns the size of a built-in fixed-width block type, or zero if variable.
pub fn clm_bpa_type_size(kind: u8) u32 {
    return switch (kind) {
        CLM_BPA_TYPE_SIGNED_BYTE, CLM_BPA_TYPE_BYTE => 1,
        CLM_BPA_TYPE_SHORT, CLM_BPA_TYPE_UNSIGNED_SHORT => 2,
        CLM_BPA_TYPE_INT, CLM_BPA_TYPE_UNSIGNED_INT, CLM_BPA_TYPE_FLOAT => 4,
        CLM_BPA_TYPE_LONG, CLM_BPA_TYPE_UNSIGNED_LONG, CLM_BPA_TYPE_DOUBLE, CLM_BPA_TYPE_DATETIME => 8,
        CLM_BPA_TYPE_UUID => 16,
        else => 0,
    };
}

/// Confirms block registry availability; the Zig registry grows dynamically.
pub fn clm_bpa_prepare_blocks() i32 {
    const result = prepareRegistry(&clm_bpa_global_blocks_capacity, clm_bpa_global_blocks_count, clm_bpa_global_blocks_tombstones);
    if (result != CLM_BPA_SUCCESS) return result;
    if (clm_bpa_global_blocks_capacity > std.math.maxInt(u32)) return CLM_BPA_FAILURE;
    blocks_index.ensureTotalCapacity(allocator, @intCast(clm_bpa_global_blocks_capacity)) catch return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

/// Confirms array registry availability; the Zig registry grows dynamically.
pub fn clm_bpa_prepare_arrays() i32 {
    const result = prepareRegistry(&clm_bpa_global_arrays_capacity, clm_bpa_global_arrays_count, clm_bpa_global_arrays_tombstones);
    if (result != CLM_BPA_SUCCESS) return result;
    if (clm_bpa_global_arrays_capacity > std.math.maxInt(u32)) return CLM_BPA_FAILURE;
    arrays_index.ensureTotalCapacity(allocator, @intCast(clm_bpa_global_arrays_capacity)) catch return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

fn prepareRegistry(capacity: *usize, count: usize, tombstones: usize) i32 {
    if (capacity.* == 0) {
        capacity.* = CLM_BPA_INITIAL_CAPACITY;
    } else {
        const occupied = std.math.add(usize, count, tombstones) catch return CLM_BPA_FAILURE;
        const required = std.math.add(usize, occupied, 1) catch return CLM_BPA_FAILURE;
        if (required < capacity.* - capacity.* / 4) return CLM_BPA_SUCCESS;
        if (count < capacity.* / 4 and tombstones != 0) return CLM_BPA_SUCCESS;
        if (capacity.* > std.math.maxInt(usize) / 2) return CLM_BPA_FAILURE;
        capacity.* *= 2;
    }
    return CLM_BPA_SUCCESS;
}

/// Rebuilds reserved index capacity for blocks (existing entries remain valid).
pub fn clm_bpa_rehash_blocks(capacity: usize) i32 {
    if (capacity == 0 or !std.math.isPowerOfTwo(capacity) or capacity < clm_bpa_global_blocks_count)
        return CLM_BPA_FAILURE;
    if (capacity > std.math.maxInt(u32)) return CLM_BPA_FAILURE;
    blocks_index.ensureTotalCapacity(allocator, @intCast(capacity)) catch return CLM_BPA_FAILURE;
    clm_bpa_global_blocks_capacity = capacity;
    clm_bpa_global_blocks_tombstones = 0;
    return CLM_BPA_SUCCESS;
}

/// Rebuilds reserved index capacity for arrays (existing entries remain valid).
pub fn clm_bpa_rehash_arrays(capacity: usize) i32 {
    if (capacity == 0 or !std.math.isPowerOfTwo(capacity) or capacity < clm_bpa_global_arrays_count)
        return CLM_BPA_FAILURE;
    if (capacity > std.math.maxInt(u32)) return CLM_BPA_FAILURE;
    arrays_index.ensureTotalCapacity(allocator, @intCast(capacity)) catch return CLM_BPA_FAILURE;
    clm_bpa_global_arrays_capacity = capacity;
    clm_bpa_global_arrays_tombstones = 0;
    return CLM_BPA_SUCCESS;
}

/// Expected O(1) indexed lookup; pointers are invalidated by delete/shutdown.
pub fn clm_bpa_find_block(id: base.clm_id) ?*clm_bpa_block_node {
    return blocks_index.get(id);
}

/// Expected O(1) indexed lookup; pointers are invalidated by delete/shutdown.
pub fn clm_bpaa_find_array(id: base.clm_id) ?*clm_bpaa_array_node {
    return arrays_index.get(id);
}

/// Adds a caller-initialized block node to the registry.
pub fn clm_bpa_insert_block(block: *clm_bpa_block_node) void {
    if (clm_bpa_is_existing(block.id) or clm_bpaa_is_existing(block.id)) return;
    if (clm_bpa_prepare_blocks() != CLM_BPA_SUCCESS) return;
    blocks_index.put(allocator, block.id, block) catch return;
    block.registry_next = blocks_head;
    blocks_head = block;
    clm_bpa_global_blocks_count += 1;
}

/// Adds a caller-initialized array node to the registry.
pub fn clm_bpa_insert_array(array: *clm_bpaa_array_node) void {
    if (clm_bpaa_is_existing(array.id) or clm_bpa_is_existing(array.id)) return;
    if (clm_bpa_prepare_arrays() != CLM_BPA_SUCCESS) return;
    arrays_index.put(allocator, array.id, array) catch return;
    array.registry_next = arrays_head;
    arrays_head = array;
    clm_bpa_global_arrays_count += 1;
}

/// Copies bounded string data into pooled storage with a trailing NUL.
pub fn clm_bpa_copy_string(value: []const u8) ?[]u8 {
    return copyString(value);
}

/// Returns whether a block with the ID exists.
pub fn clm_bpa_is_existing(id: base.clm_id) bool {
    return clm_bpa_find_block(id) != null;
}

/// Returns whether an array with the ID exists.
pub fn clm_bpaa_is_existing(id: base.clm_id) bool {
    return clm_bpaa_find_array(id) != null;
}

fn validBlockType(kind: u8, block_size: u32) ?u32 {
    if (kind == CLM_BPA_TYPE_STRING) return if (block_size == 0) 0 else null;
    if (kind == CLM_BPA_TYPE_STRUCT) return if (block_size != 0) block_size else null;
    const resolved = clm_bpa_type_size(kind);
    if (resolved == 0 or (block_size != 0 and block_size != resolved)) return null;
    return resolved;
}

/// Creates a block, optionally providing a custom STRUCT byte width.
pub fn clm_bpa_create_sized(id: base.clm_id, kind: u8, block_size: u32) i32 {
    if (clm_bpa_is_existing(id) or clm_bpaa_is_existing(id) or clm_bpa_prepare_blocks() != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    const size = validBlockType(kind, block_size) orelse return CLM_BPA_FAILURE;
    const node = allocator.create(clm_bpa_block_node) catch return CLM_BPA_FAILURE;
    errdefer allocator.destroy(node);
    var data: []u8 = &.{};
    if (kind != CLM_BPA_TYPE_STRING) {
        data = clm_bpa_pool_alloc(size) orelse return CLM_BPA_FAILURE;
    }
    node.* = .{
        .id = id,
        .type = kind,
        .block_size = size,
        .data = data,
        .registry_next = blocks_head,
    };
    blocks_index.put(allocator, id, node) catch {
        if (data.len != 0) clm_bpa_pool_free(data, data.len);
        allocator.destroy(node);
        return CLM_BPA_FAILURE;
    };
    blocks_head = node;
    clm_bpa_global_blocks_count += 1;
    return CLM_BPA_SUCCESS;
}

/// Creates a block of a built-in or string type.
pub fn clm_bpa_create(id: base.clm_id, kind: u8) i32 {
    if (kind == CLM_BPA_TYPE_STRUCT) return CLM_BPA_FAILURE;
    return clm_bpa_create_sized(id, kind, 0);
}

fn unlinkBlock(id: base.clm_id) ?*clm_bpa_block_node {
    var previous: ?*clm_bpa_block_node = null;
    var current = blocks_head;
    while (current) |block| {
        if (block.id == id) {
            if (previous) |prev| {
                prev.registry_next = block.registry_next;
            } else {
                blocks_head = block.registry_next;
            }
            block.registry_next = null;
            _ = blocks_index.remove(id);
            clm_bpa_global_blocks_count -= 1;
            return block;
        }
        previous = block;
        current = block.registry_next;
    }
    return null;
}

fn freeBlock(block: *clm_bpa_block_node) void {
    if (block.type == CLM_BPA_TYPE_STRING) {
        if (block.data.len != 0) clm_bpa_pool_free(block.data, block.data.len);
    } else {
        clm_bpa_pool_free(block.data, block.block_size);
    }
    allocator.destroy(block);
}

/// Immediately deletes a block irrespective of its reference count.
pub fn clm_bpa_delete(id: base.clm_id) i32 {
    const block = unlinkBlock(id) orelse return CLM_BPA_FAILURE;
    freeBlock(block);
    return CLM_BPA_SUCCESS;
}

/// Adds a reference to a block.
pub fn clm_bpa_ref(id: base.clm_id) i32 {
    const block = clm_bpa_find_block(id) orelse return CLM_BPA_FAILURE;
    if (block.refcount == std.math.maxInt(u16)) return CLM_BPA_FAILURE;
    block.refcount += 1;
    return CLM_BPA_SUCCESS;
}

/// Releases a block reference, deleting the block when the count reaches zero.
pub fn clm_bpa_unref(id: base.clm_id) i32 {
    const block = clm_bpa_find_block(id) orelse return CLM_BPA_FAILURE;
    if (block.refcount == 0) return CLM_BPA_FAILURE;
    block.refcount -= 1;
    if (block.refcount == 0) return clm_bpa_delete(id);
    return CLM_BPA_SUCCESS;
}

fn copyString(value: []const u8) ?[]u8 {
    const end = std.mem.indexOfScalar(u8, value, 0) orelse value.len;
    const result = clm_bpa_pool_alloc(end + 1) orelse return null;
    @memcpy(result[0..end], value[0..end]);
    result[end] = 0;
    return result;
}

fn stringContents(value: []const u8) []const u8 {
    const end = std.mem.indexOfScalar(u8, value, 0) orelse value.len;
    return value[0..end];
}

/// Returns a borrowed string value for a string block, excluding its terminator.
pub fn clm_bpa_string(id: base.clm_id) ?[]const u8 {
    const block = clm_bpa_find_block(id) orelse return null;
    if (block.type != CLM_BPA_TYPE_STRING or block.data.len == 0) return null;
    return stringContents(block.data);
}

/// Copies a fixed-width value into caller storage.
pub fn clm_bpa_get(id: base.clm_id, output: []u8) i32 {
    const block = clm_bpa_find_block(id) orelse return CLM_BPA_FAILURE;
    if (block.type == CLM_BPA_TYPE_STRING or output.len < block.block_size) return CLM_BPA_FAILURE;
    @memcpy(output[0..block.block_size], block.data);
    return CLM_BPA_GET_SUCCESS;
}

/// Replaces a fixed-width block value.
pub fn clm_bpa_set(id: base.clm_id, value: []const u8) i32 {
    const block = clm_bpa_find_block(id) orelse return CLM_BPA_FAILURE;
    if (block.type == CLM_BPA_TYPE_STRING or value.len != block.block_size) return CLM_BPA_FAILURE;
    @memmove(block.data, value);
    return CLM_BPA_SET_SUCCESS;
}

/// Copies a NUL-terminated string block value to caller storage and returns its byte length.
pub fn clm_bpa_get_string(id: base.clm_id, output: []u8) i32 {
    const value = clm_bpa_string(id) orelse return CLM_BPA_FAILURE;
    if (value.len >= output.len or value.len > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    @memcpy(output[0..value.len], value);
    output[value.len] = 0;
    return @intCast(value.len);
}

/// Replaces a string block value with an independent copy and returns its byte length.
pub fn clm_bpa_set_string_raw(id: base.clm_id, value: []const u8) i32 {
    const block = clm_bpa_find_block(id) orelse return CLM_BPA_FAILURE;
    if (block.type != CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    const replacement = copyString(value) orelse return CLM_BPA_FAILURE;
    const text = stringContents(replacement);
    if (text.len > std.math.maxInt(i32) or text.len > std.math.maxInt(u32)) {
        clm_bpa_pool_free(replacement, replacement.len);
        return CLM_BPA_FAILURE;
    }
    if (block.data.len != 0) clm_bpa_pool_free(block.data, block.data.len);
    block.data = replacement;
    block.length = @intCast(text.len);
    return @intCast(text.len);
}

/// Alias for the slice-based string setter.
pub fn clm_bpa_set_string(id: base.clm_id, value: []const u8) i32 {
    return clm_bpa_set_string_raw(id, value);
}

/// Gets fixed-width block bytes through an explicit external-memory direction alias.
pub fn clm_bpa_get_to(id: base.clm_id, destination: []u8) i32 {
    return clm_bpa_get(id, destination);
}

/// Sets fixed-width block bytes through an explicit external-memory direction alias.
pub fn clm_bpa_set_from(id: base.clm_id, source: []const u8) i32 {
    return clm_bpa_set(id, source);
}

/// Gets a string block through an explicit external-memory direction alias.
pub fn clm_bpa_get_string_to(id: base.clm_id, destination: []u8) i32 {
    return clm_bpa_get_string(id, destination);
}

/// Sets a string block through an explicit external-memory direction alias.
pub fn clm_bpa_set_string_from(id: base.clm_id, source: []const u8) i32 {
    return clm_bpa_set_string_raw(id, source);
}

/// Replaces a block using its native fixed-width or string semantics.
pub fn clm_bpa_replace(id: base.clm_id, value: []const u8) i32 {
    const block = clm_bpa_find_block(id) orelse return CLM_BPA_FAILURE;
    if (block.type == CLM_BPA_TYPE_STRING) return clm_bpa_set_string_raw(id, value);
    return clm_bpa_set(id, value);
}

/// Borrows a block payload and its byte length.
pub fn clm_bpa_borrow(id: base.clm_id) ?[]u8 {
    const block = clm_bpa_find_block(id) orelse return null;
    return block.data;
}

/// Copies one compatible block into another.
pub fn clm_bpa_copy_block(source_id: base.clm_id, destination_id: base.clm_id) i32 {
    const source = clm_bpa_find_block(source_id) orelse return CLM_BPA_FAILURE;
    const destination = clm_bpa_find_block(destination_id) orelse return CLM_BPA_FAILURE;
    if (source.type != destination.type) return CLM_BPA_FAILURE;
    if (source.type == CLM_BPA_TYPE_STRING) {
        if (source.data.len == 0) return CLM_BPA_FAILURE;
        const fresh = allocator.dupe(u8, source.data) catch return CLM_BPA_FAILURE;
        if (destination.data.len != 0) clm_bpa_pool_free(destination.data, destination.data.len);
        const pooled = clm_bpa_pool_alloc(fresh.len) orelse {
            allocator.free(fresh);
            return CLM_BPA_FAILURE;
        };
        @memcpy(pooled, fresh);
        allocator.free(fresh);
        destination.data = pooled;
        destination.length = source.length;
        return CLM_BPA_SUCCESS;
    }
    if (source.block_size != destination.block_size) return CLM_BPA_FAILURE;
    @memmove(destination.data, source.data);
    return CLM_BPA_SUCCESS;
}

fn arrayByteCount(capacity: u32, block_size: u32) ?usize {
    return std.math.mul(usize, capacity, block_size) catch null;
}

fn createArrayBacking(kind: u8, capacity: u32, block_size: u32) ?clm_bpaa_array_items {
    if (kind == CLM_BPA_TYPE_STRING) {
        const values = allocator.alloc([]u8, capacity) catch return null;
        for (values) |*value| value.* = &.{};
        return .{ .strings = values };
    }
    const size = arrayByteCount(capacity, block_size) orelse return null;
    const bytes = allocator.alloc(u8, size) catch return null;
    @memset(bytes, 0);
    return .{ .blocks = bytes };
}

/// Creates an array with optional custom STRUCT item width.
pub fn clm_bpaa_create_sized(
    id: base.clm_id,
    kind: u8,
    reserved: i32,
    block_size: u32,
    encoding: u8,
) i32 {
    if (clm_bpaa_is_existing(id) or clm_bpa_is_existing(id) or clm_bpa_prepare_arrays() != CLM_BPA_SUCCESS or reserved < 0) return CLM_BPA_FAILURE;
    const size = validBlockType(kind, block_size) orelse return CLM_BPA_FAILURE;
    const capacity: u32 = @intCast(reserved);
    const items = createArrayBacking(kind, capacity, size) orelse return CLM_BPA_FAILURE;
    const array = allocator.create(clm_bpaa_array_node) catch {
        freeArrayBacking(kind, items, 0);
        return CLM_BPA_FAILURE;
    };
    array.* = .{
        .id = id,
        .type = kind,
        .capacity = capacity,
        .block_size = size,
        .encoding = encoding,
        .items = items,
        .registry_next = arrays_head,
    };
    arrays_index.put(allocator, id, array) catch {
        freeArrayBacking(kind, items, 0);
        allocator.destroy(array);
        return CLM_BPA_FAILURE;
    };
    arrays_head = array;
    clm_bpa_global_arrays_count += 1;
    return CLM_BPA_SUCCESS;
}

/// Creates an array of a built-in or string type.
pub fn clm_bpaa_create(id: base.clm_id, kind: u8, reserved: i32, encoding: u8) i32 {
    if (kind == CLM_BPA_TYPE_STRUCT) return CLM_BPA_FAILURE;
    return clm_bpaa_create_sized(id, kind, reserved, 0, encoding);
}

fn freeArrayBacking(kind: u8, items: clm_bpaa_array_items, count: u32) void {
    if (kind == CLM_BPA_TYPE_STRING) {
        const strings = items.strings;
        for (strings[0..@min(count, @as(u32, @intCast(strings.len)))]) |text| {
            if (text.len != 0) clm_bpa_pool_free(text, text.len);
        }
        allocator.free(strings);
    } else {
        allocator.free(items.blocks);
    }
}

fn unlinkArray(id: base.clm_id) ?*clm_bpaa_array_node {
    var previous: ?*clm_bpaa_array_node = null;
    var current = arrays_head;
    while (current) |array| {
        if (array.id == id) {
            if (previous) |prev| {
                prev.registry_next = array.registry_next;
            } else {
                arrays_head = array.registry_next;
            }
            array.registry_next = null;
            _ = arrays_index.remove(id);
            clm_bpa_global_arrays_count -= 1;
            return array;
        }
        previous = array;
        current = array.registry_next;
    }
    return null;
}

/// Immediately deletes an array and its owned storage.
pub fn clm_bpaa_delete(id: base.clm_id) i32 {
    const array = unlinkArray(id) orelse return CLM_BPA_FAILURE;
    freeArrayBacking(array.type, array.items, array.count);
    allocator.destroy(array);
    return CLM_BPA_SUCCESS;
}

/// Adds a reference to an array.
pub fn clm_bpaa_ref(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.refcount == std.math.maxInt(u16)) return CLM_BPA_FAILURE;
    array.refcount += 1;
    return CLM_BPA_SUCCESS;
}

/// Releases an array reference and deletes it when the count reaches zero.
pub fn clm_bpaa_unref(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.refcount == 0) return CLM_BPA_FAILURE;
    array.refcount -= 1;
    if (array.refcount == 0) return clm_bpaa_delete(id);
    return CLM_BPA_SUCCESS;
}

fn resizeArray(array: *clm_bpaa_array_node, capacity: u32) i32 {
    if (array.type == CLM_BPA_TYPE_STRING) {
        const fresh = allocator.alloc([]u8, capacity) catch return CLM_BPA_FAILURE;
        for (fresh) |*item| item.* = &.{};
        const keep: usize = @intCast(@min(array.count, capacity));
        @memcpy(fresh[0..keep], array.items.strings[0..keep]);
        const old = array.items.strings;
        allocator.free(old);
        array.items = .{ .strings = fresh };
    } else {
        const bytes = arrayByteCount(capacity, array.block_size) orelse return CLM_BPA_FAILURE;
        const fresh = allocator.alloc(u8, bytes) catch return CLM_BPA_FAILURE;
        @memset(fresh, 0);
        const keep: usize = @as(usize, @intCast(@min(array.count, capacity))) * array.block_size;
        @memcpy(fresh[0..keep], array.items.blocks[0..keep]);
        allocator.free(array.items.blocks);
        array.items = .{ .blocks = fresh };
    }
    array.capacity = capacity;
    return CLM_BPA_SUCCESS;
}

fn ensureCapacity(array: *clm_bpaa_array_node, required: u32) i32 {
    if (required <= array.capacity) return CLM_BPA_SUCCESS;
    if (array.sealed != 0) return CLM_BPA_FAILURE;
    var capacity: u32 = @max(array.capacity, 1);
    while (capacity < required) {
        if (capacity > std.math.maxInt(u32) / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    return resizeArray(array, capacity);
}

/// Reserves additional item slots.
pub fn clm_bpaa_extend(id: base.clm_id, additional_capacity: u32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.sealed != 0 or additional_capacity == 0 or additional_capacity > std.math.maxInt(u32) - array.capacity)
        return CLM_BPA_FAILURE;
    return resizeArray(array, array.capacity + additional_capacity);
}

/// Reduces reserved slots without discarding live items.
pub fn clm_bpaa_shrink(id: base.clm_id, new_capacity: u32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.sealed != 0 or new_capacity >= array.capacity or new_capacity < array.count) return CLM_BPA_FAILURE;
    return resizeArray(array, new_capacity);
}

/// Creates an owned copy of a source array range.
pub fn clm_bpaa_slice(source_id: base.clm_id, start: i32, length: i32, destination_id: base.clm_id) i32 {
    const source = clm_bpaa_find_array(source_id) orelse return CLM_BPA_FAILURE;
    if (start < 0 or length < 0 or @as(u32, @intCast(start)) > source.count or
        @as(u32, @intCast(length)) > source.count - @as(u32, @intCast(start)) or clm_bpaa_is_existing(destination_id))
        return CLM_BPA_FAILURE;
    if (clm_bpaa_create_sized(destination_id, source.type, length, source.block_size, source.encoding) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    const destination = clm_bpaa_find_array(destination_id) orelse return CLM_BPA_FAILURE;
    const from: u32 = @intCast(start);
    const amount: u32 = @intCast(length);
    if (source.type == CLM_BPA_TYPE_STRING) {
        for (0..amount) |i| {
            const text = copyString(source.items.strings[from + i]) orelse {
                _ = clm_bpaa_delete(destination_id);
                return CLM_BPA_FAILURE;
            };
            destination.items.strings[i] = text;
            destination.count += 1;
        }
    } else if (amount > 0) {
        const byte_start: usize = @as(usize, from) * source.block_size;
        const byte_count: usize = @as(usize, amount) * source.block_size;
        @memcpy(destination.items.blocks[0..byte_count], source.items.blocks[byte_start .. byte_start + byte_count]);
        destination.count = amount;
    }
    destination.deck_size = destination.count;
    return CLM_BPA_SUCCESS;
}

/// Borrows a contiguous fixed-width range; null denotes an invalid request.
pub fn clm_bpaa_slice_borrow(source_id: base.clm_id, start: i32, length: i32) ?[]const u8 {
    const source = clm_bpaa_find_array(source_id) orelse return null;
    if (source.type == CLM_BPA_TYPE_STRING or start < 0 or length < 0) return null;
    const first: u32 = @intCast(start);
    const count: u32 = @intCast(length);
    if (first > source.count or count > source.count - first) return null;
    if (count == 0) return &.{};
    const offset: usize = @as(usize, first) * source.block_size;
    const size: usize = @as(usize, count) * source.block_size;
    return source.items.blocks[offset .. offset + size];
}

/// Copies a fixed-width range to caller storage.
pub fn clm_bpaa_slice_to(source_id: base.clm_id, start: i32, length: i32, output: []u8) i32 {
    const source = clm_bpaa_find_array(source_id) orelse return CLM_BPA_FAILURE;
    if (source.type == CLM_BPA_TYPE_STRING or start < 0 or length < 0) return CLM_BPA_FAILURE;
    const borrowed = clm_bpaa_slice_borrow(source_id, start, length) orelse return CLM_BPA_FAILURE;
    if (output.len < borrowed.len) return CLM_BPA_FAILURE;
    @memmove(output[0..borrowed.len], borrowed);
    return CLM_BPA_SUCCESS;
}

/// Copies an initialized fixed-width item.
pub fn clm_bpaa_get_raw(id: base.clm_id, index: i32, output: []u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or index < 0 or @as(u32, @intCast(index)) >= array.count or output.len < array.block_size)
        return CLM_BPA_FAILURE;
    const offset: usize = @as(usize, @intCast(index)) * array.block_size;
    @memcpy(output[0..array.block_size], array.items.blocks[offset .. offset + array.block_size]);
    return CLM_BPA_GET_SUCCESS;
}

/// Returns a borrowed array string, excluding its trailing NUL.
pub fn clm_bpaa_string(id: base.clm_id, index: i32) ?[]const u8 {
    const array = clm_bpaa_find_array(id) orelse return null;
    if (array.type != CLM_BPA_TYPE_STRING or index < 0 or @as(u32, @intCast(index)) >= array.count) return null;
    return stringContents(array.items.strings[@intCast(index)]);
}

/// Copies a string item and its terminating NUL to caller storage.
pub fn clm_bpaa_get_string_raw(id: base.clm_id, index: i32, output: []u8) i32 {
    const text = clm_bpaa_string(id, index) orelse return CLM_BPA_FAILURE;
    if (text.len >= output.len or text.len > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    @memcpy(output[0..text.len], text);
    output[text.len] = 0;
    return @intCast(text.len);
}

/// Replaces one fixed-width item.
pub fn clm_bpaa_set_raw(id: base.clm_id, index: i32, value: []const u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or index < 0 or @as(u32, @intCast(index)) >= array.count or value.len != array.block_size)
        return CLM_BPA_FAILURE;
    const offset: usize = @as(usize, @intCast(index)) * array.block_size;
    @memmove(array.items.blocks[offset .. offset + array.block_size], value);
    array.deck_size = array.count;
    return CLM_BPA_SET_SUCCESS;
}

/// Replaces one string item with an independent copy.
pub fn clm_bpaa_set_string_raw(id: base.clm_id, index: i32, value: []const u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type != CLM_BPA_TYPE_STRING or index < 0 or @as(u32, @intCast(index)) >= array.count)
        return CLM_BPA_FAILURE;
    const fresh = copyString(value) orelse return CLM_BPA_FAILURE;
    const slot = &array.items.strings[@intCast(index)];
    if (slot.len != 0) clm_bpa_pool_free(slot.*, slot.len);
    slot.* = fresh;
    array.deck_size = array.count;
    return CLM_BPA_SET_SUCCESS;
}

/// Inserts a copied item at an index.
pub fn clm_bpaa_insert_raw(id: base.clm_id, index: i32, value: []const u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (index < 0 or @as(u32, @intCast(index)) > array.count or array.count == std.math.maxInt(u32))
        return CLM_BPA_FAILURE;
    const at: u32 = @intCast(index);
    var text_copy: []u8 = &.{};
    if (array.type == CLM_BPA_TYPE_STRING) {
        text_copy = copyString(value) orelse return CLM_BPA_FAILURE;
    } else if (value.len != array.block_size) {
        return CLM_BPA_FAILURE;
    }
    const value_copy = if (array.type != CLM_BPA_TYPE_STRING)
        (allocator.dupe(u8, value) catch return CLM_BPA_FAILURE)
    else
        &.{};
    defer if (value_copy.len != 0) allocator.free(value_copy);
    if (ensureCapacity(array, array.count + 1) != CLM_BPA_SUCCESS) {
        if (text_copy.len != 0) clm_bpa_pool_free(text_copy, text_copy.len);
        return CLM_BPA_FAILURE;
    }
    if (array.type == CLM_BPA_TYPE_STRING) {
        var i = array.count;
        while (i > at) : (i -= 1) array.items.strings[i] = array.items.strings[i - 1];
        array.items.strings[at] = text_copy;
    } else {
        const offset: usize = @as(usize, at) * array.block_size;
        const tail_start: usize = @as(usize, array.count) * array.block_size;
        var i = tail_start;
        while (i > offset) : (i -= 1) array.items.blocks[i - 1 + array.block_size] = array.items.blocks[i - 1];
        @memcpy(array.items.blocks[offset .. offset + array.block_size], value_copy);
    }
    array.count += 1;
    array.index = at;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Appends a copied item.
pub fn clm_bpaa_append_raw(id: base.clm_id, value: []const u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.count > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(id, @intCast(array.count), value);
}

/// Removes an item and compacts subsequent items.
pub fn clm_bpaa_remove(id: base.clm_id, index: i32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (index < 0 or @as(u32, @intCast(index)) >= array.count) return CLM_BPA_FAILURE;
    const at: u32 = @intCast(index);
    if (array.type == CLM_BPA_TYPE_STRING) {
        const text = array.items.strings[at];
        if (text.len != 0) clm_bpa_pool_free(text, text.len);
        var i = at;
        while (i + 1 < array.count) : (i += 1) array.items.strings[i] = array.items.strings[i + 1];
        array.items.strings[array.count - 1] = &.{};
    } else {
        const offset: usize = @as(usize, at) * array.block_size;
        const end: usize = @as(usize, array.count) * array.block_size;
        var i = offset;
        while (i + array.block_size < end) : (i += 1) array.items.blocks[i] = array.items.blocks[i + array.block_size];
        @memset(array.items.blocks[end - array.block_size .. end], 0);
    }
    array.count -= 1;
    if (array.count == 0) {
        array.index = 0;
    } else if (array.index > at) {
        array.index -= 1;
    } else if (array.index >= array.count) {
        array.index = array.count - 1;
    }
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Removes the first item.
pub fn clm_bpaa_remove_first(id: base.clm_id) i32 {
    return clm_bpaa_remove(id, 0);
}

/// Removes the last item.
pub fn clm_bpaa_remove_last(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.count == 0 or array.count - 1 > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_remove(id, @intCast(array.count - 1));
}

/// Removes all items and resets cursor/deck state.
pub fn clm_bpaa_clear(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING) {
        for (array.items.strings[0..array.count]) |*text| {
            if (text.len != 0) clm_bpa_pool_free(text.*, text.len);
            text.* = &.{};
        }
    } else {
        @memset(array.items.blocks, 0);
    }
    array.count = 0;
    array.index = 0;
    array.deck_size = 0;
    return CLM_BPA_SUCCESS;
}

/// Reads array count into a caller-provided integer.
pub fn clm_bpaa_get_count_to(id: base.clm_id, output: *u32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    output.* = array.count;
    return CLM_BPA_GET_SUCCESS;
}

/// Reads reserved array capacity.
pub fn clm_bpaa_get_capacity_to(id: base.clm_id, output: *u32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    output.* = array.capacity;
    return CLM_BPA_GET_SUCCESS;
}

/// Reads the current initialized length.
pub fn clm_bpaa_get_length_to(id: base.clm_id, output: *u32) i32 {
    return clm_bpaa_get_count_to(id, output);
}

/// Reads the current item into caller storage.
pub fn clm_bpaa_get_current_raw(id: base.clm_id, output: []u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.index > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_get_raw(id, @intCast(array.index), output);
}

/// Copies the first fixed-width item.
pub fn clm_bpaa_get_first_raw(id: base.clm_id, output: []u8) i32 {
    return clm_bpaa_get_raw(id, 0, output);
}

/// Copies the final fixed-width item.
pub fn clm_bpaa_get_last_raw(id: base.clm_id, output: []u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.count == 0 or array.count - 1 > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_get_raw(id, @intCast(array.count - 1), output);
}

/// Borrows a fixed-width payload from a live array node.
pub fn clm_bpaa_advanced_borrow(id: base.clm_id, output: *?*clm_bpaa_array_node) i32 {
    output.* = null;
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.refcount == std.math.maxInt(u16)) return CLM_BPA_FAILURE;
    array.refcount += 1;
    output.* = array;
    return CLM_BPA_SUCCESS;
}

/// Releases an advanced array borrow.
pub fn clm_bpaa_advanced_release(output: *?*clm_bpaa_array_node) i32 {
    const array = output.* orelse return CLM_BPA_FAILURE;
    if (array.refcount == 0) return CLM_BPA_FAILURE;
    output.* = null;
    array.refcount -= 1;
    if (array.refcount == 0) return clm_bpaa_delete(array.id);
    return CLM_BPA_SUCCESS;
}

/// Returns the live raw data backing for a borrowed fixed-width array.
pub fn clm_bpaa_advanced_data(array: *clm_bpaa_array_node) ?[]u8 {
    if (array.type == CLM_BPA_TYPE_STRING) return null;
    return array.items.blocks;
}

/// Reads a fixed-width item without a global ID lookup.
pub fn clm_bpaa_advanced_get_raw(array: *clm_bpaa_array_node, index: i32, output: []u8) i32 {
    if (array.type == CLM_BPA_TYPE_STRING or index < 0 or @as(u32, @intCast(index)) >= array.count or output.len < array.block_size)
        return CLM_BPA_FAILURE;
    const offset: usize = @as(usize, @intCast(index)) * array.block_size;
    @memcpy(output[0..array.block_size], array.items.blocks[offset .. offset + array.block_size]);
    return CLM_BPA_GET_SUCCESS;
}

/// Replaces a fixed-width item without a global ID lookup.
pub fn clm_bpaa_advanced_set_raw(array: *clm_bpaa_array_node, index: i32, value: []const u8) i32 {
    if (array.type == CLM_BPA_TYPE_STRING or index < 0 or @as(u32, @intCast(index)) >= array.count or value.len != array.block_size)
        return CLM_BPA_FAILURE;
    const offset: usize = @as(usize, @intCast(index)) * array.block_size;
    @memmove(array.items.blocks[offset .. offset + array.block_size], value);
    array.deck_size = array.count;
    return CLM_BPA_SET_SUCCESS;
}

/// Inserts a fixed-width item into a borrowed array.
pub fn clm_bpaa_advanced_insert_raw(array: *clm_bpaa_array_node, index: i32, value: []const u8) i32 {
    if (array.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    return insertIntoArray(array, index, value);
}

fn insertIntoArray(array: *clm_bpaa_array_node, index: i32, value: []const u8) i32 {
    if (index < 0 or @as(u32, @intCast(index)) > array.count or value.len != array.block_size or array.count == std.math.maxInt(u32))
        return CLM_BPA_FAILURE;
    const at: u32 = @intCast(index);
    const snapshot = allocator.dupe(u8, value) catch return CLM_BPA_FAILURE;
    defer allocator.free(snapshot);
    if (ensureCapacity(array, array.count + 1) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    const end: usize = @as(usize, array.count) * array.block_size;
    const offset: usize = @as(usize, at) * array.block_size;
    var i = end;
    while (i > offset) : (i -= 1) array.items.blocks[i - 1 + array.block_size] = array.items.blocks[i - 1];
    @memcpy(array.items.blocks[offset .. offset + array.block_size], snapshot);
    array.count += 1;
    array.index = at;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Appends a fixed-width item to a borrowed array.
pub fn clm_bpaa_advanced_append_raw(array: *clm_bpaa_array_node, value: []const u8) i32 {
    if (array.count > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_advanced_insert_raw(array, @intCast(array.count), value);
}

/// Removes a fixed-width item from a borrowed array.
pub fn clm_bpaa_advanced_remove(array: *clm_bpaa_array_node, index: i32) i32 {
    if (array.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    return removeFromArray(array, index);
}

fn removeFromArray(array: *clm_bpaa_array_node, index: i32) i32 {
    if (index < 0 or @as(u32, @intCast(index)) >= array.count) return CLM_BPA_FAILURE;
    const at: u32 = @intCast(index);
    const offset: usize = @as(usize, at) * array.block_size;
    const end: usize = @as(usize, array.count) * array.block_size;
    var i = offset;
    while (i + array.block_size < end) : (i += 1) array.items.blocks[i] = array.items.blocks[i + array.block_size];
    @memset(array.items.blocks[end - array.block_size .. end], 0);
    array.count -= 1;
    if (array.count == 0) array.index = 0 else if (array.index > at) {
        array.index -= 1;
    } else if (array.index >= array.count) {
        array.index = array.count - 1;
    }
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Clears a borrowed fixed-width array.
pub fn clm_bpaa_advanced_clear(array: *clm_bpaa_array_node) i32 {
    if (array.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    @memset(array.items.blocks, 0);
    array.count = 0;
    array.index = 0;
    array.deck_size = 0;
    return CLM_BPA_SUCCESS;
}

/// Extends capacity for an unsealed borrowed array.
pub fn clm_bpaa_advanced_extend(array: *clm_bpaa_array_node, amount: u32) i32 {
    if (array.sealed != 0 or amount == 0 or amount > std.math.maxInt(u32) - array.capacity)
        return CLM_BPA_FAILURE;
    return resizeArray(array, array.capacity + amount);
}

/// Shrinks capacity for an unsealed borrowed array.
pub fn clm_bpaa_advanced_shrink(array: *clm_bpaa_array_node, capacity: u32) i32 {
    if (array.sealed != 0 or capacity >= array.capacity or capacity < array.count) return CLM_BPA_FAILURE;
    return resizeArray(array, capacity);
}

/// Resets the array cursor to the beginning.
pub fn clm_bpaa_reset(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    array.index = 0;
    return CLM_BPA_SUCCESS;
}

/// Moves the cursor to the first item.
pub fn clm_bpaa_move_first(id: base.clm_id) i32 {
    return clm_bpaa_reset(id);
}

/// Moves the cursor one item toward the beginning.
pub fn clm_bpaa_move_previous(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.index == 0) return CLM_BPA_FAILURE;
    array.index -= 1;
    return CLM_BPA_SUCCESS;
}

/// Moves the cursor one item toward the end.
pub fn clm_bpaa_move_next(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.index + 1 >= array.count) return CLM_BPA_FAILURE;
    array.index += 1;
    return CLM_BPA_SUCCESS;
}

/// Moves the cursor to the final item.
pub fn clm_bpaa_move_last(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.count == 0) return CLM_BPA_FAILURE;
    array.index = array.count - 1;
    return CLM_BPA_SUCCESS;
}

/// Moves the cursor to an initialized index.
pub fn clm_bpaa_move_to(id: base.clm_id, index: i32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (index < 0 or @as(u32, @intCast(index)) >= array.count) return CLM_BPA_FAILURE;
    array.index = @intCast(index);
    return CLM_BPA_SUCCESS;
}

/// Reads the current cursor position.
pub fn clm_bpaa_get_current_index_to(id: base.clm_id, output: *i32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.index > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    output.* = @intCast(array.index);
    return CLM_BPA_GET_SUCCESS;
}

/// Seals an array against capacity changes and automatic growth.
pub fn clm_bpaa_seal(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    array.sealed = 1;
    return CLM_BPA_SUCCESS;
}

/// Reads the array's sealed flag.
pub fn clm_bpaa_is_sealed_to(id: base.clm_id, output: *u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    output.* = array.sealed;
    return CLM_BPA_GET_SUCCESS;
}

/// Removes the capacity restriction.
pub fn clm_bpaa_unseal(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    array.sealed = 0;
    return CLM_BPA_SUCCESS;
}

fn itemAt(array: *clm_bpaa_array_node, index: u32) []const u8 {
    const start: usize = @as(usize, index) * array.block_size;
    return array.items.blocks[start .. start + array.block_size];
}

/// Sorts fixed-width items using a three-way comparator.
pub fn clm_bpaa_sort_with(id: base.clm_id, compare: ?clm_bpaa_compare_fn) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    const comparator = compare orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    var i: u32 = 1;
    while (i < array.count) : (i += 1) {
        var j = i;
        while (j > 0 and comparator(itemAt(array, j - 1), itemAt(array, j)) > 0) : (j -= 1) {
            _ = clm_bpaa_swap(id, @intCast(j - 1), @intCast(j));
        }
    }
    array.index = 0;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Sorts fixed-width items using a predicate that identifies out-of-order pairs.
pub fn clm_bpaa_sort(id: base.clm_id, predicate: ?clm_bpaa_predicate_fn) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    const is_out_of_order = predicate orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    if (array.count < 2) return CLM_BPA_SUCCESS;
    var pass: u32 = 0;
    while (pass + 1 < array.count) : (pass += 1) {
        var swapped = false;
        var j: u32 = 0;
        while (j + pass + 1 < array.count) : (j += 1) {
            if (is_out_of_order(itemAt(array, j), itemAt(array, j + 1))) {
                _ = clm_bpaa_swap(id, @intCast(j), @intCast(j + 1));
                swapped = true;
            }
        }
        if (!swapped) break;
    }
    array.index = 0;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Reverses a fixed-width array.
pub fn clm_bpaa_reverse(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    var left: u32 = 0;
    var right = array.count;
    while (left < right and left < right - 1) {
        right -= 1;
        _ = clm_bpaa_swap(id, @intCast(left), @intCast(right));
        left += 1;
    }
    array.index = 0;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Rotates a fixed-width suffix left or right.
pub fn clm_bpaa_rotate(array: *clm_bpaa_array_node, index: u32, positions: u32, right: bool) i32 {
    if (array.type == CLM_BPA_TYPE_STRING or index >= array.count) return CLM_BPA_FAILURE;
    const length = array.count - index;
    const shift = positions % length;
    if (shift == 0) return CLM_BPA_SUCCESS;
    const bytes_count: usize = @as(usize, shift) * array.block_size;
    const temp = allocator.alloc(u8, bytes_count) catch return CLM_BPA_FAILURE;
    defer allocator.free(temp);
    const start: usize = @as(usize, index) * array.block_size;
    const length_bytes: usize = @as(usize, length) * array.block_size;
    const shift_bytes: usize = @as(usize, shift) * array.block_size;
    if (right) {
        @memcpy(temp, array.items.blocks[start + length_bytes - shift_bytes .. start + length_bytes]);
        var i = length_bytes;
        while (i > shift_bytes) : (i -= 1) array.items.blocks[start + i - 1] = array.items.blocks[start + i - shift_bytes - 1];
        @memcpy(array.items.blocks[start .. start + shift_bytes], temp);
    } else {
        @memcpy(temp, array.items.blocks[start .. start + shift_bytes]);
        var i: usize = 0;
        while (i + shift_bytes < length_bytes) : (i += 1) array.items.blocks[start + i] = array.items.blocks[start + i + shift_bytes];
        @memcpy(array.items.blocks[start + length_bytes - shift_bytes .. start + length_bytes], temp);
    }
    array.index = 0;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Rotates the suffix left.
pub fn clm_bpaa_shift_left(id: base.clm_id, index: u32, positions: u32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    return clm_bpaa_rotate(array, index, positions, false);
}

/// Rotates the suffix right.
pub fn clm_bpaa_shift_right(id: base.clm_id, index: u32, positions: u32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    return clm_bpaa_rotate(array, index, positions, true);
}

/// Exchanges two fixed-width array elements.
pub fn clm_bpaa_swap(id: base.clm_id, index1: i32, index2: i32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or index1 < 0 or index2 < 0 or
        @as(u32, @intCast(index1)) >= array.count or @as(u32, @intCast(index2)) >= array.count)
        return CLM_BPA_FAILURE;
    if (index1 == index2) return CLM_BPA_SUCCESS;
    const temp = allocator.alloc(u8, array.block_size) catch return CLM_BPA_FAILURE;
    defer allocator.free(temp);
    const a: usize = @as(usize, @intCast(index1)) * array.block_size;
    const b: usize = @as(usize, @intCast(index2)) * array.block_size;
    @memcpy(temp, array.items.blocks[a .. a + array.block_size]);
    @memcpy(array.items.blocks[a .. a + array.block_size], array.items.blocks[b .. b + array.block_size]);
    @memcpy(array.items.blocks[b .. b + array.block_size], temp);
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

/// Copies one fixed-width item over another in the same array.
pub fn clm_bpaa_copy(id: base.clm_id, source_index: i32, destination_index: i32) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or source_index < 0 or destination_index < 0 or
        @as(u32, @intCast(source_index)) >= array.count or @as(u32, @intCast(destination_index)) >= array.count)
        return CLM_BPA_FAILURE;
    const src: usize = @as(usize, @intCast(source_index)) * array.block_size;
    const dst: usize = @as(usize, @intCast(destination_index)) * array.block_size;
    @memmove(array.items.blocks[dst .. dst + array.block_size], array.items.blocks[src .. src + array.block_size]);
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

var random_state: u64 = 0x9e3779b97f4a7c15;

fn nextRandom(bound: u32) u32 {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return @truncate(random_state % bound);
}

/// Shuffles fixed-width array items using an internal xorshift generator.
pub fn clm_bpaa_shuffle(id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or array.count > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    var index = array.count;
    while (index > 1) : (index -= 1) {
        _ = clm_bpaa_swap(id, @intCast(index - 1), @intCast(nextRandom(index)));
    }
    array.deck_size = array.count;
    array.index = 0;
    return CLM_BPA_SUCCESS;
}

/// Draws an indexed item from the active draw deck.
pub fn clm_bpaa_draw_at_raw(id: base.clm_id, index: i32, output: []u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or output.len < array.block_size or index < 0 or
        @as(u32, @intCast(index)) >= array.deck_size) return CLM_BPA_FAILURE;
    const remaining = array.deck_size - 1;
    const selected: u32 = @intCast(index);
    const from: usize = @as(usize, selected) * array.block_size;
    @memcpy(output[0..array.block_size], array.items.blocks[from .. from + array.block_size]);
    if (selected != remaining) _ = clm_bpaa_swap(id, @intCast(selected), @intCast(remaining));
    array.deck_size -= 1;
    return CLM_BPA_GET_SUCCESS;
}

/// Draws the next item, reshuffling when the deck is exhausted.
pub fn clm_bpaa_draw_raw(id: base.clm_id, output: []u8) i32 {
    const array = clm_bpaa_find_array(id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or output.len < array.block_size) return CLM_BPA_FAILURE;
    if (array.deck_size == 0) {
        if (array.count == 0 or clm_bpaa_shuffle(id) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
    }
    return clm_bpaa_draw_at_raw(id, 0, output);
}

/// Copies a compatible array element into a fixed-width block.
pub fn clm_bpaa_get(array_id: base.clm_id, index: i32, block_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    const block = clm_bpa_find_block(block_id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or block.type == CLM_BPA_TYPE_STRING or index < 0 or
        @as(u32, @intCast(index)) >= array.count or
        (array.type != CLM_BPA_TYPE_STRUCT and array.type != block.type) or
        array.block_size != block.block_size or expected_size != array.block_size)
        return CLM_BPA_FAILURE;
    const offset: usize = @as(usize, @intCast(index)) * array.block_size;
    @memcpy(block.data, array.items.blocks[offset .. offset + array.block_size]);
    return CLM_BPA_GET_SUCCESS;
}

/// Copies a compatible block over an existing array item.
pub fn clm_bpaa_set(array_id: base.clm_id, index: i32, block_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    const block = clm_bpa_find_block(block_id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or block.type == CLM_BPA_TYPE_STRING or
        (array.type != CLM_BPA_TYPE_STRUCT and array.type != block.type) or
        array.block_size != block.block_size or expected_size != array.block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(array_id, index, block.data);
}

/// Inserts a compatible block into a fixed-width array.
pub fn clm_bpaa_insert(array_id: base.clm_id, index: i32, block_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    const block = clm_bpa_find_block(block_id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or block.type == CLM_BPA_TYPE_STRING or
        (array.type != CLM_BPA_TYPE_STRUCT and array.type != block.type) or
        array.block_size != block.block_size or expected_size != array.block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(array_id, index, block.data);
}

/// Appends a compatible block into a fixed-width array.
pub fn clm_bpaa_append(array_id: base.clm_id, block_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    if (array.count > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_insert(array_id, @intCast(array.count), block_id, expected_size);
}

/// Copies a string array element into a pre-created string block.
pub fn clm_bpaa_get_string(array_id: base.clm_id, index: i32, block_id: base.clm_id) i32 {
    const text = clm_bpaa_string(array_id, index) orelse return CLM_BPA_FAILURE;
    const block = clm_bpa_find_block(block_id) orelse return CLM_BPA_FAILURE;
    if (block.type != CLM_BPA_TYPE_STRING or text.len > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    const copy = copyString(text) orelse return CLM_BPA_FAILURE;
    if (block.data.len != 0) clm_bpa_pool_free(block.data, block.data.len);
    block.data = copy;
    block.length = @intCast(text.len);
    return CLM_BPA_GET_SUCCESS;
}

/// Replaces an array string from a string block.
pub fn clm_bpaa_set_string(array_id: base.clm_id, index: i32, block_id: base.clm_id) i32 {
    const text = clm_bpa_string(block_id) orelse return CLM_BPA_FAILURE;
    const status = clm_bpaa_set_string_raw(array_id, index, text);
    return if (status < 0) CLM_BPA_FAILURE else CLM_BPA_SET_SUCCESS;
}

/// Inserts a string block value into an array.
pub fn clm_bpaa_insert_string(array_id: base.clm_id, index: i32, block_id: base.clm_id) i32 {
    const text = clm_bpa_string(block_id) orelse return CLM_BPA_FAILURE;
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    if (array.type != CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(array_id, index, text);
}

/// Appends a string block value to an array.
pub fn clm_bpaa_append_string(array_id: base.clm_id, block_id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    if (array.count > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_insert_string(array_id, @intCast(array.count), block_id);
}

/// Draws a fixed-width array item into a compatible block.
pub fn clm_bpaa_draw(array_id: base.clm_id, block_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    const block = clm_bpa_find_block(block_id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or block.type == CLM_BPA_TYPE_STRING or
        (array.type != CLM_BPA_TYPE_STRUCT and array.type != block.type) or
        block.block_size != array.block_size or expected_size != array.block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_draw_raw(array_id, block.data);
}

/// Draws a selected active-deck item into a compatible block.
pub fn clm_bpaa_draw_at(array_id: base.clm_id, index: i32, block_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    const block = clm_bpa_find_block(block_id) orelse return CLM_BPA_FAILURE;
    if (array.type == CLM_BPA_TYPE_STRING or block.type == CLM_BPA_TYPE_STRING or
        (array.type != CLM_BPA_TYPE_STRUCT and array.type != block.type) or
        block.block_size != array.block_size or expected_size != array.block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_draw_at_raw(array_id, index, block.data);
}

/// Writes an unsigned 32-bit metadata value into a compatible block.
pub fn clm_bpaa_write_u32(destination_id: base.clm_id, value: u32) i32 {
    const block = clm_bpa_find_block(destination_id) orelse return CLM_BPA_FAILURE;
    if (block.type != CLM_BPA_TYPE_UNSIGNED_INT or block.block_size != 4) return CLM_BPA_FAILURE;
    @memcpy(block.data, std.mem.asBytes(&value));
    return CLM_BPA_GET_SUCCESS;
}

/// Writes a signed 32-bit metadata value into a compatible block.
pub fn clm_bpaa_write_i32(destination_id: base.clm_id, value: i32) i32 {
    const block = clm_bpa_find_block(destination_id) orelse return CLM_BPA_FAILURE;
    if (block.type != CLM_BPA_TYPE_INT or block.block_size != 4) return CLM_BPA_FAILURE;
    @memcpy(block.data, std.mem.asBytes(&value));
    return CLM_BPA_GET_SUCCESS;
}

/// Writes a byte metadata value into a compatible block.
pub fn clm_bpaa_write_byte(destination_id: base.clm_id, value: u8) i32 {
    const block = clm_bpa_find_block(destination_id) orelse return CLM_BPA_FAILURE;
    if (block.type != CLM_BPA_TYPE_BYTE or block.block_size != 1) return CLM_BPA_FAILURE;
    block.data[0] = value;
    return CLM_BPA_GET_SUCCESS;
}

/// Writes an array count into an unsigned-integer block.
pub fn clm_bpaa_get_count(array_id: base.clm_id, destination_id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    return clm_bpaa_write_u32(destination_id, array.count);
}

/// Writes an array capacity into an unsigned-integer block.
pub fn clm_bpaa_get_capacity(array_id: base.clm_id, destination_id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    return clm_bpaa_write_u32(destination_id, array.capacity);
}

/// Writes the initialized array length into an unsigned-integer block.
pub fn clm_bpaa_get_length(array_id: base.clm_id, destination_id: base.clm_id) i32 {
    return clm_bpaa_get_count(array_id, destination_id);
}

/// Writes the cursor into a signed-integer block.
pub fn clm_bpaa_get_current_index(array_id: base.clm_id, destination_id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    if (array.index > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_write_i32(destination_id, @intCast(array.index));
}

/// Writes the sealed flag into a byte block.
pub fn clm_bpaa_is_sealed(array_id: base.clm_id, destination_id: base.clm_id) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    return clm_bpaa_write_byte(destination_id, array.sealed);
}

/// Reads the first array item into a block.
pub fn clm_bpaa_get_first(array_id: base.clm_id, destination_id: base.clm_id, expected_size: usize) i32 {
    return clm_bpaa_get(array_id, 0, destination_id, expected_size);
}

/// Reads the last array item into a block.
pub fn clm_bpaa_get_last(array_id: base.clm_id, destination_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    if (array.count == 0 or array.count - 1 > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_get(array_id, @intCast(array.count - 1), destination_id, expected_size);
}

/// Reads the cursor item into a block.
pub fn clm_bpaa_get_current(array_id: base.clm_id, destination_id: base.clm_id, expected_size: usize) i32 {
    const array = clm_bpaa_find_array(array_id) orelse return CLM_BPA_FAILURE;
    if (array.index >= array.count or array.index > std.math.maxInt(i32)) return CLM_BPA_FAILURE;
    return clm_bpaa_get(array_id, @intCast(array.index), destination_id, expected_size);
}

/// Reads a fixed-width item using the external-memory naming convention.
pub fn clm_bpaa_get_to(id: base.clm_id, index: i32, destination: []u8) i32 {
    return clm_bpaa_get_raw(id, index, destination);
}

/// Reads the first fixed-width item using the external-memory naming convention.
pub fn clm_bpaa_get_first_to(id: base.clm_id, destination: []u8) i32 {
    return clm_bpaa_get_first_raw(id, destination);
}

/// Reads the last fixed-width item using the external-memory naming convention.
pub fn clm_bpaa_get_last_to(id: base.clm_id, destination: []u8) i32 {
    return clm_bpaa_get_last_raw(id, destination);
}

/// Reads the current fixed-width item using the external-memory naming convention.
pub fn clm_bpaa_get_current_to(id: base.clm_id, destination: []u8) i32 {
    return clm_bpaa_get_current_raw(id, destination);
}

/// Reads a string item through the external-memory naming convention.
pub fn clm_bpaa_get_string_to(id: base.clm_id, index: i32, destination: []u8) i32 {
    return clm_bpaa_get_string_raw(id, index, destination);
}

/// Sets a fixed-width array item through the external-memory naming convention.
pub fn clm_bpaa_set_from(id: base.clm_id, index: i32, source: []const u8) i32 {
    return clm_bpaa_set_raw(id, index, source);
}

/// Sets a string item through the external-memory naming convention.
pub fn clm_bpaa_set_string_from(id: base.clm_id, index: i32, source: []const u8) i32 {
    return clm_bpaa_set_string_raw(id, index, source);
}

/// Convenience alias for slice-based fixed-width insertion.
pub fn clm_bpaa_insert_from(id: base.clm_id, index: i32, source: []const u8) i32 {
    return clm_bpaa_insert_raw(id, index, source);
}

/// Convenience alias for slice-based fixed-width append.
pub fn clm_bpaa_append_from(id: base.clm_id, source: []const u8) i32 {
    return clm_bpaa_append_raw(id, source);
}

/// Draws the next fixed-width item into caller storage.
pub fn clm_bpaa_draw_to(id: base.clm_id, destination: []u8) i32 {
    return clm_bpaa_draw_raw(id, destination);
}

/// Draws an indexed item into caller storage.
pub fn clm_bpaa_draw_at_to(id: base.clm_id, index: i32, destination: []u8) i32 {
    return clm_bpaa_draw_at_raw(id, index, destination);
}

/// Force-releases every registered resource and all pooled storage.
pub fn clm_bpa_shutdown() i32 {
    while (arrays_head) |array| _ = clm_bpaa_delete(array.id);
    while (blocks_head) |block| _ = clm_bpa_delete(block.id);
    clm_bpa_global_blocks_capacity = 0;
    clm_bpa_global_arrays_capacity = 0;
    clm_bpa_global_blocks_tombstones = 0;
    clm_bpa_global_arrays_tombstones = 0;
    const result = clm_bpa_pool_cleanup();
    if (result != CLM_BPA_SUCCESS) return result;
    blocks_index.deinit(allocator);
    arrays_index.deinit(allocator);
    blocks_index = .{};
    arrays_index = .{};
    return CLM_BPA_SUCCESS;
}

test "BPA scalar block and string ownership" {
    try std.testing.expectEqual(CLM_BPA_SUCCESS, clm_bpa_create(9001, CLM_BPA_TYPE_INT));
    var value: i32 = 37;
    try std.testing.expectEqual(CLM_BPA_SET_SUCCESS, clm_bpa_set(9001, std.mem.asBytes(&value)));
    var copy: i32 = 0;
    try std.testing.expectEqual(CLM_BPA_GET_SUCCESS, clm_bpa_get(9001, std.mem.asBytes(&copy)));
    try std.testing.expectEqual(@as(i32, 37), copy);
    try std.testing.expectEqual(CLM_BPA_SUCCESS, clm_bpa_delete(9001));
    try std.testing.expectEqual(CLM_BPA_SUCCESS, clm_bpa_create(9002, CLM_BPA_TYPE_STRING));
    try std.testing.expectEqual(@as(i32, 5), clm_bpa_set_string(9002, "hello"));
    try std.testing.expectEqualStrings("hello", clm_bpa_string(9002).?);
    try std.testing.expectEqual(CLM_BPA_SUCCESS, clm_bpa_delete(9002));
}
