package clm

import "core:mem"
import "core:math/rand"
import "base:runtime"

// BPA owns heap-backed allocations independently of the caller's context allocator.
// Borrowed pointers expire on deletion or relocation; advanced borrows only retain
// against ordinary unref, not explicit delete/shutdown. Registries are not thread-safe.
CLM_BPA_FAILURE :: -1
CLM_BPA_SUCCESS :: 0
CLM_BPA_GET_SUCCESS :: 1
CLM_BPA_SET_SUCCESS :: 2
CLM_BPA_TYPE_STRUCT :: 0
CLM_BPA_TYPE_SIGNED_BYTE :: 1
CLM_BPA_TYPE_BYTE :: 2
CLM_BPA_TYPE_SHORT :: 3
CLM_BPA_TYPE_UNSIGNED_SHORT :: 4
CLM_BPA_TYPE_INT :: 5
CLM_BPA_TYPE_UNSIGNED_INT :: 6
CLM_BPA_TYPE_LONG :: 7
CLM_BPA_TYPE_UNSIGNED_LONG :: 8
CLM_BPA_TYPE_FLOAT :: 9
CLM_BPA_TYPE_DOUBLE :: 10
CLM_BPA_TYPE_STRING :: 11
CLM_BPA_TYPE_UUID :: 12
CLM_BPA_TYPE_DATETIME :: 13
CLM_BPA_INITIAL_CAPACITY :: 16
CLM_BPA_POOL_SLAB_SLOTS :: 32
CLM_BPA_POOL_MAX_SLOT :: 4096
CLM_BPA_POOL_CLASSES :: 10

clm_block_allocation :: struct {
	type: u8, refcount: u16, block_size: u32, encoding: u8, length: u32, pointer: uintptr,
}
clm_block_array_allocation :: struct {
	type: u8, refcount: u16, count, capacity, block_size: u32, encoding: u8,
	index: u32, sealed: u8, deck_size: u32, pointer: uintptr,
}
clm_bpa_block_node :: struct {
	id: idkey, type: u8, refcount: u16, block_size: u32, encoding: u8, length: u32, data: rawptr,
}
clm_bpaa_array_node :: struct {
	id: idkey, type: u8, refcount: u16, count, capacity, block_size: u32, encoding: u8,
	index: u32, sealed: u8, deck_size: u32,
	items: struct {blocks: [^]u8, strings: [^]cstring},
}
clm_bpaa_compare_fn :: #type proc(left, right: rawptr) -> i32
clm_bpaa_predicate_fn :: #type proc(left, right: rawptr) -> bool
clm_bpa_pool_slab :: struct {next: ^clm_bpa_pool_slab, memory: rawptr}
clm_bpa_pool_class :: struct {
	slot_size: int, free_list: rawptr, slabs: ^clm_bpa_pool_slab, active, capacity: int,
}
clm_bpa_global_pools: [CLM_BPA_POOL_CLASSES]clm_bpa_pool_class
clm_bpa_global_blocks: [^]^clm_bpa_block_node
clm_bpa_global_arrays: [^]^clm_bpaa_array_node
clm_bpa_global_blocks_capacity, clm_bpa_global_blocks_count, clm_bpa_global_blocks_tombstones: int
clm_bpa_global_arrays_capacity, clm_bpa_global_arrays_count, clm_bpa_global_arrays_tombstones: int
clm_bpa_global_block_tombstone: clm_bpa_block_node
clm_bpa_global_array_tombstone: clm_bpaa_array_node
CLM_BPA_BLOCK_TOMBSTONE := &clm_bpa_global_block_tombstone
CLM_BPA_ARRAY_TOMBSTONE := &clm_bpa_global_array_tombstone

bpa_alloc :: proc(bytes: int) -> rawptr {
	if bytes <= 0 {return nil}
	p, err := mem.alloc(bytes, allocator=runtime.heap_allocator())
	if err != nil || p == nil {return nil}
	mem.zero(p, bytes)
	return p
}
bpa_free :: proc(p: rawptr) {if p != nil {mem.free(p, allocator=runtime.heap_allocator())}}
bpa_at :: proc(p: rawptr, offset: int) -> rawptr {return rawptr(uintptr(p) + uintptr(offset))}
bpa_strlen :: proc(p: cstring) -> int {return len(p)}
/*
Selects the smallest pooled slot class that can hold the requested size.
bytes: Requested allocation size; zero and oversized values use no class.
Returns: Pool-class index, or CLM_BPA_POOL_CLASSES for the heap fallback.
*/
clm_bpa_pool_class_for :: proc(bytes: int) -> int {
	if bytes <= 0 || bytes > CLM_BPA_POOL_MAX_SLOT {return CLM_BPA_POOL_CLASSES}
	index, size := 0, max(size_of(rawptr), 8)
	for size < bytes && index+1 < CLM_BPA_POOL_CLASSES {size *= 2; index += 1}
	return index
}
/*
Allocates zero-initialized storage from a slab pool or the heap fallback.
bytes: Number of bytes requested; zero returns nil.
Returns: A suitably aligned allocation, or nil if allocation fails.
*/
clm_bpa_pool_alloc :: proc(bytes: int) -> rawptr {
	idx := clm_bpa_pool_class_for(bytes)
	if idx == CLM_BPA_POOL_CLASSES {return bpa_alloc(bytes)}
	pool := &clm_bpa_global_pools[idx]
	if pool.slot_size == 0 {pool.slot_size = max(size_of(rawptr), 8) << uint(idx)}
	if pool.free_list == nil {
		slab := (^clm_bpa_pool_slab)(bpa_alloc(size_of(clm_bpa_pool_slab)))
		if slab == nil {return nil}
		slab.memory = bpa_alloc(pool.slot_size * CLM_BPA_POOL_SLAB_SLOTS)
		if slab.memory == nil {bpa_free(slab); return nil}
		slab.next = pool.slabs; pool.slabs = slab
		for i in 0..<CLM_BPA_POOL_SLAB_SLOTS {
			slot := bpa_at(slab.memory, i*pool.slot_size)
			(^rawptr)(slot)^ = pool.free_list; pool.free_list = slot
		}
		pool.capacity += CLM_BPA_POOL_SLAB_SLOTS
	}
	p := pool.free_list; pool.free_list = (^rawptr)(p)^; pool.active += 1
	mem.zero(p, bytes)
	return p
}
/*
Returns a pooled allocation to its free list, or frees a heap fallback.
pointer: Allocation to release; nil is ignored.
bytes: Original allocation size, used to select the matching class.
*/
clm_bpa_pool_free :: proc(pointer: rawptr, bytes: int) {
	if pointer == nil {return}
	idx := clm_bpa_pool_class_for(bytes)
	if idx == CLM_BPA_POOL_CLASSES {bpa_free(pointer); return}
	pool := &clm_bpa_global_pools[idx]
	(^rawptr)(pointer)^ = pool.free_list; pool.free_list = pointer; pool.active -= 1
}
/*
Releases all pool slabs after verifying that no pooled objects remain live.
Returns: CLM_BPA_SUCCESS when cleaned up; CLM_BPA_FAILURE if a class is active.
*/
clm_bpa_pool_cleanup :: proc() -> i32 {
	for &p in clm_bpa_global_pools {if p.active != 0 {return -1}}
	for &p in clm_bpa_global_pools {
		slab := p.slabs
		for slab != nil {next := slab.next; bpa_free(slab.memory); bpa_free(slab); slab = next}
		p = {}
	}
	return 0
}
/*
Reports usage for the pool class selected by a requested object size.
bytes: Size used to select a pooled class.
active: Receives the number of currently checked-out slots.
capacity: Receives the total slots reserved by that class.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid size or output.
*/
clm_bpa_pool_stats :: proc(bytes: int, active, capacity: ^int) -> i32 {
	idx := clm_bpa_pool_class_for(bytes)
	if idx == CLM_BPA_POOL_CLASSES || active == nil || capacity == nil {return -1}
	active^ = clm_bpa_global_pools[idx].active; capacity^ = clm_bpa_global_pools[idx].capacity
	return 0
}
/*
Mixes a 64-bit ID into a hash suitable for the global open-addressed tables.
value: Identifier to hash.
Returns: Mixed hash value; callers mask it to the table capacity.
*/
clm_bpa_hash_id :: proc(value: idkey) -> int {
	id := u64(value)
	id ~= id >> 30; id *= 0xbf58476d1ce4e5b9
	id ~= id >> 27; id *= 0x94d049bb133111eb; id ~= id >> 31
	return int(id)
}
/*
Rebuilds the block registry at a new power-of-two capacity.
capacity: New slot count; must be a nonzero power of two.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if allocation fails.
*/
clm_bpa_rehash_blocks :: proc(capacity: int) -> i32 {
	if capacity <= clm_bpa_global_blocks_count || capacity & (capacity-1) != 0 || capacity > max(int)/size_of(^clm_bpa_block_node) {return -1}
	slots := ([^]^clm_bpa_block_node)(bpa_alloc(capacity*size_of(^clm_bpa_block_node)))
	if slots == nil {return -1}
	for i in 0..<clm_bpa_global_blocks_capacity {
		b := clm_bpa_global_blocks[i]
		if b != nil && b != CLM_BPA_BLOCK_TOMBSTONE {
			slot := clm_bpa_hash_id(b.id) & (capacity-1)
			for slots[slot] != nil {slot = (slot+1) & (capacity-1)}
			slots[slot] = b
		}
	}
	bpa_free(clm_bpa_global_blocks); clm_bpa_global_blocks = slots
	clm_bpa_global_blocks_capacity = capacity; clm_bpa_global_blocks_tombstones = 0
	return 0
}
/*
Rebuilds the array registry at a new power-of-two capacity.
capacity: New slot count; must be a nonzero power of two.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if allocation fails.
*/
clm_bpa_rehash_arrays :: proc(capacity: int) -> i32 {
	if capacity <= clm_bpa_global_arrays_count || capacity & (capacity-1) != 0 || capacity > max(int)/size_of(^clm_bpaa_array_node) {return -1}
	slots := ([^]^clm_bpaa_array_node)(bpa_alloc(capacity*size_of(^clm_bpaa_array_node)))
	if slots == nil {return -1}
	for i in 0..<clm_bpa_global_arrays_capacity {
		a := clm_bpa_global_arrays[i]
		if a != nil && a != CLM_BPA_ARRAY_TOMBSTONE {
			slot := clm_bpa_hash_id(a.id) & (capacity-1)
			for slots[slot] != nil {slot = (slot+1) & (capacity-1)}
			slots[slot] = a
		}
	}
	bpa_free(clm_bpa_global_arrays); clm_bpa_global_arrays = slots
	clm_bpa_global_arrays_capacity = capacity; clm_bpa_global_arrays_tombstones = 0
	return 0
}
/*
Ensures the block registry has room and compacts tombstones if needed.
Returns: CLM_BPA_SUCCESS when insertion can proceed; otherwise failure.
*/
clm_bpa_prepare_blocks :: proc() -> i32 {
	c := clm_bpa_global_blocks_capacity
	if c == 0 {return clm_bpa_rehash_blocks(16)}
	if clm_bpa_global_blocks_count+clm_bpa_global_blocks_tombstones+1 >= c-c/4 {
		if clm_bpa_global_blocks_count < c/4 && clm_bpa_global_blocks_tombstones != 0 {return clm_bpa_rehash_blocks(c)}
		if c > max(int)/2 {return -1}
		return clm_bpa_rehash_blocks(c*2)
	}
	return 0
}
/*
Ensures the array registry has room and compacts tombstones if needed.
Returns: CLM_BPA_SUCCESS when insertion can proceed; otherwise failure.
*/
clm_bpa_prepare_arrays :: proc() -> i32 {
	c := clm_bpa_global_arrays_capacity
	if c == 0 {return clm_bpa_rehash_arrays(16)}
	if clm_bpa_global_arrays_count+clm_bpa_global_arrays_tombstones+1 >= c-c/4 {
		if clm_bpa_global_arrays_count < c/4 && clm_bpa_global_arrays_tombstones != 0 {return clm_bpa_rehash_arrays(c)}
		if c > max(int)/2 {return -1}
		return clm_bpa_rehash_arrays(c*2)
	}
	return 0
}
/*
Resolves the byte size of a built-in fixed-size BPA type.
type: BPA type tag to resolve.
Returns: Type size in bytes, or zero for variable-sized/unsupported types.
*/
clm_bpa_type_size :: proc(type: u8) -> u32 {
	switch type {
	case 1, 2: return 1
	case 3, 4: return 2
	case 5, 6, 9: return 4
	case 7, 8, 10, 13: return 8
	case 12: return 16
	}
	return 0
}
/*
Looks up a block by ID in the global block registry.
id: ID of the block to find.
Returns: The block node, or nil when no such block exists.
*/
clm_bpa_find_block :: proc(id: idkey) -> ^clm_bpa_block_node {
	c := clm_bpa_global_blocks_capacity
	if c == 0 {return nil}
	slot := clm_bpa_hash_id(id) & (c-1)
	for _ in 0..<c {
		b := clm_bpa_global_blocks[slot]
		if b == nil {return nil}
		if b != CLM_BPA_BLOCK_TOMBSTONE && b.id == id {return b}
		slot = (slot+1) & (c-1)
	}
	return nil
}
/*
Looks up an array by ID in the global array registry.
id: ID of the array to find.
Returns: The array node, or nil when no such array exists.
*/
clm_bpaa_find_array :: proc(id: idkey) -> ^clm_bpaa_array_node {
	c := clm_bpa_global_arrays_capacity
	if c == 0 {return nil}
	slot := clm_bpa_hash_id(id) & (c-1)
	for _ in 0..<c {
		a := clm_bpa_global_arrays[slot]
		if a == nil {return nil}
		if a != CLM_BPA_ARRAY_TOMBSTONE && a.id == id {return a}
		slot = (slot+1) & (c-1)
	}
	return nil
}
/*
Checks whether a block with the given ID exists in the global registry.
id: ID of the block to check.
Returns: true if the block exists; otherwise false.
*/
clm_bpa_is_existing :: proc(id: idkey) -> bool {return clm_bpa_find_block(id) != nil}
/*
Checks whether an array with the given ID exists in the global registry.
id: ID of the array to check.
Returns: true if the array exists; otherwise false.
*/
clm_bpaa_is_existing :: proc(id: idkey) -> bool {return clm_bpaa_find_array(id) != nil}
/*
Inserts a new b node into a prepared registry slot.
b: Initialized node with an ID not already present in the table.
*/
clm_bpa_insert_block :: proc(b: ^clm_bpa_block_node) {
	c := clm_bpa_global_blocks_capacity; slot := clm_bpa_hash_id(b.id) & (c-1); tomb := -1
	for clm_bpa_global_blocks[slot] != nil {
		if clm_bpa_global_blocks[slot] == CLM_BPA_BLOCK_TOMBSTONE && tomb == -1 {tomb = slot}
		slot = (slot+1) & (c-1)
	}
	if tomb != -1 {slot = tomb; clm_bpa_global_blocks_tombstones -= 1}
	clm_bpa_global_blocks[slot] = b; clm_bpa_global_blocks_count += 1
}
/*
Inserts a new a node into a prepared registry slot.
a: Initialized node with an ID not already present in the table.
*/
clm_bpa_insert_array :: proc(a: ^clm_bpaa_array_node) {
	c := clm_bpa_global_arrays_capacity; slot := clm_bpa_hash_id(a.id) & (c-1); tomb := -1
	for clm_bpa_global_arrays[slot] != nil {
		if clm_bpa_global_arrays[slot] == CLM_BPA_ARRAY_TOMBSTONE && tomb == -1 {tomb = slot}
		slot = (slot+1) & (c-1)
	}
	if tomb != -1 {slot = tomb; clm_bpa_global_arrays_tombstones -= 1}
	clm_bpa_global_arrays[slot] = a; clm_bpa_global_arrays_count += 1
}
/*
Removes a block from the registry and returns its storage to the pool.
id: ID of the block to release.
Returns: CLM_BPA_SUCCESS if removed; otherwise CLM_BPA_FAILURE.
*/
clm_bpa_remove_block :: proc(id: idkey) -> i32 {
	b := clm_bpa_find_block(id); if b == nil {return -1}
	c := clm_bpa_global_blocks_capacity; slot := clm_bpa_hash_id(id) & (c-1)
	for clm_bpa_global_blocks[slot] != b {slot = (slot+1) & (c-1)}
	clm_bpa_global_blocks[slot] = CLM_BPA_BLOCK_TOMBSTONE
	clm_bpa_global_blocks_count -= 1; clm_bpa_global_blocks_tombstones += 1
	size := int(b.block_size); if b.type == 11 {size = int(b.length)+1}
	clm_bpa_pool_free(b.data, size); bpa_free(b)
	return 0
}
/*
Removes an array and releases its items and backing storage.
id: ID of the array to release.
Returns: CLM_BPA_SUCCESS if removed; otherwise CLM_BPA_FAILURE.
*/
clm_bpa_remove_array :: proc(id: idkey) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil {return -1}
	c := clm_bpa_global_arrays_capacity; slot := clm_bpa_hash_id(id) & (c-1)
	for clm_bpa_global_arrays[slot] != a {slot = (slot+1) & (c-1)}
	clm_bpa_global_arrays[slot] = CLM_BPA_ARRAY_TOMBSTONE
	clm_bpa_global_arrays_count -= 1; clm_bpa_global_arrays_tombstones += 1
	if a.type == 11 {
		for i in 0..<a.count {clm_bpa_pool_free(rawptr(a.items.strings[i]), len(a.items.strings[i])+1)}
		bpa_free(a.items.strings)
	} else {bpa_free(a.items.blocks)}
	bpa_free(a)
	return 0
}
/*
Copies a bounded string into pool storage and records its byte length.
buffer: Source bytes; a NUL within buffer_size ends the string.
buffer_size: Number of readable source bytes.
value: Receives the allocated, NUL-terminated copy.
length: Receives the copied length excluding the terminator.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
*/
clm_bpa_copy_string :: proc(buffer: rawptr, buffer_size: u32, value: ^cstring, length: ^u32) -> i32 {
	if buffer == nil || buffer_size == 0 || value == nil || length == nil {return -1}
	bytes := ([^]u8)(buffer); n := 0
	for n < int(buffer_size) && bytes[n] != 0 {n += 1}
	p := clm_bpa_pool_alloc(n+1); if p == nil {return -1}
	mem.copy(p, buffer, n); ([^]u8)(p)[n] = 0
	value^ = cstring(p); length^ = u32(n)
	return 0
}
bpa_resolve_size :: proc(type: u8, size: u32) -> (u32, bool) {
	resolved := clm_bpa_type_size(type)
	if type == 11 {return 0, size == 0}
	if type == 0 {return size, size != 0}
	return resolved, resolved != 0 && (size == 0 || size == resolved)
}
/*
Creates a zero-initialized block with an explicit size for custom structs.
id: Unique global ID; type selects a built-in, string, or struct block.
type: Block type; use CLM_BPA_TYPE_STRUCT for custom-sized blocks.
block_size: Required byte size for STRUCT; pass zero for built-in types.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
*/
clm_bpa_create_sized :: proc(id: idkey, type: u8, block_size: u32) -> i32 {
	size, valid := bpa_resolve_size(type, block_size)
	if !valid || clm_bpa_is_existing(id) || clm_bpa_prepare_blocks() != 0 {return -1}
	b := (^clm_bpa_block_node)(bpa_alloc(size_of(clm_bpa_block_node))); if b == nil {return -1}
	if type != 11 {b.data = clm_bpa_pool_alloc(int(size)); if b.data == nil {bpa_free(b); return -1}}
	b.id = id; b.type = type; b.refcount = 1; b.block_size = size
	clm_bpa_insert_block(b)
	return 0
}
/*
Creates a zero-initialized block of a built-in type.
id: Unique global ID to assign to the block.
type: Supported fixed-size or string BPA type; STRUCT requires create_sized.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if creation is not possible.
*/
clm_bpa_create :: proc(id: idkey, type: u8) -> i32 {
	if type == 0 {return -1}
	return clm_bpa_create_sized(id, type, 0)
}
/*
Immediately releases a block regardless of its reference count.
id: ID of the block to delete.
Returns: CLM_BPA_SUCCESS if deleted; otherwise CLM_BPA_FAILURE.
*/
clm_bpa_delete :: clm_bpa_remove_block
/*
Adds one reference to an existing block.
id: ID of the block to retain.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/at the refcount limit.
*/
clm_bpa_ref :: proc(id: idkey) -> i32 {
	b := clm_bpa_find_block(id); if b == nil || b.refcount == max(u16) {return -1}
	b.refcount += 1; return 0
}
/*
Releases one reference and deletes the block when its count reaches zero.
id: ID of the block to release.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing or already unreferenced.
*/
clm_bpa_unref :: proc(id: idkey) -> i32 {
	b := clm_bpa_find_block(id); if b == nil || b.refcount == 0 {return -1}
	b.refcount -= 1; if b.refcount == 0 {return clm_bpa_remove_block(id)}
	return 0
}
/*
Copies a fixed-size block into a caller-provided output buffer.
id: ID of a non-string block.
buffer: Receives the copied block contents.
buffer_size: Destination capacity in bytes; must cover the block size.
Returns: CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
*/
clm_bpa_get :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	b := clm_bpa_find_block(id)
	if b == nil || b.type == 11 || buffer == nil || buffer_size < b.block_size {return -1}
	mem.copy(buffer, b.data, int(b.block_size)); return 1
}
/*
Replaces the contents of a fixed-size block with caller-provided bytes.
id: ID of a non-string block.
buffer: Source bytes to copy into the block.
buffer_size: Source size in bytes; must exactly match the block size.
Returns: CLM_BPA_SET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
*/
clm_bpa_set :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	b := clm_bpa_find_block(id)
	if b == nil || b.type == 11 || buffer == nil || buffer_size != b.block_size {return -1}
	mem.copy(b.data, buffer, int(b.block_size)); return 2
}
/*
Copies a string block, including its NUL terminator, into the output buffer.
id: ID of a string block.
buffer: Receives the string and its NUL terminator.
buffer_size: Destination capacity in bytes, including room for the terminator.
Returns: String byte length excluding NUL, or CLM_BPA_FAILURE if it does not fit.
*/
clm_bpa_get_string :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	b := clm_bpa_find_block(id)
	if b == nil || b.type != 11 || buffer == nil || u64(buffer_size) < u64(b.length)+1 || b.length > 0x7fff_ffff {return -1}
	if b.length != 0 {mem.copy(buffer, b.data, int(b.length))}
	([^]u8)(buffer)[b.length] = 0; return i32(b.length)
}
/*
Replaces a string block with a bounded copy of the supplied string bytes.
id: ID of a string block.
buffer: Source string bytes; a NUL within the bound terminates the string.
buffer_size: Number of readable source bytes.
Returns: String byte length excluding NUL, or CLM_BPA_FAILURE on error.
*/
clm_bpa_set_string :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	b := clm_bpa_find_block(id); p: cstring; n: u32
	if b == nil || b.type != 11 || clm_bpa_copy_string(buffer, buffer_size, &p, &n) != 0 {return -1}
	if n > 0x7fff_ffff {clm_bpa_pool_free(rawptr(p), int(n)+1); return -1}
	clm_bpa_pool_free(b.data, int(b.length)+1); b.data = rawptr(p); b.length = n
	return i32(n)
}
/*
Replaces the content of an existing block.
id: ID of the block to update.
buffer: Source content; string data is copied into the internal pool.
buffer_size: Number of readable source bytes.
Returns: CLM_BPA_SET_SUCCESS for fixed-size blocks, string byte length for
        string blocks, or CLM_BPA_FAILURE on invalid input.
*/
clm_bpa_replace :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	b := clm_bpa_find_block(id); if b == nil {return -1}
	if b.type == 11 {return clm_bpa_set_string(id, buffer, buffer_size)}
	return clm_bpa_set(id, buffer, buffer_size)
}
/*
Creates an array and reserves initial slots, with an explicit struct item size.
id: Unique global array ID; reserved is the initial item capacity.
type: Item type; block_size is required only for STRUCT items.
reserved: Initial number of reserved item slots; must be nonnegative.
block_size: Item byte width for STRUCT; pass zero for built-in/string types.
encoding: Encoding metadata retained for string arrays.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
*/
clm_bpaa_create_sized :: proc(id: idkey, type: u8, reserved: i32, block_size: u32, encoding: u8) -> i32 {
	size, valid := bpa_resolve_size(type, block_size)
	if !valid || reserved < 0 || clm_bpaa_is_existing(id) || clm_bpa_prepare_arrays() != 0 {return -1}
	a := (^clm_bpaa_array_node)(bpa_alloc(size_of(clm_bpaa_array_node))); if a == nil {return -1}
	a.id = id; a.type = type; a.refcount = 1; a.block_size = size; a.encoding = encoding
	if clm_bpaa_resize(a, u32(reserved)) != 0 {bpa_free(a); return -1}
	clm_bpa_insert_array(a); return 0
}
/*
Creates an array of a built-in type with the requested reserved capacity.
id: Unique global array ID; reserved is the initial slot capacity.
type: Supported built-in or string type; STRUCT requires create_sized.
reserved: Initial number of reserved item slots; must be nonnegative.
encoding: Encoding metadata for string arrays.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE when creation cannot proceed.
*/
clm_bpaa_create :: proc(id: idkey, type: u8, reserved: i32, encoding: u8) -> i32 {
	if type == 0 {return -1}
	return clm_bpaa_create_sized(id, type, reserved, 0, encoding)
}
/*
Immediately deletes an array and releases its items and reserved storage.
id: ID of the array to delete.
Returns: CLM_BPA_SUCCESS if deleted; otherwise CLM_BPA_FAILURE.
*/
clm_bpaa_delete :: clm_bpa_remove_array
/*
Adds one reference to an existing array.
id: ID of the array to retain.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/at the refcount limit.
*/
clm_bpaa_ref :: proc(id: idkey) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.refcount == max(u16) {return -1}
	a.refcount += 1; return 0
}
/*
Releases one array reference and deletes the array when its count reaches zero.
id: ID of the array to release.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/unreferenced.
*/
clm_bpaa_unref :: proc(id: idkey) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.refcount == 0 {return -1}
	a.refcount -= 1; if a.refcount == 0 {return clm_bpa_remove_array(id)}
	return 0
}
/*
Resizes an array's backing storage while preserving its existing items.
a: Internal array node to resize; its capacity is updated on success.
capacity: New slot count, which must be at least the live item count.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if size arithmetic/allocation fails.
*/
clm_bpaa_resize :: proc(a: ^clm_bpaa_array_node, capacity: u32) -> i32 {
	if a == nil || capacity < a.count {return -1}
	width := int(a.block_size); old: rawptr = a.items.blocks
	if a.type == 11 {width = size_of(cstring); old = a.items.strings}
	if width == 0 || u64(capacity) > u64(max(int))/u64(width) {return -1}
	p := bpa_alloc(int(capacity)*width)
	if capacity != 0 && p == nil {return -1}
	if old != nil {mem.copy(p, old, int(min(capacity, a.capacity))*width)}
	bpa_free(old)
	if a.type == 11 {a.items.strings = ([^]cstring)(p)} else {a.items.blocks = ([^]u8)(p)}
	a.capacity = capacity; return 0
}
/*
Grows storage geometrically when needed, unless the array is sealed.
a: Internal array node to grow.
needed: Minimum slot count required by the pending insertion.
Returns: CLM_BPA_SUCCESS if capacity is sufficient; otherwise failure.
*/
clm_bpaa_ensure_capacity :: proc(a: ^clm_bpaa_array_node, needed: u32) -> i32 {
	if a == nil {return -1}
	if needed <= a.capacity {return 0}
	if a.sealed != 0 {return -1}
	c := a.capacity; if c == 0 {c = 4}
	for c < needed {if c > max(u32)/2 {c = needed; break}; c *= 2}
	return clm_bpaa_resize(a, c)
}
/*
Retrieves a fixed-width element without performing an ID lookup.
a: Live borrowed metadata for a non-string a.
index: Zero-based position of the item to retrieve.
buffer: Receives the item bytes.
buffer_size: Destination capacity in bytes; must fit one item.
*/
clm_bpaa_advanced_get_raw :: proc(a: ^clm_bpaa_array_node, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	if a == nil || a.type == 11 || index < 0 || u32(index) >= a.count || buffer == nil || buffer_size < a.block_size {return -1}
	mem.copy(buffer, &a.items.blocks[int(index)*int(a.block_size)], int(a.block_size)); return 1
}
/*
Copies a fixed-size array item into a caller-provided buffer.
id: Array ID of a non-string fixed-width array.
index: Zero-based item position to copy.
buffer: Receives the item bytes.
buffer_size: Destination capacity in bytes; must fit one item.
Returns: CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
*/
clm_bpaa_get_raw :: proc(id: idkey, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	return clm_bpaa_advanced_get_raw(clm_bpaa_find_array(id), index, buffer, buffer_size)
}
/*
Replaces a fixed-width element without performing an ID lookup.
a: Live borrowed metadata for a non-string a.
index: Zero-based position of the existing item to replace.
buffer: Source bytes to copy into the item.
buffer_size: Source size in bytes; must exactly match the item size.
*/
clm_bpaa_advanced_set_raw :: proc(a: ^clm_bpaa_array_node, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	if a == nil || a.type == 11 || index < 0 || u32(index) >= a.count || buffer == nil || buffer_size != a.block_size {return -1}
	mem.copy(&a.items.blocks[int(index)*int(a.block_size)], buffer, int(a.block_size)); a.deck_size = a.count; return 2
}
/*
Replaces a fixed-size item without changing array count or capacity.
id: Array ID of a non-string fixed-width array.
index: Zero-based position of the existing item to replace.
buffer: Source bytes to copy into the item.
buffer_size: Source size in bytes; must exactly match the item size.
Returns: CLM_BPA_SET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
*/
clm_bpaa_set_raw :: proc(id: idkey, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	return clm_bpaa_advanced_set_raw(clm_bpaa_find_array(id), index, buffer, buffer_size)
}
bpa_insert :: proc(a: ^clm_bpaa_array_node, index: i32, buffer: rawptr, size: u32) -> i32 {
	if a == nil || index < 0 || u32(index) > a.count || a.count == max(u32) {return -1}
	p: rawptr; text: cstring; n: u32
	if a.type == 11 {
		if clm_bpa_copy_string(buffer, size, &text, &n) != 0 {return -1}
		p = rawptr(text)
	} else {
		if buffer == nil || size != a.block_size {return -1}
		p = bpa_alloc(int(size)); if p == nil {return -1}; mem.copy(p, buffer, int(size))
	}
	// Snapshot before growth or shifting, since buffer may point inside the array.
	if clm_bpaa_ensure_capacity(a, a.count+1) != 0 {
		if a.type == 11 {clm_bpa_pool_free(p, int(n)+1)} else {bpa_free(p)}
		return -1
	}
	if a.type == 11 {
		mem.copy(&a.items.strings[index+1], &a.items.strings[index], int(a.count-u32(index))*size_of(cstring))
		a.items.strings[index] = text
	} else {
		dst := &a.items.blocks[int(index)*int(size)]
		mem.copy(bpa_at(dst, int(size)), dst, int(a.count-u32(index))*int(size))
		mem.copy(dst, p, int(size)); bpa_free(p)
	}
	a.count += 1; a.index = u32(index); a.deck_size = a.count; return 0
}
/*
Inserts one item at an index, growing storage only if the reserve is full.
id: Array ID to modify.
index: Zero-based insertion position, from zero through the current count.
buffer: Fixed-width item bytes or source string bytes.
buffer_size: Item byte size, or readable source bound for a string.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
*/
clm_bpaa_insert_raw :: proc(id: idkey, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {return bpa_insert(clm_bpaa_find_array(id), index, buffer, buffer_size)}
/*
Appends one fixed-size item or bounded string to an array.
id: Array ID to modify.
buffer: Fixed-width item bytes or source string bytes.
buffer_size: Item byte size, or readable source bound for a string.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth failure.
*/
clm_bpaa_append_raw :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.count > 0x7fff_ffff {return -1}
	return bpa_insert(a, i32(a.count), buffer, buffer_size)
}
/*
Inserts a fixed-width element without performing an ID lookup.
a: Live borrowed metadata for an unsealed non-string a.
index: Zero-based insertion position, from zero through the current count.
buffer: Source bytes for the new item.
buffer_size: Source size in bytes; must exactly match the item size.
*/
clm_bpaa_advanced_insert_raw :: proc(a: ^clm_bpaa_array_node, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	if a == nil || a.type == 11 || a.count >= 0x7fff_ffff {return -1}
	return bpa_insert(a, index, buffer, buffer_size)
}
/*
Appends a fixed-width element without performing an ID lookup.
a: Live borrowed metadata for an unsealed non-string a.
buffer: Source bytes for the new item.
buffer_size: Source size in bytes; must exactly match the item size.
*/
clm_bpaa_advanced_append_raw :: proc(a: ^clm_bpaa_array_node, buffer: rawptr, buffer_size: u32) -> i32 {
	if a == nil || a.count > 0x7fff_ffff {return -1}
	return clm_bpaa_advanced_insert_raw(a, i32(a.count), buffer, buffer_size)
}
/*
Copies one string item, including its NUL terminator, to the output buffer.
id: Array ID of a string array.
index: Zero-based string position to copy.
buffer: Receives the string and its NUL terminator.
buffer_size: Destination capacity in bytes, including room for NUL.
Returns: String byte length excluding NUL, or CLM_BPA_FAILURE if it does not fit.
*/
clm_bpaa_get_string_raw :: proc(id: idkey, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id)
	if a == nil || a.type != 11 || index < 0 || u32(index) >= a.count || buffer == nil {return -1}
	n := len(a.items.strings[index])
	if n > 0x7fff_ffff || int(buffer_size) < n+1 {return -1}
	mem.copy(buffer, rawptr(a.items.strings[index]), n+1); return i32(n)
}
/*
Replaces a string array item with a bounded copy of caller-provided bytes.
id: Array ID of a string array.
index: Zero-based position of the existing string to replace.
buffer: Source string bytes; a NUL within the bound terminates the string.
buffer_size: Number of readable source bytes.
Returns: String byte length excluding NUL, or CLM_BPA_FAILURE on error.
*/
clm_bpaa_set_string_raw :: proc(id: idkey, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id); p: cstring; n: u32
	if a == nil || a.type != 11 || index < 0 || u32(index) >= a.count || clm_bpa_copy_string(buffer, buffer_size, &p, &n) != 0 {return -1}
	if n > 0x7fff_ffff {clm_bpa_pool_free(rawptr(p), int(n)+1); return -1}
	clm_bpa_pool_free(rawptr(a.items.strings[index]), len(a.items.strings[index])+1)
	a.items.strings[index] = p; a.deck_size = a.count; return i32(n)
}
bpa_remove :: proc(a: ^clm_bpaa_array_node, index: i32) -> i32 {
	if a == nil || index < 0 || u32(index) >= a.count {return -1}
	if a.type == 11 {
		clm_bpa_pool_free(rawptr(a.items.strings[index]), len(a.items.strings[index])+1)
		mem.copy(&a.items.strings[index], &a.items.strings[index+1], int(a.count-u32(index)-1)*size_of(cstring))
		a.items.strings[a.count-1] = nil
	} else {
		p := &a.items.blocks[int(index)*int(a.block_size)]
		mem.copy(p, bpa_at(p, int(a.block_size)), int(a.count-u32(index)-1)*int(a.block_size))
	}
	a.count -= 1
	if a.count == 0 {a.index = 0} else if a.index > u32(index) {a.index -= 1} else if a.index >= a.count {a.index = a.count-1}
	a.deck_size = a.count; return 0
}
/*
Removes an item, compacts following items, and retains reserved capacity.
id: Array ID to modify.
index: Zero-based position of the initialized item to remove.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array/index is invalid.
*/
clm_bpaa_remove :: proc(id: idkey, index: i32) -> i32 {return bpa_remove(clm_bpaa_find_array(id), index)}
/*
Removes a fixed-width element without performing an ID lookup.
a: Live borrowed metadata for a non-string a.
index: Zero-based position of the item to remove.
*/
clm_bpaa_advanced_remove :: proc(a: ^clm_bpaa_array_node, index: i32) -> i32 {
	if a == nil || a.type == 11 {return -1}; return bpa_remove(a, index)
}
/*
Removes the first item while retaining the array's reserve.
id: Array ID; the array must contain at least one item.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
*/
clm_bpaa_remove_first :: proc(id: idkey) -> i32 {return clm_bpaa_remove(id, 0)}
/*
Removes the last item while retaining the array's reserve.
id: Array ID; the array must contain at least one item.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
*/
clm_bpaa_remove_last :: proc(id: idkey) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.count == 0 || a.count-1 > 0x7fff_ffff {return -1}
	return bpa_remove(a, i32(a.count-1))
}
bpa_clear :: proc(a: ^clm_bpaa_array_node) -> i32 {
	if a == nil {return -1}
	if a.type == 11 {
		for i in 0..<a.count {clm_bpa_pool_free(rawptr(a.items.strings[i]), len(a.items.strings[i])+1); a.items.strings[i] = nil}
	} else if a.capacity != 0 {mem.zero(a.items.blocks, int(a.capacity)*int(a.block_size))}
	a.count = 0; a.index = 0; a.deck_size = 0; return 0
}
/*
Removes all items and resets the cursor/deck without releasing capacity.
id: Array ID to clear.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array does not exist.
*/
clm_bpaa_clear :: proc(id: idkey) -> i32 {return bpa_clear(clm_bpaa_find_array(id))}
/*
Clears a fixed-width a without releasing its reserved capacity.
a: Live borrowed metadata for a non-string a.
*/
clm_bpaa_advanced_clear :: proc(a: ^clm_bpaa_array_node) -> i32 {if a == nil || a.type == 11 {return -1}; return bpa_clear(a)}
/*
Reserves additional slots without performing an ID lookup.
a: Live borrowed metadata for an unsealed a.
additional_capacity: Number of additional slots to reserve; must be positive.
*/
clm_bpaa_advanced_extend :: proc(a: ^clm_bpaa_array_node, additional_capacity: u32) -> i32 {
	if a == nil || a.sealed != 0 || additional_capacity == 0 || additional_capacity > max(u32)-a.capacity {return -1}
	return clm_bpaa_resize(a, a.capacity+additional_capacity)
}
/*
Adds an explicit number of reserved slots to an unsealed array.
id: Array ID to extend.
additional_capacity: Number of additional slots to reserve; must be positive.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for sealed/invalid/overflow cases.
*/
clm_bpaa_extend :: proc(id: idkey, additional_capacity: u32) -> i32 {return clm_bpaa_advanced_extend(clm_bpaa_find_array(id), additional_capacity)}
/*
Shrinks reserved slots without performing an ID lookup.
a: Live borrowed metadata for an unsealed a.
new_capacity: New capacity; must be below current capacity and at least count.
*/
clm_bpaa_advanced_shrink :: proc(a: ^clm_bpaa_array_node, new_capacity: u32) -> i32 {
	if a == nil || a.sealed != 0 || new_capacity >= a.capacity || new_capacity < a.count {return -1}
	return clm_bpaa_resize(a, new_capacity)
}
/*
Reduces reserved slots without discarding live items on an unsealed array.
id: Array ID to shrink.
new_capacity: New slot capacity; must be below current capacity and at least count.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE when shrinking is not permitted.
*/
clm_bpaa_shrink :: proc(id: idkey, new_capacity: u32) -> i32 {return clm_bpaa_advanced_shrink(clm_bpaa_find_array(id), new_capacity)}
/*
Borrows array metadata and retains one reference against ordinary unref.
id: Array ID to borrow.
out_array: Receives the internal metadata pointer; initialized to nil.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/missing array.
@note This does not prevent explicit delete or shutdown from invalidating it.
*/
clm_bpaa_advanced_borrow :: proc(id: idkey, out_array: ^^clm_bpaa_array_node) -> i32 {
	if out_array == nil {return -1}; out_array^ = nil
	a := clm_bpaa_find_array(id); if a == nil || a.refcount == max(u16) {return -1}
	a.refcount += 1; out_array^ = a; return 0
}
/*
Releases an advanced borrow and nulls its pointer.
borrowed: Address of a live borrowed pointer.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid/released input.
@note Do not call after explicit deletion or shutdown invalidated the pointer.
*/
clm_bpaa_advanced_release :: proc(borrowed: ^^clm_bpaa_array_node) -> i32 {
	if borrowed == nil || borrowed^ == nil || borrowed^.refcount == 0 {return -1}
	id := borrowed^.id; borrowed^ = nil; return clm_bpaa_unref(id)
}
/*
Returns the contiguous fixed-width payload, or nil for string arrays.
a: A live pointer returned by clm_bpaa_advanced_borrow.
Returns: Mutable payload address; invalidated by resize, delete, or shutdown.
*/
clm_bpaa_advanced_data :: proc(a: ^clm_bpaa_array_node) -> rawptr {if a == nil || a.type == 11 {return nil}; return a.items.blocks}
/*
Moves the array cursor to index zero.
id: Array ID whose cursor should be reset.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
*/
clm_bpaa_reset :: proc(id: idkey) -> i32 {a := clm_bpaa_find_array(id); if a == nil {return -1}; a.index = 0; return 0}
/*
Moves the array cursor to its first item.
id: Array ID whose cursor should move.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
*/
clm_bpaa_move_first :: clm_bpaa_reset
/*
Moves the array cursor one item toward the beginning.
id: Array ID; the cursor must currently be greater than zero.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE at the beginning/missing array.
*/
clm_bpaa_move_previous :: proc(id: idkey) -> i32 {a := clm_bpaa_find_array(id); if a == nil || a.index == 0 {return -1}; a.index -= 1; return 0}
/*
Moves the array cursor one item toward the end.
id: Array ID; a next item must exist.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE at the end/missing array.
*/
clm_bpaa_move_next :: proc(id: idkey) -> i32 {a := clm_bpaa_find_array(id); if a == nil || u64(a.index)+1 >= u64(a.count) {return -1}; a.index += 1; return 0}
/*
Moves the array cursor to the final initialized item.
id: Array ID; the array must not be empty.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
*/
clm_bpaa_move_last :: proc(id: idkey) -> i32 {a := clm_bpaa_find_array(id); if a == nil || a.count == 0 {return -1}; a.index = a.count-1; return 0}
/*
Moves the array cursor to a specified initialized item.
id: Array ID whose cursor should move.
index: Zero-based position of an initialized item.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid index/array.
*/
clm_bpaa_move_to :: proc(id: idkey, index: i32) -> i32 {a := clm_bpaa_find_array(id); if a == nil || index < 0 || u32(index) >= a.count {return -1}; a.index = u32(index); return 0}
/*
Seals an array against capacity changes and automatic capacity growth.
id: Array ID to seal; in-capacity item mutations remain allowed.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
*/
clm_bpaa_seal :: proc(id: idkey) -> i32 {a := clm_bpaa_find_array(id); if a == nil {return -1}; a.sealed = 1; return 0}
/*
Allows capacity changes and growth for a previously sealed array.
id: Array ID to unseal.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
*/
clm_bpaa_unseal :: proc(id: idkey) -> i32 {a := clm_bpaa_find_array(id); if a == nil {return -1}; a.sealed = 0; return 0}
/*
Retrieves the number of initialized items in an array.
id: Array ID to query.
count: Receives the number of initialized items.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for a missing array/null output.
*/
clm_bpaa_get_count_to :: proc(id: idkey, count: ^u32) -> i32 {a := clm_bpaa_find_array(id); if a == nil || count == nil {return -1}; count^ = a.count; return 1}
/*
Retrieves the number of item slots currently reserved by an array.
id: Array ID to query.
capacity: Receives the number of reserved item slots.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for a missing array/null output.
*/
clm_bpaa_get_capacity_to :: proc(id: idkey, capacity: ^u32) -> i32 {a := clm_bpaa_find_array(id); if a == nil || capacity == nil {return -1}; capacity^ = a.capacity; return 1}
/*
Retrieves the current array cursor position.
id: Array ID to query.
index: Receives the zero-based cursor position.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid output/state.
*/
clm_bpaa_get_current_index_to :: proc(id: idkey, index: ^i32) -> i32 {a := clm_bpaa_find_array(id); if a == nil || index == nil || a.index > 0x7fff_ffff {return -1}; index^ = i32(a.index); return 1}
/*
Retrieves an array's sealed state.
id: Array ID to query.
sealed: Receives zero when unsealed or nonzero when sealed.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid output/array.
*/
clm_bpaa_is_sealed_to :: proc(id: idkey, sealed: ^u8) -> i32 {a := clm_bpaa_find_array(id); if a == nil || sealed == nil {return -1}; sealed^ = a.sealed; return 1}
/*
Retrieves the current item's string length, excluding its NUL terminator.
id: String-array ID to query.
length: Receives the current string's byte length, excluding NUL.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/current-empty state.
*/
clm_bpaa_get_length_to :: proc(id: idkey, length: ^u32) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || length == nil || a.type != 11 || a.index >= a.count {return -1}
	length^ = u32(len(a.items.strings[a.index])); return 1
}
/*
Copies the first fixed-size array item to the caller's buffer.
id: Array ID of a non-string fixed-width array.
buffer: Receives the first item's bytes.
buffer_size: Destination capacity in bytes; must fit one item.
Returns: CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for empty/invalid input.
*/
clm_bpaa_get_first_raw :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {return clm_bpaa_get_raw(id, 0, buffer, buffer_size)}
/*
Copies the last fixed-size array item to the caller's buffer.
id: Array ID of a non-string fixed-width array.
buffer: Receives the last item's bytes.
buffer_size: Destination capacity in bytes; must fit one item.
Returns: CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for empty/invalid input.
*/
clm_bpaa_get_last_raw :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.count == 0 || a.count-1 > 0x7fff_ffff {return -1}
	return clm_bpaa_get_raw(id, i32(a.count-1), buffer, buffer_size)
}
/*
Copies the item at the array's current cursor into the caller's buffer.
id: Array ID of a non-string fixed-width array.
buffer: Receives the current item's bytes.
buffer_size: Destination capacity in bytes; must fit one item.
Returns: CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for invalid state.
*/
clm_bpaa_get_current_raw :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.index >= a.count || a.index > 0x7fff_ffff {return -1}
	return clm_bpaa_get_raw(id, i32(a.index), buffer, buffer_size)
}
bpa_range :: proc(a: ^clm_bpaa_array_node, start, length: i32) -> bool {return a != nil && start >= 0 && length >= 0 && u32(start) <= a.count && u32(length) <= a.count-u32(start)}
/*
Borrows a contiguous fixed-width slice without copying or retaining the array.
The returned pointer is invalidated by array deletion/shutdown or any capacity
change. Keep the source array alive and do not mutate it while using the view.
id: ID of a non-string fixed-width array.
start: Zero-based first element to include.
length: Number of elements to borrow.
buffer: Receives the first sliced byte; nil for an empty slice.
buffer_size: Receives the byte size of the slice.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range.
*/
clm_bpaa_slice_borrow :: proc(id: idkey, start, length: i32, buffer: ^rawptr, buffer_size: ^int) -> i32 {
	if buffer == nil || buffer_size == nil {return -1}
	buffer^ = nil; buffer_size^ = 0
	a := clm_bpaa_find_array(id)
	if !bpa_range(a, start, length) || a.type == 11 || a.block_size == 0 || u64(length)*u64(a.block_size) > u64(max(int)) {return -1}
	if length != 0 {buffer^ = &a.items.blocks[int(start)*int(a.block_size)]; buffer_size^ = int(length)*int(a.block_size)}
	return 0
}
/*
Copies a fixed-width BPAA slice to caller storage (no NUL/string pointers).
buffer_size is in BYTES; start/length are in ELEMENTS. A zero-length slice
succeeds without touching buffer. Caller destination may overlap source.
id: ID of a non-string fixed-width array.
start: Zero-based first element to copy.
length: Number of elements to copy.
buffer: Destination for the copied bytes; may be nil for an empty slice.
buffer_size: Destination capacity in bytes.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range/capacity.
*/
clm_bpaa_slice_to :: proc(id: idkey, start, length: i32, buffer: rawptr, buffer_size: int) -> i32 {
	p: rawptr; n: int
	if clm_bpaa_slice_borrow(id, start, length, &p, &n) != 0 {return -1}
	if length == 0 {return 0}
	if buffer == nil || buffer_size < n {return -1}
	mem.copy(buffer, p, n); return 0
}
/*
Creates a new BPAA containing [start, start + length) from source.
Fixed-width elements are bulk-copied; string elements are deep-copied.
The destination keeps the source type, item width and encoding metadata.
source_id: ID of the array to slice.
start: Zero-based first element to include.
length: Number of elements to copy.
destination_id: Unused ID for the new independent array.
Returns: CLM_BPA_SUCCESS or CLM_BPA_FAILURE; no destination remains on failure.
*/
clm_bpaa_slice :: proc(source_id: idkey, start, length: i32, destination_id: idkey) -> i32 {
	a := clm_bpaa_find_array(source_id)
	if !bpa_range(a, start, length) || clm_bpaa_is_existing(destination_id) {return -1}
	size: u32; if a.type == 0 {size = a.block_size}
	if clm_bpaa_create_sized(destination_id, a.type, length, size, a.encoding) != 0 {return -1}
	d := clm_bpaa_find_array(destination_id)
	if a.type == 11 {
		for i in 0..<length {
			p := a.items.strings[start+i]
			if bpa_insert(d, i, rawptr(p), u32(len(p)+1)) != 0 {clm_bpaa_delete(destination_id); return -1}
		}
	} else if length != 0 {mem.copy(d.items.blocks, &a.items.blocks[int(start)*int(a.block_size)], int(length)*int(a.block_size)); d.count = u32(length)}
	d.index = 0; d.deck_size = d.count; return 0
}
/*
Exchanges two fixed-size items without changing the item count.
id: ID of a non-string fixed-width array.
index1: Zero-based position of the first item.
index2: Zero-based position of the second item.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid indices/type/OOM.
*/
clm_bpaa_swap :: proc(id: idkey, index1, index2: i32) -> i32 {
	a := clm_bpaa_find_array(id)
	if a == nil || a.type == 11 || index1 < 0 || index2 < 0 || u32(index1) >= a.count || u32(index2) >= a.count {return -1}
	if index1 == index2 {return 0}
	p := ([^]u8)(&a.items.blocks[int(index1)*int(a.block_size)]); q := ([^]u8)(&a.items.blocks[int(index2)*int(a.block_size)])
	for i in 0..<int(a.block_size) {p[i], q[i] = q[i], p[i]}
	a.deck_size = a.count; return 0
}
/*
Copies one fixed-size item over another within the same array.
id: ID of a non-string fixed-width array.
source_index: Zero-based position of the item to copy.
dest_index: Zero-based destination position to overwrite.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid array/index.
*/
clm_bpaa_copy :: proc(id: idkey, source_index, dest_index: i32) -> i32 {
	a := clm_bpaa_find_array(id)
	if a == nil || a.type == 11 || source_index < 0 || dest_index < 0 || u32(source_index) >= a.count || u32(dest_index) >= a.count {return -1}
	mem.copy(&a.items.blocks[int(dest_index)*int(a.block_size)], &a.items.blocks[int(source_index)*int(a.block_size)], int(a.block_size))
	a.deck_size = a.count; return 0
}
/*
Sorts fixed-size items with a qsort-style three-way comparison callback.
id: ID of a non-string fixed-width array to sort.
compare: Comparison callback; returns negative, zero, or positive.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid array/callback.
*/
clm_bpaa_sort_with :: proc(id: idkey, compare: clm_bpaa_compare_fn) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.type == 11 || compare == nil {return -1}
	if a.count < 2 {return 0}
	for i: u32 = 1; i < a.count; i += 1 {
		for j := i; j > 0; j -= 1 {
			if compare(&a.items.blocks[int(j-1)*int(a.block_size)], &a.items.blocks[int(j)*int(a.block_size)]) <= 0 {break}
			clm_bpaa_swap(id, i32(j-1), i32(j))
		}
	}
	a.index = 0; a.deck_size = a.count; return 0
}
/*
Sorts fixed-size items using a predicate that reports out-of-order pairs.
id: ID of a non-string fixed-width array to sort.
predicate: Callback that reports whether adjacent items are out of order.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid array/callback.
*/
clm_bpaa_sort :: proc(id: idkey, predicate: clm_bpaa_predicate_fn) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.type == 11 || predicate == nil {return -1}
	if a.count < 2 {return 0}
	for i: u32 = 1; i < a.count; i += 1 {
		for j := i; j > 0; j -= 1 {
			if !predicate(&a.items.blocks[int(j-1)*int(a.block_size)], &a.items.blocks[int(j)*int(a.block_size)]) {break}
			clm_bpaa_swap(id, i32(j-1), i32(j))
		}
	}
	a.index = 0; a.deck_size = a.count; return 0
}
/*
Reverses the order of all fixed-size items in an array.
id: ID of a non-string array to reverse.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for a missing/string array or OOM.
*/
clm_bpaa_reverse :: proc(id: idkey) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.type == 11 {return -1}
	if a.count < 2 {return 0}
	for i: u32 = 0; i < a.count/2; i += 1 {if clm_bpaa_swap(id, i32(i), i32(a.count-i-1)) != 0 {return -1}}
	a.index = 0; a.deck_size = a.count; return 0
}
/*
Rotates a fixed-size a suffix left or right in place.
a: Live fixed-width a node to modify.
index: Zero-based start of the suffix to rotate.
positions: Number of positions to rotate, reduced modulo the suffix length.
right: Nonzero rotates right; zero rotates left.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid suffix/type/OOM.
*/
clm_bpaa_rotate :: proc(a: ^clm_bpaa_array_node, index, positions: u32, right: i32) -> i32 {
	if a == nil || a.type == 11 || index >= a.count {return -1}
	length := a.count-index; shift := positions%length; if shift == 0 {return 0}
	if a.block_size == 0 || u64(shift)*u64(a.block_size) > u64(max(int)) {return -1}
	n := int(shift)*int(a.block_size); tail := int(length-shift)*int(a.block_size)
	p := bpa_alloc(n); if p == nil {return -1}; defer bpa_free(p)
	dst := &a.items.blocks[int(index)*int(a.block_size)]
	if right != 0 {mem.copy(p, bpa_at(dst, tail), n); mem.copy(bpa_at(dst, n), dst, tail); mem.copy(dst, p, n)
	} else {mem.copy(p, dst, n); mem.copy(dst, bpa_at(dst, n), tail); mem.copy(bpa_at(dst, tail), p, n)}
	a.index = 0; a.deck_size = a.count; return 0
}
/*
Rotates the suffix beginning at index left by positions.
id: ID of a non-string fixed-width array.
index: Zero-based start of the suffix to rotate.
positions: Number of positions to rotate the suffix left.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
*/
clm_bpaa_shift_left :: proc(id: idkey, index, positions: u32) -> i32 {return clm_bpaa_rotate(clm_bpaa_find_array(id), index, positions, 0)}
/*
Rotates the suffix beginning at index right by positions.
id: ID of a non-string fixed-width array.
index: Zero-based start of the suffix to rotate.
positions: Number of positions to rotate the suffix right.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
*/
clm_bpaa_shift_right :: proc(id: idkey, index, positions: u32) -> i32 {return clm_bpaa_rotate(clm_bpaa_find_array(id), index, positions, 1)}
/*
Randomly shuffles fixed-size array items and resets the draw deck.
id: ID of a non-string array; uses Odin's core:math/rand state.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for a missing/string array.
*/
clm_bpaa_shuffle :: proc(id: idkey) -> i32 {
	a := clm_bpaa_find_array(id); if a == nil || a.type == 11 || a.count > 0x7fff_ffff {return -1}
	for i := a.count; i > 1; i -= 1 {if clm_bpaa_swap(id, i32(i-1), i32(rand.uint32()%i)) != 0 {return -1}}
	a.deck_size = a.count; a.index = 0; return 0
}
/*
Draws a selected item from the active deck and removes it from that deck.
id: Array ID of a non-string fixed-width array.
index: Zero-based position within the active draw deck.
buffer: Receives the drawn item bytes.
buffer_size: Destination capacity in bytes; must fit one item.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if invalid/exhausted.
*/
clm_bpaa_draw_at_raw :: proc(id: idkey, index: i32, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id)
	if a == nil || a.type == 11 || buffer == nil || buffer_size < a.block_size || index < 0 || u32(index) >= a.deck_size {return -1}
	last := a.deck_size-1
	if u32(index) != last {if clm_bpaa_swap(id, index, i32(last)) != 0 {return -1}}
	mem.copy(buffer, &a.items.blocks[int(last)*int(a.block_size)], int(a.block_size))
	a.deck_size = last; return 1
}
/*
Draws the next item, reshuffling the array when its deck is exhausted.
id: ID of a nonempty fixed-size array.
buffer: Receives the drawn item bytes.
buffer_size: Destination capacity in bytes; must fit one item.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/empty input.
*/
clm_bpaa_draw_raw :: proc(id: idkey, buffer: rawptr, buffer_size: u32) -> i32 {
	a := clm_bpaa_find_array(id)
	if a == nil || a.type == 11 || buffer == nil || buffer_size < a.block_size {return -1}
	if a.deck_size == 0 {if a.count == 0 || clm_bpaa_shuffle(id) != 0 {return -1}}
	return clm_bpaa_draw_at_raw(id, 0, buffer, buffer_size)
}
/*
Returns a temporary direct pointer to a block's payload and its byte size.
id: ID of the block whose payload to borrow.
address: Receives the borrowed payload pointer.
size: Receives the payload size in bytes, including NUL for strings.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input.
@note Do not retain the pointer across deletion or string replacement.
*/
clm_bpa_borrow :: proc(id: idkey, address: ^rawptr, size: ^int) -> i32 {
	b := clm_bpa_find_block(id); if b == nil || address == nil || size == nil {return -1}
	address^ = b.data; size^ = int(b.block_size); if b.type == 11 {size^ = int(b.length)+1}; return 1
}
/*
Copies one block's value into another pre-created block of a compatible type.
source_id: ID of the block to copy from.
destination_id: ID of the compatible block to overwrite.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if types/sizes are incompatible.
*/
clm_bpa_copy_block :: proc(source_id, destination_id: idkey) -> i32 {
	s := clm_bpa_find_block(source_id); d := clm_bpa_find_block(destination_id)
	if s == nil || d == nil || s.type != d.type {return -1}
	if s.type == 11 {if s.data == nil {return -1}; if clm_bpa_set_string(destination_id, s.data, s.length+1) < 0 {return -1}; return 0}
	if s.block_size != d.block_size {return -1}
	mem.copy(d.data, s.data, int(s.block_size)); return 0
}
bpa_compatible :: proc(a: ^clm_bpaa_array_node, b: ^clm_bpa_block_node, expected_size: int) -> bool {
	return a != nil && b != nil && a.type != 11 && b.type != 11 && (a.type == 0 || a.type == b.type) && a.block_size == b.block_size && expected_size == int(a.block_size)
}
/*
Copies a fixed-size array item into a compatible BPA block.
array_id: Source array; index selects the item to copy.
block_id: Destination block; expected_size must match the item size.
index: Zero-based position of the item to copy.
expected_size: Expected byte size of the array item and destination block.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
*/
clm_bpaa_get :: proc(array_id: idkey, index: i32, block_id: idkey, expected_size: int) -> i32 {
	a := clm_bpaa_find_array(array_id); b := clm_bpa_find_block(block_id)
	if !bpa_compatible(a, b, expected_size) {return -1}; return clm_bpaa_get_raw(array_id, index, b.data, b.block_size)
}
/*
Copies a compatible BPA block's value over an existing fixed-size array item.
array_id: Destination array; index selects the item to overwrite.
block_id: Source block; expected_size must match both payload sizes.
index: Zero-based position of the existing item to overwrite.
expected_size: Expected byte size of the array item and source block.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
*/
clm_bpaa_set :: proc(array_id: idkey, index: i32, block_id: idkey, expected_size: int) -> i32 {
	a := clm_bpaa_find_array(array_id); b := clm_bpa_find_block(block_id)
	if !bpa_compatible(a, b, expected_size) {return -1}; return clm_bpaa_set_raw(array_id, index, b.data, b.block_size)
}
/*
Inserts a compatible BPA block's value into a fixed-size array.
array_id: Destination array; index may equal its current item count.
block_id: Source block; expected_size must match the item size.
index: Zero-based insertion position, from zero through the current count.
expected_size: Expected byte size of the array item and source block.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for incompatible input/growth.
*/
clm_bpaa_insert :: proc(array_id: idkey, index: i32, block_id: idkey, expected_size: int) -> i32 {
	a := clm_bpaa_find_array(array_id); b := clm_bpa_find_block(block_id)
	if !bpa_compatible(a, b, expected_size) {return -1}; return clm_bpaa_insert_raw(array_id, index, b.data, b.block_size)
}
/*
Appends a compatible BPA block's value to a fixed-size array.
array_id: Destination array; block_id identifies the source block.
block_id: Source block whose value is appended.
expected_size: Expected byte size of the array item and source block.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for incompatible input/growth.
*/
clm_bpaa_append :: proc(array_id, block_id: idkey, expected_size: int) -> i32 {
	a := clm_bpaa_find_array(array_id); if a == nil || a.count > 0x7fff_ffff {return -1}
	return clm_bpaa_insert(array_id, i32(a.count), block_id, expected_size)
}
/*
Copies a string array item into a pre-created string BPA block.
array_id: Source string array; index selects the string.
block_id: Destination string block whose current value is replaced.
index: Zero-based position of the string to copy.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid resource types.
*/
clm_bpaa_get_string :: proc(array_id: idkey, index: i32, block_id: idkey) -> i32 {
	a := clm_bpaa_find_array(array_id); b := clm_bpa_find_block(block_id)
	if a == nil || b == nil || a.type != 11 || b.type != 11 || index < 0 || u32(index) >= a.count {return -1}
	p := a.items.strings[index]; if clm_bpa_set_string(block_id, rawptr(p), u32(len(p)+1)) < 0 {return -1}; return 1
}
/*
Replaces an existing string array item with a string BPA block's value.
array_id: Destination string array; index selects the item to replace.
block_id: Source string block.
index: Zero-based position of the existing string to replace.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid resource/index.
*/
clm_bpaa_set_string :: proc(array_id: idkey, index: i32, block_id: idkey) -> i32 {
	b := clm_bpa_find_block(block_id); if b == nil || b.type != 11 || b.data == nil {return -1}
	if clm_bpaa_set_string_raw(array_id, index, b.data, b.length+1) < 0 {return -1}; return 2
}
/*
Inserts a string BPA block's value at an index in a string array.
array_id: Destination string array; index may equal its item count.
block_id: Source string block; its value is copied independently.
index: Zero-based insertion position, from zero through the current count.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth.
*/
clm_bpaa_insert_string :: proc(array_id: idkey, index: i32, block_id: idkey) -> i32 {
	b := clm_bpa_find_block(block_id); if b == nil || b.type != 11 || b.data == nil {return -1}
	return clm_bpaa_insert_raw(array_id, index, b.data, b.length+1)
}
/*
Appends a string BPA block's value to a string array.
array_id: Destination string array.
block_id: Source string block; its value is copied independently.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth.
*/
clm_bpaa_append_string :: proc(array_id, block_id: idkey) -> i32 {
	a := clm_bpaa_find_array(array_id); if a == nil || a.count > 0x7fff_ffff {return -1}
	return clm_bpaa_insert_string(array_id, i32(a.count), block_id)
}
/*
Draws a fixed-size item into a compatible BPA block.
array_id: Non-string array; block_id is the destination block.
block_id: Destination block that receives the drawn item.
expected_size: Expected byte size of the array item and destination block.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if empty/incompatible.
*/
clm_bpaa_draw :: proc(array_id, block_id: idkey, expected_size: int) -> i32 {
	a := clm_bpaa_find_array(array_id); b := clm_bpa_find_block(block_id)
	if !bpa_compatible(a, b, expected_size) {return -1}; return clm_bpaa_draw_raw(array_id, b.data, b.block_size)
}
/*
Draws a selected active-deck item into a compatible BPA block.
array_id: Non-string array; index selects a position in the current deck.
block_id: Destination block; expected_size must match both item sizes.
index: Zero-based position within the active draw deck.
expected_size: Expected byte size of the array item and destination block.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/exhausted input.
*/
clm_bpaa_draw_at :: proc(array_id: idkey, index: i32, block_id: idkey, expected_size: int) -> i32 {
	a := clm_bpaa_find_array(array_id); b := clm_bpa_find_block(block_id)
	if !bpa_compatible(a, b, expected_size) {return -1}; return clm_bpaa_draw_at_raw(array_id, index, b.data, b.block_size)
}
clm_bpa_get_to :: clm_bpa_get
clm_bpa_set_from :: clm_bpa_set
clm_bpa_get_string_to :: clm_bpa_get_string
clm_bpa_set_string_from :: clm_bpa_set_string
clm_bpaa_get_to :: clm_bpaa_get_raw
clm_bpaa_get_first_to :: clm_bpaa_get_first_raw
clm_bpaa_get_last_to :: clm_bpaa_get_last_raw
clm_bpaa_get_current_to :: clm_bpaa_get_current_raw
clm_bpaa_get_string_to :: clm_bpaa_get_string_raw
clm_bpaa_set_from :: clm_bpaa_set_raw
clm_bpaa_set_string_from :: clm_bpaa_set_string_raw
clm_bpaa_insert_from :: clm_bpaa_insert_raw
clm_bpaa_append_from :: clm_bpaa_append_raw
clm_bpaa_draw_to :: clm_bpaa_draw_raw
clm_bpaa_draw_at_to :: clm_bpaa_draw_at_raw
clm_bpaa_write_u32 :: proc(id: idkey, value: u32) -> i32 {b := clm_bpa_find_block(id); if b == nil || b.type != 6 || b.block_size != 4 || b.data == nil {return -1}; v := value; mem.copy(b.data, &v, 4); return 1}
clm_bpaa_write_i32 :: proc(id: idkey, value: i32) -> i32 {b := clm_bpa_find_block(id); if b == nil || b.type != 5 || b.block_size != 4 || b.data == nil {return -1}; v := value; mem.copy(b.data, &v, 4); return 1}
clm_bpaa_write_byte :: proc(id: idkey, value: u8) -> i32 {b := clm_bpa_find_block(id); if b == nil || b.type != 2 || b.block_size != 1 || b.data == nil {return -1}; v := value; mem.copy(b.data, &v, 1); return 1}
clm_bpaa_get_count :: proc(id, destination_id: idkey) -> i32 {v: u32; if clm_bpaa_get_count_to(id, &v) != 1 {return -1}; return clm_bpaa_write_u32(destination_id, v)}
clm_bpaa_get_capacity :: proc(id, destination_id: idkey) -> i32 {v: u32; if clm_bpaa_get_capacity_to(id, &v) != 1 {return -1}; return clm_bpaa_write_u32(destination_id, v)}
clm_bpaa_get_length :: proc(id, destination_id: idkey) -> i32 {v: u32; if clm_bpaa_get_length_to(id, &v) != 1 {return -1}; return clm_bpaa_write_u32(destination_id, v)}
clm_bpaa_get_current_index :: proc(id, destination_id: idkey) -> i32 {v: i32; if clm_bpaa_get_current_index_to(id, &v) != 1 {return -1}; return clm_bpaa_write_i32(destination_id, v)}
clm_bpaa_is_sealed :: proc(id, destination_id: idkey) -> i32 {v: u8; if clm_bpaa_is_sealed_to(id, &v) != 1 {return -1}; return clm_bpaa_write_byte(destination_id, v)}
clm_bpaa_get_first :: proc(id, destination_id: idkey, expected_size: int) -> i32 {return clm_bpaa_get(id, 0, destination_id, expected_size)}
clm_bpaa_get_last :: proc(id, destination_id: idkey, expected_size: int) -> i32 {a := clm_bpaa_find_array(id); if a == nil || a.count == 0 || a.count-1 > 0x7fff_ffff {return -1}; return clm_bpaa_get(id, i32(a.count-1), destination_id, expected_size)}
clm_bpaa_get_current :: proc(id, destination_id: idkey, expected_size: int) -> i32 {a := clm_bpaa_find_array(id); if a == nil || a.index >= a.count || a.index > 0x7fff_ffff {return -1}; return clm_bpaa_get(id, i32(a.index), destination_id, expected_size)}
/*
Releases all pool slabs and registry storage after all BPA resources are gone.
Returns: CLM_BPA_SUCCESS when cleanup completes; failure if pooled blocks remain.
@note Call at process/module shutdown; it invalidates registry state.
*/
clm_bpa_shutdown :: proc() -> i32 {
	for i in 0..<clm_bpa_global_arrays_capacity {a := clm_bpa_global_arrays[i]; if a != nil && a != CLM_BPA_ARRAY_TOMBSTONE {clm_bpa_remove_array(a.id)}}
	bpa_free(clm_bpa_global_arrays); clm_bpa_global_arrays = nil
	clm_bpa_global_arrays_capacity = 0; clm_bpa_global_arrays_count = 0; clm_bpa_global_arrays_tombstones = 0
	for i in 0..<clm_bpa_global_blocks_capacity {b := clm_bpa_global_blocks[i]; if b != nil && b != CLM_BPA_BLOCK_TOMBSTONE {clm_bpa_remove_block(b.id)}}
	bpa_free(clm_bpa_global_blocks); clm_bpa_global_blocks = nil
	clm_bpa_global_blocks_capacity = 0; clm_bpa_global_blocks_count = 0; clm_bpa_global_blocks_tombstones = 0
	return clm_bpa_pool_cleanup()
}
