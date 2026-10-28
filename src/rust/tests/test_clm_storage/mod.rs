//! Tests for row-major storage grids and atlas frame access.

use super::clm::{bpa, storage};
use super::{TestResult, TestRunner};

/// Runs fixed-width grid, slice, fill, and atlas checks.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - Storage");

	runner.assert(
		&mut result,
		storage::new_2d(100, bpa::TYPE_INT, 0, 4, 3) == bpa::SUCCESS
			&& storage::new_2d(100, bpa::TYPE_INT, 0, 4, 3) == bpa::FAILURE,
		"create a grid and reject a duplicate ID",
	);
	let mut value = [0_u8; 4];
	runner.assert(
		&mut result,
		storage::get_to(100, 0, 2, 3, &mut value) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(value) == 0
			&& storage::set_from(100, 0, 1, 2, &72_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& storage::get_to(100, 0, 1, 2, &mut value) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(value) == 72,
		"read and write zero-based row-major coordinates",
	);
	runner.assert(
		&mut result,
		storage::get_to(100, 1, 1, 2, &mut value) == bpa::FAILURE
			&& storage::get_to(100, 0, 3, 2, &mut value) == bpa::FAILURE
			&& storage::set_from(100, 0, 1, 2, &[1, 2]) == bpa::FAILURE,
		"reject out-of-range coordinates and incorrect element widths",
	);
	runner.assert(
		&mut result,
		storage::get_info(100).is_some_and(|info| {
			info.columns == 4 && info.rows == 3 && info.layers == 1 && !info.atlas
		}),
		"report grid dimensions and element metadata",
	);
	runner.assert(
		&mut result,
		storage::slice(100, 0, 1, 1, 1, 2, 2, 101) == bpa::SUCCESS
			&& storage::get_to(101, 0, 0, 1, &mut value) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(value) == 72
			&& storage::set_from(101, 0, 0, 1, &21_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& storage::get_to(100, 0, 1, 2, &mut value) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(value) == 72,
		"copy a rectangular slice into independent storage",
	);
	runner.assert(
		&mut result,
		storage::slice(100, 0, 2, 2, 1, 2, 2, 102) == bpa::FAILURE
			&& storage::delete(101) == bpa::SUCCESS
			&& storage::delete(100) == bpa::SUCCESS
			&& storage::get_info(100).is_none(),
		"reject slices beyond the source and delete storage",
	);

	runner.assert(
		&mut result,
		storage::new(110, bpa::TYPE_BYTE, 0, 2, 3, 2) == bpa::SUCCESS
			&& storage::set_from(110, 1, 2, 1, &[9]) == bpa::SET_SUCCESS,
		"create a layered 3D grid",
	);
	let mut byte = [0_u8; 1];
	runner.assert(
		&mut result,
		storage::get_to(110, 0, 2, 1, &mut byte) == bpa::GET_SUCCESS
			&& byte[0] == 0
			&& storage::slice(110, 1, 1, 0, 1, 2, 2, 111) == bpa::SUCCESS
			&& storage::get_to(111, 0, 1, 1, &mut byte) == bpa::GET_SUCCESS
			&& byte[0] == 9,
		"slice the requested layer and preserve its coordinates",
	);
	storage::delete(111);
	storage::delete(110);

	let frame = [1_u8, 2, 3, 4];
	let mut frame_output = [0_u8; 4];
	runner.assert(
		&mut result,
		storage::atlas_new(120, bpa::TYPE_BYTE, 0, 4, 4, 2, 2) == bpa::SUCCESS
			&& storage::atlas_set_frame_from(120, 0, 1, 1, &frame) == bpa::SET_SUCCESS
			&& storage::atlas_get_frame_to(120, 0, 1, 1, &mut frame_output) == bpa::GET_SUCCESS
			&& frame_output == frame,
		"write and read a tightly packed atlas frame",
	);
	runner.assert(
		&mut result,
		storage::atlas_get_frame_to(120, 0, 0, 0, &mut frame_output) == bpa::GET_SUCCESS
			&& frame_output == [0; 4]
			&& storage::atlas_get_frame_to(120, 0, 2, 0, &mut frame_output) == bpa::FAILURE
			&& storage::atlas_get_frame_to(120, 0, 1, 1, &mut frame_output[..3]) == bpa::FAILURE,
		"validate atlas coordinates and destination capacity",
	);
	runner.assert(
		&mut result,
		storage::atlas_slice_frame(120, 0, 1, 1, 121) == bpa::SUCCESS
			&& storage::get_to(121, 0, 1, 1, &mut byte) == bpa::GET_SUCCESS
			&& byte[0] == 4
			&& storage::delete(121) == bpa::SUCCESS
			&& storage::delete(120) == bpa::SUCCESS,
		"copy an atlas frame into regular grid storage",
	);
	runner.assert(
		&mut result,
		storage::atlas_new(122, bpa::TYPE_BYTE, 0, 5, 4, 2, 2) == bpa::FAILURE
			&& storage::new_2d(123, bpa::TYPE_STRING, 0, 2, 2) == bpa::FAILURE
			&& storage::new_2d(123, bpa::TYPE_INT, 0, 0, 2) == bpa::FAILURE
			&& storage::new(124, bpa::TYPE_INT, 0, u32::MAX as usize, u32::MAX as usize, 2)
				== bpa::FAILURE,
		"reject invalid atlas divisibility, unsupported types, and overflow",
	);
	runner.assert(
		&mut result,
		storage::atlas_new_3d(130, bpa::TYPE_BYTE, 0, 4, 4, 2, 2, 2) == bpa::SUCCESS
			&& storage::atlas_set_frame_from(130, 1, 0, 1, &frame) == bpa::SET_SUCCESS
			&& storage::atlas_get_frame_to(130, 1, 0, 1, &mut frame_output) == bpa::GET_SUCCESS
			&& frame_output == frame
			&& storage::delete(130) == bpa::SUCCESS,
		"address atlas frames on separate layers",
	);

	runner.assert(
		&mut result,
		storage::new_2d(140, bpa::TYPE_INT, 0, 2, 2) == bpa::SUCCESS
			&& storage::fill_from(140, &5_i32.to_ne_bytes()) == bpa::SUCCESS
			&& storage::get_to(140, 0, 1, 1, &mut value) == bpa::GET_SUCCESS
			&& i32::from_ne_bytes(value) == 5
			&& bpa::create(141, bpa::TYPE_INT) == bpa::SUCCESS
			&& bpa::set(141, &83_i32.to_ne_bytes()) == bpa::SET_SUCCESS
			&& storage::set(140, 0, 0, 0, 141) == bpa::SET_SUCCESS
			&& storage::get(140, 0, 0, 0, 141) == bpa::GET_SUCCESS
			&& bpa::block_bytes(141).is_some_and(|bytes| i32::from_ne_bytes(bytes.try_into().unwrap()) == 83)
			&& bpa::delete(141) == bpa::SUCCESS
			&& storage::delete(140) == bpa::SUCCESS,
		"fill storage and transfer elements through BPA blocks",
	);
	runner.assert(
		&mut result,
		bpa::shutdown() == bpa::SUCCESS,
		"shut down the shared BPA registry after storage use",
	);

	runner.summary(&result);
}