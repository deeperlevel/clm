//! Tests for typed integer-key dictionaries and insertion-order iteration.

use super::clm::{bpa, dictionary};
use super::{TestResult, TestRunner};

/// Runs dictionary creation, typed copies, mutation, and iteration checks.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - Dictionary");
	let id = 71_000;
	runner.assert(
		&mut result,
		dictionary::new(id, bpa::TYPE_INT, 0) == bpa::SUCCESS
			&& dictionary::count(id) == 0,
		"create an empty integer dictionary",
	);
	runner.assert(
		&mut result,
		dictionary::new(id, bpa::TYPE_INT, 0) == bpa::FAILURE
			&& dictionary::new(71_003, bpa::TYPE_STRING, 1) == bpa::FAILURE,
		"reject duplicate IDs and invalid string value widths",
	);

	let mut inserted = true;
	for index in 0..40_u64 {
		inserted &= dictionary::set_from(id, 1_000 + index, &((index * 10) as i32).to_ne_bytes())
			== bpa::SET_SUCCESS;
	}
	runner.assert(
		&mut result,
		inserted && dictionary::count(id) == 40,
		"insert entries and grow dictionary storage",
	);
	let mut output = [0_u8; 4];
	runner.assert(
		&mut result,
		dictionary::set_from(id, 1_005, &9_876_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& dictionary::get_to(id, 1_005, &mut output) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(output) == 9_876
			&& dictionary::count(id) == 40,
		"replace a value without adding a key",
	);

	let source_id = 71_001;
	let destination_id = 71_002;
	runner.assert(
		&mut result,
		bpa::create(source_id, bpa::TYPE_INT) == bpa::SUCCESS
			&& bpa::set(source_id, &9_876_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& dictionary::set(id, 0, source_id) == bpa::SET_SUCCESS
			&& bpa::create(destination_id, bpa::TYPE_INT) == bpa::SUCCESS
			&& dictionary::get(id, 0, destination_id) == bpa::GET_SUCCESS
			&& bpa::block_bytes(destination_id) == Some(9_876_i32.to_ne_bytes().to_vec())
			&& dictionary::new(71_010, bpa::TYPE_STRING, 0) == bpa::SUCCESS,
		"copy values into dictionaries from BPA blocks",
	);
	runner.assert(
		&mut result,
		dictionary::new(71_011, bpa::TYPE_STRUCT, 8) == bpa::SUCCESS
			&& dictionary::set_from(71_011, 7, &[11, 0, 0, 0, 22, 0, 0, 0]) == bpa::SET_SUCCESS
			&& dictionary::get_to(71_011, 7, &mut [0_u8; 8]) == bpa::GET_SUCCESS,
		"store fixed-size structure values",
	);
	runner.assert(
		&mut result,
		dictionary::add_from(id, 1_000, &1_i32.to_ne_bytes()) == bpa::FAILURE
			&& dictionary::update_from(id, 99_000, &1_i32.to_ne_bytes()) == bpa::FAILURE
			&& dictionary::update_from(id, 1_000, &7_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& dictionary::count(id) == 41,
		"enforce insert-only and update-only keyed operations",
	);

	let first_keys = (0..40_u64).map(|index| 1_000 + index).collect::<Vec<_>>();
	runner.assert(
		&mut result,
		dictionary::contains(id, 0),
		"retain key zero copied from a BPA block",
	);
	let expected_keys = [first_keys, vec![0]].concat();
	runner.assert(
		&mut result,
		dictionary::keys(id).is_some_and(|keys| keys == expected_keys),
		"retain key insertion order",
	);
	runner.assert(
		&mut result,
		dictionary::remove(id, 1_005) == bpa::SUCCESS
			&& !dictionary::contains(id, 1_005)
			&& dictionary::set_from(id, 99_999, &12_345_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& dictionary::count(id) == 41,
		"remove keys and insert into the remaining dictionary",
	);
	runner.assert(
		&mut result,
		dictionary::iterate_reset(id) == bpa::SUCCESS
			&& dictionary::iterate_next(id) == Some(1_000)
			&& dictionary::iterate_next(id) == Some(1_001),
		"iterate keys in insertion order",
	);
	runner.assert(
		&mut result,
		dictionary::remove(id, 1_000) == bpa::SUCCESS
			&& dictionary::add_from(id, 1_000, &1_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& dictionary::keys(id).is_some_and(|keys| keys.last() == Some(&1_000)),
		"append a removed-and-reinserted key to the end of iteration order",
	);
	runner.assert(
		&mut result,
		dictionary::clear(id) == bpa::SUCCESS
			&& dictionary::count(id) == 0
			&& !dictionary::contains(id, 1_000)
			&& dictionary::delete(id) == bpa::SUCCESS,
		"clear while retaining the dictionary, then delete it",
	);

	let text = b"dictionary value\0";
	let mut output_text = [0_u8; 32];
	runner.assert(
		&mut result,
		bpa::create(71_004, bpa::TYPE_STRING) == bpa::SUCCESS
			&& bpa::set_string(71_004, text) == 16
			&& dictionary::set(71_010, 5, 71_004) == bpa::SET_SUCCESS
			&& dictionary::get_to(71_010, 5, &mut output_text) == bpa::GET_SUCCESS
			&& &output_text[..text.len()] == text,
		"store and retrieve NUL-terminated string values",
	);
	runner.assert(
		&mut result,
		dictionary::set_from(71_010, 6, b"\0") == bpa::SET_SUCCESS
			&& dictionary::get_to(71_010, 6, &mut output_text) == bpa::GET_SUCCESS
			&& output_text[0] == 0,
		"store and retrieve an empty string",
	);
	runner.assert(
		&mut result,
		dictionary::get(71_010, 5, 71_012) == bpa::FAILURE
			&& bpa::create(71_012, bpa::TYPE_STRING) == bpa::SUCCESS
			&& dictionary::get(71_010, 5, 71_012) == bpa::GET_SUCCESS
			&& bpa::string_bytes(71_012).as_deref() == Some(&text[..text.len() - 1]),
		"retrieve a string into a compatible BPA block",
	);

	for dict_id in [71_010, 71_011] {
		runner.assert(
			&mut result,
			dictionary::delete(dict_id) == bpa::SUCCESS,
			"delete a typed dictionary",
		);
	}
	runner.assert(
		&mut result,
		bpa::delete(source_id) == bpa::SUCCESS
			&& bpa::delete(destination_id) == bpa::SUCCESS
			&& bpa::delete(71_004) == bpa::SUCCESS
			&& bpa::delete(71_012) == bpa::SUCCESS,
		"release source and destination BPA blocks",
	);
	runner.summary(&result);
}