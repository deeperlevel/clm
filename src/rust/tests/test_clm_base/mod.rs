//! Tests for the scalar aliases and geometry layouts in `clm::base`.

use super::{TestResult, TestRunner};
use super::clm::base;

macro_rules! assert_layout_size {
	($runner:expr, $result:expr, $type:ty, $expected:expr) => {{
		let expected = $expected;
		let description = format!(
			"size_of::<{}>() == {expected}",
			stringify!($type)
		);
		$runner.assert(
			$result,
			std::mem::size_of::<$type>() == expected,
			&description,
		);
	}};
}

macro_rules! assert_geometry_sizes {
	($runner:expr, $result:expr, $bytes:expr, $position:ty, $position_3d:ty,
	 $size:ty, $size_3d:ty, $block:ty, $block_3d:ty) => {{
		assert_layout_size!($runner, $result, $position, $bytes * 2);
		assert_layout_size!($runner, $result, $position_3d, $bytes * 3);
		assert_layout_size!($runner, $result, $size, $bytes * 2);
		assert_layout_size!($runner, $result, $size_3d, $bytes * 3);
		assert_layout_size!($runner, $result, $block, $bytes * 4);
		assert_layout_size!($runner, $result, $block_3d, $bytes * 6);
	}};
}

/// Runs the base type and C-layout checks and adds the results to the report.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - Base");

	let mut position = base::Position { x: 10, y: 20 };
	runner.assert(
		&mut result,
		position.x == 10 && position.y == 20,
		"position.x == 10 && position.y == 20",
	);
	position.x = 30;
	runner.assert(&mut result, position.x == 30, "position.x == 30");

	let position_3d = base::Position3d { x: 10, y: 20, z: 30 };
	runner.assert(
		&mut result,
		position_3d.x == 10 && position_3d.y == 20 && position_3d.z == 30,
		"position_3d.x == 10 && position_3d.y == 20 && position_3d.z == 30",
	);

	let mut size = base::Size { width: 100, height: 200 };
	runner.assert(
		&mut result,
		size.width == 100 && size.height == 200,
		"size.width == 100 && size.height == 200",
	);
	size.width = 300;
	runner.assert(&mut result, size.width == 300, "size.width == 300");

	let size_3d = base::Size3d {
		width: 100,
		height: 200,
		depth: 300,
	};
	runner.assert(
		&mut result,
		size_3d.width == 100 && size_3d.height == 200 && size_3d.depth == 300,
		"size_3d.width == 100 && size_3d.height == 200 && size_3d.depth == 300",
	);

	let block = base::Block {
		x: 10,
		y: 20,
		width: 100,
		height: 200,
	};
	runner.assert(
		&mut result,
		block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200,
		"block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200",
	);

	let block_3d = base::Block3d {
		x: 10,
		y: 20,
		z: 30,
		width: 100,
		height: 200,
		depth: 300,
	};
	runner.assert(
		&mut result,
		block_3d.x == 10 && block_3d.y == 20 && block_3d.z == 30,
		"block_3d.x == 10 && block_3d.y == 20 && block_3d.z == 30",
	);
	runner.assert(
		&mut result,
		block_3d.width == 100 && block_3d.height == 200 && block_3d.depth == 300,
		"block_3d.width == 100 && block_3d.height == 200 && block_3d.depth == 300",
	);

	let float_position = base::PositionF32 { x: 10.5, y: 20.25 };
	runner.assert(
		&mut result,
		float_position.x == 10.5 && float_position.y == 20.25,
		"float_position.x == 10.5 && float_position.y == 20.25",
	);
	let float_position_3d = base::Position3dF32 {
		x: 10.5,
		y: 20.25,
		z: 30.75,
	};
	runner.assert(
		&mut result,
		float_position_3d.x == 10.5
			&& float_position_3d.y == 20.25
			&& float_position_3d.z == 30.75,
		"float_position_3d coordinates retain their values",
	);
	let float_size = base::SizeF32 {
		width: 100.5,
		height: 200.25,
	};
	runner.assert(
		&mut result,
		float_size.width == 100.5 && float_size.height == 200.25,
		"float_size dimensions retain their values",
	);
	let float_size_3d = base::Size3dF32 {
		width: 100.5,
		height: 200.25,
		depth: 300.75,
	};
	runner.assert(
		&mut result,
		float_size_3d.width == 100.5
			&& float_size_3d.height == 200.25
			&& float_size_3d.depth == 300.75,
		"float_size_3d dimensions retain their values",
	);
	let float_block = base::BlockF32 {
		x: 10.5,
		y: 20.25,
		width: 100.5,
		height: 200.25,
	};
	runner.assert(
		&mut result,
		float_block.x == 10.5
			&& float_block.y == 20.25
			&& float_block.width == 100.5
			&& float_block.height == 200.25,
		"float_block fields retain their values",
	);
	let float_block_3d = base::Block3dF32 {
		x: 10.5,
		y: 20.25,
		z: 30.75,
		width: 100.5,
		height: 200.25,
		depth: 300.75,
	};
	runner.assert(
		&mut result,
		float_block_3d.x == 10.5
			&& float_block_3d.y == 20.25
			&& float_block_3d.z == 30.75
			&& float_block_3d.width == 100.5
			&& float_block_3d.height == 200.25
			&& float_block_3d.depth == 300.75,
		"float_block_3d fields retain their values",
	);

	assert_geometry_sizes!(runner, &mut result, 1, base::PositionI8, base::Position3dI8, base::SizeI8, base::Size3dI8, base::BlockI8, base::Block3dI8);
	assert_geometry_sizes!(runner, &mut result, 1, base::PositionU8, base::Position3dU8, base::SizeU8, base::Size3dU8, base::BlockU8, base::Block3dU8);
	assert_geometry_sizes!(runner, &mut result, 2, base::PositionI16, base::Position3dI16, base::SizeI16, base::Size3dI16, base::BlockI16, base::Block3dI16);
	assert_geometry_sizes!(runner, &mut result, 2, base::PositionU16, base::Position3dU16, base::SizeU16, base::Size3dU16, base::BlockU16, base::Block3dU16);
	assert_geometry_sizes!(runner, &mut result, 4, base::PositionI32, base::Position3dI32, base::SizeI32, base::Size3dI32, base::BlockI32, base::Block3dI32);
	assert_geometry_sizes!(runner, &mut result, 4, base::PositionU32, base::Position3dU32, base::SizeU32, base::Size3dU32, base::BlockU32, base::Block3dU32);
	assert_geometry_sizes!(runner, &mut result, 8, base::PositionI64, base::Position3dI64, base::SizeI64, base::Size3dI64, base::BlockI64, base::Block3dI64);
	assert_geometry_sizes!(runner, &mut result, 8, base::PositionU64, base::Position3dU64, base::SizeU64, base::Size3dU64, base::BlockU64, base::Block3dU64);
	assert_geometry_sizes!(runner, &mut result, 4, base::PositionF32, base::Position3dF32, base::SizeF32, base::Size3dF32, base::BlockF32, base::Block3dF32);
	assert_geometry_sizes!(runner, &mut result, 8, base::PositionF64, base::Position3dF64, base::SizeF64, base::Size3dF64, base::BlockF64, base::Block3dF64);

	runner.summary(&result);
}