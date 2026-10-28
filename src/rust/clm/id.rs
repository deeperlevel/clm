//! Named numeric ID registration and sequential ID generation.
//!
//! Names are valid UTF-8 Rust strings. Case-insensitive matching folds ASCII
//! letters only; non-ASCII characters remain byte-for-byte case-sensitive.

use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

pub type Id = u64;

/// Sentinel returned by [`get`] and [`gen_next`] when no ID is available.
pub const NOT_FOUND: Id = u64::MAX;

#[derive(Default)]
struct Registry {
	entries: HashMap<String, Entry>,
	case_sensitive: bool,
	next_generated: Id,
	generator_exhausted: bool,
}

struct Entry {
	spelling: String,
	id: Id,
}

static REGISTRY: OnceLock<Mutex<Registry>> = OnceLock::new();

fn registry() -> &'static Mutex<Registry> {
	REGISTRY.get_or_init(|| {
		Mutex::new(Registry {
			case_sensitive: true,
			..Registry::default()
		})
	})
}

fn lock_registry() -> std::sync::MutexGuard<'static, Registry> {
	registry()
		.lock()
		.unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn normalized(name: &str, case_sensitive: bool) -> Option<String> {
	if name.is_empty() || name.contains('\0') {
		return None;
	}
	Some(if case_sensitive {
		name.to_owned()
	} else {
		name.to_ascii_lowercase()
	})
}

/// Registers a nonempty name and rejects duplicate effective names.
/// Multiple distinct names may map to the same numeric ID.
pub fn register(name: &str, id: Id) -> i32 {
	let mut state = lock_registry();
	let Some(key) = normalized(name, state.case_sensitive) else {
		return crate::clm::bpa::FAILURE;
	};
	if state.entries.contains_key(&key) {
		return crate::clm::bpa::FAILURE;
	}
	state.entries.insert(
		key,
		Entry {
			spelling: name.to_owned(),
			id,
		},
	);
	crate::clm::bpa::SET_SUCCESS
}

/// Registers a name; equivalent to [`register`].
pub fn map(name: &str, id: Id) -> i32 {
	register(name, id)
}

/// Resolves a registered name, returning [`NOT_FOUND`] when absent or invalid.
pub fn get(name: &str) -> Id {
	let state = lock_registry();
	let Some(key) = normalized(name, state.case_sensitive) else {
		return NOT_FOUND;
	};
	state.entries.get(&key).map_or(NOT_FOUND, |entry| entry.id)
}

/// Resolves a name with an explicit status so [`NOT_FOUND`] can be stored normally.
pub fn try_get(name: &str) -> Result<Id, i32> {
	let state = lock_registry();
	let key = normalized(name, state.case_sensitive).ok_or(crate::clm::bpa::FAILURE)?;
	state
		.entries
		.get(&key)
		.map(|entry| entry.id)
		.ok_or(crate::clm::bpa::FAILURE)
}

/// Inserts a new mapping or updates the ID for an existing effective name.
/// Updating retains the originally registered spelling.
pub fn set(name: &str, id: Id) -> i32 {
	let mut state = lock_registry();
	let Some(key) = normalized(name, state.case_sensitive) else {
		return crate::clm::bpa::FAILURE;
	};
	if let Some(entry) = state.entries.get_mut(&key) {
		entry.id = id;
		return crate::clm::bpa::SET_SUCCESS;
	}
	state.entries.insert(
		key,
		Entry {
			spelling: name.to_owned(),
			id,
		},
	);
	crate::clm::bpa::SET_SUCCESS
}

/// Removes one mapping by name.
pub fn remove(name: &str) -> i32 {
	let mut state = lock_registry();
	let Some(key) = normalized(name, state.case_sensitive) else {
		return crate::clm::bpa::FAILURE;
	};
	if state.entries.remove(&key).is_some() {
		crate::clm::bpa::SUCCESS
	} else {
		crate::clm::bpa::FAILURE
	}
}

/// Removes every registered name that maps to `id`.
pub fn unmap(id: Id) -> i32 {
	let mut state = lock_registry();
	let previous = state.entries.len();
	state.entries.retain(|_, entry| entry.id != id);
	if state.entries.len() != previous {
		crate::clm::bpa::SUCCESS
	} else {
		crate::clm::bpa::FAILURE
	}
}

/// Removes one mapping by name; equivalent to [`remove`].
pub fn unmap_name(name: &str) -> i32 {
	remove(name)
}

/// Returns whether exact-case name matching is enabled.
pub fn is_case_sensitive() -> bool {
	lock_registry().case_sensitive
}

/// Changes name matching policy, rejecting any case-fold collision atomically.
pub fn set_case_sensitive(enabled: bool) -> i32 {
	let mut state = lock_registry();
	if state.case_sensitive == enabled {
		return crate::clm::bpa::SUCCESS;
	}
	let mut rebuilt = HashMap::with_capacity(state.entries.len());
	for entry in state.entries.values() {
		let Some(key) = normalized(&entry.spelling, enabled) else {
			return crate::clm::bpa::FAILURE;
		};
		if rebuilt
			.insert(
				key,
				Entry {
					spelling: entry.spelling.clone(),
					id: entry.id,
				},
			)
			.is_some()
		{
			return crate::clm::bpa::FAILURE;
		}
	}
	state.entries = rebuilt;
	state.case_sensitive = enabled;
	crate::clm::bpa::SUCCESS
}

/// Removes all mappings while preserving the active matching mode.
pub fn clear() -> i32 {
	lock_registry().entries.clear();
	crate::clm::bpa::SUCCESS
}

/// Clears registry storage; future operations recreate it lazily.
pub fn shutdown() -> i32 {
	clear()
}

/// Resets sequential ID generation so the next call returns zero.
pub fn gen_reset() {
	let mut state = lock_registry();
	state.next_generated = 0;
	state.generator_exhausted = false;
}

/// Returns the next sequential ID, beginning at zero, or [`NOT_FOUND`] on exhaustion.
pub fn gen_next() -> Id {
	let mut state = lock_registry();
	if state.generator_exhausted {
		return NOT_FOUND;
	}
	let value = state.next_generated;
	if value == NOT_FOUND - 1 {
		state.generator_exhausted = true;
	} else {
		state.next_generated += 1;
	}
	value
}