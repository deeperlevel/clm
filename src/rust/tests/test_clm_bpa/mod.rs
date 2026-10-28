//! Tests for block and array storage in `clm::bpa`.

use super::clm::bpa;
use super::{TestResult, TestRunner};

/// Runs block, array, capacity, cursor, and safe-borrow checks.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - BPA");

	let block_id = 60_001;
	let initial = 123_i32;
	let mut output = [0_u8; 4];
	runner.assert(
		&mut result,
		!bpa::is_existing(block_id) && !bpa::array_is_existing(block_id),
		"new block ID is initially unused",
	);
	runner.assert(
		&mut result,
		bpa::create(block_id, bpa::TYPE_INT) == bpa::SUCCESS,
		"create a built-in fixed-size block",
	);
	runner.assert(
		&mut result,
		bpa::set(block_id, &initial.to_ne_bytes()) == bpa::SET_SUCCESS,
		"write exact-width block contents",
	);
	runner.assert(
		&mut result,
		bpa::get(block_id, &mut output) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(output) == initial,
		"read block contents into caller-owned bytes",
	);
	runner.assert(
		&mut result,
		bpa::get(block_id, &mut output[..2]) == bpa::FAILURE
			&& bpa::set(block_id, &[0; 2]) == bpa::FAILURE,
		"reject undersized fixed-width transfers",
	);
	runner.assert(
		&mut result,
		bpa::reference(block_id) == bpa::SUCCESS
			&& bpa::unref(block_id) == bpa::SUCCESS
			&& bpa::is_existing(block_id),
		"retain and release one block reference",
	);
	runner.assert(
		&mut result,
		bpa::unref(block_id) == bpa::SUCCESS && !bpa::is_existing(block_id),
		"delete a block when its final reference is released",
	);
	let pool_before = bpa::pool_stats(4).expect("built-in integer uses a pool class");
	runner.assert(
		&mut result,
		bpa::create_sized(60_003, bpa::TYPE_STRUCT, 8) == bpa::SUCCESS
			&& bpa::create(60_004, bpa::TYPE_STRUCT) == bpa::FAILURE
			&& bpa::set(60_003, &[1, 2, 3, 4, 5, 6, 7, 8]) == bpa::SET_SUCCESS
			&& bpa::borrow(60_003) == Some(vec![1, 2, 3, 4, 5, 6, 7, 8])
			&& bpa::delete(60_003) == bpa::SUCCESS,
		"create sized struct blocks and borrow payload snapshots",
	);
	runner.assert(
		&mut result,
		bpa::create(60_005, bpa::TYPE_INT) == bpa::SUCCESS
			&& bpa::create(60_006, bpa::TYPE_INT) == bpa::SUCCESS
			&& bpa::set(60_005, &99_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& bpa::copy_block(60_005, 60_006) == bpa::SUCCESS
			&& bpa::block_bytes(60_006) == Some(99_i32.to_ne_bytes().to_vec())
			&& bpa::write_i32(60_006, 42) == bpa::SET_SUCCESS
			&& bpa::delete(60_005) == bpa::SUCCESS
			&& bpa::delete(60_006) == bpa::SUCCESS,
		"copy compatible BPA blocks and perform typed writes",
	);
	runner.assert(
		&mut result,
		bpa::create(60_007, bpa::TYPE_UNSIGNED_INT) == bpa::SUCCESS
			&& bpa::write_u32(60_007, 0x1234_5678) == bpa::SET_SUCCESS
			&& bpa::block_bytes(60_007) == Some(0x1234_5678_u32.to_ne_bytes().to_vec())
			&& bpa::create(60_008, bpa::TYPE_BYTE) == bpa::SUCCESS
			&& bpa::write_byte(60_008, 0xA5) == bpa::SET_SUCCESS
			&& bpa::block_bytes(60_008) == Some(vec![0xA5])
			&& bpa::pool_cleanup() == bpa::FAILURE
			&& bpa::delete(60_007) == bpa::SUCCESS
			&& bpa::delete(60_008) == bpa::SUCCESS,
		"write typed values and reject pool cleanup while allocations are live",
	);
	runner.assert(
		&mut result,
		bpa::pool_stats(4).is_some_and(|(active, capacity)| {
			active == pool_before.0 && capacity >= pool_before.1
		}),
		"reuse pooled allocation capacity after block deletion",
	);

	let text_id = 60_002;
	runner.assert(
		&mut result,
		bpa::create(text_id, bpa::TYPE_STRING) == bpa::SUCCESS
			&& bpa::set_string(text_id, b"hello\0") == 5,
		"create and set a bounded string block",
	);
	let mut text = [0_u8; 16];
	runner.assert(
		&mut result,
		bpa::get_string(text_id, &mut text) == 5 && &text[..6] == b"hello\0",
		"copy a string block including its terminator",
	);
	runner.assert(
		&mut result,
		bpa::replace(text_id, b"world!\0") == 6
			&& bpa::get_string(text_id, &mut text) == 6
			&& &text[..7] == b"world!\0",
		"replace a string block and report its byte length",
	);
	runner.assert(
		&mut result,
		bpa::delete(text_id) == bpa::SUCCESS,
		"delete a string block",
	);

	let array_id = 60_010;
	let values = [4_i32, 1, 3, 2];
	runner.assert(
		&mut result,
		bpa::array_create(array_id, bpa::TYPE_INT, 1, 0) == bpa::SUCCESS,
		"create a fixed-width array with reserved capacity",
	);
	let mut appends_succeeded = true;
	for value in values {
		appends_succeeded &= bpa::array_append_raw(array_id, &value.to_ne_bytes()) == bpa::SUCCESS;
	}
	runner.assert(
		&mut result,
		appends_succeeded && bpa::array_count(array_id) == Some(4),
		"append elements and grow capacity as needed",
	);
	runner.assert(
		&mut result,
		bpa::array_insert_raw(array_id, 1, &1_i32.to_ne_bytes()) == bpa::SUCCESS
			&& bpa::array_set_raw(array_id, 1, &4_i32.to_ne_bytes()) == bpa::SET_SUCCESS,
		"insert and replace initialized array items",
	);
	runner.assert(
		&mut result,
		bpa::array_count(array_id) == Some(5)
			&& bpa::array_capacity(array_id).is_some_and(|capacity| capacity >= 5)
			&& bpa::array_insert_raw(array_id, usize::MAX, &[0; 4]) == bpa::FAILURE
			&& bpa::array_remove(array_id, usize::MAX) == bpa::FAILURE,
		"query array capacity and reject invalid item positions",
	);
	let mut item = [0_u8; 4];
	runner.assert(
		&mut result,
		bpa::array_get_raw(array_id, 1, &mut item) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(item) == 4,
		"retrieve an indexed fixed-width item",
	);
	runner.assert(
		&mut result,
		bpa::array_get_first_raw(array_id, &mut item) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(item) == 4
			&& bpa::array_get_last_raw(array_id, &mut item) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(item) == 2,
		"read the first and last initialized items",
	);
	runner.assert(
		&mut result,
		bpa::array_move_to(array_id, 2) == bpa::SUCCESS
			&& bpa::array_current_index(array_id) == Some(2)
			&& bpa::array_move_next(array_id) == bpa::SUCCESS
			&& bpa::array_move_next(array_id) == bpa::SUCCESS
			&& bpa::array_move_next(array_id) == bpa::FAILURE,
		"navigate the array cursor and reject moving past the end",
	);
	runner.assert(
		&mut result,
		bpa::array_reverse(array_id) == bpa::SUCCESS
			&& bpa::array_sort_with(array_id, |left, right| {
				i32::from_ne_bytes(left.try_into().expect("four-byte element"))
					.cmp(&i32::from_ne_bytes(right.try_into().expect("four-byte element")))
			}) == bpa::SUCCESS,
		"reverse and sort fixed-width items",
	);
	let expected_sorted = [1_i32, 2, 3, 4, 4];
	let sorted_ok = expected_sorted.iter().enumerate().all(|(index, expected)| {
		let mut bytes = [0_u8; 4];
		bpa::array_get_raw(array_id, index, &mut bytes) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(bytes) == *expected
	});
	runner.assert(
		&mut result,
		sorted_ok,
		"sort comparator orders every fixed-width item",
	);
	runner.assert(
		&mut result,
		bpa::array_get_current_raw(array_id, &mut item) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(item) == 1
			&& bpa::array_extend(array_id, 3) == bpa::SUCCESS
			&& bpa::array_capacity(array_id) == Some(11)
			&& bpa::array_shrink(array_id, 6) == bpa::SUCCESS
			&& bpa::array_capacity(array_id) == Some(6),
		"read the cursor item and extend or shrink reserved capacity",
	);
	let slice = bpa::array_slice_borrow(array_id, 1, 2);
	let expected_slice = [2_i32.to_ne_bytes(), 3_i32.to_ne_bytes()].concat();
	runner.assert(
		&mut result,
		slice.as_deref() == Some(expected_slice.as_slice()),
		"copy a contiguous fixed-width slice into safe owned bytes",
	);
	let mut copied_slice = [0_u8; 8];
	runner.assert(
		&mut result,
		bpa::array_slice_to(array_id, 1, 2, &mut copied_slice) == bpa::SUCCESS
			&& bpa::array_slice_to(array_id, 5, 0, &mut []) == bpa::SUCCESS
			&& bpa::array_slice_to(array_id, 0, 3, &mut [0; 4]) == bpa::FAILURE
			&& bpa::array_slice_borrow(array_id, usize::MAX, 0).is_none(),
		"copy valid slices and reject invalid ranges or capacity",
	);
	let copy_id = 60_011;
	runner.assert(
		&mut result,
		bpa::array_slice(array_id, 1, 2, copy_id) == bpa::SUCCESS
			&& bpa::array_count(copy_id) == Some(2),
		"create an independent array slice",
	);
	runner.assert(
		&mut result,
		bpa::array_seal(array_id) == bpa::SUCCESS
			&& bpa::array_append_raw(array_id, &9_i32.to_ne_bytes()) == bpa::SUCCESS
			&& bpa::array_extend(array_id, 1) == bpa::FAILURE
			&& bpa::array_unseal(array_id) == bpa::SUCCESS,
		"allow in-capacity mutations but forbid sealed capacity changes",
	);
	runner.assert(
		&mut result,
		bpa::array_swap(array_id, 0, 1) == bpa::SUCCESS
			&& bpa::array_copy(array_id, 0, 1) == bpa::SUCCESS
			&& bpa::array_rotate(array_id, 0, 2, true) == bpa::SUCCESS
			&& bpa::array_shift_left(array_id, 0, 3) == bpa::SUCCESS
			&& bpa::array_shift_right(array_id, 0, 3) == bpa::SUCCESS
			&& bpa::array_reverse(array_id) == bpa::SUCCESS
			&& bpa::array_shuffle(array_id) == bpa::SUCCESS,
		"swap, copy, rotate, shift, reverse, and shuffle array items",
	);
	let advanced_ok = if let Some(advanced) = bpa::advanced_borrow(array_id) {
		let append = advanced.append_raw(&10_i32.to_ne_bytes());
		let mut observed = [0_u8; 4];
		append == bpa::SUCCESS
			&& advanced.get_raw(advanced.counts().0 - 1, &mut observed) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(observed) == 10
	} else {
		false
	};
	runner.assert(
		&mut result,
		advanced_ok,
		"use an advanced handle for safe direct array operations",
	);
	runner.assert(
		&mut result,
		bpa::array_clear(array_id) == bpa::SUCCESS
			&& bpa::array_count(array_id) == Some(0)
			&& bpa::array_delete(array_id) == bpa::SUCCESS
			&& bpa::array_delete(copy_id) == bpa::SUCCESS,
		"clear while retaining reserve, then delete arrays",
	);
	runner.assert(
		&mut result,
		bpa::array_create(60_012, bpa::TYPE_INT, 1, 0) == bpa::SUCCESS
			&& bpa::array_reference(60_012) == bpa::SUCCESS
			&& bpa::array_unref(60_012) == bpa::SUCCESS
			&& bpa::array_is_existing(60_012)
			&& bpa::array_unref(60_012) == bpa::SUCCESS
			&& !bpa::array_is_existing(60_012),
		"delete arrays when their final reference is released",
	);

	let advanced_id = 60_013;
	let advanced_ok = bpa::array_create(advanced_id, bpa::TYPE_INT, 2, 0) == bpa::SUCCESS
		&& bpa::array_append_raw(advanced_id, &2_i32.to_ne_bytes()) == bpa::SUCCESS;
	let advanced_ok = advanced_ok && if let Some(advanced) = bpa::advanced_borrow(advanced_id) {
		let write = advanced.with_data_mut(|bytes| bytes[..4].copy_from_slice(&3_i32.to_ne_bytes()));
		let mut value = [0_u8; 4];
		write.is_some()
			&& advanced.get_raw(0, &mut value) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(value) == 3
			&& advanced.set_raw(0, &4_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& advanced.extend(2) == bpa::SUCCESS
			&& advanced.append_raw(&6_i32.to_ne_bytes()) == bpa::SUCCESS
			&& advanced.insert_raw(1, &5_i32.to_ne_bytes()) == bpa::SUCCESS
			&& advanced.remove(0) == bpa::SUCCESS
			&& advanced.counts().0 == 2
			&& advanced.shrink(2) == bpa::SUCCESS
			&& advanced.clear() == bpa::SUCCESS
			&& advanced.counts().0 == 0
			&& bpa::array_unref(advanced_id) == bpa::SUCCESS
			&& bpa::array_is_existing(advanced_id)
	} else {
		false
	};
	if advanced_ok {
		drop(bpa::advanced_borrow(advanced_id));
	}
	runner.assert(
		&mut result,
		advanced_ok && !bpa::array_is_existing(advanced_id),
		"mutate arrays through a retained advanced handle and release its final reference",
	);

	let string_array_id = 60_020;
	runner.assert(
		&mut result,
		bpa::array_create(string_array_id, bpa::TYPE_STRING, 0, 1) == bpa::SUCCESS
			&& bpa::array_append_raw(string_array_id, b"hello\0") == bpa::SUCCESS
			&& bpa::array_append_raw(string_array_id, b"\0") == bpa::SUCCESS,
		"create and append to a string array",
	);
	let mut string_output = [0_u8; 16];
	runner.assert(
		&mut result,
		bpa::array_get_string_raw(string_array_id, 0, &mut string_output) == 5
			&& &string_output[..6] == b"hello\0"
			&& bpa::array_set_string_raw(string_array_id, 1, b"updated\0") == 7,
		"get and set bounded string-array elements",
	);
	runner.assert(
		&mut result,
		bpa::array_remove(string_array_id, 0) == bpa::SUCCESS
			&& bpa::array_get_string_raw(string_array_id, 0, &mut string_output) == 7
			&& &string_output[..8] == b"updated\0"
			&& bpa::array_delete(string_array_id) == bpa::SUCCESS,
		"remove and delete string-array elements",
	);
	runner.assert(
		&mut result,
		bpa::array_create(60_023, bpa::TYPE_STRING, 0, 1) == bpa::SUCCESS
			&& bpa::array_append_raw(60_023, b"original\0") == bpa::SUCCESS
			&& bpa::array_slice(60_023, 0, 1, 60_024) == bpa::SUCCESS
			&& bpa::array_set_string_raw(60_023, 0, b"changed\0") == 7
			&& bpa::array_string_bytes(60_024, 0).as_deref() == Some(b"original".as_slice())
			&& bpa::array_slice(60_023, 1, 0, 60_025) == bpa::SUCCESS
			&& bpa::array_count(60_025) == Some(0)
			&& bpa::array_slice(60_023, 2, 0, 60_026) == bpa::FAILURE
			&& bpa::array_delete(60_023) == bpa::SUCCESS
			&& bpa::array_delete(60_024) == bpa::SUCCESS
			&& bpa::array_delete(60_025) == bpa::SUCCESS,
		"deep-copy string slices and validate empty or out-of-range slices",
	);
	runner.assert(
		&mut result,
		bpa::create(60_021, bpa::TYPE_STRING) == bpa::SUCCESS
			&& bpa::set_string(60_021, b"copy me\0") == 7
			&& bpa::array_create(60_022, bpa::TYPE_STRING, 0, 1) == bpa::SUCCESS
			&& bpa::array_append_string_from_block(60_022, 60_021) == bpa::SUCCESS
			&& bpa::array_set_string_from_block(60_022, 0, 60_021) == bpa::SET_SUCCESS
			&& bpa::array_get_string_to_block(60_022, 0, 60_021) == bpa::GET_SUCCESS
			&& bpa::string_bytes(60_021).as_deref() == Some(b"copy me".as_slice())
			&& bpa::array_delete(60_022) == bpa::SUCCESS
			&& bpa::delete(60_021) == bpa::SUCCESS,
		"transfer strings between array items and BPA string blocks",
	);

	runner.assert(
		&mut result,
		bpa::array_create(60_030, bpa::TYPE_INT, 0, 0) == bpa::SUCCESS
			&& [4_i32, 1, 3, 2].iter().all(|value| bpa::array_append_raw(60_030, &value.to_ne_bytes()) == bpa::SUCCESS)
			&& bpa::array_shuffle(60_030) == bpa::SUCCESS,
		"create and shuffle an independent draw deck",
	);
	let mut drawn = [0_i32; 4];
	let draw_ok = (0..4).all(|draw_index| {
		let mut bytes = [0_u8; 4];
		bpa::array_draw_raw(60_030, &mut bytes) == bpa::GET_SUCCESS
			&& { drawn[draw_index] = i32::from_ne_bytes(bytes); true }
	});
	runner.assert(
		&mut result,
		draw_ok
			&& drawn.iter().copied().collect::<std::collections::BTreeSet<_>>() == [1, 2, 3, 4].into()
			&& bpa::array_draw_at_raw(60_030, Some(0), &mut [0; 4]) == bpa::FAILURE
			&& bpa::array_draw_raw(60_030, &mut [0; 4]) == bpa::GET_SUCCESS
			&& bpa::array_delete(60_030) == bpa::SUCCESS,
		"draw each shuffled item once and reshuffle when the deck is exhausted",
	);

	runner.assert(
		&mut result,
		bpa::array_create(60_031, bpa::TYPE_INT, 0, 0) == bpa::SUCCESS
			&& bpa::array_append_raw(60_031, &17_i32.to_ne_bytes()) == bpa::SUCCESS
			&& bpa::array_get_to_block(60_031, 0, 60_032) == bpa::FAILURE
			&& bpa::create(60_032, bpa::TYPE_INT) == bpa::SUCCESS
			&& bpa::array_get_to_block(60_031, 0, 60_032) == bpa::GET_SUCCESS
			&& bpa::block_bytes(60_032) == Some(17_i32.to_ne_bytes().to_vec())
			&& bpa::array_set_from_block(60_031, 0, 60_032) == bpa::SET_SUCCESS
			&& bpa::array_delete(60_031) == bpa::SUCCESS
			&& bpa::delete(60_032) == bpa::SUCCESS
			&& bpa::pool_cleanup() == bpa::SUCCESS,
		"release pool accounting after all blocks are deleted",
	);
	runner.summary(&result);
}