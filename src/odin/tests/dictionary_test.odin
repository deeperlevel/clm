package tests

import "../clm"
import "core:strings"

dictionary_tests :: proc() -> i32 {
	r: test_result
	test_begin("CLM - Dictionary")
	blocks_before := clm.clm_bpa_global_blocks_count
	arrays_before := clm.clm_bpa_global_arrays_count
	D :: clm.idkey(71000)
	S :: clm.CLM_BPA_SUCCESS
	F :: clm.CLM_BPA_FAILURE
	SET :: clm.CLM_BPA_SET_SUCCESS
	GET :: clm.CLM_BPA_GET_SUCCESS
	assert_test(&r, clm.clm_dictionary_new(D, clm.CLM_BPA_TYPE_INT, 0) == S, "create integer dictionary")
	assert_test(&r, clm.clm_dictionary_count(D) == 0, "starts empty")
	assert_test(&r, clm.clm_dictionary_new(D, clm.CLM_BPA_TYPE_INT, 0) == F, "duplicate ID")
	assert_test(&r, clm.clm_dictionary_new(71003, clm.CLM_BPA_TYPE_STRING, 1) == F, "invalid string size")
	assert_test(&r, clm.clm_bpa_create_sized(71040, clm.CLM_BPA_TYPE_STRUCT, size_of(clm.clm_dictionary_metadata)) == S &&
		clm.clm_dictionary_count(71040) == F && clm.clm_dictionary_delete(71040) == F &&
		clm.clm_bpa_delete(71040) == S, "unrelated struct is not a dictionary")
	inserted := true
	for i: i32 = 0; i < 40; i += 1 {
		item := i * 10
		if clm.clm_dictionary_set_from(D, clm.idkey(1000+i), &item, size_of(item)) != SET { inserted = false }
	}
	assert_test(&r, inserted && clm.clm_dictionary_count(D) == 40, "insert and grow table")
	assert_test(&r, !clm.clm_dictionary_contains(D, 0), "zero absent")
	value: i32
	source := i32(9876)
	assert_test(&r, clm.clm_dictionary_set_from(D, 1005, &source, size_of(source)) == SET &&
		clm.clm_dictionary_get_to(D, 1005, &value, size_of(value)) == GET && value == source &&
		clm.clm_dictionary_count(D) == 40, "replace without adding")
	assert_test(&r, clm.clm_bpa_create(71001, clm.CLM_BPA_TYPE_INT) == S &&
		clm.clm_bpa_set(71001, &source, size_of(source)) == SET &&
		clm.clm_dictionary_set(D, 0, 71001) == SET &&
		clm.clm_dictionary_get_to(D, 0, &value, size_of(value)) == GET &&
		value == source && clm.clm_dictionary_count(D) == 41, "copy BPA source under zero key")
	assert_test(&r, clm.clm_bpa_create(71002, clm.CLM_BPA_TYPE_INT) == S &&
		clm.clm_dictionary_get(D, 0, 71002) == GET &&
		clm.clm_bpa_get(71002, &value, size_of(value)) == GET && value == source, "copy into BPA destination")
	source = 100
	clm.clm_bpa_set(71001, &source, size_of(source))
	assert_test(&r, clm.clm_dictionary_get_to(D, 0, &value, size_of(value)) == GET && value == 9876, "source copy is independent")
	source = 9876
	assert_test(&r, clm.clm_dictionary_remove(D, 1005) == S && !clm.clm_dictionary_contains(D, 1005) &&
		clm.clm_dictionary_count(D) == 40, "remove and compact")
	assert_test(&r, clm.clm_dictionary_get_to(D, 1005, &value, size_of(value)) == F, "missing get")
	new_value := i32(12345)
	assert_test(&r, clm.clm_dictionary_set_from(D, 99999, &new_value, size_of(new_value)) == SET &&
		clm.clm_dictionary_count(D) == 41, "insert with tombstones")
	key: clm.idkey
	seen: [42]bool
	valid := clm.clm_dictionary_iterate_reset(D) == S
	count := 0
	for valid && clm.clm_dictionary_iterate_next(D, &key) == GET {
		expected: i32
		index: int
		if key == 0 { expected = source; index = 0
		} else if key == 99999 { expected = new_value; index = 1
		} else if key >= 1000 && key < 1040 && key != 1005 {
			expected = i32(key - 1000) * 10
			index = int(key - 1000) + 2
		} else { valid = false; break }
		if seen[index] || !clm.clm_dictionary_contains(D, key) ||
		   clm.clm_dictionary_get_to(D, key, &value, size_of(value)) != GET || value != expected { valid = false }
		seen[index] = true
		count += 1
	}
	assert_test(&r, valid && count == 41, "iterate every key/value exactly once")
	assert_test(&r, clm.clm_dictionary_iterate_reset(D) == S && clm.clm_dictionary_iterate_next(D, &key) == GET, "reset iterator")
	dictionary_text: cstring = "dictionary value"
	empty_text: cstring = ""
	assert_test(&r, clm.clm_bpa_create(71004, clm.CLM_BPA_TYPE_STRING) == S &&
		clm.clm_bpa_set_string(71004, rawptr(dictionary_text), 17) == 16 &&
		clm.clm_dictionary_new(71010, clm.CLM_BPA_TYPE_STRING, 0) == S &&
		clm.clm_dictionary_set(71010, 5, 71004) == SET, "copy BPA string")
	text: [32]u8
	assert_test(&r, clm.clm_dictionary_get_to(71010, 5, &text, size_of(text)) == GET &&
		string(cast(cstring)&text[0]) == "dictionary value", "retrieve string")
	assert_test(&r, clm.clm_dictionary_set_from(71010, 6, rawptr(empty_text), 1) == SET &&
		clm.clm_dictionary_get_to(71010, 6, &text, size_of(text)) == GET && text[0] == 0, "empty string")
	owned := strings.clone_to_cstring("owned text")
	assert_test(&r, clm.clm_dictionary_set_from(71010, 7, rawptr(owned), 11) == SET, "copy caller string")
	delete(owned)
	assert_test(&r, clm.clm_dictionary_get_to(71010, 7, &text, size_of(text)) == GET &&
		string(cast(cstring)&text[0]) == "owned text", "caller memory not retained")
	pair := [2]i32{11, 22}
	pair_result: [2]i32
	assert_test(&r, clm.clm_dictionary_new(71020, clm.CLM_BPA_TYPE_STRUCT, size_of(pair)) == S &&
		clm.clm_dictionary_set_from(71020, 7, &pair, size_of(pair)) == SET &&
		clm.clm_dictionary_get_to(71020, 7, &pair_result, size_of(pair_result)) == GET &&
		pair_result == pair, "fixed struct values")
	collision_keys: [3]clm.idkey
	bucket: int
	n := 0
	for candidate := clm.idkey(0); candidate < 100000 && n < 3; candidate += 1 {
		b := clm.clm_bpa_hash_id(candidate) & 15
		if n == 0 { bucket = b }
		if b == bucket { collision_keys[n] = candidate; n += 1 }
	}
	valid = n == 3 && clm.clm_dictionary_new(71030, clm.CLM_BPA_TYPE_INT, 0) == S
	for i in 0..<3 {
		item := i32(i+1)
		if clm.clm_dictionary_set_from(71030, collision_keys[i], &item, size_of(item)) != SET { valid = false }
	}
	assert_test(&r, valid && clm.clm_dictionary_remove(71030, collision_keys[0]) == S &&
		clm.clm_dictionary_get_to(71030, collision_keys[2], &value, size_of(value)) == GET && value == 3, "probe past deleted collision")
	assert_test(&r, clm.clm_dictionary_new(71050, clm.CLM_BPA_TYPE_INT, 0) == S, "ordered dictionary")
	ordered := [4]clm.idkey{900, 2, 77, 0}
	valid = true
	for k, i in ordered {
		item := i32(i + 100)
		if clm.clm_dictionary_add_from(71050, k, &item, size_of(item)) != SET { valid = false }
	}
	assert_test(&r, valid && clm.clm_dictionary_count(71050) == 4, "ordered add")
	assert_test(&r, clm.clm_dictionary_add_from(71050, 2, &source, size_of(source)) == F, "insert-only duplicate")
	assert_test(&r, clm.clm_dictionary_update_from(71050, 333, &source, size_of(source)) == F, "update-only absent")
	assert_test(&r, clm.clm_dictionary_iterate_next(71050, &key) == GET && key == 900 &&
		clm.clm_dictionary_update_from(71050, 2, &source, size_of(source)) == SET &&
		clm.clm_dictionary_iterate_next(71050, &key) == GET && key == 2, "replacement preserves cursor")
	copied: [4]clm.idkey
	assert_test(&r, clm.clm_dictionary_keys_to(71050, raw_data(copied[:]), 4) == GET && copied == ordered, "ordered key copy")
	assert_test(&r, clm.clm_dictionary_keys_to(71050, raw_data(copied[:]), 3) == F &&
		clm.clm_dictionary_keys_to(71050, nil, 4) == F, "reject insufficient/null key output")
	valid = clm.clm_dictionary_iterate_reset(71050) == S
	for k in ordered {
		if clm.clm_dictionary_iterate_next(71050, &key) != GET || key != k { valid = false }
	}
	assert_test(&r, valid && clm.clm_dictionary_iterate_next(71050, &key) == F, "ordered iterator exhaustion")
	assert_test(&r, clm.clm_dictionary_remove(71050, 2) == S &&
		clm.clm_dictionary_add_from(71050, 2, &source, size_of(source)) == SET &&
		clm.clm_dictionary_keys_to(71050, raw_data(copied[:]), 4) == GET &&
		copied == ([4]clm.idkey{900, 77, 0, 2}) &&
		clm.clm_dictionary_iterate_next(71050, &key) == GET && key == 900, "reinsert appends and resets cursor")
	assert_test(&r, clm.clm_dictionary_set_from(D, 55, nil, 4) == F &&
		clm.clm_dictionary_set_from(D, 55, &source, 1) == F &&
		clm.clm_dictionary_get_to(D, 0, &value, 1) == F &&
		clm.clm_dictionary_get(D, 0, 71004) == F &&
		clm.clm_dictionary_set(D, 55, 71004) == F &&
		clm.clm_dictionary_iterate_next(D, nil) == F, "reject incompatible buffers and BPA types")
	bad := [2]u8{'a', 'b'}
	assert_test(&r, clm.clm_dictionary_set_from(71010, 99, &bad, 2) == SET &&
		clm.clm_dictionary_get_to(71010, 99, &text, size_of(text)) == GET &&
		text[0] == 'a' && text[1] == 'b' && text[2] == 0, "BPA terminates bounded string buffers")
	assert_test(&r, clm.clm_dictionary_clear(71050) == S && clm.clm_dictionary_count(71050) == 0 &&
		!clm.clm_dictionary_contains(71050, 900) && clm.clm_dictionary_keys_to(71050, nil, 0) == GET, "clear retains empty resource")
	meta: clm.clm_dictionary_metadata
	assert_test(&r, clm.clm_dictionary_internal_read(D, &meta) == S, "read linked BPA metadata")
	assert_test(&r, clm.clm_dictionary_delete(71050) == S && clm.clm_dictionary_delete(71030) == S &&
		clm.clm_dictionary_delete(71020) == S && clm.clm_dictionary_delete(71010) == S &&
		clm.clm_dictionary_delete(D) == S, "delete all dictionaries")
	assert_test(&r, clm.clm_dictionary_count(D) == F &&
		!clm.clm_bpaa_is_existing(meta.slots_id) && !clm.clm_bpaa_is_existing(meta.values_id) &&
		!clm.clm_bpaa_is_existing(meta.keys_id), "delete releases backing arrays")
	assert_test(&r, clm.clm_bpa_delete(71001) == S && clm.clm_bpa_delete(71002) == S &&
		clm.clm_bpa_delete(71004) == S, "release BPA blocks")
	assert_test(&r, clm.clm_bpa_global_blocks_count == blocks_before &&
		clm.clm_bpa_global_arrays_count == arrays_before, "no leaked BPA/BPAA resources")
	test_summary_counts(r.passed, r.failed)
	return 0 if r.failed == 0 else 1
}
