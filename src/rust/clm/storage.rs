//! Fixed-width row-major grids and atlas storage corresponding to C `storage.h`.
//!
//! Coordinates are zero-based in layer, row, column order. Columns vary
//! fastest, followed by rows and layers. Stored values are fixed-width bytes;
//! string values are unsupported.

use crate::clm::bpa::{self, Id};
use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

/// Dimensions and element metadata for an existing storage resource.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct StorageInfo {
	/// Number of elements along the column (x) axis.
	pub columns: u32,
	/// Number of elements along the row (y) axis.
	pub rows: u32,
	/// Number of layers.
	pub layers: u32,
	/// Atlas frame width; zero for regular grids.
	pub frame_columns: u32,
	/// Atlas frame height; zero for regular grids.
	pub frame_rows: u32,
	/// Byte width of every stored value.
	pub element_size: usize,
	/// BPA value type used for each element.
	pub type_code: u8,
	/// Whether this resource is configured as an atlas.
	pub atlas: bool,
	/// Internal data ID retained for parity with the C metadata structure.
	pub data_id: Id,
}

struct Storage {
	info: StorageInfo,
	data: Vec<u8>,
}

static STORAGE: OnceLock<Mutex<HashMap<Id, Storage>>> = OnceLock::new();
static NEXT_DATA_ID: OnceLock<Mutex<Id>> = OnceLock::new();

fn storages() -> &'static Mutex<HashMap<Id, Storage>> {
	STORAGE.get_or_init(|| Mutex::new(HashMap::new()))
}

fn lock_storages() -> std::sync::MutexGuard<'static, HashMap<Id, Storage>> {
	storages()
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn allocate_data_id() -> Id {
	let counter = NEXT_DATA_ID.get_or_init(|| Mutex::new(0xF000_0000_0000_0000));
	let mut next = counter
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner);
	let id = *next;
	*next = next.wrapping_add(1);
	id
}

fn create_internal(
	id: Id,
	type_code: u8,
	struct_size: usize,
	columns: usize,
	rows: usize,
	layers: usize,
	frame_columns: usize,
	frame_rows: usize,
	atlas: bool,
) -> i32 {
	if columns == 0
		|| rows == 0
		|| layers == 0
		|| columns > u32::MAX as usize
		|| rows > u32::MAX as usize
		|| layers > u32::MAX as usize
		|| (atlas
			&& (frame_columns == 0
				|| frame_rows == 0
				|| columns % frame_columns != 0
				|| rows % frame_rows != 0))
		|| (!atlas && (frame_columns != 0 || frame_rows != 0))
		|| type_code == bpa::TYPE_STRING
	{
		return bpa::FAILURE;
	}
	let element_size = if type_code == bpa::TYPE_STRUCT {
		struct_size
	} else {
		let built_in_size = bpa::type_size(type_code);
		if struct_size != 0 && struct_size != built_in_size {
			return bpa::FAILURE;
		}
		built_in_size
	};
	if element_size == 0 || element_size > u32::MAX as usize {
		return bpa::FAILURE;
	}
	let Some(count) = columns.checked_mul(rows).and_then(|value| value.checked_mul(layers)) else {
		return bpa::FAILURE;
	};
	if count == 0 || count > i32::MAX as usize {
		return bpa::FAILURE;
	}
	let Some(byte_count) = count.checked_mul(element_size) else {
		return bpa::FAILURE;
	};
	let mut data = Vec::new();
	if data.try_reserve_exact(byte_count).is_err() {
		return bpa::FAILURE;
	}
	data.resize(byte_count, 0);
	if bpa::is_existing(id) || bpa::array_is_existing(id) {
		return bpa::FAILURE;
	}
	let mut resources = lock_storages();
	if resources.contains_key(&id) {
		return bpa::FAILURE;
	}
	if bpa::create_sized(id, bpa::TYPE_STRUCT, 1) != bpa::SUCCESS {
		return bpa::FAILURE;
	}
	resources.insert(
		id,
		Storage {
			info: StorageInfo {
				columns: columns as u32,
				rows: rows as u32,
				layers: layers as u32,
				frame_columns: frame_columns as u32,
				frame_rows: frame_rows as u32,
				element_size,
				type_code,
				atlas,
				data_id: allocate_data_id(),
			},
			data,
		},
	);
	bpa::SUCCESS
}

/// Creates a three-dimensional row-major storage grid initialized to zero.
pub fn new(id: Id, type_code: u8, struct_size: usize, columns: usize, rows: usize, layers: usize) -> i32 {
	create_internal(id, type_code, struct_size, columns, rows, layers, 0, 0, false)
}

/// Creates a two-dimensional row-major grid with one layer.
pub fn new_2d(id: Id, type_code: u8, struct_size: usize, columns: usize, rows: usize) -> i32 {
	new(id, type_code, struct_size, columns, rows, 1)
}

/// Creates a 2D atlas whose grid dimensions are divisible by the frame dimensions.
pub fn atlas_new(
	id: Id,
	type_code: u8,
	struct_size: usize,
	columns: usize,
	rows: usize,
	frame_columns: usize,
	frame_rows: usize,
) -> i32 {
	create_internal(id, type_code, struct_size, columns, rows, 1, frame_columns, frame_rows, true)
}

/// Creates a layered atlas; every layer has the same frame grid.
pub fn atlas_new_3d(
	id: Id,
	type_code: u8,
	struct_size: usize,
	columns: usize,
	rows: usize,
	layers: usize,
	frame_columns: usize,
	frame_rows: usize,
) -> i32 {
	create_internal(
		id,
		type_code,
		struct_size,
		columns,
		rows,
		layers,
		frame_columns,
		frame_rows,
		true,
	)
}

/// Returns dimensions and element metadata for a storage resource.
pub fn get_info(id: Id) -> Option<StorageInfo> {
	lock_storages().get(&id).map(|storage| storage.info)
}

/// Deletes a storage resource.
pub fn delete(id: Id) -> i32 {
	if lock_storages().remove(&id).is_none() {
		return bpa::FAILURE;
	}
	bpa::delete(id)
}

fn byte_offset(info: StorageInfo, layer: usize, row: usize, column: usize) -> Option<usize> {
	if layer >= info.layers as usize || row >= info.rows as usize || column >= info.columns as usize {
		return None;
	}
	((layer * info.rows as usize + row) * info.columns as usize + column)
		.checked_mul(info.element_size)
}

/// Copies one element into caller-owned memory using zero-based coordinates.
pub fn get_to(id: Id, layer: usize, row: usize, column: usize, destination: &mut [u8]) -> i32 {
	let mut resources = lock_storages();
	let Some(storage) = resources.get_mut(&id) else {
		return bpa::FAILURE;
	};
	let Some(offset) = byte_offset(storage.info, layer, row, column) else {
		return bpa::FAILURE;
	};
	let end = offset + storage.info.element_size;
	if destination.len() < storage.info.element_size {
		return bpa::FAILURE;
	}
	destination[..storage.info.element_size].copy_from_slice(&storage.data[offset..end]);
	bpa::GET_SUCCESS
}

/// Replaces one element from caller-owned bytes; the source width must match exactly.
pub fn set_from(id: Id, layer: usize, row: usize, column: usize, source: &[u8]) -> i32 {
	let mut resources = lock_storages();
	let Some(storage) = resources.get_mut(&id) else {
		return bpa::FAILURE;
	};
	let Some(offset) = byte_offset(storage.info, layer, row, column) else {
		return bpa::FAILURE;
	};
	let size = storage.info.element_size;
	if source.len() != size {
		return bpa::FAILURE;
	}
	storage.data[offset..offset + size].copy_from_slice(source);
	bpa::SET_SUCCESS
}

/// Copies one element to a compatible BPA block.
pub fn get(id: Id, layer: usize, row: usize, column: usize, destination_block_id: Id) -> i32 {
	let Some(info) = get_info(id) else {
		return bpa::FAILURE;
	};
	let Some(block_info) = bpa::block_info(destination_block_id) else {
		return bpa::FAILURE;
	};
	if block_info.type_code != info.type_code || block_info.block_size != info.element_size {
		return bpa::FAILURE;
	}
	let mut bytes = vec![0; info.element_size];
	if get_to(id, layer, row, column, &mut bytes) != bpa::GET_SUCCESS {
		return bpa::FAILURE;
	}
	if bpa::set(destination_block_id, &bytes) == bpa::SET_SUCCESS {
		bpa::GET_SUCCESS
	} else {
		bpa::FAILURE
	}
}

/// Copies a compatible BPA block into one element.
pub fn set(id: Id, layer: usize, row: usize, column: usize, source_block_id: Id) -> i32 {
	let Some(info) = get_info(id) else {
		return bpa::FAILURE;
	};
	let Some(block_info) = bpa::block_info(source_block_id) else {
		return bpa::FAILURE;
	};
	if block_info.type_code != info.type_code || block_info.block_size != info.element_size {
		return bpa::FAILURE;
	}
	let Some(bytes) = bpa::block_bytes(source_block_id) else {
		return bpa::FAILURE;
	};
	set_from(id, layer, row, column, &bytes)
}

/// Replaces every element with a copy of the same fixed-width value.
pub fn fill_from(id: Id, value: &[u8]) -> i32 {
	let mut resources = lock_storages();
	let Some(storage) = resources.get_mut(&id) else {
		return bpa::FAILURE;
	};
	let size = storage.info.element_size;
	if value.len() != size {
		return bpa::FAILURE;
	}
	for item in storage.data.chunks_exact_mut(size) {
		item.copy_from_slice(value);
	}
	bpa::SUCCESS
}

/// Copies a contiguous rectangular or volumetric region into a new regular grid.
pub fn slice(
	source_id: Id,
	layer: usize,
	row: usize,
	column: usize,
	layers: usize,
	rows: usize,
	columns: usize,
	destination_id: Id,
) -> i32 {
	let Some(source_info) = get_info(source_id) else {
		return bpa::FAILURE;
	};
	if layers == 0
		|| rows == 0
		|| columns == 0
		|| layer > source_info.layers as usize
		|| row > source_info.rows as usize
		|| column > source_info.columns as usize
		|| layers > source_info.layers as usize - layer
		|| rows > source_info.rows as usize - row
		|| columns > source_info.columns as usize - column
	{
		return bpa::FAILURE;
	}
	if new(
		destination_id,
		source_info.type_code,
		source_info.element_size,
		columns,
		rows,
		layers,
	) != bpa::SUCCESS
	{
		return bpa::FAILURE;
	}
	let mut resources = lock_storages();
	if !resources.contains_key(&source_id) {
		resources.remove(&destination_id);
		return bpa::FAILURE;
	}
	let size = source_info.element_size;
	let Some(slice_bytes) = layers
		.checked_mul(rows)
		.and_then(|count| count.checked_mul(columns))
		.and_then(|count| count.checked_mul(size))
	else {
		resources.remove(&destination_id);
		return bpa::FAILURE;
	};
	let mut copied = vec![0; slice_bytes];
	{
		let source = resources.get(&source_id).expect("source existence checked");
	for z in 0..layers {
		for y in 0..rows {
			let from = ((layer + z) * source_info.rows as usize + row + y)
				* source_info.columns as usize
				+ column;
			let to = (z * rows + y) * columns;
			let length = columns * size;
			copied[to * size..to * size + length]
				.copy_from_slice(&source.data[from * size..from * size + length]);
		}
	}
	}
	let Some(destination) = resources.get_mut(&destination_id) else {
		return bpa::FAILURE;
	};
	destination.data = copied;
	bpa::SUCCESS
}

fn atlas_range(info: StorageInfo, layer: usize, frame_row: usize, frame_column: usize) -> Option<(usize, usize)> {
	if !info.atlas
		|| layer >= info.layers as usize
		|| frame_row >= info.rows as usize / info.frame_rows as usize
		|| frame_column >= info.columns as usize / info.frame_columns as usize
	{
		return None;
	}
	let start = (layer * info.rows as usize + frame_row * info.frame_rows as usize)
		.checked_mul(info.columns as usize)?
		.checked_add(frame_column * info.frame_columns as usize)?;
	Some((start, info.frame_columns as usize * info.frame_rows as usize))
}

/// Copies one atlas frame to a tightly packed row-major destination.
pub fn atlas_get_frame_to(
	id: Id,
	layer: usize,
	frame_row: usize,
	frame_column: usize,
	destination: &mut [u8],
) -> i32 {
	let mut resources = lock_storages();
	let Some(storage) = resources.get_mut(&id) else {
		return bpa::FAILURE;
	};
	let Some((start, count)) = atlas_range(storage.info, layer, frame_row, frame_column) else {
		return bpa::FAILURE;
	};
	let info = storage.info;
	let row_bytes = info.frame_columns as usize * info.element_size;
	let required = row_bytes * info.frame_rows as usize;
	if destination.len() < required {
		return bpa::FAILURE;
	}
	let mut output_offset = 0;
	for local_row in 0..info.frame_rows as usize {
		let offset = start * info.element_size
			+ local_row * info.columns as usize * info.element_size;
		destination[output_offset..output_offset + row_bytes]
			.copy_from_slice(&storage.data[offset..offset + row_bytes]);
		output_offset += row_bytes;
	}
	let _ = count;
	bpa::GET_SUCCESS
}

/// Copies a tightly packed buffer into one atlas frame; source size must match exactly.
pub fn atlas_set_frame_from(
	id: Id,
	layer: usize,
	frame_row: usize,
	frame_column: usize,
	source: &[u8],
) -> i32 {
	let mut resources = lock_storages();
	let Some(storage) = resources.get_mut(&id) else {
		return bpa::FAILURE;
	};
	let Some((start, _)) = atlas_range(storage.info, layer, frame_row, frame_column) else {
		return bpa::FAILURE;
	};
	let info = storage.info;
	let row_bytes = info.frame_columns as usize * info.element_size;
	let required = row_bytes * info.frame_rows as usize;
	if source.len() != required {
		return bpa::FAILURE;
	}
	for local_row in 0..info.frame_rows as usize {
		let offset = start * info.element_size
			+ local_row * info.columns as usize * info.element_size;
		let source_offset = local_row * row_bytes;
		storage.data[offset..offset + row_bytes]
			.copy_from_slice(&source[source_offset..source_offset + row_bytes]);
	}
	bpa::SET_SUCCESS
}

/// Copies an atlas frame into a new regular 2D storage grid.
pub fn atlas_slice_frame(
	id: Id,
	layer: usize,
	frame_row: usize,
	frame_column: usize,
	destination_id: Id,
) -> i32 {
	let Some(info) = get_info(id) else {
		return bpa::FAILURE;
	};
	if atlas_range(info, layer, frame_row, frame_column).is_none() {
		return bpa::FAILURE;
	}
	slice(
		id,
		layer,
		frame_row * info.frame_rows as usize,
		frame_column * info.frame_columns as usize,
		1,
		info.frame_rows as usize,
		info.frame_columns as usize,
		destination_id,
	)
}