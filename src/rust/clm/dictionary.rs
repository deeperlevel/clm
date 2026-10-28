//! Integer-keyed typed dictionaries corresponding to the C dictionary module.
//!
//! Values are copied into owned byte vectors. Iteration follows insertion order;
//! replacing a value preserves its position, while removing and readding a key
//! places it at the end.

use crate::clm::bpa;
use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

pub type Id = u64;

struct Dictionary {
	type_code: u8,
	value_size: usize,
	values: HashMap<Id, Vec<u8>>,
	keys: Vec<Id>,
	iterator: usize,
}

static DICTIONARIES: OnceLock<Mutex<HashMap<Id, Dictionary>>> = OnceLock::new();

fn dictionaries() -> &'static Mutex<HashMap<Id, Dictionary>> {
	DICTIONARIES.get_or_init(|| Mutex::new(HashMap::new()))
}

fn lock_dictionaries() -> std::sync::MutexGuard<'static, HashMap<Id, Dictionary>> {
	dictionaries()
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn resolve_value_size(type_code: u8, size: usize) -> Option<usize> {
	match type_code {
		bpa::TYPE_STRUCT if size > 0 && size <= u32::MAX as usize => Some(size),
		bpa::TYPE_STRING if size == 0 => Some(0),
		bpa::TYPE_STRUCT | bpa::TYPE_STRING => None,
		_ => {
			let fixed_size = bpa::type_size(type_code);
			(fixed_size > 0 && (size == 0 || size == fixed_size)).then_some(fixed_size)
		}
	}
}

/// Creates an empty dictionary for values of a fixed BPA type.
/// `STRUCT` values require a nonzero byte width; all other types use `size = 0`.
pub fn new(id: Id, type_code: u8, size: usize) -> i32 {
	let Some(value_size) = resolve_value_size(type_code, size) else {
		return bpa::FAILURE;
	};
	if bpa::is_existing(id) || bpa::array_is_existing(id) {
		return bpa::FAILURE;
	}
	let mut dictionaries = lock_dictionaries();
	if dictionaries.contains_key(&id) {
		return bpa::FAILURE;
	}
	if bpa::create_sized(id, bpa::TYPE_STRUCT, 1) != bpa::SUCCESS {
		return bpa::FAILURE;
	}
	dictionaries.insert(
		id,
		Dictionary {
			type_code,
			value_size,
			values: HashMap::new(),
			keys: Vec::new(),
			iterator: 0,
		},
	);
	bpa::SUCCESS
}

/// Deletes a dictionary and all of its owned values.
pub fn delete(id: Id) -> i32 {
	if lock_dictionaries().remove(&id).is_none() {
		return bpa::FAILURE;
	}
	bpa::delete(id)
}

fn bounded_value(type_code: u8, value_size: usize, source: &[u8]) -> Option<Vec<u8>> {
	if source.is_empty() || source.len() > u32::MAX as usize {
		return None;
	}
	if type_code == bpa::TYPE_STRING {
		let length = source.iter().position(|byte| *byte == 0).unwrap_or(source.len());
		Some(source[..length].to_vec())
	} else if source.len() == value_size {
		Some(source.to_vec())
	} else {
		None
	}
}

/// Stores or replaces a value copied from caller-owned bytes.
/// String input is bounded by the first NUL byte; fixed-width values require an exact size.
pub fn set_from(id: Id, key: Id, source: &[u8]) -> i32 {
	let mut dictionaries = lock_dictionaries();
	let Some(dictionary) = dictionaries.get_mut(&id) else {
		return bpa::FAILURE;
	};
	let Some(value) = bounded_value(dictionary.type_code, dictionary.value_size, source) else {
		return bpa::FAILURE;
	};
	let is_new = !dictionary.values.contains_key(&key);
	dictionary.values.insert(key, value);
	if is_new {
		dictionary.keys.push(key);
		dictionary.iterator = 0;
	}
	if dictionary.type_code == bpa::TYPE_STRING || dictionary.value_size > 0 {
		bpa::SET_SUCCESS
	} else {
		bpa::FAILURE
	}
}

/// Copies a compatible BPA block's value into the dictionary.
pub fn set(id: Id, key: Id, source_block_id: Id) -> i32 {
	let Some(info) = bpa::block_info(source_block_id) else {
		return bpa::FAILURE;
	};
	let source = if info.type_code == bpa::TYPE_STRING {
		bpa::string_bytes(source_block_id)
	} else {
		bpa::block_bytes(source_block_id)
	};
	let Some(mut source) = source else {
		return bpa::FAILURE;
	};
	{
		let dictionaries = lock_dictionaries();
		let Some(dictionary) = dictionaries.get(&id) else {
			return bpa::FAILURE;
		};
		if info.type_code != dictionary.type_code
			|| (info.type_code != bpa::TYPE_STRING && info.block_size != dictionary.value_size)
		{
			return bpa::FAILURE;
		}
	}
	if info.type_code == bpa::TYPE_STRING {
		source.push(0);
	}
	set_from(id, key, &source)
}

/// Retrieves a value into caller-owned memory; strings include their NUL terminator.
pub fn get_to(id: Id, key: Id, destination: &mut [u8]) -> i32 {
	let dictionaries = lock_dictionaries();
	let Some(dictionary) = dictionaries.get(&id) else {
		return bpa::FAILURE;
	};
	let Some(value) = dictionary.values.get(&key) else {
		return bpa::FAILURE;
	};
	let required = value.len() + usize::from(dictionary.type_code == bpa::TYPE_STRING);
	if destination.len() < required {
		return bpa::FAILURE;
	}
	destination[..value.len()].copy_from_slice(value);
	if dictionary.type_code == bpa::TYPE_STRING {
		destination[value.len()] = 0;
	}
	bpa::GET_SUCCESS
}

/// Retrieves a value into a compatible BPA block.
pub fn get(id: Id, key: Id, destination_block_id: Id) -> i32 {
	let (value_type, value) = {
		let dictionaries = lock_dictionaries();
		let Some(dictionary) = dictionaries.get(&id) else {
			return bpa::FAILURE;
		};
		let Some(value) = dictionary.values.get(&key) else {
			return bpa::FAILURE;
		};
		(dictionary.type_code, value.clone())
	};
	let Some(info) = bpa::block_info(destination_block_id) else {
		return bpa::FAILURE;
	};
	if info.type_code != value_type
		|| (value_type != bpa::TYPE_STRING && info.block_size != value.len())
	{
		return bpa::FAILURE;
	}
	if value_type == bpa::TYPE_STRING {
		let mut terminated = value;
		terminated.push(0);
		if bpa::set_string(destination_block_id, &terminated) >= 0 {
			bpa::GET_SUCCESS
		} else {
			bpa::FAILURE
		}
	} else if bpa::set(destination_block_id, &value) == bpa::SET_SUCCESS {
		bpa::GET_SUCCESS
	} else {
		bpa::FAILURE
	}
}

/// Checks whether a key is present.
pub fn contains(id: Id, key: Id) -> bool {
	lock_dictionaries()
		.get(&id)
	.is_some_and(|dictionary| dictionary.values.contains_key(&key))
}

/// Returns the number of entries, or [`bpa::FAILURE`] if the ID is invalid.
pub fn count(id: Id) -> i32 {
	lock_dictionaries()
		.get(&id)
	.and_then(|dictionary| i32::try_from(dictionary.values.len()).ok())
		.unwrap_or(bpa::FAILURE)
}

/// Removes a key and its value.
pub fn remove(id: Id, key: Id) -> i32 {
	let mut dictionaries = lock_dictionaries();
	let Some(dictionary) = dictionaries.get_mut(&id) else {
		return bpa::FAILURE;
	};
	if dictionary.values.remove(&key).is_none() {
		return bpa::FAILURE;
	}
	dictionary.keys.retain(|stored_key| *stored_key != key);
	dictionary.iterator = 0;
	bpa::SUCCESS
}

/// Resets ordered key iteration to the first entry.
pub fn iterate_reset(id: Id) -> i32 {
	let mut dictionaries = lock_dictionaries();
	let Some(dictionary) = dictionaries.get_mut(&id) else {
		return bpa::FAILURE;
	};
	dictionary.iterator = 0;
	bpa::SUCCESS
}

/// Returns the next key in insertion order, or `None` when exhausted or invalid.
pub fn iterate_next(id: Id) -> Option<Id> {
	let mut dictionaries = lock_dictionaries();
	let dictionary = dictionaries.get_mut(&id)?;
	let key = *dictionary.keys.get(dictionary.iterator)?;
	dictionary.iterator += 1;
	Some(key)
}

/// Removes every entry while retaining the dictionary's configured value type.
pub fn clear(id: Id) -> i32 {
	let mut dictionaries = lock_dictionaries();
	let Some(dictionary) = dictionaries.get_mut(&id) else {
		return bpa::FAILURE;
	};
	dictionary.values.clear();
	dictionary.keys.clear();
	dictionary.iterator = 0;
	bpa::SUCCESS
}

/// Inserts a new key/value pair and rejects duplicate keys.
pub fn add_from(id: Id, key: Id, value: &[u8]) -> i32 {
	if contains(id, key) {
		return bpa::FAILURE;
	}
	set_from(id, key, value)
}

/// Replaces the value for an existing key without changing its insertion position.
pub fn update_from(id: Id, key: Id, value: &[u8]) -> i32 {
	if !contains(id, key) {
		return bpa::FAILURE;
	}
	set_from(id, key, value)
}

/// Copies keys to a new vector in insertion order.
pub fn keys(id: Id) -> Option<Vec<Id>> {
	lock_dictionaries().get(&id).map(|dictionary| dictionary.keys.clone())
}