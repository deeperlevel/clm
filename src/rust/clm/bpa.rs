//! ID-indexed block and array storage corresponding to the C BPA module.
//!
//! Values are owned by Rust containers and accessed through checked methods.
//! The registry is synchronized; no raw pointers or unsafe code are required.

use std::cmp::Ordering;
use std::collections::HashMap;
use std::sync::{Arc, Mutex, MutexGuard, OnceLock};

pub type Id = u64;

pub const FAILURE: i32 = -1;
pub const SUCCESS: i32 = 0;
pub const GET_SUCCESS: i32 = 1;
pub const SET_SUCCESS: i32 = 2;

pub const TYPE_STRUCT: u8 = 0;
pub const TYPE_SIGNED_BYTE: u8 = 1;
pub const TYPE_BYTE: u8 = 2;
pub const TYPE_SHORT: u8 = 3;
pub const TYPE_UNSIGNED_SHORT: u8 = 4;
pub const TYPE_INT: u8 = 5;
pub const TYPE_UNSIGNED_INT: u8 = 6;
pub const TYPE_LONG: u8 = 7;
pub const TYPE_UNSIGNED_LONG: u8 = 8;
pub const TYPE_FLOAT: u8 = 9;
pub const TYPE_DOUBLE: u8 = 10;
pub const TYPE_STRING: u8 = 11;
pub const TYPE_UUID: u8 = 12;
pub const TYPE_DATETIME: u8 = 13;

const POOL_SLAB_SLOTS: usize = 32;
const POOL_MAX_SLOT: usize = 4096;

#[derive(Default, Clone, Copy)]
struct PoolClass {
	active: usize,
	capacity: usize,
}

#[derive(Clone)]
enum BlockData {
	Fixed(Vec<u8>),
	String(Vec<u8>),
}

#[derive(Clone)]
struct Block {
	type_code: u8,
	refcount: u16,
	block_size: usize,
	encoding: u8,
	data: BlockData,
	pool_bytes: Option<usize>,
}

/// Copyable metadata for an existing block.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct BlockInfo {
	pub type_code: u8,
	pub refcount: u16,
	pub block_size: usize,
	pub encoding: u8,
	pub length: usize,
}

/// Copyable metadata for an existing array.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ArrayInfo {
	pub type_code: u8,
	pub refcount: u16,
	pub count: usize,
	pub capacity: usize,
	pub block_size: usize,
	pub encoding: u8,
	pub cursor: usize,
	pub sealed: bool,
}

enum ArrayData {
	Fixed(Vec<u8>),
	Strings(Vec<Vec<u8>>),
}

struct Array {
	type_code: u8,
	refcount: u16,
	block_size: usize,
	encoding: u8,
	count: usize,
	capacity: usize,
	cursor: usize,
	sealed: bool,
	deck: Vec<usize>,
	deck_cursor: usize,
	items: ArrayData,
}

#[derive(Default)]
struct Store {
	blocks: HashMap<Id, Block>,
	arrays: HashMap<Id, Arc<Mutex<Array>>>,
	pools: [PoolClass; 10],
}

static STORE: OnceLock<Mutex<Store>> = OnceLock::new();
static RANDOM_STATE: OnceLock<Mutex<u64>> = OnceLock::new();

fn store() -> MutexGuard<'static, Store> {
	STORE
		.get_or_init(|| Mutex::new(Store::default()))
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn pool_class_for(bytes: usize) -> Option<usize> {
	if bytes == 0 || bytes > POOL_MAX_SLOT {
		return None;
	}
	let mut size = 8;
	let mut index = 0;
	while size < bytes && index < 9 {
		size *= 2;
		index += 1;
	}
	(size >= bytes).then_some(index)
}

fn pool_allocate(store: &mut Store, bytes: usize) -> Option<usize> {
	let class = pool_class_for(bytes)?;
	let pool = &mut store.pools[class];
	if pool.active == pool.capacity {
		pool.capacity = pool.capacity.checked_add(POOL_SLAB_SLOTS)?;
	}
	pool.active += 1;
	Some(class)
}

fn pool_release(store: &mut Store, class: Option<usize>) {
	if let Some(class) = class {
		store.pools[class].active = store.pools[class].active.saturating_sub(1);
	}
}

/// Returns the fixed byte width of a built-in BPA type, or zero when variable-sized.
pub fn type_size(type_code: u8) -> usize {
	match type_code {
		TYPE_SIGNED_BYTE | TYPE_BYTE => 1,
		TYPE_SHORT | TYPE_UNSIGNED_SHORT => 2,
		TYPE_INT | TYPE_UNSIGNED_INT | TYPE_FLOAT => 4,
		TYPE_LONG | TYPE_UNSIGNED_LONG | TYPE_DOUBLE | TYPE_DATETIME => 8,
		TYPE_UUID => 16,
		_ => 0,
	}
}

/// Checks whether a block with the ID exists.
pub fn is_existing(id: Id) -> bool {
	store().blocks.contains_key(&id)
}

/// Returns type, size, encoding, and reference metadata for a block.
pub fn block_info(id: Id) -> Option<BlockInfo> {
	let registry = store();
	let block = registry.blocks.get(&id)?;
	let length = match &block.data {
		BlockData::Fixed(bytes) | BlockData::String(bytes) => bytes.len(),
	};
	Some(BlockInfo {
		type_code: block.type_code,
		refcount: block.refcount,
		block_size: block.block_size,
		encoding: block.encoding,
		length,
	})
}

/// Returns a copy of the payload of a fixed-width block.
pub fn block_bytes(id: Id) -> Option<Vec<u8>> {
	let registry = store();
	let block = registry.blocks.get(&id)?;
	match &block.data {
		BlockData::Fixed(bytes) => Some(bytes.clone()),
		BlockData::String(_) => None,
	}
}

/// Returns a copy of a string block's bytes without a terminating NUL.
pub fn string_bytes(id: Id) -> Option<Vec<u8>> {
	let registry = store();
	let block = registry.blocks.get(&id)?;
	match &block.data {
		BlockData::String(bytes) => Some(bytes.clone()),
		BlockData::Fixed(_) => None,
	}
}

/// Updates the encoding metadata retained by a block.
pub fn set_encoding(id: Id, encoding: u8) -> i32 {
	let mut registry = store();
	let Some(block) = registry.blocks.get_mut(&id) else {
		return FAILURE;
	};
	block.encoding = encoding;
	SUCCESS
}

/// Writes a little native-endian unsigned 32-bit value to a matching BPA block.
pub fn write_u32(destination_id: Id, value: u32) -> i32 {
	if block_info(destination_id).is_none_or(|info| info.type_code != TYPE_UNSIGNED_INT || info.block_size != 4) {
		return FAILURE;
	}
	set(destination_id, &value.to_ne_bytes())
}

/// Writes a native-endian signed 32-bit value to a matching BPA block.
pub fn write_i32(destination_id: Id, value: i32) -> i32 {
	if block_info(destination_id).is_none_or(|info| info.type_code != TYPE_INT || info.block_size != 4) {
		return FAILURE;
	}
	set(destination_id, &value.to_ne_bytes())
}

/// Writes one byte to a matching BPA byte block.
pub fn write_byte(destination_id: Id, value: u8) -> i32 {
	if block_info(destination_id).is_none_or(|info| info.type_code != TYPE_BYTE || info.block_size != 1) {
		return FAILURE;
	}
	set(destination_id, &[value])
}

/// Checks whether an array with the ID exists.
pub fn array_is_existing(id: Id) -> bool {
	store().arrays.contains_key(&id)
}

/// Creates a zero-initialized block of a built-in or string type.
/// Custom `STRUCT` blocks require [`create_sized`].
pub fn create(id: Id, type_code: u8) -> i32 {
	if type_code == TYPE_STRUCT {
		return FAILURE;
	}
	create_sized(id, type_code, 0)
}

/// Creates a zero-initialized block, specifying its width for `STRUCT` values.
///
/// `block_size` must be zero for built-in types and string blocks, and nonzero
/// for `STRUCT` blocks.
pub fn create_sized(id: Id, type_code: u8, block_size: usize) -> i32 {
	let mut registry = store();
	if registry.blocks.contains_key(&id) || registry.arrays.contains_key(&id) {
		return FAILURE;
	}
	let is_string = type_code == TYPE_STRING;
	let resolved_size = if type_code == TYPE_STRUCT {
		if block_size == 0 || block_size > u32::MAX as usize {
			return FAILURE;
		}
		block_size
	} else if is_string {
		if block_size != 0 {
			return FAILURE;
		}
		0
	} else {
		let size = type_size(type_code);
		if size == 0 || (block_size != 0 && block_size != size) {
			return FAILURE;
		}
		size
	};
	let pool_bytes = if is_string {
		None
	} else {
		pool_allocate(&mut registry, resolved_size)
	};
	if !is_string && pool_class_for(resolved_size).is_some() && pool_bytes.is_none() {
		return FAILURE;
	}
	let data = if is_string {
		BlockData::String(Vec::new())
	} else {
		BlockData::Fixed(vec![0; resolved_size])
	};
	registry.blocks.insert(
		id,
		Block {
			type_code,
			refcount: 1,
			block_size: resolved_size,
			encoding: 0,
			data,
			pool_bytes,
		},
	);
	SUCCESS
}

/// Immediately deletes a block, regardless of its reference count.
pub fn delete(id: Id) -> i32 {
	let mut registry = store();
	let Some(block) = registry.blocks.remove(&id) else {
		return FAILURE;
	};
	pool_release(&mut registry, block.pool_bytes);
	SUCCESS
}

/// Adds one reference to a block unless its counter is already full.
pub fn reference(id: Id) -> i32 {
	let mut registry = store();
	let Some(block) = registry.blocks.get_mut(&id) else {
		return FAILURE;
	};
	if block.refcount == u16::MAX {
		return FAILURE;
	}
	block.refcount += 1;
	SUCCESS
}

/// Releases one reference and deletes the block when the count reaches zero.
pub fn unref(id: Id) -> i32 {
	let mut registry = store();
	let Some(block) = registry.blocks.get_mut(&id) else {
		return FAILURE;
	};
	if block.refcount == 0 {
		return FAILURE;
	}
	block.refcount -= 1;
	if block.refcount != 0 {
		return SUCCESS;
	}
	let block = registry.blocks.remove(&id).expect("block was checked above");
	pool_release(&mut registry, block.pool_bytes);
	SUCCESS
}

/// Copies a fixed-size block into a caller-owned slice.
pub fn get(id: Id, destination: &mut [u8]) -> i32 {
	let registry = store();
	let Some(block) = registry.blocks.get(&id) else {
		return FAILURE;
	};
	let BlockData::Fixed(bytes) = &block.data else {
		return FAILURE;
	};
	if destination.len() < bytes.len() {
		return FAILURE;
	}
	destination[..bytes.len()].copy_from_slice(bytes);
	GET_SUCCESS
}

/// Replaces a fixed-size block from a caller-owned slice of exactly matching width.
pub fn set(id: Id, source: &[u8]) -> i32 {
	let mut registry = store();
	let Some(block) = registry.blocks.get_mut(&id) else {
		return FAILURE;
	};
	let BlockData::Fixed(bytes) = &mut block.data else {
		return FAILURE;
	};
	if source.len() != bytes.len() {
		return FAILURE;
	}
	bytes.copy_from_slice(source);
	SET_SUCCESS
}

/// Copies a NUL-terminated string block to a destination slice.
/// Returns the byte length excluding the terminator, or [`FAILURE`].
pub fn get_string(id: Id, destination: &mut [u8]) -> i32 {
	let registry = store();
	let Some(block) = registry.blocks.get(&id) else {
		return FAILURE;
	};
	let BlockData::String(bytes) = &block.data else {
		return FAILURE;
	};
	if bytes.len() > i32::MAX as usize || destination.len() < bytes.len() + 1 {
		return FAILURE;
	}
	destination[..bytes.len()].copy_from_slice(bytes);
	destination[bytes.len()] = 0;
	bytes.len() as i32
}

fn bounded_text(source: &[u8]) -> Option<Vec<u8>> {
	if source.is_empty() {
		return None;
	}
	let end = source.iter().position(|byte| *byte == 0).unwrap_or(source.len());
	(end <= i32::MAX as usize).then(|| source[..end].to_vec())
}

/// Replaces a string block with bytes up to the first NUL in `source`.
/// Returns the stored byte length excluding NUL, or [`FAILURE`].
pub fn set_string(id: Id, source: &[u8]) -> i32 {
	let Some(text) = bounded_text(source) else {
		return FAILURE;
	};
	let length = text.len();
	let mut registry = store();
	let Some(block) = registry.blocks.get_mut(&id) else {
		return FAILURE;
	};
	if !matches!(block.data, BlockData::String(_)) {
		return FAILURE;
	}
	let old_pool = block.pool_bytes;
	let new_pool = pool_allocate(&mut registry, text.len() + 1);
	let Some(block) = registry.blocks.get_mut(&id) else {
		return FAILURE;
	};
	let BlockData::String(bytes) = &mut block.data else {
		return FAILURE;
	};
	*bytes = text;
	block.pool_bytes = new_pool;
	pool_release(&mut registry, old_pool);
	length as i32
}

/// Replaces a block's contents; string blocks use bounded NUL-terminated bytes.
pub fn replace(id: Id, source: &[u8]) -> i32 {
	let is_string = {
		let registry = store();
		registry
			.blocks
			.get(&id)
			.is_some_and(|block| matches!(block.data, BlockData::String(_)))
	};
	if is_string {
		set_string(id, source)
	} else {
		set(id, source)
	}
}

/// Returns an owned snapshot of a block payload; string snapshots include NUL.
///
/// This replaces the C API's lifetime-bound raw pointer with a copy that remains
/// valid after the block is modified or deleted.
pub fn borrow(id: Id) -> Option<Vec<u8>> {
	let registry = store();
	let block = registry.blocks.get(&id)?;
	match &block.data {
		BlockData::Fixed(bytes) => Some(bytes.clone()),
		BlockData::String(bytes) => {
			let mut copy = bytes.clone();
			copy.push(0);
			Some(copy)
		}
	}
}

/// Copies one block into a compatible pre-created block.
pub fn copy_block(source_id: Id, destination_id: Id) -> i32 {
	let Some(source_info) = block_info(source_id) else {
		return FAILURE;
	};
	let Some(destination_info) = block_info(destination_id) else {
		return FAILURE;
	};
	if source_info.type_code != destination_info.type_code
		|| (source_info.type_code != TYPE_STRING
			&& source_info.block_size != destination_info.block_size)
	{
		return FAILURE;
	}
	match source_info.type_code {
		TYPE_STRING => string_bytes(source_id)
			.map(|mut bytes| {
				bytes.push(0);
				set_string(destination_id, &bytes)
			})
			.filter(|status| *status >= 0)
			.map_or(FAILURE, |_| SUCCESS),
		_ => block_bytes(source_id)
			.map(|bytes| set(destination_id, &bytes))
			.filter(|status| *status == SET_SUCCESS)
			.map_or(FAILURE, |_| SUCCESS),
	}
}

/// Returns the active/capacity slot counts for the size class selected by `bytes`.
pub fn pool_stats(bytes: usize) -> Option<(usize, usize)> {
	let class = pool_class_for(bytes)?;
	let registry = store();
	let pool = registry.pools[class];
	Some((pool.active, pool.capacity))
}

/// Releases reusable pool accounting when no pooled block allocations remain live.
pub fn pool_cleanup() -> i32 {
	let mut registry = store();
	if registry.pools.iter().any(|pool| pool.active != 0) {
		return FAILURE;
	}
	registry.pools = [PoolClass::default(); 10];
	SUCCESS
}

fn array_handle(id: Id) -> Option<Arc<Mutex<Array>>> {
	store().arrays.get(&id).cloned()
}

fn lock_array(handle: &Arc<Mutex<Array>>) -> std::sync::MutexGuard<'_, Array> {
	handle
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn checked_item_size(type_code: u8, custom_size: usize) -> Option<usize> {
	if type_code == TYPE_STRING {
		(custom_size == 0).then_some(0)
	} else if type_code == TYPE_STRUCT {
		(custom_size > 0 && custom_size <= u32::MAX as usize).then_some(custom_size)
	} else {
		let size = type_size(type_code);
		(size > 0 && (custom_size == 0 || custom_size == size)).then_some(size)
	}
}

fn new_array(type_code: u8, capacity: usize, encoding: u8, item_size: usize) -> Array {
	let items = if type_code == TYPE_STRING {
		ArrayData::Strings(Vec::with_capacity(capacity))
	} else {
		ArrayData::Fixed(vec![0; capacity.saturating_mul(item_size)])
	};
	Array {
		type_code,
		refcount: 1,
		block_size: item_size,
		encoding,
		count: 0,
		capacity,
		cursor: 0,
		sealed: false,
		deck: Vec::new(),
		deck_cursor: 0,
		items,
	}
}

/// Creates a built-in or string array with initial reserved capacity.
pub fn array_create(id: Id, type_code: u8, reserved: usize, encoding: u8) -> i32 {
	if type_code == TYPE_STRUCT {
		return FAILURE;
	}
	array_create_sized(id, type_code, reserved, 0, encoding)
}

/// Creates an array with an explicit element width for `STRUCT` items.
pub fn array_create_sized(
	id: Id,
	type_code: u8,
	reserved: usize,
	block_size: usize,
	encoding: u8,
) -> i32 {
	let Some(item_size) = checked_item_size(type_code, block_size) else {
		return FAILURE;
	};
	if reserved > i32::MAX as usize || (item_size > 0 && reserved.checked_mul(item_size).is_none()) {
		return FAILURE;
	}
	let mut registry = store();
	if registry.blocks.contains_key(&id) || registry.arrays.contains_key(&id) {
		return FAILURE;
	}
	registry.arrays.insert(
		id,
		Arc::new(Mutex::new(new_array(type_code, reserved, encoding, item_size))),
	);
	SUCCESS
}

/// Immediately deletes an array regardless of its reference count.
pub fn array_delete(id: Id) -> i32 {
	if store().arrays.remove(&id).is_some() {
		SUCCESS
	} else {
		FAILURE
	}
}

/// Adds one reference to an array unless its counter is full.
pub fn array_reference(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.refcount == u16::MAX {
		return FAILURE;
	}
	array.refcount += 1;
	SUCCESS
}

/// Releases one array reference and deletes it when the count reaches zero.
pub fn array_unref(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.refcount == 0 {
		return FAILURE;
	}
	array.refcount -= 1;
	let remove = array.refcount == 0;
	drop(array);
	if remove {
		array_delete(id)
	} else {
		SUCCESS
	}
}

/// Checks whether an array exists.
pub fn array_exists(id: Id) -> bool {
	array_handle(id).is_some()
}

fn ensure_capacity(array: &mut Array, needed: usize) -> bool {
	if needed <= array.capacity {
		return true;
	}
	if array.sealed || needed > i32::MAX as usize {
		return false;
	}
	let mut capacity = if array.capacity == 0 { 4 } else { array.capacity };
	while capacity < needed {
		let Some(next) = capacity.checked_mul(2) else {
			capacity = needed;
			break;
		};
		capacity = next;
	}
	resize_array(array, capacity)
}

fn resize_array(array: &mut Array, capacity: usize) -> bool {
	if capacity < array.count || capacity > i32::MAX as usize {
		return false;
	}
	match &mut array.items {
		ArrayData::Fixed(bytes) => {
			let Some(byte_capacity) = capacity.checked_mul(array.block_size) else {
				return false;
			};
			bytes.resize(byte_capacity, 0);
		}
		ArrayData::Strings(values) => {
			if capacity > values.capacity() {
				values.reserve(capacity - values.capacity());
			}
		}
	}
	array.capacity = capacity;
	true
}

/// Reserves additional slots for an unsealed array.
pub fn array_extend(id: Id, additional: usize) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.sealed || additional == 0 {
		return FAILURE;
	}
	let Some(capacity) = array.capacity.checked_add(additional) else {
		return FAILURE;
	};
	if resize_array(&mut array, capacity) { SUCCESS } else { FAILURE }
}

/// Reduces reserved slots on an unsealed array without discarding live elements.
pub fn array_shrink(id: Id, capacity: usize) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.sealed || capacity >= array.capacity || capacity < array.count {
		return FAILURE;
	}
	if resize_array(&mut array, capacity) { SUCCESS } else { FAILURE }
}

/// Appends a fixed-width element or a bounded string to an array.
pub fn array_append_raw(id: Id, source: &[u8]) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	let index = array.count;
	insert_locked(&mut array, index, source)
}

/// Inserts a fixed-width element or bounded string at an initialized-item index.
pub fn array_insert_raw(id: Id, index: usize, source: &[u8]) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	insert_locked(&mut lock_array(&handle), index, source)
}

fn insert_locked(array: &mut Array, index: usize, source: &[u8]) -> i32 {
	if index > array.count || array.count >= i32::MAX as usize {
		return FAILURE;
	}
	if array.type_code == TYPE_STRING {
		let Some(value) = bounded_text(source) else {
			return FAILURE;
		};
		if !ensure_capacity(array, array.count + 1) {
			return FAILURE;
		}
		let ArrayData::Strings(values) = &mut array.items else {
			return FAILURE;
		};
		values.insert(index, value);
	} else {
		if source.len() != array.block_size || !ensure_capacity(array, array.count + 1) {
			return FAILURE;
		}
		let ArrayData::Fixed(bytes) = &mut array.items else {
			return FAILURE;
		};
		let start = index * array.block_size;
		let end = array.count * array.block_size;
		bytes.copy_within(start..end, start + array.block_size);
		bytes[start..start + array.block_size].copy_from_slice(source);
	}
	array.count += 1;
	array.cursor = index;
	reset_deck(array);
	SUCCESS
}

/// Copies an initialized fixed-width element to a destination slice.
pub fn array_get_raw(id: Id, index: usize, destination: &mut [u8]) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let array = lock_array(&handle);
	let ArrayData::Fixed(bytes) = &array.items else {
		return FAILURE;
	};
	if index >= array.count || destination.len() < array.block_size {
		return FAILURE;
	}
	let start = index * array.block_size;
	destination[..array.block_size].copy_from_slice(&bytes[start..start + array.block_size]);
	GET_SUCCESS
}

/// Copies the first fixed-width array item to caller-owned storage.
pub fn array_get_first_raw(id: Id, destination: &mut [u8]) -> i32 {
	array_get_raw(id, 0, destination)
}

/// Copies the final fixed-width array item to caller-owned storage.
pub fn array_get_last_raw(id: Id, destination: &mut [u8]) -> i32 {
	let Some(count) = array_count(id) else {
		return FAILURE;
	};
	count.checked_sub(1).map_or(FAILURE, |index| array_get_raw(id, index, destination))
}

/// Copies the cursor's fixed-width item to caller-owned storage.
pub fn array_get_current_raw(id: Id, destination: &mut [u8]) -> i32 {
	let Some(index) = array_current_index(id) else {
		return FAILURE;
	};
	array_get_raw(id, index, destination)
}

/// Retrieves a fixed-width array item into a compatible BPA block.
pub fn array_get_to_block(id: Id, index: usize, destination_id: Id) -> i32 {
	let Some(array_info) = array_info(id) else {
		return FAILURE;
	};
	let Some(block_info) = block_info(destination_id) else {
		return FAILURE;
	};
	if array_info.type_code == TYPE_STRING
		|| array_info.type_code != block_info.type_code
		|| array_info.block_size != block_info.block_size
	{
		return FAILURE;
	}
	let mut copied = vec![0; array_info.block_size];
	if array_get_raw(id, index, &mut copied) != GET_SUCCESS {
		return FAILURE;
	}
	if set(destination_id, &copied) == SET_SUCCESS { GET_SUCCESS } else { FAILURE }
}

/// Replaces a fixed-width array item from a compatible BPA block.
pub fn array_set_from_block(id: Id, index: usize, source_id: Id) -> i32 {
	let Some(array_info) = array_info(id) else {
		return FAILURE;
	};
	let Some(source_info) = block_info(source_id) else {
		return FAILURE;
	};
	if array_info.type_code == TYPE_STRING
		|| array_info.type_code != source_info.type_code
		|| array_info.block_size != source_info.block_size
	{
		return FAILURE;
	}
	let Some(bytes) = block_bytes(source_id) else {
		return FAILURE;
	};
	array_set_raw(id, index, &bytes)
}

/// Inserts a fixed-width item copied from a compatible BPA block.
pub fn array_insert_from_block(id: Id, index: usize, source_id: Id) -> i32 {
	let Some(info) = array_info(id) else {
		return FAILURE;
	};
	let Some(source_info) = block_info(source_id) else {
		return FAILURE;
	};
	if info.type_code != source_info.type_code || info.block_size != source_info.block_size {
		return FAILURE;
	}
	let Some(bytes) = block_bytes(source_id) else {
		return FAILURE;
	};
	array_insert_raw(id, index, &bytes)
}

/// Appends a fixed-width item copied from a compatible BPA block.
pub fn array_append_from_block(id: Id, source_id: Id) -> i32 {
	let Some(index) = array_count(id) else {
		return FAILURE;
	};
	array_insert_from_block(id, index, source_id)
}

/// Copies a string element to a NUL-terminated destination slice.
/// Returns its byte length excluding NUL, or [`FAILURE`].
pub fn array_get_string_raw(id: Id, index: usize, destination: &mut [u8]) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let array = lock_array(&handle);
	let ArrayData::Strings(values) = &array.items else {
		return FAILURE;
	};
	let Some(value) = values.get(index) else {
		return FAILURE;
	};
	if value.len() > i32::MAX as usize || destination.len() < value.len() + 1 {
		return FAILURE;
	}
	destination[..value.len()].copy_from_slice(value);
	destination[value.len()] = 0;
	value.len() as i32
}

/// Copies a string-array item into a pre-created BPA string block.
pub fn array_get_string_to_block(id: Id, index: usize, destination_id: Id) -> i32 {
	if array_info(id).is_none_or(|info| info.type_code != TYPE_STRING)
		|| block_info(destination_id).is_none_or(|info| info.type_code != TYPE_STRING)
	{
		return FAILURE;
	}
	let Some(mut value) = array_string_bytes(id, index) else {
		return FAILURE;
	};
	value.push(0);
	if set_string(destination_id, &value) < 0 {
		return FAILURE;
	}
	let encoding = array_info(id).map_or(0, |info| info.encoding);
	let _ = set_encoding(destination_id, encoding);
	GET_SUCCESS
}

/// Replaces a string-array item with a compatible BPA string block's contents.
pub fn array_set_string_from_block(id: Id, index: usize, source_id: Id) -> i32 {
	if array_info(id).is_none_or(|info| info.type_code != TYPE_STRING)
		|| block_info(source_id).is_none_or(|info| info.type_code != TYPE_STRING)
	{
		return FAILURE;
	}
	let Some(mut value) = string_bytes(source_id) else {
		return FAILURE;
	};
	value.push(0);
	if array_set_string_raw(id, index, &value) < 0 { FAILURE } else { SET_SUCCESS }
}

/// Inserts a BPA string block's contents into a string array.
pub fn array_insert_string_from_block(id: Id, index: usize, source_id: Id) -> i32 {
	if array_info(id).is_none_or(|info| info.type_code != TYPE_STRING)
		|| block_info(source_id).is_none_or(|info| info.type_code != TYPE_STRING)
	{
		return FAILURE;
	}
	let Some(mut value) = string_bytes(source_id) else {
		return FAILURE;
	};
	value.push(0);
	array_insert_raw(id, index, &value)
}

/// Appends a BPA string block's contents to a string array.
pub fn array_append_string_from_block(id: Id, source_id: Id) -> i32 {
	let Some(index) = array_count(id) else {
		return FAILURE;
	};
	array_insert_string_from_block(id, index, source_id)
}

/// Replaces an existing fixed-size element.
pub fn array_set_raw(id: Id, index: usize, source: &[u8]) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if index >= array.count || source.len() != array.block_size {
		return FAILURE;
	}
	let size = array.block_size;
	let ArrayData::Fixed(bytes) = &mut array.items else {
		return FAILURE;
	};
	let start = index * size;
	bytes[start..start + size].copy_from_slice(source);
	reset_deck(&mut array);
	SET_SUCCESS
}

/// Replaces an existing string element from bounded NUL-terminated bytes.
pub fn array_set_string_raw(id: Id, index: usize, source: &[u8]) -> i32 {
	let Some(value) = bounded_text(source) else {
		return FAILURE;
	};
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if index >= array.count {
		return FAILURE;
	}
	let ArrayData::Strings(values) = &mut array.items else {
		return FAILURE;
	};
	let length = value.len();
	values[index] = value;
	reset_deck(&mut array);
	length.try_into().unwrap_or(FAILURE)
}

/// Removes an initialized element and compacts all following elements.
pub fn array_remove(id: Id, index: usize) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if index >= array.count {
		return FAILURE;
	}
	let count = array.count;
	let size = array.block_size;
	match &mut array.items {
		ArrayData::Fixed(bytes) => {
			let start = index * size;
			let end = count * size;
			bytes.copy_within(start + size..end, start);
			let tail = (count - 1) * size;
			bytes[tail..tail + size].fill(0);
		}
		ArrayData::Strings(values) => {
			values.remove(index);
		}
	}
	array.count -= 1;
	if array.count == 0 {
		array.cursor = 0;
	} else if array.cursor > index {
		array.cursor -= 1;
	} else if array.cursor >= array.count {
		array.cursor = array.count - 1;
	}
	reset_deck(&mut array);
	SUCCESS
}

/// Removes the first item while retaining reserved array capacity.
pub fn array_remove_first(id: Id) -> i32 {
	array_remove(id, 0)
}

/// Removes the last item while retaining reserved array capacity.
pub fn array_remove_last(id: Id) -> i32 {
	array_count(id)
		.and_then(|count| count.checked_sub(1))
	.map_or(FAILURE, |index| array_remove(id, index))
}

/// Removes all items while retaining the array's capacity and configuration.
pub fn array_clear(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	match &mut array.items {
		ArrayData::Fixed(bytes) => bytes.fill(0),
		ArrayData::Strings(values) => values.clear(),
	}
	array.count = 0;
	array.cursor = 0;
	reset_deck(&mut array);
	SUCCESS
}

/// Returns the initialized-item count.
pub fn array_count(id: Id) -> Option<usize> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	Some(array.count)
}

/// Returns the reserved slot count.
pub fn array_capacity(id: Id) -> Option<usize> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	Some(array.capacity)
}

/// Returns the current array cursor index.
pub fn array_current_index(id: Id) -> Option<usize> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	Some(array.cursor)
}

/// Returns the byte length of the current string item.
pub fn array_current_string_length(id: Id) -> Option<usize> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	if array.type_code != TYPE_STRING || array.cursor >= array.count {
		return None;
	}
	let ArrayData::Strings(values) = &array.items else {
		return None;
	};
	Some(values[array.cursor].len())
}

/// Returns copyable metadata for an existing array.
pub fn array_info(id: Id) -> Option<ArrayInfo> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	Some(ArrayInfo {
		type_code: array.type_code,
		refcount: array.refcount,
		count: array.count,
		capacity: array.capacity,
		block_size: array.block_size,
		encoding: array.encoding,
		cursor: array.cursor,
		sealed: array.sealed,
	})
}

/// Returns an owned copy of one string-array item without a trailing NUL.
pub fn array_string_bytes(id: Id, index: usize) -> Option<Vec<u8>> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	let ArrayData::Strings(values) = &array.items else {
		return None;
	};
	values.get(index).cloned()
}

/// Moves the cursor to index zero, including for an empty array.
pub fn array_reset(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	lock_array(&handle).cursor = 0;
	SUCCESS
}

/// Moves the cursor to the first item.
pub fn array_move_first(id: Id) -> i32 {
	array_reset(id)
}

/// Moves the cursor to the final initialized item.
pub fn array_move_last(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.count == 0 {
		return FAILURE;
	}
	array.cursor = array.count - 1;
	SUCCESS
}

/// Moves the cursor forward by one initialized item.
pub fn array_move_next(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.cursor + 1 >= array.count {
		return FAILURE;
	}
	array.cursor += 1;
	SUCCESS
}

/// Moves the cursor backward by one item.
pub fn array_move_previous(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.cursor == 0 {
		return FAILURE;
	}
	array.cursor -= 1;
	SUCCESS
}

/// Moves the cursor to an initialized item.
pub fn array_move_to(id: Id, index: usize) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if index >= array.count {
		return FAILURE;
	}
	array.cursor = index;
	SUCCESS
}

/// Seals an array against capacity changes and automatic growth.
pub fn array_seal(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	lock_array(&handle).sealed = true;
	SUCCESS
}

/// Removes the capacity seal from an array.
pub fn array_unseal(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	lock_array(&handle).sealed = false;
	SUCCESS
}

/// Reports whether an array is sealed.
pub fn array_is_sealed(id: Id) -> Option<bool> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	Some(array.sealed)
}

fn reset_deck(array: &mut Array) {
	array.deck = (0..array.count).collect();
	array.deck_cursor = 0;
}

fn next_random() -> u64 {
	let random = RANDOM_STATE.get_or_init(|| Mutex::new(0x9e37_79b9_7f4a_7c15));
	let mut state = random
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner);
	*state ^= *state << 13;
	*state ^= *state >> 7;
	*state ^= *state << 17;
	*state
}

fn shuffle_indices(values: &mut [usize]) {
	for index in (1..values.len()).rev() {
		let other = (next_random() as usize) % (index + 1);
		values.swap(index, other);
	}
}

fn shuffle_values<T>(values: &mut [T]) {
	for index in (1..values.len()).rev() {
		let other = (next_random() as usize) % (index + 1);
		values.swap(index, other);
	}
}

/// Reorders fixed-width elements using a three-way comparator.
pub fn array_sort_with<F>(id: Id, mut compare: F) -> i32
where
	F: FnMut(&[u8], &[u8]) -> Ordering,
{
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING {
		return FAILURE;
	}
	let item_size = array.block_size;
	let count = array.count;
	let ArrayData::Fixed(bytes) = &mut array.items else {
		return FAILURE;
	};
	let mut items = bytes[..count * item_size]
		.chunks_exact(item_size)
		.map(<[u8]>::to_vec)
		.collect::<Vec<_>>();
	items.sort_by(|left, right| compare(left, right));
	for (index, item) in items.iter().enumerate() {
		let start = index * item_size;
		bytes[start..start + item_size].copy_from_slice(item);
	}
	array.cursor = 0;
	reset_deck(&mut array);
	SUCCESS
}

/// Reverses the initialized items in place.
pub fn array_reverse(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING {
		return FAILURE;
	}
	let count = array.count;
	let size = array.block_size;
	match &mut array.items {
		ArrayData::Fixed(bytes) => {
			for left in 0..count / 2 {
				let right = count - left - 1;
				for byte in 0..size {
					bytes.swap(left * size + byte, right * size + byte);
				}
			}
		}
		ArrayData::Strings(_) => unreachable!(),
	}
	reset_deck(&mut array);
	SUCCESS
}

/// Swaps two initialized elements.
pub fn array_swap(id: Id, left: usize, right: usize) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING || left >= array.count || right >= array.count {
		return FAILURE;
	}
	let size = array.block_size;
	match &mut array.items {
		ArrayData::Fixed(bytes) => {
			for offset in 0..size {
				bytes.swap(left * size + offset, right * size + offset);
			}
		}
		ArrayData::Strings(_) => unreachable!(),
	}
	reset_deck(&mut array);
	SUCCESS
}

/// Copies one initialized element over another.
pub fn array_copy(id: Id, source: usize, destination: usize) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING || source >= array.count || destination >= array.count {
		return FAILURE;
	}
	let size = array.block_size;
	match &mut array.items {
		ArrayData::Fixed(bytes) => {
			bytes.copy_within(source * size..(source + 1) * size, destination * size);
		}
		ArrayData::Strings(_) => unreachable!(),
	}
	reset_deck(&mut array);
	SUCCESS
}

/// Rotates the fixed-width suffix beginning at `index` by `positions` items.
/// `right` selects right rotation; false rotates left. String arrays are unsupported.
pub fn array_rotate(id: Id, index: usize, positions: usize, right: bool) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING || index >= array.count {
		return FAILURE;
	}
	let length = array.count - index;
	let shift = positions % length;
	if shift == 0 {
		return SUCCESS;
	}
	let item_count = array.count;
	let size = array.block_size;
	let ArrayData::Fixed(bytes) = &mut array.items else {
		return FAILURE;
	};
	let suffix = &mut bytes[index * size..item_count * size];
	if right {
		suffix.rotate_right(shift * size);
	} else {
		suffix.rotate_left(shift * size);
	}
	array.cursor = 0;
	reset_deck(&mut array);
	SUCCESS
}

/// Rotates the suffix beginning at `index` left by `positions` items.
pub fn array_shift_left(id: Id, index: usize, positions: usize) -> i32 {
	array_rotate(id, index, positions, false)
}

/// Rotates the suffix beginning at `index` right by `positions` items.
pub fn array_shift_right(id: Id, index: usize, positions: usize) -> i32 {
	array_rotate(id, index, positions, true)
}

/// Shuffles the initialized items in place.
pub fn array_shuffle(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING {
		return FAILURE;
	}
	let item_count = array.count;
	let item_size = array.block_size;
	match &mut array.items {
		ArrayData::Fixed(bytes) => {
			let mut indices = (0..item_count).collect::<Vec<_>>();
			shuffle_indices(&mut indices);
			let original = bytes[..item_count * item_size].to_vec();
			for (target, source) in indices.into_iter().enumerate() {
				bytes[target * item_size..(target + 1) * item_size]
					.copy_from_slice(&original[source * item_size..(source + 1) * item_size]);
			}
		}
		ArrayData::Strings(_) => unreachable!(),
	}
	reset_deck(&mut array);
	SUCCESS
}

/// Shuffles string-array items and resets their draw deck.
pub fn array_shuffle_strings(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code != TYPE_STRING {
		return FAILURE;
	}
	let ArrayData::Strings(values) = &mut array.items else {
		return FAILURE;
	};
	shuffle_values(values);
	array.cursor = 0;
	reset_deck(&mut array);
	SUCCESS
}

/// Sorts string-array items lexicographically by their bytes.
pub fn array_sort_strings(id: Id) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	let ArrayData::Strings(values) = &mut array.items else {
		return FAILURE;
	};
	values.sort();
	array.cursor = 0;
	reset_deck(&mut array);
	SUCCESS
}

/// Copies an initialized range into a newly-created independent array.
pub fn array_slice(source_id: Id, start: usize, length: usize, destination_id: Id) -> i32 {
	let Some(source_handle) = array_handle(source_id) else {
		return FAILURE;
	};
	let source = lock_array(&source_handle);
	if start > source.count || length > source.count - start {
		return FAILURE;
	}
	let type_code = source.type_code;
	let block_size = source.block_size;
	let encoding = source.encoding;
	let items = match &source.items {
		ArrayData::Fixed(bytes) => ArrayData::Fixed(
			bytes[start * block_size..(start + length) * block_size].to_vec(),
		),
		ArrayData::Strings(values) => ArrayData::Strings(values[start..start + length].to_vec()),
	};
	drop(source);
	let mut registry = store();
	if registry.blocks.contains_key(&destination_id) || registry.arrays.contains_key(&destination_id) {
		return FAILURE;
	}
	let mut result = new_array(type_code, length, encoding, block_size);
	result.count = length;
	result.items = items;
	registry.arrays.insert(destination_id, Arc::new(Mutex::new(result)));
	SUCCESS
}

/// Returns a safe owned snapshot of a fixed-width byte range.
///
/// The C interface exposed a raw pointer invalidated by mutation. Rust returns
/// an owned snapshot instead, so the source array can safely be changed later.
pub fn array_slice_borrow(id: Id, start: usize, length: usize) -> Option<Vec<u8>> {
	let handle = array_handle(id)?;
	let array = lock_array(&handle);
	if array.type_code == TYPE_STRING || start > array.count || length > array.count - start {
		return None;
	}
	let ArrayData::Fixed(bytes) = &array.items else {
		return None;
	};
	let begin = start * array.block_size;
	Some(bytes[begin..begin + length * array.block_size].to_vec())
}

/// Copies a fixed-width range to caller-owned storage.
pub fn array_slice_to(id: Id, start: usize, length: usize, destination: &mut [u8]) -> i32 {
	let Some(slice) = array_slice_borrow(id, start, length) else {
		return FAILURE;
	};
	if destination.len() < slice.len() {
		return FAILURE;
	}
	destination[..slice.len()].copy_from_slice(&slice);
	SUCCESS
}

fn draw_index(array: &mut Array, requested: Option<usize>) -> Option<usize> {
	if array.count == 0 {
		return None;
	}
	if requested.is_none() && array.deck_cursor >= array.deck.len() {
		array.deck = (0..array.count).collect();
		shuffle_indices(&mut array.deck);
		array.deck_cursor = 0;
	}
	if let Some(deck_offset) = requested {
		if array.deck_cursor >= array.deck.len()
			|| deck_offset >= array.deck.len() - array.deck_cursor
		{
			return None;
		}
		let selected_position = array.deck_cursor + deck_offset;
		let selected = array.deck[selected_position];
		array.deck.swap(array.deck_cursor, selected_position);
		array.deck_cursor += 1;
		return Some(selected);
	}
	let index = *array.deck.get(array.deck_cursor)?;
	array.deck_cursor += 1;
	Some(index)
}

/// Draws an item from the current shuffled deck and copies it into the output.
pub fn array_draw_raw(id: Id, destination: &mut [u8]) -> i32 {
	array_draw_at_raw(id, None, destination)
}

/// Draws the requested fixed-width item, or the next item when `index` is `None`.
pub fn array_draw_at_raw(id: Id, index: Option<usize>, destination: &mut [u8]) -> i32 {
	let Some(handle) = array_handle(id) else {
		return FAILURE;
	};
	let mut array = lock_array(&handle);
	if array.type_code == TYPE_STRING || destination.len() < array.block_size {
		return FAILURE;
	}
	let Some(index) = draw_index(&mut array, index) else {
		return FAILURE;
	};
	let ArrayData::Fixed(bytes) = &array.items else {
		return FAILURE;
	};
	let start = index * array.block_size;
	destination[..array.block_size].copy_from_slice(&bytes[start..start + array.block_size]);
	GET_SUCCESS
}

/// Draws a string item from the active deck or a requested item index.
pub fn array_draw_string(id: Id, requested: Option<usize>) -> Option<Vec<u8>> {
	let handle = array_handle(id)?;
	let mut array = lock_array(&handle);
	let index = draw_index(&mut array, requested)?;
	let ArrayData::Strings(values) = &array.items else {
		return None;
	};
	values.get(index).cloned()
}

/// Draws a fixed-width array item into a compatible BPA block.
pub fn array_draw_to_block(id: Id, destination_id: Id) -> i32 {
	let Some(info) = array_info(id) else {
		return FAILURE;
	};
	let Some(block) = block_info(destination_id) else {
		return FAILURE;
	};
	if info.type_code != block.type_code || info.block_size != block.block_size {
		return FAILURE;
	}
	let mut value = vec![0; info.block_size];
	if array_draw_raw(id, &mut value) != GET_SUCCESS {
		return FAILURE;
	}
	if set(destination_id, &value) == SET_SUCCESS { GET_SUCCESS } else { FAILURE }
}

/// Draws a fixed-width item from a deck slot into a compatible BPA block.
pub fn array_draw_at_to_block(id: Id, deck_slot: usize, destination_id: Id) -> i32 {
	let Some(info) = array_info(id) else {
		return FAILURE;
	};
	let Some(block) = block_info(destination_id) else {
		return FAILURE;
	};
	if info.type_code != block.type_code || info.block_size != block.block_size {
		return FAILURE;
	}
	let mut value = vec![0; info.block_size];
	if array_draw_at_raw(id, Some(deck_slot), &mut value) != GET_SUCCESS {
		return FAILURE;
	}
	if set(destination_id, &value) == SET_SUCCESS { GET_SUCCESS } else { FAILURE }
}

/// Draws a string item into a compatible BPA string block.
pub fn array_draw_string_to_block(id: Id, deck_slot: Option<usize>, destination_id: Id) -> i32 {
	if block_info(destination_id).is_none_or(|info| info.type_code != TYPE_STRING) {
		return FAILURE;
	}
	let Some(mut value) = array_draw_string(id, deck_slot) else {
		return FAILURE;
	};
	value.push(0);
	if set_string(destination_id, &value) < 0 { FAILURE } else { GET_SUCCESS }
}

/// Creates a safe advanced handle that retains the array until dropped.
pub fn advanced_borrow(id: Id) -> Option<AdvancedArray> {
	let handle = array_handle(id)?;
	{
		let mut array = lock_array(&handle);
		if array.refcount == u16::MAX {
			return None;
		}
		array.refcount += 1;
	}
	Some(AdvancedArray { id, handle })
}

/// Safe advanced access to an array without repeated registry lookups.
pub struct AdvancedArray {
	id: Id,
	handle: Arc<Mutex<Array>>,
}

impl AdvancedArray {
	/// Returns the array's initialized item count and reserved capacity.
	pub fn counts(&self) -> (usize, usize) {
		let array = lock_array(&self.handle);
		(array.count, array.capacity)
	}

	/// Applies a closure to the contiguous fixed-width payload.
	/// String arrays have no fixed-width payload and return `None`.
	pub fn with_data_mut<R>(&self, action: impl FnOnce(&mut [u8]) -> R) -> Option<R> {
		let mut array = lock_array(&self.handle);
		let count = array.count;
		let size = array.block_size;
		let ArrayData::Fixed(bytes) = &mut array.items else {
			return None;
		};
		Some(action(&mut bytes[..count * size]))
	}

	/// Reads a fixed-width item without looking up its ID again.
	pub fn get_raw(&self, index: usize, destination: &mut [u8]) -> i32 {
		let array = lock_array(&self.handle);
		let ArrayData::Fixed(bytes) = &array.items else {
			return FAILURE;
		};
		if index >= array.count || destination.len() < array.block_size {
			return FAILURE;
		}
		let start = index * array.block_size;
		destination[..array.block_size].copy_from_slice(&bytes[start..start + array.block_size]);
		GET_SUCCESS
	}

	/// Replaces a fixed-width item without another registry lookup.
	pub fn set_raw(&self, index: usize, source: &[u8]) -> i32 {
		let mut array = lock_array(&self.handle);
		if index >= array.count || source.len() != array.block_size {
			return FAILURE;
		}
		let size = array.block_size;
		let ArrayData::Fixed(bytes) = &mut array.items else {
			return FAILURE;
		};
		bytes[index * size..(index + 1) * size].copy_from_slice(source);
		reset_deck(&mut array);
		SET_SUCCESS
	}

	/// Appends a fixed-width item using the retained array handle.
	pub fn append_raw(&self, source: &[u8]) -> i32 {
		let mut array = lock_array(&self.handle);
		let index = array.count;
		insert_locked(&mut array, index, source)
	}

	/// Inserts a fixed-width item using the retained array handle.
	pub fn insert_raw(&self, index: usize, source: &[u8]) -> i32 {
		insert_locked(&mut lock_array(&self.handle), index, source)
	}

	/// Removes a fixed-width item using the retained array handle.
	pub fn remove(&self, index: usize) -> i32 {
		let mut array = lock_array(&self.handle);
		if array.type_code == TYPE_STRING || index >= array.count {
			return FAILURE;
		}
		let size = array.block_size;
		let end = array.count * size;
		let start = index * size;
		let old_count = array.count;
		let ArrayData::Fixed(bytes) = &mut array.items else {
			return FAILURE;
		};
		bytes.copy_within(start + size..end, start);
		bytes[(old_count - 1) * size..old_count * size].fill(0);
		array.count -= 1;
		array.cursor = array.cursor.min(array.count.saturating_sub(1));
		reset_deck(&mut array);
		SUCCESS
	}

	/// Clears fixed-width items while retaining reserved capacity.
	pub fn clear(&self) -> i32 {
		let mut array = lock_array(&self.handle);
		if array.type_code == TYPE_STRING {
			return FAILURE;
		}
		if let ArrayData::Fixed(bytes) = &mut array.items {
			bytes.fill(0);
		}
		array.count = 0;
		array.cursor = 0;
		reset_deck(&mut array);
		SUCCESS
	}

	/// Reserves additional capacity on an unsealed array.
	pub fn extend(&self, additional: usize) -> i32 {
		let mut array = lock_array(&self.handle);
		if array.sealed || additional == 0 {
			return FAILURE;
		}
		let Some(capacity) = array.capacity.checked_add(additional) else {
			return FAILURE;
		};
		if resize_array(&mut array, capacity) { SUCCESS } else { FAILURE }
	}

	/// Shrinks capacity on an unsealed array without discarding live items.
	pub fn shrink(&self, capacity: usize) -> i32 {
		let mut array = lock_array(&self.handle);
		if array.sealed || capacity >= array.capacity || capacity < array.count {
			return FAILURE;
		}
		if resize_array(&mut array, capacity) { SUCCESS } else { FAILURE }
	}

	/// Returns the ID associated with this retained handle.
	pub fn id(&self) -> Id {
		self.id
	}
}

impl Drop for AdvancedArray {
	fn drop(&mut self) {
		let remove = {
			let mut array = lock_array(&self.handle);
			array.refcount = array.refcount.saturating_sub(1);
			array.refcount == 0
		};
		if remove {
			array_delete(self.id);
		}
	}
}

/// Releases all BPA blocks and arrays and resets pool statistics.
pub fn shutdown() -> i32 {
	let mut registry = store();
	registry.blocks.clear();
	registry.arrays.clear();
	registry.pools = [PoolClass::default(); 10];
	SUCCESS
}