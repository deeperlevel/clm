package clm

import "core:strings"

ID_STORAGE_ID :: idkey(0x0F000001)
CLM_ID_INDEX_STORAGE_ID :: idkey(0x0F000002)
CLM_ID_NAMES_STORAGE_ID :: idkey(0x0F000004)
CLM_ID_VALUES_STORAGE_ID :: idkey(0x0F000005)
CLM_ID_NAME_BUFFER_ID :: idkey(0x0F000003)
CLM_ID_NOT_FOUND :: ~idkey(0)

/*
Global ID-generator and registry state. Shared by the clm package.
*/
clm_id_generator_value: idkey
clm_id_next_entry_value: idkey
clm_id_active_index := ID_STORAGE_ID
clm_id_generator_exhausted: bool
clm_id_case_sensitive := true

clm_id_internal_fold :: proc(c: u8, sensitive: bool) -> u8 {
	return c + ('a' - 'A') if !sensitive && c >= 'A' && c <= 'Z' else c
}

/*
Hashes a name, applying this binding's ASCII case-folding policy.
*/
clm_id_internal_hash_name :: proc(name: string, case_sensitive: bool) -> u32 {
	hash := u32(2166136261)
	for i in 0..<len(name) {
		if name[i] == 0 { break }
		hash = (hash ~ u32(clm_id_internal_fold(name[i], case_sensitive))) * 16777619
	}
	return hash
}

/*
Validates the UTF-8 bytes in a NUL-terminated name.
*/
clm_id_internal_valid_name :: proc(name: string) -> bool {
	i := 0
	for i < len(name) && name[i] != 0 {
		c := name[i]
		if c <= 0x7f { i += 1; continue }
		width: int
		if c >= 0xc2 && c <= 0xdf { width = 2
		} else if c >= 0xe0 && c <= 0xef { width = 3
		} else if c >= 0xf0 && c <= 0xf4 { width = 4
		} else { return false }
		if i + width > len(name) { return false }
		for j in 1..<width { if name[i+j] < 0x80 || name[i+j] > 0xbf { return false } }
		if (c == 0xe0 && name[i+1] < 0xa0) || (c == 0xed && name[i+1] >= 0xa0) ||
		   (c == 0xf0 && name[i+1] < 0x90) || (c == 0xf4 && name[i+1] >= 0x90) { return false }
		i += width
	}
	return true
}

clm_id_internal_name :: proc(name: string) -> string {
	for i in 0..<len(name) { if name[i] == 0 { return name[:i] } }
	return name
}

/*
Compares names exactly, or with ASCII-only case folding when requested.
*/
clm_id_internal_names_equal :: proc(left, right: string, sensitive: bool) -> bool {
	a := clm_id_internal_name(left)
	b := clm_id_internal_name(right)
	if len(a) != len(b) { return false }
	for i in 0..<len(a) { if clm_id_internal_fold(a[i], sensitive) != clm_id_internal_fold(b[i], sensitive) { return false } }
	return true
}

/*
Initializes private dictionaries and the shared BPA string scratch block.
*/
clm_id_internal_ensure_storage :: proc() -> i32 {
	ids := [4]idkey{ID_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_VALUES_STORAGE_ID}
	ready := true
	for id in ids { if clm_dictionary_count(id) < 0 { ready = false } }
	buffer := clm_bpa_find_block(CLM_ID_NAME_BUFFER_ID)
	if ready && buffer != nil && buffer.type == CLM_BPA_TYPE_STRING { return CLM_BPA_SUCCESS }
	for id in ([5]idkey{ID_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_VALUES_STORAGE_ID, CLM_ID_NAME_BUFFER_ID}) {
		if clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) { return CLM_BPA_FAILURE }
	}
	for id, i in ids {
		kind := u8(CLM_BPA_TYPE_STRUCT)
		width := size_of(idkey)
		if id == CLM_ID_NAMES_STORAGE_ID { kind = CLM_BPA_TYPE_STRING; width = 0 }
		if clm_dictionary_new(id, kind, width) != CLM_BPA_SUCCESS {
			for j in 0..<i { clm_dictionary_delete(ids[j]) }
			return CLM_BPA_FAILURE
		}
	}
	if clm_bpa_create(CLM_ID_NAME_BUFFER_ID, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS {
		for id in ids { clm_dictionary_delete(id) }
		return CLM_BPA_FAILURE
	}
	clm_id_active_index = ID_STORAGE_ID
	clm_id_next_entry_value = 0
	return CLM_BPA_SUCCESS
}

/*
Copies one internally stored name into caller-owned temporary memory.
*/
clm_id_internal_copy_name :: proc(entry_id: idkey) -> (name: string, ok: bool) {
	if clm_dictionary_get(CLM_ID_NAMES_STORAGE_ID, entry_id, CLM_ID_NAME_BUFFER_ID) != CLM_BPA_GET_SUCCESS { return }
	b := clm_bpa_find_block(CLM_ID_NAME_BUFFER_ID)
	if b == nil || b.type != CLM_BPA_TYPE_STRING || b.data == nil || b.length == 0xffffffff { return }
	name = strings.clone(string((cast([^]u8)b.data)[:int(b.length)]))
	ok = true
	return
}

/*
Finds a name in an index dictionary, including tombstone-aware probing.
*/
clm_id_internal_find_name :: proc(index_id: idkey, name: string, sensitive: bool) -> (entry_id, slot_key: idkey, found, ok: bool) {
	count := clm_dictionary_count(index_id)
	if count < 0 { return }
	hash := clm_id_internal_hash_name(name, sensitive)
	tombstone := CLM_ID_NOT_FOUND
	for probe in 0..=u32(count) {
		candidate := idkey(hash + probe)
		if !clm_dictionary_contains(index_id, candidate) {
			slot_key = tombstone if tombstone != CLM_ID_NOT_FOUND else candidate
			ok = true
			return
		}
		entry: idkey
		if clm_dictionary_get_to(index_id, candidate, &entry, size_of(entry)) != CLM_BPA_GET_SUCCESS { return }
		if entry == CLM_ID_NOT_FOUND {
			if tombstone == CLM_ID_NOT_FOUND { tombstone = candidate }
			continue
		}
		stored, copied := clm_id_internal_copy_name(entry)
		if !copied { return }
		matches := clm_id_internal_names_equal(stored, name, sensitive)
		delete(stored)
		if matches { entry_id = entry; slot_key = candidate; found = true; ok = true; return }
	}
	if tombstone != CLM_ID_NOT_FOUND { slot_key = tombstone; ok = true }
	return
}

/*
Allocates an unused internal entry key without colliding with active entries.
*/
clm_id_internal_allocate_entry_id :: proc(out_id: ^idkey) -> i32 {
	count := clm_dictionary_count(CLM_ID_NAMES_STORAGE_ID)
	if count < 0 || out_id == nil { return CLM_BPA_FAILURE }
	for attempt in 0..=u32(count) {
		candidate := clm_id_next_entry_value + idkey(attempt)
		if candidate != CLM_ID_NOT_FOUND && !clm_dictionary_contains(CLM_ID_NAMES_STORAGE_ID, candidate) {
			out_id^ = candidate
			clm_id_next_entry_value = 0 if candidate == CLM_ID_NOT_FOUND - 1 else candidate + 1
			return CLM_BPA_SUCCESS
		}
	}
	return CLM_BPA_FAILURE
}

/*
Adds or replaces a hash-index slot with an internal entry key.
*/
clm_id_internal_store_index :: proc(index_id, slot, entry_id: idkey) -> i32 {
	entry := entry_id
	return clm_dictionary_set_from(index_id, slot, &entry, size_of(entry))
}

// Names use Odin strings; they are copied and obey the C ASCII-only case policy.
/*
Inserts a new, nonempty name mapping and rejects duplicate effective names.
Multiple names may map to the same numeric ID.
name: Name to register, compared according to the active case policy.
id: Numeric ID associated with the name.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid/duplicate input.
*/
clm_id_register :: proc(name: string, id: idkey) -> i32 {
	n := clm_id_internal_name(name)
	if len(n) == 0 || u64(len(n)) > 0xffffffff || !clm_id_internal_valid_name(n) ||
	   clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	_, slot, found, ok := clm_id_internal_find_name(clm_id_active_index, n, clm_id_case_sensitive)
	entry: idkey
	if !ok || found || clm_id_internal_allocate_entry_id(&entry) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	text := strings.clone_to_cstring(n)
	defer delete(text)
	if clm_dictionary_set_from(CLM_ID_NAMES_STORAGE_ID, entry, rawptr(text), len(n)+1) != CLM_BPA_SET_SUCCESS { return CLM_BPA_FAILURE }
	value := id
	if clm_dictionary_set_from(CLM_ID_VALUES_STORAGE_ID, entry, &value, size_of(value)) != CLM_BPA_SET_SUCCESS {
		clm_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry)
		return CLM_BPA_FAILURE
	}
	if clm_id_internal_store_index(clm_id_active_index, slot, entry) != CLM_BPA_SET_SUCCESS {
		clm_dictionary_remove(CLM_ID_VALUES_STORAGE_ID, entry)
		clm_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry)
		return CLM_BPA_FAILURE
	}
	return CLM_BPA_SET_SUCCESS
}

/*
Maps a name to an ID; alias for clm_id_register.
name: Nonempty UTF-8 name to register.
id: Numeric ID; multiple distinct names may share it.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for a duplicate/invalid name.
*/
clm_id_map :: clm_id_register

/*
Resolves a name and reports success separately from the mapped ID.
This permits CLM_ID_NOT_FOUND to be used as an ordinary mapped ID.
name: Name to look up.
out_id: Receives the numeric ID; must not be nil.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
*/
clm_id_try_get :: proc(name: string, out_id: ^idkey) -> i32 {
	n := clm_id_internal_name(name)
	if out_id == nil || len(n) == 0 || !clm_id_internal_valid_name(n) ||
	   clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	entry, _, found, ok := clm_id_internal_find_name(clm_id_active_index, n, clm_id_case_sensitive)
	if !ok || !found { return CLM_BPA_FAILURE }
	value: idkey
	if clm_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry, &value, size_of(value)) != CLM_BPA_GET_SUCCESS { return CLM_BPA_FAILURE }
	out_id^ = value
	return CLM_BPA_GET_SUCCESS
}

/*
Resolves a registered name to its numeric ID.
name: Name to look up using the active case policy.
Returns: The mapped ID, or CLM_ID_NOT_FOUND when absent/invalid.
*/
clm_id_get :: proc(name: string) -> idkey {
	value := CLM_ID_NOT_FOUND
	clm_id_try_get(name, &value)
	return value
}

/*
Inserts a mapping or replaces the ID for an existing effective name.
Updating retains the originally registered spelling and insertion order.
name: Name to insert or update.
id: New numeric ID.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
*/
clm_id_set :: proc(name: string, id: idkey) -> i32 {
	n := clm_id_internal_name(name)
	if len(n) == 0 || u64(len(n)) > 0xffffffff || !clm_id_internal_valid_name(n) ||
	   clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	entry, _, found, ok := clm_id_internal_find_name(clm_id_active_index, n, clm_id_case_sensitive)
	if !ok { return CLM_BPA_FAILURE }
	if !found { return clm_id_register(n, id) }
	value := id
	return clm_dictionary_update_from(CLM_ID_VALUES_STORAGE_ID, entry, &value, size_of(value))
}

/*
Removes one mapping by name, leaving mappings for aliases intact.
name: Name to remove under the active case policy.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the name is absent/invalid.
*/
clm_id_remove :: proc(name: string) -> i32 {
	n := clm_id_internal_name(name)
	if len(n) == 0 || !clm_id_internal_valid_name(n) ||
	   clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	entry, slot, found, ok := clm_id_internal_find_name(clm_id_active_index, n, clm_id_case_sensitive)
	if !ok || !found ||
	   clm_id_internal_store_index(clm_id_active_index, slot, CLM_ID_NOT_FOUND) != CLM_BPA_SET_SUCCESS ||
	   clm_dictionary_remove(CLM_ID_VALUES_STORAGE_ID, entry) != CLM_BPA_SUCCESS ||
	   clm_dictionary_remove(CLM_ID_NAMES_STORAGE_ID, entry) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	return CLM_BPA_SUCCESS
}

/*
Removes one mapping by name; equivalent to clm_id_remove.
name: Name to remove.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
*/
clm_id_unmap_name :: clm_id_remove

/*
Removes every registered name that maps to the supplied numeric ID.
id: Numeric ID whose aliases should all be removed.
Returns: CLM_BPA_SUCCESS if at least one mapping was removed; otherwise failure.
*/
clm_id_unmap :: proc(id: idkey) -> i32 {
	if clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	removed := false
	for {
		entry, current: idkey
		found := false
		if clm_dictionary_iterate_reset(CLM_ID_NAMES_STORAGE_ID) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
		for clm_dictionary_iterate_next(CLM_ID_NAMES_STORAGE_ID, &entry) == CLM_BPA_GET_SUCCESS {
			if clm_dictionary_get_to(CLM_ID_VALUES_STORAGE_ID, entry, &current, size_of(current)) != CLM_BPA_GET_SUCCESS { return CLM_BPA_FAILURE }
			if current == id { found = true; break }
		}
		if !found { break }
		name, ok := clm_id_internal_copy_name(entry)
		if !ok { return CLM_BPA_FAILURE }
		status := clm_id_remove(name)
		delete(name)
		if status != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
		removed = true
	}
	return CLM_BPA_SUCCESS if removed else CLM_BPA_FAILURE
}

/*
Returns whether exact-case matching is enabled.
*/
clm_id_is_case_sensitive :: proc() -> bool { return clm_id_case_sensitive }

// Build a second index first, so case-fold collisions cannot alter active state.
/*
Changes name matching policy after validating all existing names.
Case-insensitive matching in this binding folds ASCII letters; bytes outside
ASCII are compared exactly. If folding creates a collision, state is unchanged.
enabled: true for exact matching; false for ASCII case-insensitive.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if validation/allocation fails.
*/
clm_id_set_case_sensitive :: proc(enabled: bool) -> i32 {
	if clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	if enabled == clm_id_case_sensitive { return CLM_BPA_SUCCESS }
	next := CLM_ID_INDEX_STORAGE_ID if clm_id_active_index == ID_STORAGE_ID else ID_STORAGE_ID
	if clm_dictionary_clear(next) != CLM_BPA_SUCCESS ||
	   clm_dictionary_iterate_reset(CLM_ID_NAMES_STORAGE_ID) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	entry: idkey
	for clm_dictionary_iterate_next(CLM_ID_NAMES_STORAGE_ID, &entry) == CLM_BPA_GET_SUCCESS {
		name, copied := clm_id_internal_copy_name(entry)
		if !copied { clm_dictionary_clear(next); return CLM_BPA_FAILURE }
		_, slot, found, ok := clm_id_internal_find_name(next, name, enabled)
		delete(name)
		if !ok || found || clm_dictionary_add_from(next, slot, &entry, size_of(entry)) != CLM_BPA_SET_SUCCESS {
			clm_dictionary_clear(next)
			return CLM_BPA_FAILURE
		}
	}
	clm_id_active_index = next
	clm_id_case_sensitive = enabled
	return CLM_BPA_SUCCESS
}

/*
Removes all mappings while preserving the active matching policy.
*/
clm_id_clear :: proc() -> i32 {
	if clm_id_internal_ensure_storage() != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	for id in ([4]idkey{ID_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_VALUES_STORAGE_ID}) {
		if clm_dictionary_clear(id) != CLM_BPA_SUCCESS { return CLM_BPA_FAILURE }
	}
	clm_id_active_index = ID_STORAGE_ID
	clm_id_next_entry_value = 0
	return CLM_BPA_SUCCESS
}

/*
Releases the private dictionaries and scratch block.
The next mapping operation lazily recreates storage. Registry matching mode
and ID generator state are retained.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if cleanup cannot complete.
*/
clm_id_shutdown :: proc() -> i32 {
	status := i32(CLM_BPA_SUCCESS)
	for id in ([4]idkey{CLM_ID_VALUES_STORAGE_ID, CLM_ID_NAMES_STORAGE_ID, CLM_ID_INDEX_STORAGE_ID, ID_STORAGE_ID}) {
		if clm_dictionary_delete(id) != CLM_BPA_SUCCESS { status = CLM_BPA_FAILURE }
	}
	if clm_bpa_delete(CLM_ID_NAME_BUFFER_ID) != CLM_BPA_SUCCESS { status = CLM_BPA_FAILURE }
	clm_id_active_index = ID_STORAGE_ID
	clm_id_next_entry_value = 0
	return status
}

/*
Resets sequential ID generation so the next call returns zero.
*/
clm_id_gen_reset :: proc() {
	clm_id_generator_value = 0
	clm_id_generator_exhausted = false
}

/*
Returns the next sequential ID, beginning at zero.
Generated IDs are not checked against IDs supplied to registration functions.
After the available range is exhausted, this returns CLM_ID_NOT_FOUND.
*/
clm_id_gen_next :: proc() -> idkey {
	if clm_id_generator_exhausted { return CLM_ID_NOT_FOUND }
	result := clm_id_generator_value
	if result == CLM_ID_NOT_FOUND - 1 { clm_id_generator_exhausted = true
	} else { clm_id_generator_value += 1 }
	return result
}
