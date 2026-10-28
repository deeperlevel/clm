package clm

// BPA owns the metadata, hash slots, ordered keys, and copied values.
CLM_DICTIONARY_INITIAL_CAPACITY :: 16
CLM_DICTIONARY_METADATA_MAGIC :: u64(0x434C4D444943544E)
CLM_DICTIONARY_SLOT_EMPTY :: u8(0)
CLM_DICTIONARY_SLOT_OCCUPIED :: u8(1)
CLM_DICTIONARY_SLOT_DELETED :: u8(2)

/*
Internal open-addressing slot holding one key and its value-array index.
*/
clm_dictionary_slot :: struct {
	key: idkey,
	value_index: u32,
	state: u8,
}

/*
Internal dictionary metadata stored in a BPA STRUCT block.
*/
clm_dictionary_metadata :: struct {
	magic: u64,
	slots_id, values_id, keys_id: idkey,
	value_size, count, iterator: u32,
	value_type: u8,
	reserved: [3]u8,
}

clm_dictionary_internal_metadata :: proc(id: idkey) -> ^clm_bpa_block_node {
	b := clm_bpa_find_block(id)
	if b == nil || b.type != CLM_BPA_TYPE_STRUCT ||
	   b.block_size != size_of(clm_dictionary_metadata) || b.data == nil {
		return nil
	}
	if (^clm_dictionary_metadata)(b.data).magic != CLM_DICTIONARY_METADATA_MAGIC {
		return nil
	}
	return b
}

clm_dictionary_internal_meta :: proc(id: idkey) -> ^clm_dictionary_metadata {
	b := clm_dictionary_internal_metadata(id)
	if b == nil { return nil }
	return (^clm_dictionary_metadata)(b.data)
}

clm_dictionary_internal_read :: proc(id: idkey, metadata: ^clm_dictionary_metadata) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil || metadata == nil { return CLM_BPA_FAILURE }
	metadata^ = m^
	return CLM_BPA_SUCCESS
}

clm_dictionary_internal_array :: proc(id: idkey, kind: u8, width: u32) -> ^clm_bpaa_array_node {
	a := clm_bpaa_find_array(id)
	if a == nil || a.type != kind || (kind == CLM_BPA_TYPE_STRUCT && a.block_size != width) {
		return nil
	}
	return a
}

clm_dictionary_internal_slots :: proc(a: ^clm_bpaa_array_node) -> [^]clm_dictionary_slot {
	if a == nil || a.type != CLM_BPA_TYPE_STRUCT ||
	   a.block_size != size_of(clm_dictionary_slot) || a.count != a.capacity || a.items.blocks == nil {
		return nil
	}
	return cast([^]clm_dictionary_slot)a.items.blocks
}

clm_dictionary_internal_allocate_array_id :: proc(a, b: idkey, out_id: ^idkey) -> i32 {
	if out_id == nil { return CLM_BPA_FAILURE }
	candidate := ~idkey(0)
	for {
		if candidate != a && candidate != b && !clm_bpaa_is_existing(candidate) {
			out_id^ = candidate
			return CLM_BPA_SUCCESS
		}
		if candidate == 0 { return CLM_BPA_FAILURE }
		candidate -= 1
	}
}

clm_dictionary_internal_create_slots :: proc(id: idkey, capacity: u32) -> i32 {
	if capacity == 0 || capacity > 0x7fffffff ||
	   clm_bpaa_create_sized(id, CLM_BPA_TYPE_STRUCT, i32(capacity), size_of(clm_dictionary_slot), 0) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	empty: clm_dictionary_slot
	for _ in 0..<capacity {
		if clm_bpaa_append_raw(id, &empty, size_of(empty)) != CLM_BPA_SUCCESS {
			clm_bpaa_delete(id)
			return CLM_BPA_FAILURE
		}
	}
	return CLM_BPA_SUCCESS
}

clm_dictionary_internal_find_slot :: proc(slots: [^]clm_dictionary_slot, capacity: u32, key: idkey, found: ^bool) -> u32 {
	first_deleted := capacity
	index := u32(clm_bpa_hash_id(key) & int(capacity - 1))
	found^ = false
	for _ in 0..<capacity {
		switch slots[index].state {
		case CLM_DICTIONARY_SLOT_EMPTY:
			return first_deleted if first_deleted != capacity else index
		case CLM_DICTIONARY_SLOT_OCCUPIED:
			if slots[index].key == key { found^ = true; return index }
		case CLM_DICTIONARY_SLOT_DELETED:
			if first_deleted == capacity { first_deleted = index }
		}
		index = (index + 1) & (capacity - 1)
	}
	return first_deleted
}

clm_dictionary_internal_rehash :: proc(id: idkey, m: ^clm_dictionary_metadata, capacity: u32) -> i32 {
	new_id: idkey
	if m == nil || capacity < CLM_DICTIONARY_INITIAL_CAPACITY || capacity > 0x7fffffff ||
	   capacity & (capacity - 1) != 0 ||
	   clm_dictionary_internal_allocate_array_id(id, m.values_id, &new_id) != CLM_BPA_SUCCESS ||
	   clm_dictionary_internal_create_slots(new_id, capacity) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	old := clm_dictionary_internal_array(m.slots_id, CLM_BPA_TYPE_STRUCT, size_of(clm_dictionary_slot))
	slots := clm_dictionary_internal_slots(old)
	next := clm_dictionary_internal_slots(clm_bpaa_find_array(new_id))
	if slots == nil || next == nil {
		clm_bpaa_delete(new_id)
		return CLM_BPA_FAILURE
	}
	for i in 0..<old.capacity {
		if slots[i].state == CLM_DICTIONARY_SLOT_OCCUPIED {
			found: bool
			target := clm_dictionary_internal_find_slot(next, capacity, slots[i].key, &found)
			if found || target == capacity { clm_bpaa_delete(new_id); return CLM_BPA_FAILURE }
			next[target] = slots[i]
		}
	}
	old_id := m.slots_id
	m.slots_id = new_id
	return clm_bpaa_delete(old_id)
}

clm_dictionary_internal_lookup :: proc(id, key: idkey) -> (m: ^clm_dictionary_metadata, a: ^clm_bpaa_array_node, slots: [^]clm_dictionary_slot, index: u32, found: bool) {
	m = clm_dictionary_internal_meta(id)
	if m == nil { return }
	keys := clm_dictionary_internal_array(m.keys_id, CLM_BPA_TYPE_STRUCT, size_of(idkey))
	values := clm_bpaa_find_array(m.values_id)
	if keys == nil || values == nil || keys.count != values.count || keys.count != m.count { m = nil; return }
	a = clm_dictionary_internal_array(m.slots_id, CLM_BPA_TYPE_STRUCT, size_of(clm_dictionary_slot))
	slots = clm_dictionary_internal_slots(a)
	if slots == nil { m = nil; return }
	index = clm_dictionary_internal_find_slot(slots, a.capacity, key, &found)
	return
}

// Fixed-width values require their exact size; BPA bounds and terminates strings.
/*
Creates an empty dictionary for values of the specified BPA kind.
id: Unique dictionary ID; must not already identify a BPA block.
kind: BPA value kind, including STRING or STRUCT.
size: Required byte width for STRUCT; pass zero for other types.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
*/
clm_dictionary_new :: proc(id: idkey, kind: u8, size: int) -> i32 {
	if clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) || size < 0 || u64(size) > 0xffffffff {
		return CLM_BPA_FAILURE
	}
	width := clm_bpa_type_size(kind)
	switch kind {
	case CLM_BPA_TYPE_STRUCT:
		if size == 0 { return CLM_BPA_FAILURE }
		width = u32(size)
	case CLM_BPA_TYPE_STRING:
		if size != 0 { return CLM_BPA_FAILURE }
		width = 0
	case:
		if width == 0 || (size != 0 && size != int(width)) { return CLM_BPA_FAILURE }
	}
	m: clm_dictionary_metadata
	if clm_dictionary_internal_allocate_array_id(id, ~idkey(0), &m.slots_id) != CLM_BPA_SUCCESS ||
	   clm_dictionary_internal_create_slots(m.slots_id, CLM_DICTIONARY_INITIAL_CAPACITY) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	if clm_dictionary_internal_allocate_array_id(id, m.slots_id, &m.values_id) != CLM_BPA_SUCCESS ||
	   clm_bpaa_create_sized(m.values_id, kind, 0, width, 0) != CLM_BPA_SUCCESS {
		clm_bpaa_delete(m.slots_id)
		return CLM_BPA_FAILURE
	}
	if clm_dictionary_internal_allocate_array_id(id, m.values_id, &m.keys_id) != CLM_BPA_SUCCESS ||
	   clm_bpaa_create_sized(m.keys_id, CLM_BPA_TYPE_STRUCT, 0, size_of(idkey), 0) != CLM_BPA_SUCCESS {
		clm_bpaa_delete(m.values_id); clm_bpaa_delete(m.slots_id)
		return CLM_BPA_FAILURE
	}
	if clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT, size_of(m)) != CLM_BPA_SUCCESS {
		clm_bpaa_delete(m.keys_id); clm_bpaa_delete(m.values_id); clm_bpaa_delete(m.slots_id)
		return CLM_BPA_FAILURE
	}
	m.magic = CLM_DICTIONARY_METADATA_MAGIC
	m.value_type = kind
	m.value_size = width
	(^clm_dictionary_metadata)(clm_bpa_find_block(id).data)^ = m
	return CLM_BPA_SUCCESS
}

/*
Deletes a dictionary and its BPA/BPAA-owned storage.
id: Dictionary ID to delete.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if it is missing/invalid.
*/
clm_dictionary_delete :: proc(id: idkey) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil { return CLM_BPA_FAILURE }
	status := i32(CLM_BPA_SUCCESS)
	for array_id in ([3]idkey{m.keys_id, m.values_id, m.slots_id}) {
		if clm_bpaa_delete(array_id) != CLM_BPA_SUCCESS { status = CLM_BPA_FAILURE }
	}
	if clm_bpa_delete(id) != CLM_BPA_SUCCESS { status = CLM_BPA_FAILURE }
	return status
}

/*
Stores or replaces a value from a caller-owned buffer.
STRING input ends at the first NUL within value_size, or uses all value_size bytes;
BPA appends a terminator to its owned copy.
id: Dictionary ID.
key: Integer key to create or replace.
value: Source value bytes.
value_size: Source byte count; fixed-width values require an exact match.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation failure.
*/
clm_dictionary_set_from :: proc(id, key: idkey, value: rawptr, value_size: int) -> i32 {
	m, a, slots, index, found := clm_dictionary_internal_lookup(id, key)
	if value == nil || m == nil || value_size < 0 || u64(value_size) > 0xffffffff ||
	   (m.value_type != CLM_BPA_TYPE_STRING && value_size != int(m.value_size)) { return CLM_BPA_FAILURE }
	values := clm_dictionary_internal_array(m.values_id, m.value_type, m.value_size)
	if values == nil || (found && slots[index].value_index >= values.count) { return CLM_BPA_FAILURE }
	if found {
		status: i32
		if m.value_type == CLM_BPA_TYPE_STRING {
			status = clm_bpaa_set_string_raw(m.values_id, i32(slots[index].value_index), value, u32(value_size))
		} else {
			status = clm_bpaa_set_raw(m.values_id, i32(slots[index].value_index), value, u32(value_size))
		}
		return CLM_BPA_FAILURE if status < 0 else CLM_BPA_SET_SUCCESS
	}
	if m.count >= 0x7fffffff { return CLM_BPA_FAILURE }
	deleted: u32
	for i in 0..<a.capacity { if slots[i].state == CLM_DICTIONARY_SLOT_DELETED { deleted += 1 } }
	if m.count + deleted + 1 >= a.capacity - a.capacity / 4 {
		capacity := a.capacity
		if m.count + 1 >= capacity - capacity / 4 {
			if capacity > 0x7fffffff / 2 { return CLM_BPA_FAILURE }
			capacity *= 2
		}
		if clm_dictionary_internal_rehash(id, m, capacity) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
		a = clm_bpaa_find_array(m.slots_id)
		slots = clm_dictionary_internal_slots(a)
	}
	index = clm_dictionary_internal_find_slot(slots, a.capacity, key, &found)
	if found || index >= a.capacity { return CLM_BPA_FAILURE }
	value_index := m.count
	key_copy := key
	if clm_bpaa_append_raw(m.keys_id, &key_copy, size_of(key_copy)) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	if clm_bpaa_append_raw(m.values_id, value, u32(value_size)) != CLM_BPA_SUCCESS {
		clm_bpaa_remove(m.keys_id, i32(value_index))
		return CLM_BPA_FAILURE
	}
	slots[index] = clm_dictionary_slot{key, value_index, CLM_DICTIONARY_SLOT_OCCUPIED}
	m.count += 1
	m.iterator = 0
	return CLM_BPA_SET_SUCCESS
}

/*
Stores or replaces a copy of a compatible BPA block's value.
id: Dictionary ID.
key: Integer key to create or replace.
source_id: ID of the BPA block whose value to copy.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
*/
clm_dictionary_set :: proc(id, key, source_id: idkey) -> i32 {
	m := clm_dictionary_internal_meta(id)
	b := clm_bpa_find_block(source_id)
	if m == nil || b == nil || b.type != m.value_type ||
	   (b.type != CLM_BPA_TYPE_STRING && (b.data == nil || b.block_size != m.value_size)) { return CLM_BPA_FAILURE }
	if b.type == CLM_BPA_TYPE_STRING {
		empty: u8
		return clm_dictionary_set_from(id, key, &empty if b.data == nil else b.data, int(b.length) + 1)
	}
	return clm_dictionary_set_from(id, key, b.data, int(b.block_size))
}

/*
Retrieves a value into caller-owned memory.
id: Dictionary ID.
key: Key whose value to retrieve.
buffer: Receives the value; strings include their NUL terminator.
buffer_size: Destination capacity in bytes.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if missing/incompatible.
*/
clm_dictionary_get_to :: proc(id, key: idkey, buffer: rawptr, buffer_size: int) -> i32 {
	m, _, slots, index, found := clm_dictionary_internal_lookup(id, key)
	if m == nil || !found || buffer == nil || buffer_size < 0 || u64(buffer_size) > 0xffffffff { return CLM_BPA_FAILURE }
	values := clm_dictionary_internal_array(m.values_id, m.value_type, m.value_size)
	if values == nil || slots[index].value_index >= values.count { return CLM_BPA_FAILURE }
	status: i32
	if m.value_type == CLM_BPA_TYPE_STRING {
		status = clm_bpaa_get_string_raw(m.values_id, i32(slots[index].value_index), buffer, u32(buffer_size))
	} else {
		status = clm_bpaa_get_raw(m.values_id, i32(slots[index].value_index), buffer, u32(buffer_size))
	}
	return CLM_BPA_FAILURE if status < 0 else CLM_BPA_GET_SUCCESS
}

/*
Retrieves a value into a compatible, pre-created BPA block.
id: Dictionary ID.
key: Key whose value to retrieve.
destination_id: Compatible BPA destination block.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if missing/incompatible.
*/
clm_dictionary_get :: proc(id, key, destination_id: idkey) -> i32 {
	m, _, slots, index, found := clm_dictionary_internal_lookup(id, key)
	b := clm_bpa_find_block(destination_id)
	if m == nil || !found || b == nil || b.type != m.value_type ||
	   (b.type != CLM_BPA_TYPE_STRING && b.block_size != m.value_size) { return CLM_BPA_FAILURE }
	values := clm_dictionary_internal_array(m.values_id, m.value_type, m.value_size)
	if values == nil || slots[index].value_index >= values.count { return CLM_BPA_FAILURE }
	if m.value_type == CLM_BPA_TYPE_STRING {
		return clm_bpaa_get_string(m.values_id, i32(slots[index].value_index), destination_id)
	}
	return clm_bpaa_get(m.values_id, i32(slots[index].value_index), destination_id, int(m.value_size))
}

/*
Checks whether a key currently exists.
id: Dictionary ID.
key: Integer key to check.
Returns: true when present; false when absent or the dictionary is invalid.
*/
clm_dictionary_contains :: proc(id, key: idkey) -> bool {
	m, _, _, _, found := clm_dictionary_internal_lookup(id, key)
	return m != nil && found
}

/*
Returns the number of key/value pairs, or CLM_BPA_FAILURE if invalid.
id: Dictionary ID to query.
Returns: Entry count as clm_int, or CLM_BPA_FAILURE.
*/
clm_dictionary_count :: proc(id: idkey) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil || m.count > 0x7fffffff { return CLM_BPA_FAILURE }
	return i32(m.count)
}

/*
Removes a key and its value, preserving all other mappings.
id: Dictionary ID.
key: Integer key to remove.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the key is absent/invalid.
*/
clm_dictionary_remove :: proc(id, key: idkey) -> i32 {
	m, a, slots, index, found := clm_dictionary_internal_lookup(id, key)
	if m == nil || !found { return CLM_BPA_FAILURE }
	values := clm_dictionary_internal_array(m.values_id, m.value_type, m.value_size)
	removed := slots[index].value_index
	if values == nil || removed >= values.count || removed > 0x7fffffff ||
	   clm_bpaa_remove(m.values_id, i32(removed)) != CLM_BPA_SUCCESS ||
	   clm_bpaa_remove(m.keys_id, i32(removed)) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	slots[index].state = CLM_DICTIONARY_SLOT_DELETED
	m.count -= 1
	m.iterator = 0
	for i in 0..<a.capacity {
		if slots[i].state == CLM_DICTIONARY_SLOT_OCCUPIED && slots[i].value_index > removed { slots[i].value_index -= 1 }
	}
	return CLM_BPA_SUCCESS
}

/*
Resets ordered key iteration to the first currently stored key.
id: Dictionary ID.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if invalid.
*/
clm_dictionary_iterate_reset :: proc(id: idkey) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil { return CLM_BPA_FAILURE }
	m.iterator = 0
	return CLM_BPA_SUCCESS
}

// Structural changes reset iteration; value replacement preserves its cursor.
/*
Returns the next key in insertion order.
The returned key can be passed to clm_dictionary_get or
clm_dictionary_get_to to retrieve its value. Adding/removing keys or clearing
the dictionary resets iteration; replacing a value does not.
id: Dictionary ID.
key: Receives the next key; must not be nil.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE when exhausted/invalid.
*/
clm_dictionary_iterate_next :: proc(id: idkey, key: ^idkey) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil || key == nil { return CLM_BPA_FAILURE }
	keys := clm_dictionary_internal_array(m.keys_id, CLM_BPA_TYPE_STRUCT, size_of(idkey))
	if keys == nil || keys.count != m.count || m.iterator >= m.count ||
	   clm_bpaa_get_raw(m.keys_id, i32(m.iterator), key, size_of(idkey)) != CLM_BPA_GET_SUCCESS { return CLM_BPA_FAILURE }
	m.iterator += 1
	return CLM_BPA_GET_SUCCESS
}

/*
Removes every entry while retaining the dictionary ID and configured value type.
Reserved BPAA capacity is retained; the iteration cursor is reset.
id: Dictionary ID.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the dictionary is invalid.
*/
clm_dictionary_clear :: proc(id: idkey) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil { return CLM_BPA_FAILURE }
	a := clm_dictionary_internal_array(m.slots_id, CLM_BPA_TYPE_STRUCT, size_of(clm_dictionary_slot))
	slots := clm_dictionary_internal_slots(a)
	if slots == nil || clm_bpaa_clear(m.values_id) != CLM_BPA_SUCCESS ||
	   clm_bpaa_clear(m.keys_id) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	for i in 0..<a.capacity { slots[i] = {} }
	m.count = 0
	m.iterator = 0
	return CLM_BPA_SUCCESS
}

/*
Inserts a new key/value pair, failing if the key already exists.
The value is copied from caller-owned memory and follows set_from's type and
size rules. Successful insertion places the key at the end of iteration order.
id: Dictionary ID.
key: New key; must not already exist.
value: Source value bytes.
size: Source byte count.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for duplicate/invalid input.
*/
clm_dictionary_add_from :: proc(id, key: idkey, value: rawptr, size: int) -> i32 {
	if clm_dictionary_count(id) < 0 || clm_dictionary_contains(id, key) { return CLM_BPA_FAILURE }
	return clm_dictionary_set_from(id, key, value, size)
}

/*
Replaces the value for an existing key without changing its insertion order.
The value is copied from caller-owned memory and follows set_from's type and
size rules.
id: Dictionary ID.
key: Existing key to update.
value: Replacement value bytes.
size: Replacement byte count.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE if the key is missing/invalid.
*/
clm_dictionary_update_from :: proc(id, key: idkey, value: rawptr, size: int) -> i32 {
	if !clm_dictionary_contains(id, key) { return CLM_BPA_FAILURE }
	return clm_dictionary_set_from(id, key, value, size)
}

/*
Copies all keys to caller-owned memory in insertion order.
The buffer must hold at least clm_dictionary_count(id) keys. A nil buffer is
allowed only when the dictionary is empty.
id: Dictionary ID.
buffer: Receives the ordered keys.
capacity: Number of clm_id elements available in buffer.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for insufficient capacity or
        an invalid dictionary.
*/
clm_dictionary_keys_to :: proc(id: idkey, buffer: [^]idkey, capacity: int) -> i32 {
	m := clm_dictionary_internal_meta(id)
	if m == nil || capacity < int(m.count) || (m.count != 0 && buffer == nil) { return CLM_BPA_FAILURE }
	for i in 0..<m.count {
		if clm_bpaa_get_raw(m.keys_id, i32(i), &buffer[i], size_of(idkey)) != CLM_BPA_GET_SUCCESS { return CLM_BPA_FAILURE }
	}
	return CLM_BPA_GET_SUCCESS
}
