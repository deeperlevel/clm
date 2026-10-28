//! UTF-8 strings and string arrays backed by the CLM BPA registry.
//!
//! Text is stored internally as validated UTF-8. String indices and lengths
//! count Unicode scalar values; byte exports may use UTF-8, Latin-1, or UTF-16LE.

use crate::clm::bpa::{self, Id};
use std::sync::atomic::{AtomicU8, Ordering};

pub const ENCODING_UTF8: u8 = 1;
pub const ENCODING_LATIN1: u8 = 2;
pub const ENCODING_UTF16LE: u8 = 3;

static DEFAULT_ENCODING: AtomicU8 = AtomicU8::new(ENCODING_UTF8);

fn supported_encoding(encoding: u8) -> bool {
	matches!(encoding, ENCODING_UTF8 | ENCODING_LATIN1 | ENCODING_UTF16LE)
}

fn text(id: Id) -> Option<(String, u8)> {
	let info = bpa::block_info(id)?;
	if info.type_code != bpa::TYPE_STRING {
		return None;
	}
	let bytes = bpa::string_bytes(id)?;
	let text = String::from_utf8(bytes).ok()?;
	(!text.contains('\0')).then_some((text, info.encoding))
}

fn c_bytes(text: &str) -> Option<Vec<u8>> {
	if text.contains('\0') {
		return None;
	}
	let mut bytes = text.as_bytes().to_vec();
	bytes.push(0);
	Some(bytes)
}

fn char_count(text: &str) -> usize {
	text.chars().count()
}

fn char_offset(text: &str, index: usize) -> Option<usize> {
	if index > char_count(text) {
		return None;
	}
	Some(text.char_indices().nth(index).map_or(text.len(), |(offset, _)| offset))
}

fn encoded(text: &str, encoding: u8) -> Option<Vec<u8>> {
	match encoding {
		ENCODING_UTF8 => Some(text.as_bytes().to_vec()),
		ENCODING_LATIN1 => text
			.chars()
			.map(|character| u8::try_from(u32::from(character)).ok())
			.collect(),
		ENCODING_UTF16LE => Some(
			text.encode_utf16()
				.flat_map(u16::to_le_bytes)
				.collect(),
		),
		_ => None,
	}
}

fn decode(bytes: &[u8], encoding: u8) -> Option<String> {
	let text = match encoding {
		ENCODING_UTF8 => std::str::from_utf8(bytes).ok()?.to_owned(),
		ENCODING_LATIN1 => bytes.iter().map(|byte| char::from(*byte)).collect(),
		ENCODING_UTF16LE => {
			if bytes.len() % 2 != 0 {
				return None;
			}
			let words = bytes
				.chunks_exact(2)
				.map(|pair| u16::from_le_bytes([pair[0], pair[1]]));
			char::decode_utf16(words).collect::<Result<String, _>>().ok()?
		}
		_ => return None,
	};
	(!text.contains('\0')).then_some(text)
}

/// Changes the preferred byte-export encoding for subsequently created strings.
pub fn set_default_encoding(encoding: u8) -> i32 {
	if !supported_encoding(encoding) {
		return bpa::FAILURE;
	}
	DEFAULT_ENCODING.store(encoding, Ordering::Relaxed);
	bpa::SUCCESS
}

/// Returns the runtime default byte-export encoding, initially UTF-8.
pub fn get_default_encoding() -> u8 {
	DEFAULT_ENCODING.load(Ordering::Relaxed)
}

/// Creates a string BPA block initialized with valid UTF-8 text.
pub fn new(id: Id, value: &str) -> bool {
	let Some(bytes) = c_bytes(value) else {
		return false;
	};
	if bpa::create(id, bpa::TYPE_STRING) != bpa::SUCCESS {
		return false;
	}
	if bpa::set_string(id, &bytes) < 0 {
		let _ = bpa::delete(id);
		return false;
	}
	bpa::set_encoding(id, get_default_encoding()) == bpa::SUCCESS
}

/// Deletes a string block without affecting blocks of other types.
pub fn delete(id: Id) -> bool {
	bpa::block_info(id).is_some_and(|info| {
		info.type_code == bpa::TYPE_STRING && bpa::delete(id) == bpa::SUCCESS
	})
}

/// Replaces a string block and returns its Unicode scalar count, or [`bpa::FAILURE`].
pub fn set(id: Id, value: &str) -> i32 {
	let Some(bytes) = c_bytes(value) else {
		return bpa::FAILURE;
	};
	if bpa::block_info(id).is_none_or(|info| info.type_code != bpa::TYPE_STRING) {
		return bpa::FAILURE;
	}
	if bpa::set_string(id, &bytes) < 0 {
		return bpa::FAILURE;
	}
	i32::try_from(char_count(value)).unwrap_or(bpa::FAILURE)
}

/// Appends UTF-8 text and returns the number of characters appended.
pub fn append(id: Id, suffix: &str) -> i32 {
	let Some((mut current, _)) = text(id) else {
		return bpa::FAILURE;
	};
	if suffix.contains('\0') {
		return bpa::FAILURE;
	}
	let count = char_count(suffix);
	current.push_str(suffix);
	if set(id, &current) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(count).unwrap_or(bpa::FAILURE)
	}
}

/// Appends UTF-8 text followed by LF and returns the appended character count.
pub fn append_line(id: Id, suffix: &str) -> i32 {
	if suffix.contains('\0') {
		return bpa::FAILURE;
	}
	let count = char_count(suffix) + 1;
	let Some((mut current, _)) = text(id) else {
		return bpa::FAILURE;
	};
	current.push_str(suffix);
	current.push('\n');
	if set(id, &current) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(count).unwrap_or(bpa::FAILURE)
	}
}

/// Inserts text at a Unicode character index and returns the inserted count.
pub fn insert(id: Id, index: usize, inserted: &str) -> i32 {
	if inserted.contains('\0') {
		return bpa::FAILURE;
	}
	let Some((current, _)) = text(id) else {
		return bpa::FAILURE;
	};
	let Some(offset) = char_offset(&current, index) else {
		return bpa::FAILURE;
	};
	let mut result = String::with_capacity(current.len() + inserted.len());
	result.push_str(&current[..offset]);
	result.push_str(inserted);
	result.push_str(&current[offset..]);
	if set(id, &result) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(char_count(inserted)).unwrap_or(bpa::FAILURE)
	}
}

/// Removes a Unicode character range and returns its length.
pub fn remove(id: Id, index: usize, length: usize) -> i32 {
	let Some((current, _)) = text(id) else {
		return bpa::FAILURE;
	};
	let count = char_count(&current);
	if index > count || length > count - index {
		return bpa::FAILURE;
	}
	let Some(start) = char_offset(&current, index) else {
		return bpa::FAILURE;
	};
	let Some(end) = char_offset(&current, index + length) else {
		return bpa::FAILURE;
	};
	let mut result = String::with_capacity(current.len() - (end - start));
	result.push_str(&current[..start]);
	result.push_str(&current[end..]);
	if set(id, &result) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(length).unwrap_or(bpa::FAILURE)
	}
}

/// Returns whether a string contains a valid search string.
pub fn contains(id: Id, search: &str) -> bool {
	!search.contains('\0') && text(id).is_some_and(|(value, _)| value.contains(search))
}

/// Returns the string length in Unicode scalar values, or failure if the ID is invalid.
pub fn length(id: Id) -> i32 {
	text(id)
		.and_then(|(value, _)| i32::try_from(char_count(&value)).ok())
		.unwrap_or(bpa::FAILURE)
}

/// Replaces every non-overlapping occurrence and returns the match count.
/// An empty search is invalid; replacement text may be empty.
pub fn replace(id: Id, search: &str, replacement: &str) -> i32 {
	if search.is_empty() || search.contains('\0') || replacement.contains('\0') {
		return bpa::FAILURE;
	}
	let Some((current, _)) = text(id) else {
		return bpa::FAILURE;
	};
	let count = current.matches(search).count();
	if count == 0 {
		return 0;
	}
	let result = current.replace(search, replacement);
	if set(id, &result) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(count).unwrap_or(bpa::FAILURE)
	}
}

/// Exports the text using its preferred encoding; `None` queries the required byte count.
pub fn get_bytes(id: Id, destination: Option<&mut [u8]>) -> i32 {
	let Some((value, encoding)) = text(id) else {
		return bpa::FAILURE;
	};
	let Some(bytes) = encoded(&value, encoding) else {
		return bpa::FAILURE;
	};
	let Ok(length) = i32::try_from(bytes.len()) else {
		return bpa::FAILURE;
	};
	let Some(destination) = destination else {
		return length;
	};
	if destination.len() < bytes.len() {
		return bpa::FAILURE;
	}
	destination[..bytes.len()].copy_from_slice(&bytes);
	length
}

/// Decodes external bytes into UTF-8 storage and records the source encoding preference.
/// Existing strings are preserved when decoding or validation fails.
pub fn from_bytes(id: Id, encoding: u8, bytes: &[u8]) -> i32 {
	if !supported_encoding(encoding) {
		return bpa::FAILURE;
	}
	let Some(value) = decode(bytes, encoding) else {
		return bpa::FAILURE;
	};
	let created = if let Some(info) = bpa::block_info(id) {
		if info.type_code != bpa::TYPE_STRING {
			return bpa::FAILURE;
		}
		false
	} else {
		if bpa::create(id, bpa::TYPE_STRING) != bpa::SUCCESS {
			return bpa::FAILURE;
		}
		true
	};
	let Some(c_value) = c_bytes(&value) else {
		if created {
			let _ = bpa::delete(id);
		}
		return bpa::FAILURE;
	};
	if bpa::set_string(id, &c_value) < 0 || bpa::set_encoding(id, encoding) != bpa::SUCCESS {
		if created {
			let _ = bpa::delete(id);
		}
		return bpa::FAILURE;
	}
	bpa::SUCCESS
}

/// Changes a string's preferred export encoding without changing its text.
pub fn change_encoding(id: Id, encoding: u8) -> i32 {
	if !supported_encoding(encoding) {
		return bpa::FAILURE;
	}
	let Some((value, _)) = text(id) else {
		return bpa::FAILURE;
	};
	if encoded(&value, encoding).is_none() {
		return bpa::FAILURE;
	}
	bpa::set_encoding(id, encoding)
}

/// Copies a Unicode code-point range into an independent string block.
pub fn slice(source_id: Id, start: usize, length: usize, destination_id: Id) -> i32 {
	let Some((source, encoding)) = text(source_id) else {
		return bpa::FAILURE;
	};
	let count = char_count(&source);
	if start > count || length > count - start || bpa::is_existing(destination_id) || bpa::array_is_existing(destination_id) {
		return bpa::FAILURE;
	}
	let value = source.chars().skip(start).take(length).collect::<String>();
	if !new(destination_id, &value) || change_encoding(destination_id, encoding) != bpa::SUCCESS {
		let _ = delete(destination_id);
		return bpa::FAILURE;
	}
	bpa::SUCCESS
}

/// Returns an owned UTF-8 snapshot of a code-point range.
///
/// C exposed a borrowed pointer whose lifetime depended on the source block.
/// Rust returns an owned snapshot to keep the result valid across mutations.
pub fn slice_borrow(source_id: Id, start: usize, length: usize) -> Option<String> {
	let (source, _) = text(source_id)?;
	let count = char_count(&source);
	if start > count || length > count - start {
		return None;
	}
	Some(source.chars().skip(start).take(length).collect())
}

/// Creates a string array with UTF-8 internal elements.
pub fn array_new(id: Id, reserved: usize) -> bool {
	bpa::array_create(id, bpa::TYPE_STRING, reserved, ENCODING_UTF8) == bpa::SUCCESS
}

fn valid_string_array(id: Id) -> bool {
	bpa::array_info(id).is_some_and(|info| {
		info.type_code == bpa::TYPE_STRING && info.encoding == ENCODING_UTF8
	})
}

/// Deletes a string array without affecting arrays of other types.
pub fn array_delete(id: Id) -> bool {
	valid_string_array(id) && bpa::array_delete(id) == bpa::SUCCESS
}

/// Returns the number of array items, or failure for an invalid ID.
pub fn array_count(id: Id) -> i32 {
	if !valid_string_array(id) {
		return bpa::FAILURE;
	}
	bpa::array_count(id)
		.and_then(|count| i32::try_from(count).ok())
		.unwrap_or(bpa::FAILURE)
}

/// Copies an array string into an existing string BPA block.
pub fn array_get(id: Id, index: usize, destination_string_id: Id) -> i32 {
	if !valid_string_array(id)
		|| bpa::block_info(destination_string_id)
			.is_none_or(|info| info.type_code != bpa::TYPE_STRING)
	{
		return bpa::FAILURE;
	}
	let Some(value) = bpa::array_string_bytes(id, index) else {
		return bpa::FAILURE;
	};
	let mut c_value = value;
	c_value.push(0);
	if bpa::set_string(destination_string_id, &c_value) < 0 {
		return bpa::FAILURE;
	}
	let _ = bpa::set_encoding(destination_string_id, ENCODING_UTF8);
	bpa::GET_SUCCESS
}

/// Replaces an array string from an existing string BPA block.
pub fn array_set(id: Id, index: usize, source_string_id: Id) -> i32 {
	if !valid_string_array(id) {
		return bpa::FAILURE;
	}
	let Some(value) = bpa::string_bytes(source_string_id) else {
		return bpa::FAILURE;
	};
	if std::str::from_utf8(&value).is_err() || value.contains(&0) {
		return bpa::FAILURE;
	}
	let mut c_value = value;
	c_value.push(0);
	if bpa::array_set_string_raw(id, index, &c_value) >= 0 {
		bpa::SET_SUCCESS
	} else {
		bpa::FAILURE
	}
}

/// Inserts an existing string BPA block at an array index.
pub fn array_insert(id: Id, index: usize, source_string_id: Id) -> i32 {
	if !valid_string_array(id) {
		return bpa::FAILURE;
	}
	let Some(value) = bpa::string_bytes(source_string_id) else {
		return bpa::FAILURE;
	};
	if std::str::from_utf8(&value).is_err() || value.contains(&0) {
		return bpa::FAILURE;
	}
	let mut c_value = value;
	c_value.push(0);
	bpa::array_insert_raw(id, index, &c_value)
}

/// Appends an existing string BPA block to an array.
pub fn array_append(id: Id, source_string_id: Id) -> i32 {
	let Some(count) = bpa::array_count(id) else {
		return bpa::FAILURE;
	};
	array_insert(id, count, source_string_id)
}

/// Removes an array item by zero-based index.
pub fn array_remove(id: Id, index: usize) -> i32 {
	if valid_string_array(id) { bpa::array_remove(id, index) } else { bpa::FAILURE }
}

/// Clears array items while preserving their reserved capacity.
pub fn array_clear(id: Id) -> i32 {
	if valid_string_array(id) { bpa::array_clear(id) } else { bpa::FAILURE }
}

/// Finds the first exact string match, returning its index or failure.
pub fn array_find(id: Id, source_string_id: Id) -> i32 {
	if !valid_string_array(id) {
		return bpa::FAILURE;
	}
	let Some(search) = bpa::string_bytes(source_string_id) else {
		return bpa::FAILURE;
	};
	let Some(count) = bpa::array_count(id) else {
		return bpa::FAILURE;
	};
	(0..count)
		.find(|index| bpa::array_string_bytes(id, *index).as_deref() == Some(&search))
		.and_then(|index| i32::try_from(index).ok())
		.unwrap_or(bpa::FAILURE)
}

/// Returns the item length in Unicode scalar values.
pub fn array_length_at(id: Id, index: usize) -> i32 {
	let Some(value) = bpa::array_string_bytes(id, index) else {
		return bpa::FAILURE;
	};
	String::from_utf8(value)
		.ok()
		.and_then(|value| i32::try_from(char_count(&value)).ok())
		.unwrap_or(bpa::FAILURE)
}

/// Returns whether an indexed string contains a valid search string.
pub fn array_contains_at(id: Id, index: usize, search: &str) -> bool {
	if search.contains('\0') {
		return false;
	}
	bpa::array_string_bytes(id, index)
		.and_then(|value| String::from_utf8(value).ok())
		.is_some_and(|value| value.contains(search))
}

/// Appends text to one array string and returns the appended character count.
pub fn array_append_at(id: Id, index: usize, suffix: &str) -> i32 {
	if suffix.contains('\0') {
		return bpa::FAILURE;
	}
	let Some(mut current) = bpa::array_string_bytes(id, index).and_then(|bytes| String::from_utf8(bytes).ok()) else {
		return bpa::FAILURE;
	};
	let count = char_count(suffix);
	current.push_str(suffix);
	let mut bytes = current.into_bytes();
	bytes.push(0);
	let status = bpa::array_set_string_raw(id, index, &bytes);
	if status < 0 { bpa::FAILURE } else { i32::try_from(count).unwrap_or(bpa::FAILURE) }
}

/// Appends text and LF to one array string.
pub fn array_append_line_at(id: Id, index: usize, suffix: &str) -> i32 {
	if suffix.contains('\0') {
		return bpa::FAILURE;
	}
	let Some(mut current) = bpa::array_string_bytes(id, index).and_then(|bytes| String::from_utf8(bytes).ok()) else {
		return bpa::FAILURE;
	};
	let count = char_count(suffix) + 1;
	current.push_str(suffix);
	current.push('\n');
	let mut bytes = current.into_bytes();
	bytes.push(0);
	let status = bpa::array_set_string_raw(id, index, &bytes);
	if status < 0 { bpa::FAILURE } else { i32::try_from(count).unwrap_or(bpa::FAILURE) }
}

/// Inserts text at a Unicode character index in one array string.
pub fn array_insert_at(id: Id, index: usize, character_index: usize, inserted: &str) -> i32 {
	if inserted.contains('\0') {
		return bpa::FAILURE;
	}
	let Some(current) = bpa::array_string_bytes(id, index).and_then(|bytes| String::from_utf8(bytes).ok()) else {
		return bpa::FAILURE;
	};
	let Some(offset) = char_offset(&current, character_index) else {
		return bpa::FAILURE;
	};
	let mut result = String::with_capacity(current.len() + inserted.len());
	result.push_str(&current[..offset]);
	result.push_str(inserted);
	result.push_str(&current[offset..]);
	let count = char_count(inserted);
	let mut bytes = result.into_bytes();
	bytes.push(0);
	if bpa::array_set_string_raw(id, index, &bytes) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(count).unwrap_or(bpa::FAILURE)
	}
}

/// Removes a Unicode character range from one array string.
pub fn array_remove_at(id: Id, index: usize, character_index: usize, length: usize) -> i32 {
	let Some(current) = bpa::array_string_bytes(id, index).and_then(|bytes| String::from_utf8(bytes).ok()) else {
		return bpa::FAILURE;
	};
	let count = char_count(&current);
	if character_index > count || length > count - character_index {
		return bpa::FAILURE;
	}
	let (Some(start), Some(end)) = (char_offset(&current, character_index), char_offset(&current, character_index + length)) else {
		return bpa::FAILURE;
	};
	let mut result = String::with_capacity(current.len() - (end - start));
	result.push_str(&current[..start]);
	result.push_str(&current[end..]);
	let mut bytes = result.into_bytes();
	bytes.push(0);
	if bpa::array_set_string_raw(id, index, &bytes) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(length).unwrap_or(bpa::FAILURE)
	}
}

/// Replaces all non-overlapping occurrences in one array string.
pub fn array_replace_at(id: Id, index: usize, search: &str, replacement: &str) -> i32 {
	if search.is_empty() || search.contains('\0') || replacement.contains('\0') {
		return bpa::FAILURE;
	}
	let Some(current) = bpa::array_string_bytes(id, index).and_then(|bytes| String::from_utf8(bytes).ok()) else {
		return bpa::FAILURE;
	};
	let count = current.matches(search).count();
	let result = current.replace(search, replacement);
	let mut bytes = result.into_bytes();
	bytes.push(0);
	if bpa::array_set_string_raw(id, index, &bytes) < 0 {
		bpa::FAILURE
	} else {
		i32::try_from(count).unwrap_or(bpa::FAILURE)
	}
}

/// Sorts strings lexicographically by their UTF-8 bytes.
pub fn array_sort(id: Id) -> i32 {
	if valid_string_array(id) { bpa::array_sort_strings(id) } else { bpa::FAILURE }
}

/// Randomly shuffles string items and resets the draw deck.
pub fn array_shuffle(id: Id) -> i32 {
	if valid_string_array(id) { bpa::array_shuffle_strings(id) } else { bpa::FAILURE }
}

/// Moves the array cursor to the first item.
pub fn array_move_first(id: Id) -> i32 { if valid_string_array(id) { bpa::array_move_first(id) } else { bpa::FAILURE } }
/// Moves the array cursor to the preceding item.
pub fn array_move_previous(id: Id) -> i32 { if valid_string_array(id) { bpa::array_move_previous(id) } else { bpa::FAILURE } }
/// Moves the array cursor to the next item.
pub fn array_move_next(id: Id) -> i32 { if valid_string_array(id) { bpa::array_move_next(id) } else { bpa::FAILURE } }
/// Moves the array cursor to its final item.
pub fn array_move_last(id: Id) -> i32 { if valid_string_array(id) { bpa::array_move_last(id) } else { bpa::FAILURE } }
/// Moves the array cursor to an initialized index.
pub fn array_move_to(id: Id, index: usize) -> i32 { if valid_string_array(id) { bpa::array_move_to(id, index) } else { bpa::FAILURE } }

/// Copies the current string-array item into a BPA string block.
pub fn array_get_current(id: Id, destination_string_id: Id) -> i32 {
	let Some(info) = bpa::array_info(id) else {
		return bpa::FAILURE;
	};
	array_get(id, info.cursor, destination_string_id)
}

/// Draws an indexed item from the deck into a BPA string block.
pub fn array_draw_at(id: Id, index: usize, destination_string_id: Id) -> i32 {
	if !valid_string_array(id)
		|| bpa::block_info(destination_string_id).is_none_or(|info| info.type_code != bpa::TYPE_STRING)
	{
		return bpa::FAILURE;
	}
	let Some(mut bytes) = bpa::array_draw_string(id, Some(index)) else {
		return bpa::FAILURE;
	};
	bytes.push(0);
	if bpa::set_string(destination_string_id, &bytes) < 0 { bpa::FAILURE } else { bpa::GET_SUCCESS }
}

/// Draws the next item, reshuffling when the active deck is exhausted.
pub fn array_draw(id: Id, destination_string_id: Id) -> i32 {
	if !valid_string_array(id)
		|| bpa::block_info(destination_string_id).is_none_or(|info| info.type_code != bpa::TYPE_STRING)
	{
		return bpa::FAILURE;
	}
	let Some(mut bytes) = bpa::array_draw_string(id, None) else {
		return bpa::FAILURE;
	};
	bytes.push(0);
	if bpa::set_string(destination_string_id, &bytes) < 0 { bpa::FAILURE } else { bpa::GET_SUCCESS }
}