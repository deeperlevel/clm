//! Tests for Unicode string blocks, byte encodings, and string arrays.

use super::clm::{bpa, string};
use super::{TestResult, TestRunner};

/// Runs string editing, encoding conversion, slicing, and array checks.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - String");
	let string_id = 90_001;
	let initial = "Aé🐱";
	runner.assert(
		&mut result,
		string::new(string_id, initial) && string::length(string_id) == 3,
		"create UTF-8 text and count Unicode scalar values",
	);
	runner.assert(
		&mut result,
		bpa::block_info(string_id).is_some_and(|info| info.encoding == string::ENCODING_UTF8)
			&& string::append(string_id, "!") == 1
			&& string::insert(string_id, 1, "中") == 1
			&& string::slice_borrow(string_id, 0, 5).as_deref() == Some("A中é🐱!"),
		"append and insert by Unicode character index",
	);
	runner.assert(
		&mut result,
		string::remove(string_id, 1, 1) == 1
			&& string::replace(string_id, "é", "中") == 1
			&& string::replace(string_id, "!", "") == 1
			&& string::contains(string_id, "🐱"),
		"remove, replace, and search Unicode text",
	);
	runner.assert(
		&mut result,
		string::append_line(string_id, "!") == 2
			&& string::contains(string_id, "\n")
			&& string::remove(string_id, usize::MAX, 1) == bpa::FAILURE
			&& string::insert(string_id, usize::MAX, "x") == bpa::FAILURE
			&& string::replace(string_id, "", "x") == bpa::FAILURE,
		"append lines and reject invalid edit ranges",
	);

	runner.assert(
		&mut result,
		string::get_default_encoding() == string::ENCODING_UTF8
			&& string::set_default_encoding(string::ENCODING_LATIN1) == bpa::SUCCESS
			&& string::new(90_100, "café")
			&& bpa::block_info(90_100).is_some_and(|info| info.encoding == string::ENCODING_LATIN1),
		"set the export default for subsequently created strings",
	);
	let expected_latin = [b'c', b'a', b'f', 0xE9];
	let mut encoded = [0xA5_u8; 16];
	runner.assert(
		&mut result,
		string::get_bytes(90_100, None) == 4
			&& string::get_bytes(90_100, Some(&mut encoded[..3])) == bpa::FAILURE
			&& encoded[0] == 0xA5
			&& string::get_bytes(90_100, Some(&mut encoded)) == 4
			&& encoded[..4] == expected_latin,
		"export Latin-1 bytes and reject undersized output without writing",
	);
	runner.assert(
		&mut result,
		string::from_bytes(90_101, string::ENCODING_LATIN1, &expected_latin) == bpa::SUCCESS
			&& string::slice_borrow(90_101, 0, 4).as_deref() == Some("café")
			&& string::from_bytes(90_101, string::ENCODING_UTF8, &[0xC0, 0xAF]) == bpa::FAILURE
			&& string::slice_borrow(90_101, 0, 4).as_deref() == Some("café"),
		"decode Latin-1 and preserve existing content after invalid input",
	);
	runner.assert(
		&mut result,
		string::from_bytes(90_102, string::ENCODING_UTF16LE, &[0x00, 0xD8]) == bpa::FAILURE
			&& !bpa::is_existing(90_102)
			&& string::from_bytes(90_102, string::ENCODING_UTF8, b"A\0B") == bpa::FAILURE
			&& string::from_bytes(90_102, string::ENCODING_UTF16LE, &[0x41]) == bpa::FAILURE
			&& string::set_default_encoding(0xFF) == bpa::FAILURE,
		"reject malformed UTF-16, embedded NUL, odd byte counts, and unknown encodings",
	);

	let unicode = "Aé🐱";
	let expected_utf16 = [0x41, 0x00, 0xE9, 0x00, 0x3D, 0xD8, 0x31, 0xDC];
	runner.assert(
		&mut result,
		string::new(90_103, unicode)
			&& string::change_encoding(90_103, string::ENCODING_UTF16LE) == bpa::SUCCESS
			&& string::get_bytes(90_103, None) == expected_utf16.len() as i32
			&& string::get_bytes(90_103, Some(&mut encoded)) == expected_utf16.len() as i32
			&& encoded[..expected_utf16.len()] == expected_utf16,
		"export UTF-16LE including a surrogate pair",
	);
	runner.assert(
		&mut result,
		string::from_bytes(90_104, string::ENCODING_UTF16LE, &expected_utf16) == bpa::SUCCESS
			&& string::slice_borrow(90_104, 0, 3).as_deref() == Some(unicode)
			&& string::change_encoding(90_103, string::ENCODING_LATIN1) == bpa::FAILURE
			&& bpa::block_info(90_103).is_some_and(|info| info.encoding == string::ENCODING_UTF16LE),
		"round-trip UTF-16LE and keep unrepresentable encoding changes atomic",
	);
	runner.assert(
		&mut result,
		string::slice_borrow(90_104, 1, 2).as_deref() == Some("é🐱")
			&& string::slice(90_104, 1, 2, 90_105) == bpa::SUCCESS
			&& string::slice_borrow(90_105, 0, 2).as_deref() == Some("é🐱")
			&& bpa::block_info(90_105).is_some_and(|info| info.encoding == string::ENCODING_UTF16LE)
			&& string::slice(90_104, 3, 0, 90_106) == bpa::SUCCESS
			&& string::slice(90_104, 4, 0, 90_107) == bpa::FAILURE,
		"copy and snapshot Unicode slices with strict bounds",
	);

	runner.assert(
		&mut result,
		string::new(90_200, "pear")
			&& string::new(90_201, "apple")
			&& string::new(90_202, "banana")
			&& string::new(90_203, "")
			&& string::array_new(90_210, 0)
			&& string::array_append(90_210, 90_200) == bpa::SUCCESS
			&& string::array_append(90_210, 90_201) == bpa::SUCCESS
			&& string::array_insert(90_210, 1, 90_202) == bpa::SUCCESS
			&& string::array_count(90_210) == 3
			&& string::array_find(90_210, 90_202) == 1,
		"create and mutate a string array using BPA strings",
	);
	runner.assert(
		&mut result,
		string::array_get(90_210, 1, 90_203) == bpa::GET_SUCCESS
			&& bpa::string_bytes(90_203).as_deref() == Some(b"banana".as_slice())
			&& string::array_length_at(90_210, 1) == 6
			&& string::array_contains_at(90_210, 1, "ana")
			&& string::array_append_at(90_210, 1, "!") == 1
			&& string::array_append_line_at(90_210, 1, ".") == 2
			&& string::array_insert_at(90_210, 1, 0, "!") == 1
			&& string::array_remove_at(90_210, 1, 0, 1) == 1
			&& string::array_replace_at(90_210, 1, "a", "A") == 3
			&& string::array_get(90_210, 1, 90_203) == bpa::GET_SUCCESS
			&& bpa::string_bytes(90_203).as_deref() == Some(b"bAnAnA!.\n".as_slice()),
		"perform code-point edits on an indexed array string",
	);
	runner.assert(
		&mut result,
		bpa::create(90_204, bpa::TYPE_STRING) == bpa::SUCCESS
			&& bpa::set_string(90_204, b"valid\0") == 5
			&& bpa::create(90_205, bpa::TYPE_STRING) == bpa::SUCCESS
			&& bpa::set_string(90_205, &[0xC0, 0xAF, 0]) == 2
			&& string::array_new(90_211, 0)
			&& string::array_append(90_211, 90_204) == bpa::SUCCESS
			&& string::array_append(90_211, 90_205) == bpa::FAILURE
			&& string::array_set(90_211, 0, 90_205) == bpa::FAILURE
			&& bpa::array_string_bytes(90_211, 0).as_deref() == Some(b"valid".as_slice())
			&& string::array_delete(90_211)
			&& string::delete(90_204)
			&& string::delete(90_205),
		"reject malformed UTF-8 BPA strings in string arrays without mutating them",
	);
	runner.assert(
		&mut result,
		string::array_set(90_210, 0, 90_201) == bpa::SET_SUCCESS
			&& string::array_set(90_210, 1, 90_202) == bpa::SET_SUCCESS
			&& string::array_sort(90_210) == bpa::SUCCESS
			&& string::array_move_last(90_210) == bpa::SUCCESS
			&& string::array_get_current(90_210, 90_203) == bpa::GET_SUCCESS
			&& bpa::string_bytes(90_203).as_deref() == Some(b"banana".as_slice())
			&& string::array_move_first(90_210) == bpa::SUCCESS
			&& string::array_move_next(90_210) == bpa::SUCCESS
			&& string::array_move_previous(90_210) == bpa::SUCCESS
			&& string::array_move_to(90_210, 2) == bpa::SUCCESS
			&& string::array_remove(90_210, 1) == bpa::SUCCESS,
		"sort string arrays and navigate their cursor",
	);
	runner.assert(
		&mut result,
		string::array_shuffle(90_210) == bpa::SUCCESS
			&& string::array_draw_at(90_210, 0, 90_203) == bpa::GET_SUCCESS
			&& string::array_draw(90_210, 90_203) == bpa::GET_SUCCESS
			&& string::array_draw(90_210, 90_203) == bpa::GET_SUCCESS
			&& string::array_draw_at(90_210, 99, 90_203) == bpa::FAILURE
			&& string::array_clear(90_210) == bpa::SUCCESS
			&& string::array_count(90_210) == 0
			&& string::array_delete(90_210),
		"shuffle, draw, clear, and delete a string array",
	);

	for id in [90_001, 90_100, 90_101, 90_103, 90_104, 90_105, 90_106, 90_200, 90_201, 90_202, 90_203] {
		let _ = string::delete(id);
	}
	string::set_default_encoding(string::ENCODING_UTF8);
	runner.assert(
		&mut result,
		string::get_default_encoding() == string::ENCODING_UTF8,
		"restore the UTF-8 runtime default",
	);
	runner.summary(&result);
}