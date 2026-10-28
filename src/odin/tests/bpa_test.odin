package tests

import "../clm"
import "core:mem"

bpa_compare_ints :: proc(left, right: rawptr) -> i32 {
	l, r: i32
	mem.copy(&l, left, 4); mem.copy(&r, right, 4)
	if l < r {return -1}; if l > r {return 1}; return 0
}
bpa_out_of_order :: proc(left, right: rawptr) -> bool {return bpa_compare_ints(left, right) > 0}
bpa_text_is :: proc(buffer: []u8, expected: string) -> bool {
	if len(buffer) <= len(expected) || buffer[len(expected)] != 0 {return false}
	for c, i in expected {if buffer[i] != u8(c) {return false}}
	return true
}

bpa_tests :: proc() -> i32 {
	r: test_result
	test_begin("CLM - Block Pool Allocation (BPA)")
	value, index: i32
	count, capacity, length: u32
	sealed: u8
	values := [4]i32{4, 1, 3, 2}
	struct_value := [2]i32{123, 456}
	struct_result: [2]i32
	output: [8]u32
	text: [32]u8
	greeting := [6]u8{'h','e','l','l','o',0}
	replacement := [7]u8{'w','o','r','l','d','!',0}
	updated := [15]u8{'w','o','r','l','d','!',' ','u','p','d','a','t','e','d',0}
	empty := [1]u8{0}
	assert_test(&r, !clm.clm_bpa_is_existing(1))
	assert_test(&r, !clm.clm_bpaa_is_existing(10))
	assert_test(&r, clm.clm_bpa_create(1, clm.CLM_BPA_TYPE_INT) == 0)
	assert_test(&r, clm.clm_bpa_is_existing(1) && !clm.clm_bpaa_is_existing(1))
	value = 9
	assert_test(&r, clm.clm_bpa_replace(1, &value, 4) == 2)
	assert_test(&r, clm.clm_bpa_get(1, &value, 4) == 1 && value == 9)
	assert_test(&r, clm.clm_bpa_create(1, clm.CLM_BPA_TYPE_INT) == -1)
	assert_test(&r, clm.clm_bpa_set(1, &values[0], 4) == 2)
	assert_test(&r, clm.clm_bpa_get(1, &value, 4) == 1 && value == 4)
	assert_test(&r, clm.clm_bpa_get(1, &value, 1) == -1)
	assert_test(&r, clm.clm_bpa_ref(1) == 0)
	assert_test(&r, clm.clm_bpa_unref(1) == 0 && clm.clm_bpa_is_existing(1))
	assert_test(&r, clm.clm_bpa_get(1, &value, 4) == 1)
	assert_test(&r, clm.clm_bpa_unref(1) == 0 && !clm.clm_bpa_is_existing(1))
	assert_test(&r, clm.clm_bpa_get(1, &value, 4) == -1)
	assert_test(&r, clm.clm_bpa_create_sized(2, 0, 8) == 0)
	assert_test(&r, clm.clm_bpa_set(2, &struct_value, 8) == 2)
	assert_test(&r, clm.clm_bpa_get(2, &struct_result, 8) == 1 && struct_result == struct_value)
	assert_test(&r, clm.clm_bpa_delete(2) == 0)
	assert_test(&r, clm.clm_bpa_create(3, 11) == 0)
	assert_test(&r, clm.clm_bpa_set_string(3, &greeting, 6) == 5)
	assert_test(&r, clm.clm_bpa_get_string(3, &text, 32) == 5 && bpa_text_is(text[:], "hello"))
	assert_test(&r, clm.clm_bpa_get_string(3, &text, 5) == -1)
	assert_test(&r, clm.clm_bpa_set_string(3, &replacement, 7) == 6)
	assert_test(&r, clm.clm_bpa_get_string(3, &text, 32) == 6 && bpa_text_is(text[:], "world!"))
	assert_test(&r, clm.clm_bpa_replace(3, &updated, 15) == 14)
	assert_test(&r, clm.clm_bpa_get_string(3, &text, 32) == 14 && bpa_text_is(text[:], "world! updated"))
	assert_test(&r, clm.clm_bpa_replace(999, &updated, 15) == -1)
	assert_test(&r, clm.clm_bpa_delete(3) == 0)
	assert_test(&r, clm.clm_bpa_create(4, 0) == -1)

	assert_test(&r, clm.clm_bpaa_create(10, 5, 1, 0) == 0)
	assert_test(&r, clm.clm_bpaa_is_existing(10) && !clm.clm_bpa_is_existing(10))
	for &v in values {assert_test(&r, clm.clm_bpaa_append_raw(10, &v, 4) == 0)}
	assert_test(&r, clm.clm_bpaa_get_count_to(10, &count) == 1 && count == 4)
	assert_test(&r, clm.clm_bpaa_get_capacity_to(10, &capacity) == 1 && capacity >= 4)
	assert_test(&r, clm.clm_bpaa_get_first_raw(10, &value, 4) == 1 && value == 4)
	assert_test(&r, clm.clm_bpaa_get_last_raw(10, &value, 4) == 1 && value == 2)
	assert_test(&r, clm.clm_bpaa_get_raw(10, -1, &output, 4) == -1)
	assert_test(&r, clm.clm_bpaa_get_raw(10, 4, &output, 4) == -1)
	assert_test(&r, clm.clm_bpaa_insert_raw(10, 1, &values[1], 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_raw(10, 1, &value, 4) == 1 && value == 1)
	assert_test(&r, clm.clm_bpaa_set_raw(10, 1, &values[0], 4) == 2)
	assert_test(&r, clm.clm_bpaa_move_to(10, 2) == 0)
	assert_test(&r, clm.clm_bpaa_get_current_index_to(10, &index) == 1 && index == 2)
	assert_test(&r, clm.clm_bpaa_get_current_raw(10, &value, 4) == 1)
	assert_test(&r, clm.clm_bpaa_move_previous(10) == 0)
	assert_test(&r, clm.clm_bpaa_move_next(10) == 0)
	assert_test(&r, clm.clm_bpaa_move_last(10) == 0)
	assert_test(&r, clm.clm_bpaa_move_next(10) == -1)
	assert_test(&r, clm.clm_bpaa_reset(10) == 0)
	assert_test(&r, clm.clm_bpaa_extend(10, 2) == 0)
	assert_test(&r, clm.clm_bpaa_get_capacity_to(10, &capacity) == 1)
	assert_test(&r, clm.clm_bpaa_shrink(10, capacity-1) == 0)
	assert_test(&r, clm.clm_bpaa_reverse(10) == 0)
	assert_test(&r, clm.clm_bpaa_sort_with(10, bpa_compare_ints) == 0)
	for i: i32 = 1; i < 5; i += 1 {
		current, previous: i32
		assert_test(&r, clm.clm_bpaa_get_raw(10, i, &current, 4) == 1)
		assert_test(&r, clm.clm_bpaa_get_raw(10, i-1, &previous, 4) == 1 && current >= previous)
	}
	assert_test(&r, clm.clm_bpaa_shift_left(10, 1, 1) == 0)
	assert_test(&r, clm.clm_bpaa_shift_right(10, 1, 1) == 0)
	assert_test(&r, clm.clm_bpaa_swap(10, 0, 1) == 0)
	assert_test(&r, clm.clm_bpaa_copy(10, 0, 1) == 0)
	assert_test(&r, clm.clm_bpaa_sort(10, nil) == -1)
	assert_test(&r, clm.clm_bpaa_shuffle(10) == 0)
	drawn: [5]i32
	assert_test(&r, clm.clm_bpaa_draw_at_raw(10, 2, &drawn[0], 4) == 1)
	for i in 1..<5 {assert_test(&r, clm.clm_bpaa_draw_raw(10, &drawn[i], 4) == 1)}
	assert_test(&r, clm.clm_bpaa_draw_raw(10, &value, 4) == 1)
	draw_count := 0
	for i in 1..=4 {
		occurrences := 0
		for v in drawn {if v == i32(i) {occurrences += 1}}
		expected := 1
		if i == 1 {expected = 0} else if i == 2 || i == 4 {expected = 2}
		assert_test(&r, occurrences == expected, "draw deck preserves the exact multiset without repetition")
		draw_count += occurrences
	}
	assert_test(&r, draw_count == 5)
	assert_test(&r, clm.clm_bpaa_is_sealed_to(10, &sealed) == 1 && sealed == 0)
	assert_test(&r, clm.clm_bpaa_seal(10) == 0)
	assert_test(&r, clm.clm_bpaa_is_sealed_to(10, &sealed) == 1 && sealed == 1)
	assert_test(&r, clm.clm_bpaa_append_raw(10, &values[0], 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_count_to(10, &count) == 1)
	assert_test(&r, clm.clm_bpaa_get_capacity_to(10, &capacity) == 1)
	for count < capacity {assert_test(&r, clm.clm_bpaa_append_raw(10, &values[0], 4) == 0); count += 1}
	assert_test(&r, clm.clm_bpaa_append_raw(10, &values[0], 4) == -1)
	assert_test(&r, clm.clm_bpaa_clear(10) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(10, &values[0], 4) == 0)
	assert_test(&r, clm.clm_bpaa_remove_first(10) == 0)
	assert_test(&r, clm.clm_bpaa_clear(10) == 0)
	assert_test(&r, clm.clm_bpaa_unseal(10) == 0)
	assert_test(&r, clm.clm_bpaa_extend(10, 1) == 0)
	assert_test(&r, clm.clm_bpaa_get_count_to(10, &count) == 1 && count == 0)
	assert_test(&r, clm.clm_bpaa_clear(10) == 0)
	assert_test(&r, clm.clm_bpaa_draw_raw(10, &value, 4) == -1)
	assert_test(&r, clm.clm_bpaa_ref(10) == 0)
	assert_test(&r, clm.clm_bpaa_unref(10) == 0)
	assert_test(&r, clm.clm_bpaa_unref(10) == 0 && !clm.clm_bpaa_is_existing(10))
	assert_test(&r, clm.clm_bpaa_get_count_to(10, &count) == -1)

	assert_test(&r, clm.clm_bpaa_create(11, 11, 0, 1) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(11, &greeting, 6) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(11, &empty, 1) == 0)
	assert_test(&r, clm.clm_bpaa_get_string_raw(11, 0, &text, 32) == 5 && bpa_text_is(text[:], "hello"))
	assert_test(&r, clm.clm_bpaa_set_string_raw(11, 1, &replacement, 7) == 6)
	assert_test(&r, clm.clm_bpaa_move_first(11) == 0)
	assert_test(&r, clm.clm_bpaa_get_length_to(11, &length) == 1 && length == 5)
	assert_test(&r, clm.clm_bpaa_remove(11, 0) == 0)
	assert_test(&r, clm.clm_bpaa_get_string_raw(11, 0, &text, 32) == 6 && bpa_text_is(text[:], "world!"))
	assert_test(&r, clm.clm_bpaa_clear(11) == 0)
	assert_test(&r, clm.clm_bpaa_delete(11) == 0 && !clm.clm_bpaa_is_existing(11))
	assert_test(&r, clm.clm_bpaa_create_sized(12, 0, 1, 4, 0) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(12, &values[0], 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_raw(12, 0, &value, 4) == 1 && value == 4)
	assert_test(&r, clm.clm_bpaa_delete(12) == 0)

	copied: [2]i32
	borrowed: rawptr
	borrowed_size: int
	assert_test(&r, clm.clm_bpaa_create(13, 5, 0, 0) == 0)
	for &v in values {assert_test(&r, clm.clm_bpaa_append_raw(13, &v, 4) == 0)}
	assert_test(&r, clm.clm_bpaa_slice(13, 1, 2, 14) == 0)
	assert_test(&r, clm.clm_bpaa_get_count_to(14, &count) == 1 && count == 2)
	assert_test(&r, clm.clm_bpaa_get_raw(14, 0, &copied[0], 4) == 1 && copied[0] == 1)
	assert_test(&r, clm.clm_bpaa_get_raw(14, 1, &copied[1], 4) == 1 && copied[1] == 3)
	assert_test(&r, clm.clm_bpaa_slice_to(13, 1, 2, &copied, 8) == 0 && copied == [2]i32{1,3})
	assert_test(&r, clm.clm_bpaa_slice(13, 4, 0, 19) == 0)
	assert_test(&r, clm.clm_bpaa_get_count_to(19, &count) == 1 && count == 0)
	assert_test(&r, clm.clm_bpaa_delete(19) == 0)
	assert_test(&r, clm.clm_bpaa_slice_borrow(13, 1, 2, &borrowed, &borrowed_size) == 0 && borrowed != nil && borrowed_size == 8)
	mem.copy(&copied, borrowed, 8)
	assert_test(&r, copied == [2]i32{1,3})
	assert_test(&r, clm.clm_bpaa_slice_borrow(13, 4, 0, &borrowed, &borrowed_size) == 0 && borrowed == nil && borrowed_size == 0)
	assert_test(&r, clm.clm_bpaa_slice_to(13, 4, 0, nil, 0) == 0)
	assert_test(&r, clm.clm_bpaa_slice(13, 3, 2, 18) == -1)
	assert_test(&r, clm.clm_bpaa_slice_to(13, -1, 1, &copied, 8) == -1)
	assert_test(&r, clm.clm_bpaa_create(15, 11, 0, 0) == 0)
	assert_test(&r, clm.clm_bpaa_slice_borrow(15, 0, 0, &borrowed, &borrowed_size) == -1)
	assert_test(&r, clm.clm_bpa_create(17, 11) == 0)
	assert_test(&r, clm.clm_bpa_set_string(17, &greeting, 6) == 5)
	assert_test(&r, clm.clm_bpaa_append_string(15, 17) == 0)
	assert_test(&r, clm.clm_bpaa_slice(15, 0, 1, 16) == 0)
	changed := [8]u8{'c','h','a','n','g','e','d',0}
	assert_test(&r, clm.clm_bpaa_set_string_raw(15, 0, &changed, 8) == 7)
	assert_test(&r, clm.clm_bpaa_get_string_raw(16, 0, &text, 32) == 5 && bpa_text_is(text[:], "hello"))
	assert_test(&r, clm.clm_bpaa_delete(16) == 0)
	assert_test(&r, clm.clm_bpa_delete(17) == 0)
	assert_test(&r, clm.clm_bpaa_delete(15) == 0)
	assert_test(&r, clm.clm_bpaa_delete(14) == 0)
	assert_test(&r, clm.clm_bpaa_delete(13) == 0)
	wide_id := clm.idkey(0x100000001)
	assert_test(&r, clm.clm_bpaa_create(wide_id, 5, 0, 0) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(wide_id, &values[0], 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_raw(wide_id, 0, &value, 4) == 1 && value == 4)
	assert_test(&r, clm.clm_bpaa_delete(wide_id) == 0)
	for i in 0..<64 {assert_test(&r, clm.clm_bpa_create(1000+clm.idkey(i), 5) == 0)}
	for i := 0; i < 64; i += 2 {assert_test(&r, clm.clm_bpa_delete(1000+clm.idkey(i)) == 0)}
	for i := 1; i < 64; i += 2 {assert_test(&r, clm.clm_bpa_unref(1000+clm.idkey(i)) == 0)}
	for i in 0..<32 {assert_test(&r, clm.clm_bpa_create(2000+clm.idkey(i), 5) == 0)}
	for i in 0..<32 {assert_test(&r, clm.clm_bpa_delete(2000+clm.idkey(i)) == 0)}
	for i in 0..<32 {assert_test(&r, clm.clm_bpaa_create(3000+clm.idkey(i), 5, 0, 0) == 0)}
	for i := 0; i < 32; i += 2 {assert_test(&r, clm.clm_bpaa_delete(3000+clm.idkey(i)) == 0)}
	for i in 0..<16 {assert_test(&r, clm.clm_bpaa_create(4000+clm.idkey(i), 5, 0, 0) == 0)}
	for i := 1; i < 32; i += 2 {assert_test(&r, clm.clm_bpaa_delete(3000+clm.idkey(i)) == 0)}
	for i in 0..<16 {assert_test(&r, clm.clm_bpaa_delete(4000+clm.idkey(i)) == 0)}

	active_before, reserved_before, active_after, reserved_after, pool_capacity_after_delete: int
	initial, observed, another: i32 = 777, 0, 888
	assert_test(&r, clm.clm_bpa_pool_stats(4, &active_before, &reserved_before) == 0)
	assert_test(&r, clm.clm_bpa_create(50001, 5) == 0)
	assert_test(&r, clm.clm_bpa_set(50001, &initial, 4) == 2)
	assert_test(&r, clm.clm_bpa_pool_stats(4, &active_after, &reserved_after) == 0)
	assert_test(&r, active_after == active_before+1 && reserved_after >= active_after)
	assert_test(&r, clm.clm_bpaa_create_sized(50002, 0, 0, 4, 0) == 0)
	assert_test(&r, clm.clm_bpaa_append(50002, 50001, 4) == 0)
	assert_test(&r, clm.clm_bpaa_append(50002, 50001, 5) == -1)
	assert_test(&r, clm.clm_bpaa_get(50002, 0, 50001, 4) == 1)
	assert_test(&r, clm.clm_bpa_get(50001, &observed, 4) == 1 && observed == 777)
	assert_test(&r, clm.clm_bpa_set(50001, &another, 4) == 2)
	assert_test(&r, clm.clm_bpaa_set(50002, 0, 50001, 4) == 2)
	assert_test(&r, clm.clm_bpaa_insert(50002, 0, 50001, 4) == 0)
	assert_test(&r, clm.clm_bpaa_get(50002, 1, 50001, 4) == 1)
	assert_test(&r, clm.clm_bpa_get(50001, &observed, 4) == 1 && observed == 888)
	assert_test(&r, clm.clm_bpaa_draw_at(50002, 0, 50001, 4) == 1)
	assert_test(&r, clm.clm_bpaa_draw(50002, 50001, 4) == 1)
	assert_test(&r, clm.clm_bpaa_get(50002, 0, 919191, 4) == -1)
	assert_test(&r, clm.clm_bpaa_delete(50002) == 0)
	assert_test(&r, clm.clm_bpa_delete(50001) == 0)
	assert_test(&r, clm.clm_bpa_pool_stats(4, &active_after, &pool_capacity_after_delete) == 0)
	assert_test(&r, active_after == active_before && pool_capacity_after_delete == reserved_after)
	assert_test(&r, clm.clm_bpa_create(50001, 5) == 0)
	assert_test(&r, clm.clm_bpa_get(50001, &observed, 4) == 1 && observed == 0)
	assert_test(&r, clm.clm_bpa_delete(50001) == 0)
	assert_test(&r, clm.clm_bpa_create(50003, 11) == 0)
	assert_test(&r, clm.clm_bpa_set_string(50003, &greeting, 6) == 5)
	assert_test(&r, clm.clm_bpaa_create(50004, 11, 0, 1) == 0)
	assert_test(&r, clm.clm_bpaa_append_string(50004, 50003) == 0)
	assert_test(&r, clm.clm_bpa_set_string(50003, &replacement, 7) == 6)
	assert_test(&r, clm.clm_bpaa_get_string(50004, 0, 50003) == 1)
	assert_test(&r, clm.clm_bpa_get_string(50003, &text, 32) == 5 && bpa_text_is(text[:], "hello"))
	assert_test(&r, clm.clm_bpaa_set_string(50004, 0, 50003) == 2)
	assert_test(&r, clm.clm_bpaa_delete(50004) == 0)
	assert_test(&r, clm.clm_bpa_delete(50003) == 0)

	x, y, check: i32 = 17, 19, 0
	assert_test(&r, clm.clm_bpaa_create(51000, 5, 1, 0) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(51000, &x, 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_raw(51000, 0, &check, 4) == 1 && check == 17)
	assert_test(&r, clm.clm_bpaa_append_raw(51000, &y, 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_raw(51000, 1, &check, 4) == 1 && check == 19)
	assert_test(&r, clm.clm_bpaa_insert_raw(51000, 0, &clm.clm_bpaa_find_array(51000).items.blocks[4], 4) == 0)
	assert_test(&r, clm.clm_bpaa_get_raw(51000, 0, &check, 4) == 1 && check == 19)
	assert_test(&r, clm.clm_bpaa_get_raw(51000, 2, &check, 4) == 1 && check == 19)
	assert_test(&r, clm.clm_bpaa_delete(51000) == 0)

	array: ^clm.clm_bpaa_array_node
	advanced_initial := [2]i32{2,4}; third: i32 = 6
	assert_test(&r, clm.clm_bpaa_create(52000, 5, 1, 0) == 0)
	assert_test(&r, clm.clm_bpaa_append_raw(52000, &advanced_initial[0], 4) == 0)
	assert_test(&r, clm.clm_bpaa_advanced_borrow(52000, &array) == 0 && array != nil)
	assert_test(&r, array.count == 1 && array.capacity == 1 && array.block_size == 4)
	assert_test(&r, clm.clm_bpaa_advanced_data(array) != nil)
	([^]i32)(clm.clm_bpaa_advanced_data(array))[0] = 3
	assert_test(&r, clm.clm_bpaa_advanced_get_raw(array, 0, &observed, 4) == 1 && observed == 3)
	assert_test(&r, clm.clm_bpaa_advanced_set_raw(array, 0, &advanced_initial[1], 4) == 2)
	assert_test(&r, clm.clm_bpaa_advanced_extend(array, 1) == 0)
	assert_test(&r, clm.clm_bpaa_advanced_append_raw(array, &third, 4) == 0)
	assert_test(&r, clm.clm_bpaa_advanced_insert_raw(array, 1, &advanced_initial[0], 4) == 0)
	assert_test(&r, array.count == 3 && clm.clm_bpaa_advanced_get_raw(array, 1, &observed, 4) == 1 && observed == 2)
	assert_test(&r, clm.clm_bpaa_advanced_remove(array, 0) == 0 && array.count == 2)
	assert_test(&r, clm.clm_bpaa_advanced_shrink(array, array.count) == 0 && array.capacity == array.count)
	assert_test(&r, clm.clm_bpaa_advanced_clear(array) == 0 && array.count == 0)
	assert_test(&r, clm.clm_bpaa_unref(52000) == 0 && clm.clm_bpaa_is_existing(52000))
	assert_test(&r, clm.clm_bpaa_advanced_release(&array) == 0 && array == nil)
	assert_test(&r, !clm.clm_bpaa_is_existing(52000))
	assert_test(&r, clm.clm_bpaa_advanced_release(&array) == -1)
	assert_test(&r, clm.clm_bpaa_advanced_borrow(52001, &array) == -1 && array == nil)
	assert_test(&r, clm.clm_bpaa_create(52001, 11, 0, 0) == 0)
	assert_test(&r, clm.clm_bpaa_advanced_borrow(52001, &array) == 0)
	assert_test(&r, clm.clm_bpaa_advanced_data(array) == nil)
	assert_test(&r, clm.clm_bpaa_advanced_get_raw(array, 0, &value, 4) == -1)
	assert_test(&r, clm.clm_bpaa_advanced_release(&array) == 0)
	assert_test(&r, clm.clm_bpaa_delete(52001) == 0)
	assert_test(&r, clm.clm_bpa_pool_cleanup() == 0)
	bpa_boundary_tests(&r)
	test_summary_counts(r.passed, r.failed)
	return i32(r.failed != 0)
}

bpa_boundary_tests :: proc(r: ^test_result) {
	value: i32 = 42
	text: [16]u8
	assert_test(r, clm.clm_bpa_create_sized(60000, 0, 0) == -1)
	assert_test(r, clm.clm_bpa_create_sized(60000, 11, 1) == -1)
	assert_test(r, clm.clm_bpa_create_sized(60000, 5, 8) == -1)
	assert_test(r, clm.clm_bpa_create(60000, 255) == -1)
	assert_test(r, clm.clm_bpaa_create(60000, 5, -1, 0) == -1)
	assert_test(r, clm.clm_bpaa_create(60000, 0, 0, 0) == -1)
	assert_test(r, clm.clm_bpaa_create_sized(60000, 0, 0, 0, 0) == -1)
	assert_test(r, clm.clm_bpaa_create_sized(60000, 11, 0, 1, 0) == -1)
	assert_test(r, clm.clm_bpaa_create_sized(60000, 5, 0, 8, 0) == -1)
	assert_test(r, clm.clm_bpaa_create(60000, 255, 0, 0) == -1)
	assert_test(r, clm.clm_bpa_create(60000, 5) == 0)
	assert_test(r, clm.clm_bpa_create(60001, 6) == 0)
	assert_test(r, clm.clm_bpa_create(60002, 11) == 0)
	assert_test(r, clm.clm_bpa_create(60003, 2) == 0)
	assert_test(r, clm.clm_bpa_create(60004, 11) == 0)
	assert_test(r, clm.clm_bpa_set(60000, nil, 4) == -1)
	assert_test(r, clm.clm_bpa_set(60000, &value, 3) == -1)
	assert_test(r, clm.clm_bpa_set(60000, &value, 5) == -1)
	assert_test(r, clm.clm_bpa_get(60000, nil, 4) == -1)
	assert_test(r, clm.clm_bpa_get(60002, &text, 16) == -1)
	assert_test(r, clm.clm_bpa_set(60002, &value, 4) == -1)
	assert_test(r, clm.clm_bpa_get_string(60000, &text, 16) == -1)
	assert_test(r, clm.clm_bpa_set_string(60000, &text, 16) == -1)
	assert_test(r, clm.clm_bpa_get_string(60002, &text, 1) == 0 && text[0] == 0)
	assert_test(r, clm.clm_bpa_set_string(60002, nil, 1) == -1)
	assert_test(r, clm.clm_bpa_set_string(60002, &text, 0) == -1)
	unterminated := [3]u8{'a','b','c'}
	assert_test(r, clm.clm_bpa_set_string_from(60002, &unterminated, 3) == 3)
	assert_test(r, clm.clm_bpa_get_string_to(60002, &text, 4) == 3 && bpa_text_is(text[:], "abc"))
	assert_test(r, clm.clm_bpa_copy_block(60002, 60004) == 0)
	assert_test(r, clm.clm_bpa_set_string(60002, &text, 1) == 1)
	assert_test(r, clm.clm_bpa_get_string(60004, &text, 16) == 3 && bpa_text_is(text[:], "abc"))
	assert_test(r, clm.clm_bpa_copy_block(60000, 60001) == -1)
	assert_test(r, clm.clm_bpa_copy_block(999999, 60001) == -1)
	assert_test(r, clm.clm_bpa_set_from(60000, &value, 4) == 2)
	assert_test(r, clm.clm_bpa_get_to(60000, &value, 4) == 1)
	p: rawptr; n: int
	assert_test(r, clm.clm_bpa_borrow(60000, &p, &n) == 1 && n == 4 && (^i32)(p)^ == 42)
	assert_test(r, clm.clm_bpa_borrow(60004, &p, &n) == 1 && n == 4)
	assert_test(r, clm.clm_bpa_borrow(60000, nil, &n) == -1)
	assert_test(r, clm.clm_bpa_borrow(60000, &p, nil) == -1)
	assert_test(r, clm.clm_bpa_pool_cleanup() == -1)
	b := clm.clm_bpa_find_block(60000)
	b.refcount = max(u16); assert_test(r, clm.clm_bpa_ref(60000) == -1)
	b.refcount = 0; assert_test(r, clm.clm_bpa_unref(60000) == -1); b.refcount = 1

	assert_test(r, clm.clm_bpaa_create(60000, 5, 3, 0) == 0, "block and array ID namespaces are independent")
	assert_test(r, clm.clm_bpaa_create(60000, 5, 0, 0) == -1)
	assert_test(r, clm.clm_bpaa_remove_first(60000) == -1)
	assert_test(r, clm.clm_bpaa_remove_last(60000) == -1)
	assert_test(r, clm.clm_bpaa_move_last(60000) == -1)
	assert_test(r, clm.clm_bpaa_move_previous(60000) == -1)
	assert_test(r, clm.clm_bpaa_move_next(60000) == -1)
	assert_test(r, clm.clm_bpaa_move_first(60000) == 0)
	assert_test(r, clm.clm_bpaa_move_to(60000, -1) == -1)
	assert_test(r, clm.clm_bpaa_append_from(60000, &value, 3) == -1)
	assert_test(r, clm.clm_bpaa_append_raw(60000, nil, 4) == -1)
	assert_test(r, clm.clm_bpaa_insert_from(60000, -1, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_insert_raw(60000, 1, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_append(60000, 60001, 4) == -1, "typed transfers reject equal-size mismatched tags")
	assert_test(r, clm.clm_bpaa_append(60000, 60000, 4) == 0)
	assert_test(r, clm.clm_bpaa_set_from(60000, 0, &value, 4) == 2)
	assert_test(r, clm.clm_bpaa_get_to(60000, 0, &value, 4) == 1 && value == 42)
	assert_test(r, clm.clm_bpaa_get_first_to(60000, &value, 4) == 1)
	assert_test(r, clm.clm_bpaa_get_last_to(60000, &value, 4) == 1)
	assert_test(r, clm.clm_bpaa_get_current_to(60000, &value, 4) == 1)
	assert_test(r, clm.clm_bpaa_get_first(60000, 60000, 4) == 1)
	assert_test(r, clm.clm_bpaa_get_last(60000, 60000, 4) == 1)
	assert_test(r, clm.clm_bpaa_get_current(60000, 60000, 4) == 1)
	assert_test(r, clm.clm_bpaa_get_raw(60000, 0, nil, 4) == -1)
	assert_test(r, clm.clm_bpaa_get_raw(60000, 0, &value, 3) == -1)
	assert_test(r, clm.clm_bpaa_set_raw(60000, 0, &value, 5) == -1)
	assert_test(r, clm.clm_bpaa_set_raw(60000, 1, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_get_count(60000, 60001) == 1)
	u: u32; assert_test(r, clm.clm_bpa_get(60001, &u, 4) == 1 && u == 1)
	assert_test(r, clm.clm_bpaa_get_capacity(60000, 60001) == 1)
	assert_test(r, clm.clm_bpa_get(60001, &u, 4) == 1 && u == 3)
	assert_test(r, clm.clm_bpaa_get_count(60000, 60000) == -1)
	assert_test(r, clm.clm_bpaa_get_current_index(60000, 60000) == 1)
	assert_test(r, clm.clm_bpaa_is_sealed(60000, 60003) == 1)
	assert_test(r, clm.clm_bpaa_get_count_to(60000, nil) == -1)
	assert_test(r, clm.clm_bpaa_get_capacity_to(60000, nil) == -1)
	assert_test(r, clm.clm_bpaa_get_current_index_to(60000, nil) == -1)
	assert_test(r, clm.clm_bpaa_is_sealed_to(60000, nil) == -1)
	assert_test(r, clm.clm_bpaa_get_length_to(60000, &u) == -1)
	assert_test(r, clm.clm_bpaa_extend(60000, 0) == -1)
	assert_test(r, clm.clm_bpaa_extend(60000, max(u32)) == -1)
	assert_test(r, clm.clm_bpaa_shrink(60000, 0) == -1)
	assert_test(r, clm.clm_bpaa_shrink(60000, 3) == -1)
	assert_test(r, clm.clm_bpaa_seal(60000) == 0)
	assert_test(r, clm.clm_bpaa_extend(60000, 1) == -1)
	assert_test(r, clm.clm_bpaa_shrink(60000, 2) == -1)
	assert_test(r, clm.clm_bpaa_set_raw(60000, 0, &value, 4) == 2)
	assert_test(r, clm.clm_bpaa_unseal(60000) == 0)
	assert_test(r, clm.clm_bpaa_append_raw(60000, &value, 4) == 0)
	assert_test(r, clm.clm_bpaa_sort(60000, bpa_out_of_order) == 0)
	assert_test(r, clm.clm_bpaa_shift_left(60000, 0, 2) == 0)
	assert_test(r, clm.clm_bpaa_shift_right(60000, 2, 1) == -1)
	assert_test(r, clm.clm_bpaa_swap(60000, -1, 0) == -1)
	assert_test(r, clm.clm_bpaa_swap(60000, 0, 0) == 0)
	assert_test(r, clm.clm_bpaa_copy(60000, 0, 2) == -1)
	assert_test(r, clm.clm_bpaa_slice(60000, 0, 1, 60000) == -1)
	assert_test(r, clm.clm_bpaa_slice_to(60000, 0, 1, nil, 4) == -1)
	assert_test(r, clm.clm_bpaa_slice_to(60000, 0, 1, &value, 3) == -1)
	assert_test(r, clm.clm_bpaa_slice_borrow(60000, 0, 1, nil, &n) == -1)
	assert_test(r, clm.clm_bpaa_draw_at_raw(60000, -1, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_draw_at_to(60000, 0, &value, 4) == 1)
	assert_test(r, clm.clm_bpaa_draw_to(60000, &value, 4) == 1)
	assert_test(r, clm.clm_bpaa_draw_at_raw(60000, 0, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_remove_last(60000) == 0)
	a := clm.clm_bpaa_find_array(60000)
	a.refcount = max(u16); assert_test(r, clm.clm_bpaa_ref(60000) == -1)
	borrow: ^clm.clm_bpaa_array_node
	assert_test(r, clm.clm_bpaa_advanced_borrow(60000, &borrow) == -1 && borrow == nil)
	a.refcount = 0; assert_test(r, clm.clm_bpaa_unref(60000) == -1); a.refcount = 1
	assert_test(r, clm.clm_bpaa_advanced_borrow(60000, nil) == -1)
	assert_test(r, clm.clm_bpaa_advanced_release(nil) == -1)
	assert_test(r, clm.clm_bpaa_advanced_get_raw(nil, 0, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_advanced_set_raw(nil, 0, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_advanced_insert_raw(nil, 0, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_advanced_append_raw(nil, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_advanced_remove(nil, 0) == -1)
	assert_test(r, clm.clm_bpaa_advanced_clear(nil) == -1)
	assert_test(r, clm.clm_bpaa_advanced_extend(nil, 1) == -1)
	assert_test(r, clm.clm_bpaa_advanced_shrink(nil, 0) == -1)

	assert_test(r, clm.clm_bpaa_create(60002, 11, 0, 1) == 0)
	assert_test(r, clm.clm_bpaa_append_string(60002, 60004) == 0)
	assert_test(r, clm.clm_bpaa_get_length(60002, 60001) == 1)
	assert_test(r, clm.clm_bpa_get(60001, &u, 4) == 1 && u == 3)
	assert_test(r, clm.clm_bpaa_get_string_to(60002, 0, &text, 3) == -1)
	assert_test(r, clm.clm_bpaa_get_string_to(60002, 0, &text, 4) == 3)
	assert_test(r, clm.clm_bpaa_set_string_from(60002, 0, &text, 4) == 3)
	assert_test(r, clm.clm_bpaa_get_raw(60002, 0, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_set_raw(60002, 0, &value, 4) == -1)
	assert_test(r, clm.clm_bpaa_reverse(60002) == -1)
	assert_test(r, clm.clm_bpaa_sort_with(60002, bpa_compare_ints) == -1)
	assert_test(r, clm.clm_bpaa_shuffle(60002) == -1)
	assert_test(r, clm.clm_bpaa_shift_left(60002, 0, 1) == -1)
	assert_test(r, clm.clm_bpaa_copy(60002, 0, 0) == -1)
	assert_test(r, clm.clm_bpaa_draw_raw(60002, &value, 4) == -1)
	for type: u8 = 0; type <= 14; type += 1 {
		expected: u32
		switch type {
		case 1,2: expected = 1
		case 3,4: expected = 2
		case 5,6,9: expected = 4
		case 7,8,10,13: expected = 8
		case 12: expected = 16
		}
		assert_test(r, clm.clm_bpa_type_size(type) == expected)
	}
	for bytes in ([7]int{1,8,9,16,17,4096,4097}) {
		pool_value := clm.clm_bpa_pool_alloc(bytes)
		assert_test(r, pool_value != nil)
		zero := true
		for i in 0..<bytes {zero = zero && ([^]u8)(pool_value)[i] == 0}
		assert_test(r, zero)
		clm.clm_bpa_pool_free(pool_value, bytes)
	}
	assert_test(r, clm.clm_bpa_pool_alloc(0) == nil)
	assert_test(r, clm.clm_bpa_pool_class_for(4097) == clm.CLM_BPA_POOL_CLASSES)
	assert_test(r, clm.clm_bpa_pool_stats(0, &n, &n) == -1)
	assert_test(r, clm.clm_bpa_pool_stats(4, nil, &n) == -1)
	clm.clm_bpa_pool_free(nil, 4)
	for id in 60000..=60004 {assert_test(r, clm.clm_bpa_delete(clm.idkey(id)) == 0)}
	assert_test(r, clm.clm_bpaa_delete(60000) == 0)
	assert_test(r, clm.clm_bpaa_delete(60002) == 0)
	assert_test(r, clm.clm_bpa_delete(999999) == -1)
	assert_test(r, clm.clm_bpa_ref(999999) == -1)
	assert_test(r, clm.clm_bpa_unref(999999) == -1)
	assert_test(r, clm.clm_bpaa_delete(999999) == -1)
	assert_test(r, clm.clm_bpaa_ref(999999) == -1)
	assert_test(r, clm.clm_bpaa_unref(999999) == -1)
	assert_test(r, clm.clm_bpa_pool_cleanup() == 0)
	assert_test(r, clm.clm_bpa_create(61000, 5) == 0)
	assert_test(r, clm.clm_bpa_ref(61000) == 0)
	assert_test(r, clm.clm_bpa_create_sized(61001, 0, 4097) == 0)
	assert_test(r, clm.clm_bpaa_create(61000, 11, 0, 0) == 0)
	assert_test(r, clm.clm_bpaa_append_raw(61000, &unterminated, 3) == 0)
	assert_test(r, clm.clm_bpaa_advanced_borrow(61000, &borrow) == 0)
	assert_test(r, clm.clm_bpa_shutdown() == 0, "shutdown force-releases blocks, string arrays, slabs, and outstanding references")
	borrow = nil
	assert_test(r, !clm.clm_bpa_is_existing(61000) && !clm.clm_bpa_is_existing(61001) && !clm.clm_bpaa_is_existing(61000))
	assert_test(r, clm.clm_bpa_global_blocks == nil && clm.clm_bpa_global_arrays == nil)
	assert_test(r, clm.clm_bpa_global_blocks_capacity == 0 && clm.clm_bpa_global_arrays_capacity == 0)
	assert_test(r, clm.clm_bpa_pool_stats(4, &n, &n) == 0 && n == 0)
	assert_test(r, clm.clm_bpa_shutdown() == 0, "shutdown is idempotent")
	{
		context.allocator = mem.nil_allocator()
		assert_test(r, clm.clm_bpa_create(61000, 5) == 0, "BPA persistent storage does not depend on a caller's transient allocator")
		assert_test(r, clm.clm_bpa_get(61000, &value, 4) == 1 && value == 0)
		assert_test(r, clm.clm_bpa_delete(61000) == 0)
	}
	assert_test(r, clm.clm_bpa_shutdown() == 0, "BPA can restart after shutdown")
}
