package tests

import "../clm"
import "core:bytes"

string_test_equals :: proc(id: clm.idkey, expected: string) -> bool {
	block := clm.clm_bpa_find_block(id)
	if block == nil || block.type != clm.CLM_BPA_TYPE_STRING { return false }
	actual := ""
	if block.length > 0 {
		if block.data == nil { return false }
		actual = string(([^]u8)(block.data)[:int(block.length)])
	}
	return actual == expected
}

string_tests :: proc() -> i32 {
	r: test_result
	test_begin("CLM - String")
	string_test_edits(&r)
	string_test_encodings(&r)
	string_test_slices(&r)
	string_test_arrays(&r)
	string_test_invalid(&r)
	string_test_numbers(&r)
	test_summary_counts(r.passed, r.failed)
	if r.failed != 0 { return 1 }
	return 0
}

string_test_edits :: proc(r: ^test_result) {
	id :: clm.idkey(90001)
	assert_test(r, clm.clm_string_new(id, "Aé🐱"))
	defer clm.clm_string_delete(id)
	assert_test(r, clm.clm_string_length(id) == 3)
	assert_test(r, clm.clm_bpa_find_block(id).encoding == clm.CLM_STRING_ENCODING_UTF8)
	assert_test(r, !clm.clm_string_new(id, "duplicate"))
	assert_test(r, string_test_equals(id, "Aé🐱"))
	assert_test(r, clm.clm_string_append(id, "!") == 1)
	assert_test(r, clm.clm_string_insert(id, 1, "中") == 1)
	assert_test(r, string_test_equals(id, "A中é🐱!"))
	assert_test(r, clm.clm_string_remove(id, 1, 1) == 1)
	assert_test(r, string_test_equals(id, "Aé🐱!"))
	assert_test(r, clm.clm_string_replace(id, "é", "中") == 1)
	assert_test(r, string_test_equals(id, "A中🐱!"))
	assert_test(r, clm.clm_string_replace(id, "!", "") == 1)
	assert_test(r, clm.clm_string_contains(id, "🐱"))
	assert_test(r, clm.clm_string_append_line(id, "!") == 2)
	assert_test(r, string_test_equals(id, "A中🐱!\n"))
	assert_test(r, clm.clm_string_contains(id, "\n"))
	assert_test(r, clm.clm_string_length(id) == 5)
	assert_test(r, clm.clm_string_remove(id, -1, 1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_insert(id, 0x7fff_ffff, "x") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_replace(id, "", "x") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_set(id, "red red") == 7)
	assert_test(r, clm.clm_string_replace(id, "red", "blue") == 2)
	assert_test(r, string_test_equals(id, "blue blue"))
	assert_test(r, clm.clm_string_replace(id, "absent", "x") == 0)
	assert_test(r, clm.clm_string_contains(id, ""))
	assert_test(r, !clm.clm_string_contains(id, "absent"))
	assert_test(r, clm.clm_string_set(id, "aaaaa") == 5)
	assert_test(r, clm.clm_string_replace(id, "aa", "b") == 2)
	assert_test(r, string_test_equals(id, "bba"), "replace uses non-overlapping matches")
	assert_test(r, clm.clm_string_replace(id, "b", "bb") == 2)
	assert_test(r, string_test_equals(id, "bbbba"), "replacement text is not searched again")
	assert_test(r, clm.clm_string_remove(id, 5, 0) == 0)
	assert_test(r, clm.clm_string_remove(id, 6, 0) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_remove(id, 0, -1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_remove(id, 4, 2) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_insert(id, -1, "x") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_insert(id, 0, "") == 0)
	assert_test(r, clm.clm_string_append(id, "") == 0)
	assert_test(r, clm.clm_string_remove(id, 0, 5) == 5)
	assert_test(r, string_test_equals(id, ""))
	assert_test(r, clm.clm_string_length(id) == 0)
	assert_test(r, clm.clm_string_remove(id, 0, 0) == 0)
	assert_test(r, clm.clm_string_append_line(id, "") == 1)
	assert_test(r, string_test_equals(id, "\n"))
	assert_test(r, clm.clm_string_set(id, "é🐱") == 2)
	borrowed: string
	assert_test(r, clm.clm_string_slice_borrow(id, 0, 2, &borrowed) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_set(id, borrowed) == 2, "set accepts a view of its own storage")
	assert_test(r, string_test_equals(id, "é🐱"))
	assert_test(r, clm.clm_string_slice_borrow(id, 0, 1, &borrowed) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_append(id, borrowed) == 1, "append accepts a view of its own storage")
	assert_test(r, string_test_equals(id, "é🐱é"))
	scratch :: clm.idkey(90002)
	assert_test(r, clm.clm_string_new(scratch, "scratch"))
	assert_test(r, clm.clm_string_delete(scratch))
	assert_test(r, !clm.clm_string_delete(scratch))
}

string_test_encodings :: proc(r: ^test_result) {
	latin :: clm.idkey(90100)
	latin_copy :: clm.idkey(90101)
	unicode :: clm.idkey(90102)
	unicode_copy :: clm.idkey(90103)
	invalid :: clm.idkey(90104)
	defer clm.clm_string_set_default_encoding(clm.CLM_STRING_ENCODING_UTF8)
	defer clm.clm_string_delete(latin)
	defer clm.clm_string_delete(latin_copy)
	defer clm.clm_string_delete(unicode)
	defer clm.clm_string_delete(unicode_copy)
	expected_latin := []u8{'c', 'a', 'f', 0xe9}
	expected_utf16 := []u8{0x41, 0, 0xe9, 0, 0x3d, 0xd8, 0x31, 0xdc}
	encoded: [16]u8
	assert_test(r, clm.clm_string_get_default_encoding() == clm.CLM_STRING_ENCODING_UTF8)
	assert_test(r, clm.clm_string_set_default_encoding(clm.CLM_STRING_ENCODING_LATIN1) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_new(latin, "café"))
	assert_test(r, clm.clm_bpa_find_block(latin).encoding == clm.CLM_STRING_ENCODING_LATIN1)
	assert_test(r, clm.clm_string_get_bytes(latin, nil) == 4)
	for &b in encoded { b = 0xa5 }
	assert_test(r, clm.clm_string_get_bytes(latin, encoded[:3]) == clm.CLM_BPA_FAILURE)
	assert_test(r, encoded[0] == 0xa5 && encoded[2] == 0xa5 && encoded[3] == 0xa5, "short output remains untouched")
	assert_test(r, clm.clm_string_get_bytes(latin, encoded[:]) == 4)
	assert_test(r, bytes.equal(encoded[:4], expected_latin) && encoded[4] == 0xa5, "Latin-1 export has no NUL")
	assert_test(r, clm.clm_string_from_bytes(latin_copy, clm.CLM_STRING_ENCODING_LATIN1, expected_latin) == clm.CLM_BPA_SUCCESS)
	assert_test(r, string_test_equals(latin_copy, "café"))
	assert_test(r, clm.clm_bpa_find_block(latin_copy).encoding == clm.CLM_STRING_ENCODING_LATIN1)
	assert_test(r, clm.clm_string_from_bytes(latin_copy, clm.CLM_STRING_ENCODING_UTF8, []u8{0xc0, 0xaf}) == clm.CLM_BPA_FAILURE)
	assert_test(r, string_test_equals(latin_copy, "café"))
	assert_test(r, clm.clm_bpa_find_block(latin_copy).encoding == clm.CLM_STRING_ENCODING_LATIN1, "failed import preserves encoding")
	assert_test(r, clm.clm_string_from_bytes(invalid, clm.CLM_STRING_ENCODING_UTF16LE, []u8{0, 0xd8}) == clm.CLM_BPA_FAILURE)
	assert_test(r, !clm.clm_bpa_is_existing(invalid))
	assert_test(r, clm.clm_string_from_bytes(invalid, clm.CLM_STRING_ENCODING_UTF8, []u8{'A', 0, 'B'}) == clm.CLM_BPA_FAILURE)
	assert_test(r, !clm.clm_bpa_is_existing(invalid))
	assert_test(r, clm.clm_string_from_bytes(invalid, clm.CLM_STRING_ENCODING_UTF16LE, expected_utf16[:1]) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_set_default_encoding(0xff) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_get_default_encoding() == clm.CLM_STRING_ENCODING_LATIN1)
	assert_test(r, clm.clm_string_new(unicode, "Aé🐱"))
	assert_test(r, clm.clm_string_get_bytes(unicode, nil) == clm.CLM_BPA_FAILURE, "default preference may be unrepresentable")
	assert_test(r, clm.clm_string_change_encoding(unicode, clm.CLM_STRING_ENCODING_UTF16LE) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_get_bytes(unicode, nil) == 8)
	for &b in encoded { b = 0xa5 }
	assert_test(r, clm.clm_string_get_bytes(unicode, encoded[:7]) == clm.CLM_BPA_FAILURE)
	assert_test(r, encoded[0] == 0xa5 && encoded[6] == 0xa5)
	assert_test(r, clm.clm_string_get_bytes(unicode, encoded[:]) == 8)
	assert_test(r, bytes.equal(encoded[:8], expected_utf16) && encoded[8] == 0xa5, "UTF-16LE surrogate pair, without BOM or NUL")
	assert_test(r, clm.clm_string_from_bytes(unicode_copy, clm.CLM_STRING_ENCODING_UTF16LE, expected_utf16) == clm.CLM_BPA_SUCCESS)
	assert_test(r, string_test_equals(unicode_copy, "Aé🐱"))
	assert_test(r, clm.clm_string_change_encoding(unicode, clm.CLM_STRING_ENCODING_LATIN1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_bpa_find_block(unicode).encoding == clm.CLM_STRING_ENCODING_UTF16LE)
	assert_test(r, clm.clm_string_change_encoding(unicode, 0xff) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_change_encoding(unicode, clm.CLM_STRING_ENCODING_UTF8) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_get_bytes(unicode, nil) == 7)
	for &b in encoded { b = 0xa5 }
	assert_test(r, clm.clm_string_get_bytes(unicode, encoded[:7]) == 7)
	assert_test(r, string(encoded[:7]) == "Aé🐱" && encoded[7] == 0xa5)
	identity := clm.clm_bpa_find_block(unicode_copy)
	assert_test(r, clm.clm_string_from_bytes(unicode_copy, clm.CLM_STRING_ENCODING_UTF8, transmute([]u8)string("new")) == clm.CLM_BPA_SUCCESS)
	assert_test(r, identity == clm.clm_bpa_find_block(unicode_copy), "import preserves BPA resource identity")
	assert_test(r, string_test_equals(unicode_copy, "new"))
	assert_test(r, clm.clm_string_from_bytes(unicode_copy, clm.CLM_STRING_ENCODING_UTF16LE, nil) == clm.CLM_BPA_SUCCESS)
	assert_test(r, string_test_equals(unicode_copy, "") && clm.clm_string_get_bytes(unicode_copy, nil) == 0)
	assert_test(r, clm.clm_string_set_default_encoding(clm.CLM_STRING_ENCODING_UTF8) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_bpa_find_block(latin).encoding == clm.CLM_STRING_ENCODING_LATIN1)

	for encoding in ([]u8{clm.CLM_STRING_ENCODING_UTF8, clm.CLM_STRING_ENCODING_LATIN1, clm.CLM_STRING_ENCODING_UTF16LE}) {
		assert_test(r, clm.clm_string_from_bytes(invalid, encoding, nil) == clm.CLM_BPA_SUCCESS)
		assert_test(r, clm.clm_string_length(invalid) == 0)
		assert_test(r, clm.clm_string_delete(invalid))
	}
	for data in ([][]u8{
		{0, 0xdc}, {0, 0xd8, 'A', 0}, {0, 0xd8, 0, 0xd8},
		{0, 0xdc, 0, 0xd8}, {0, 0}, {'A', 0, 0, 0},
	}) {
		assert_test(r, clm.clm_string_from_bytes(invalid, clm.CLM_STRING_ENCODING_UTF16LE, data) == clm.CLM_BPA_FAILURE)
		assert_test(r, !clm.clm_bpa_is_existing(invalid))
	}
	assert_test(r, clm.clm_string_from_bytes(invalid, clm.CLM_STRING_ENCODING_LATIN1, []u8{'A', 0}) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_from_bytes(invalid, 0xff, nil) == clm.CLM_BPA_FAILURE)
	// Scalar boundaries exercise every UTF-8/UTF-16 encoding width.
	edge_text :: "\x01\x7f\u0080\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff"
	assert_test(r, clm.clm_string_new(invalid, edge_text))
	assert_test(r, clm.clm_string_length(invalid) == 10)
	assert_test(r, clm.clm_string_change_encoding(invalid, clm.CLM_STRING_ENCODING_UTF16LE) == clm.CLM_BPA_SUCCESS)
	edge_bytes: [24]u8
	assert_test(r, clm.clm_string_get_bytes(invalid, edge_bytes[:]) == 24)
	assert_test(r, clm.clm_string_from_bytes(unicode_copy, clm.CLM_STRING_ENCODING_UTF16LE, edge_bytes[:]) == clm.CLM_BPA_SUCCESS)
	assert_test(r, string_test_equals(unicode_copy, edge_text))
	assert_test(r, clm.clm_string_delete(invalid))
}

string_test_slices :: proc(r: ^test_result) {
	source :: clm.idkey(90200)
	copy_id :: clm.idkey(90201)
	empty_id :: clm.idkey(90202)
	invalid :: clm.idkey(90203)
	assert_test(r, clm.clm_string_new(source, "Aé🐱"))
	defer clm.clm_string_delete(source)
	defer clm.clm_string_delete(copy_id)
	defer clm.clm_string_delete(empty_id)
	assert_test(r, clm.clm_string_change_encoding(source, clm.CLM_STRING_ENCODING_UTF16LE) == clm.CLM_BPA_SUCCESS)
	borrowed: string
	assert_test(r, clm.clm_string_slice_borrow(source, 1, 2, &borrowed) == clm.CLM_BPA_SUCCESS)
	assert_test(r, borrowed == "é🐱" && len(borrowed) == 6)
	block := clm.clm_bpa_find_block(source)
	assert_test(r, raw_data(borrowed) == &([^]u8)(block.data)[1], "borrowed view aliases owner bytes")
	assert_test(r, clm.clm_string_slice(source, 1, 2, copy_id) == clm.CLM_BPA_SUCCESS)
	assert_test(r, string_test_equals(copy_id, "é🐱"))
	assert_test(r, clm.clm_bpa_find_block(copy_id).encoding == clm.CLM_STRING_ENCODING_UTF16LE)
	assert_test(r, clm.clm_string_slice_borrow(source, 3, 0, &borrowed) == clm.CLM_BPA_SUCCESS)
	assert_test(r, len(borrowed) == 0 && raw_data(borrowed) != nil)
	assert_test(r, clm.clm_string_slice(source, 3, 0, empty_id) == clm.CLM_BPA_SUCCESS)
	assert_test(r, string_test_equals(empty_id, ""))
	assert_test(r, clm.clm_string_slice(source, 4, 0, invalid) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice(source, -1, 1, invalid) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice(source, 0, -1, invalid) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice(source, 2, 2, invalid) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice(source, 0x7fff_ffff, 0x7fff_ffff, invalid) == clm.CLM_BPA_FAILURE)
	assert_test(r, !clm.clm_bpa_is_existing(invalid))
	assert_test(r, clm.clm_string_slice_borrow(source, 4, 0, &borrowed) == clm.CLM_BPA_FAILURE)
	assert_test(r, borrowed == "")
	assert_test(r, clm.clm_string_slice_borrow(source, -1, 1, &borrowed) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice_borrow(source, 0, -1, &borrowed) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice_borrow(source, 0, 1, nil) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice(source, 0, 1, copy_id) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_slice(source, 0, 1, source) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_set(source, "changed") == 7)
	assert_test(r, string_test_equals(copy_id, "é🐱"), "copied slice survives source mutation")
	assert_test(r, clm.clm_string_delete(source))
	assert_test(r, string_test_equals(copy_id, "é🐱"), "copied slice survives source deletion")
}

string_test_arrays :: proc(r: ^test_result) {
	array :: clm.idkey(90300)
	first :: clm.idkey(90301)
	second :: clm.idkey(90302)
	third :: clm.idkey(90303)
	scratch :: clm.idkey(90304)
	assert_test(r, clm.clm_string_new(first, "pear"))
	assert_test(r, clm.clm_string_new(second, "apple"))
	assert_test(r, clm.clm_string_new(third, "banana"))
	assert_test(r, clm.clm_string_new(scratch, ""))
	defer clm.clm_string_delete(first)
	defer clm.clm_string_delete(second)
	defer clm.clm_string_delete(third)
	defer clm.clm_string_delete(scratch)
	assert_test(r, clm.clm_string_array_new(array, 0))
	defer clm.clm_string_array_delete(array)
	assert_test(r, !clm.clm_string_array_new(array, 0))
	assert_test(r, clm.clm_string_array_count(array) == 0)
	assert_test(r, clm.clm_string_array_move_first(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_sort(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_shuffle(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_draw(array, scratch) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_append(array, first) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_append(array, second) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_insert(array, 1, third) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_count(array) == 3)
	assert_test(r, clm.clm_string_array_find(array, third) == 1)
	assert_test(r, clm.clm_string_array_get(array, 1, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, string_test_equals(scratch, "banana"))
	assert_test(r, clm.clm_string_array_length_at(array, 1) == 6)
	assert_test(r, clm.clm_string_array_contains_at(array, 1, "ana"))
	assert_test(r, clm.clm_string_array_append_at(array, 1, "!") == 1)
	assert_test(r, clm.clm_string_array_append_line_at(array, 1, ".") == 2)
	assert_test(r, clm.clm_string_array_insert_at(array, 1, 0, "!") == 1)
	assert_test(r, clm.clm_string_array_remove_at(array, 1, 0, 1) == 1)
	assert_test(r, clm.clm_string_array_replace_at(array, 1, "a", "A") == 3)
	assert_test(r, clm.clm_string_array_get(array, 1, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, string_test_equals(scratch, "bAnAnA!.\n"))
	assert_test(r, clm.clm_string_array_set(array, 0, second) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(r, clm.clm_string_array_set(array, 1, third) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(r, clm.clm_string_array_sort(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_move_last(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_get_current(array, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, string_test_equals(scratch, "banana"))
	assert_test(r, clm.clm_string_array_move_first(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_move_next(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_move_previous(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_move_to(array, 2) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_remove(array, 1) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_count(array) == 2)
	assert_test(r, clm.clm_string_array_shuffle(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_draw_at(array, 0, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, clm.clm_bpaa_find_array(array).deck_size == 1)
	assert_test(r, clm.clm_string_array_draw(array, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, clm.clm_bpaa_find_array(array).deck_size == 0)
	assert_test(r, clm.clm_string_array_draw(array, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, clm.clm_string_array_draw_at(array, 99, scratch) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_draw_at(array, -1, scratch) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_clear(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_count(array) == 0)
	// Independent copies and non-ASCII indexed edits.
	assert_test(r, clm.clm_string_set(first, "Aé🐱") == 3)
	assert_test(r, clm.clm_string_change_encoding(first, clm.CLM_STRING_ENCODING_UTF16LE) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_array_append(array, first) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_string_set(first, "changed") == 7)
	assert_test(r, clm.clm_string_array_length_at(array, 0) == 3)
	assert_test(r, clm.clm_string_array_insert_at(array, 0, 1, "中") == 1)
	assert_test(r, clm.clm_string_array_remove_at(array, 0, 2, 1) == 1)
	assert_test(r, clm.clm_string_array_replace_at(array, 0, "🐱", "éé") == 1)
	assert_test(r, clm.clm_string_array_get(array, 0, scratch) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(r, string_test_equals(scratch, "A中éé"))
	assert_test(r, clm.clm_bpa_find_block(scratch).encoding == clm.CLM_STRING_ENCODING_UTF8)
	assert_test(r, clm.clm_string_array_replace_at(array, 0, "missing", "") == 0)
	assert_test(r, clm.clm_string_array_replace_at(array, 0, "é", "") == 2)
	assert_test(r, clm.clm_string_array_remove_at(array, 0, 2, 0) == 0)
	assert_test(r, clm.clm_string_array_remove_at(array, 0, 3, 0) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_remove_at(array, 0, -1, 1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_remove_at(array, 0, 0, -1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_insert_at(array, 0, -1, "") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_insert_at(array, 0, 3, "") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_insert_at(array, 0, 2, "") == 0)
	assert_test(r, clm.clm_string_array_contains_at(array, 0, ""))
	assert_test(r, clm.clm_string_array_replace_at(array, 0, "", "x") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_append_at(array, 0, "\xc0\xaf") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_append_line_at(array, 0, "\xc0\xaf") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_insert_at(array, 0, 0, "\xed\xa0\x80") == clm.CLM_BPA_FAILURE)
	assert_test(r, !clm.clm_string_array_contains_at(array, 0, "\x80"))
	assert_test(r, clm.clm_string_array_replace_at(array, 0, "A", "\xff") == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_replace_at(array, 0, "\xff", "") == clm.CLM_BPA_FAILURE)
	for index in ([]i32{-1, 1, 0x7fff_ffff}) {
		assert_test(r, clm.clm_string_array_length_at(array, index) == clm.CLM_BPA_FAILURE)
		assert_test(r, !clm.clm_string_array_contains_at(array, index, ""))
		assert_test(r, clm.clm_string_array_append_at(array, index, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_append_line_at(array, index, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_insert_at(array, index, 0, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_remove_at(array, index, 0, 0) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_replace_at(array, index, "A", "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_get(array, index, scratch) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_set(array, index, first) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_remove(array, index) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_move_to(array, index) == clm.CLM_BPA_FAILURE)
	}
	assert_test(r, clm.clm_string_array_find(array, first) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_find(array, 999999) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_get(array, 0, 999999) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_set(array, 0, 999999) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_insert(array, 0, 999999) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_array_append(array, 999999) == clm.CLM_BPA_FAILURE)
	deck_before := clm.clm_bpaa_find_array(array).deck_size
	assert_test(r, clm.clm_string_array_draw_at(array, 0, 999999) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_bpaa_find_array(array).deck_size == deck_before, "failed draw does not consume item")
	capacity := clm.clm_bpaa_find_array(array).capacity
	assert_test(r, clm.clm_string_array_clear(array) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_bpaa_find_array(array).capacity == capacity)
	// Heap sort exercises odd/even sizes, duplicate keys and UTF-8 byte order.
	for value in ([]string{"🐱", "é", "a", "中", "a", "", "z"}) {
		assert_test(r, clm.clm_string_set(first, value) >= 0)
		assert_test(r, clm.clm_string_array_append(array, first) == clm.CLM_BPA_SUCCESS)
	}
	assert_test(r, clm.clm_string_array_sort(array) == clm.CLM_BPA_SUCCESS)
	for expected, i in ([]string{"", "a", "a", "z", "é", "中", "🐱"}) {
		assert_test(r, clm.clm_string_array_get(array, i32(i), scratch) == clm.CLM_BPA_GET_SUCCESS)
		assert_test(r, string_test_equals(scratch, expected))
	}
	assert_test(r, clm.clm_string_array_delete(array))
	assert_test(r, !clm.clm_string_array_delete(array))
	assert_test(r, clm.clm_string_array_count(array) == clm.CLM_BPA_FAILURE)
	assert_test(r, !clm.clm_string_array_new(array, -1))
}

string_test_invalid :: proc(r: ^test_result) {
	id :: clm.idkey(90400)
	other :: clm.idkey(90401)
	assert_test(r, clm.clm_string_new(id, "unchanged"))
	defer clm.clm_string_delete(id)
	for text in ([]string{
		"\xc0\xaf", "\xc1\xbf", "\x80", "\xff", "\xfe", "\xc2",
		"\xe0\x80\x80", "\xe2\x82", "\xed\xa0\x80", "\xed\xbf\xbf",
		"\xf0\x80\x80\x80", "\xf0\x9f\x90", "\xf4\x90\x80\x80",
		"\xf5\x80\x80\x80", "\xe2A\xac", "\xf8\x88\x80\x80\x80", "A\x00B",
	}) {
		assert_test(r, !clm.clm_string_new(other, text))
		assert_test(r, !clm.clm_bpa_is_existing(other))
		assert_test(r, clm.clm_string_set(id, text) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_append(id, text) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_append_line(id, text) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_insert(id, 0, text) == clm.CLM_BPA_FAILURE)
		assert_test(r, !clm.clm_string_contains(id, text))
		assert_test(r, clm.clm_string_replace(id, text, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_replace(id, "u", text) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_from_bytes(id, clm.CLM_STRING_ENCODING_UTF8, transmute([]u8)text) == clm.CLM_BPA_FAILURE)
		assert_test(r, string_test_equals(id, "unchanged"), "invalid operations preserve original text")
	}
	assert_test(r, clm.clm_bpa_create(other, clm.CLM_BPA_TYPE_INT) == clm.CLM_BPA_SUCCESS)
	for missing_or_wrong in ([]clm.idkey{other, 999999}) {
		assert_test(r, !clm.clm_string_delete(missing_or_wrong))
		assert_test(r, clm.clm_string_length(missing_or_wrong) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_set(missing_or_wrong, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_append(missing_or_wrong, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_append_line(missing_or_wrong, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_insert(missing_or_wrong, 0, "") == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_remove(missing_or_wrong, 0, 0) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_replace(missing_or_wrong, "a", "") == clm.CLM_BPA_FAILURE)
		assert_test(r, !clm.clm_string_contains(missing_or_wrong, ""))
		assert_test(r, clm.clm_string_get_bytes(missing_or_wrong, nil) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_change_encoding(missing_or_wrong, clm.CLM_STRING_ENCODING_UTF8) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_slice(missing_or_wrong, 0, 0, 999998) == clm.CLM_BPA_FAILURE)
		assert_test(r, !clm.clm_string_array_delete(missing_or_wrong))
		assert_test(r, clm.clm_string_array_count(missing_or_wrong) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_sort(missing_or_wrong) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_shuffle(missing_or_wrong) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_clear(missing_or_wrong) == clm.CLM_BPA_FAILURE)
		assert_test(r, clm.clm_string_array_draw(missing_or_wrong, id) == clm.CLM_BPA_FAILURE)
	}
	assert_test(r, clm.clm_string_from_bytes(other, clm.CLM_STRING_ENCODING_UTF8, transmute([]u8)string("text")) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_bpa_is_existing(other), "string delete/import preserves non-string blocks")
	assert_test(r, clm.clm_bpa_delete(other) == clm.CLM_BPA_SUCCESS)
	assert_test(r, clm.clm_bpaa_create(other, clm.CLM_BPA_TYPE_INT, 0, clm.CLM_STRING_ENCODING_UTF8) == clm.CLM_BPA_SUCCESS)
	assert_test(r, !clm.clm_string_array_delete(other))
	assert_test(r, clm.clm_bpaa_find_array(other) != nil)
	assert_test(r, clm.clm_string_array_append(other, id) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_bpaa_delete(other) == clm.CLM_BPA_SUCCESS)

	count, offset := -7, 0
	assert_test(r, clm.clm_string_measure("é", 2, clm.CLM_STRING_ENCODING_UTF8, &count) == clm.CLM_BPA_SUCCESS && count == 1)
	assert_test(r, clm.clm_string_measure("é", 1, clm.CLM_STRING_ENCODING_UTF8, &count) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_measure("", 0, clm.CLM_STRING_ENCODING_LATIN1, &count) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_measure("", 1, clm.CLM_STRING_ENCODING_UTF8, &count) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_measure("", 0, clm.CLM_STRING_ENCODING_UTF8, nil) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_offset("Aé🐱", 7, clm.CLM_STRING_ENCODING_UTF8, 3, &offset) == clm.CLM_BPA_SUCCESS && offset == 7)
	assert_test(r, clm.clm_string_offset("Aé🐱", 7, clm.CLM_STRING_ENCODING_UTF8, 4, &offset) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_offset("", 0, clm.CLM_STRING_ENCODING_LATIN1, 0, &offset) == clm.CLM_BPA_FAILURE)
	assert_test(r, !clm.clm_string_utf8_next(nil, 0, &offset))
	assert_test(r, !clm.clm_string_utf8_next([]u8{'A'}, 1, nil))
	scalar: u32
	offset = 0
	assert_test(r, clm.clm_string_utf8_decode_one(transmute([]u8)string("🐱"), 4, &offset, &scalar) == clm.CLM_BPA_SUCCESS && scalar == 0x1f431 && offset == 4)
	out: [4]u8
	assert_test(r, clm.clm_string_utf8_encode_one(0x1f431, out[:]) == 4 && string(out[:]) == "🐱")
	assert_test(r, clm.clm_string_utf8_encode_one(0xd800, out[:]) == 0)
	assert_test(r, clm.clm_string_utf8_encode_one(0x110000, out[:]) == 0)
	assert_test(r, clm.clm_string_utf8_encode_one(0x1f431, out[:3]) == 0)
}

string_test_numbers :: proc(r: ^test_result) {
	id :: clm.idkey(90500)
	assert_test(r, clm.clm_string_new(id, ""))
	defer clm.clm_string_delete(id)
	signed: i64
	unsigned: u64
	real: f64
	for value in ([]i64{0, 1, -1, 0x7fff_ffff_ffff_ffff, -0x7fff_ffff_ffff_ffff-1}) {
		assert_test(r, clm.clm_string_from_int(id, value) > 0)
		assert_test(r, clm.clm_string_to_int(id, &signed) == clm.CLM_BPA_SUCCESS && signed == value)
	}
	for value in ([]u64{0, 1, 0xffff_ffff_ffff_ffff}) {
		assert_test(r, clm.clm_string_from_uint(id, value) > 0)
		assert_test(r, clm.clm_string_to_uint(id, &unsigned) == clm.CLM_BPA_SUCCESS && unsigned == value)
	}
	for value in ([]f64{0, -1.5, 1234.125, 1e100, 1e-100}) {
		assert_test(r, clm.clm_string_from_float(id, value) > 0)
		assert_test(r, clm.clm_string_to_float(id, &real) == clm.CLM_BPA_SUCCESS && real == value)
	}
	for text in ([]string{"", " ", "12x", "1.5", "9223372036854775808", "-9223372036854775809"}) {
		signed = 123
		assert_test(r, clm.clm_string_set(id, text) >= 0)
		assert_test(r, clm.clm_string_to_int(id, &signed) == clm.CLM_BPA_FAILURE && signed == 123)
	}
	for text in ([]string{"", "-1", "18446744073709551616", "1x"}) {
		unsigned = 123
		assert_test(r, clm.clm_string_set(id, text) >= 0)
		assert_test(r, clm.clm_string_to_uint(id, &unsigned) == clm.CLM_BPA_FAILURE && unsigned == 123)
	}
	for text in ([]string{"", "x", "1.5x"}) {
		real = 123
		assert_test(r, clm.clm_string_set(id, text) >= 0)
		assert_test(r, clm.clm_string_to_float(id, &real) == clm.CLM_BPA_FAILURE && real == 123)
	}
	assert_test(r, clm.clm_string_to_int(id, nil) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_to_uint(id, nil) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_to_float(id, nil) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_to_int(999999, &signed) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_to_uint(999999, &unsigned) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_to_float(999999, &real) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_from_int(999999, 1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_from_uint(999999, 1) == clm.CLM_BPA_FAILURE)
	assert_test(r, clm.clm_string_from_float(999999, 1) == clm.CLM_BPA_FAILURE)
}
