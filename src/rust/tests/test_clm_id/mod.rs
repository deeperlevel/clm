//! Tests for named IDs, case matching, aliases, and sequential generation.

use super::clm::{bpa, id};
use super::{TestResult, TestRunner};

/// Runs the ID registry and generator checks.
pub(super) fn run(runner: &mut TestRunner) {
	let mut result = TestResult::default();
	runner.begin("CLM - ID");

	runner.assert(
		&mut result,
		id::is_case_sensitive() && id::clear() == bpa::SUCCESS,
		"name matching starts case-sensitive and registry can be cleared",
	);
	runner.assert(
		&mut result,
		id::register("host", 1001) == bpa::SET_SUCCESS
			&& id::get("host") == 1001
			&& id::map("mapped-name", 1002) == bpa::SET_SUCCESS
			&& id::register("default_host", 1001) == bpa::SET_SUCCESS,
		"register names and allow aliases for a numeric ID",
	);
	runner.assert(
		&mut result,
		id::register("host", 1002) == bpa::FAILURE
			&& id::get("HOST") == id::NOT_FOUND
			&& id::register("", 1003) == bpa::FAILURE
			&& id::register("bad\0name", 1003) == bpa::FAILURE,
		"reject duplicate, empty, and NUL-containing names",
	);
	runner.assert(
		&mut result,
		id::try_get("host") == Ok(1001)
			&& id::try_get("missing") == Err(bpa::FAILURE),
		"provide status-based lookup",
	);
	runner.assert(
		&mut result,
		id::set("host", 2001) == bpa::SET_SUCCESS
			&& id::get("host") == 2001
			&& id::get("default_host") == 1001
			&& id::set("new_name", 3001) == bpa::SET_SUCCESS,
		"update one mapping and insert another through set",
	);
	runner.assert(
		&mut result,
		id::remove("new_name") == bpa::SUCCESS
			&& id::remove("missing") == bpa::FAILURE
			&& id::get("new_name") == id::NOT_FOUND,
		"remove a single name and report unknown removals",
	);

	runner.assert(
		&mut result,
		id::register("Host", 4001) == bpa::SET_SUCCESS
			&& id::set_case_sensitive(false) == bpa::FAILURE
			&& id::is_case_sensitive()
			&& id::get("host") == 2001,
		"reject case-fold collisions without changing registry state",
	);
	runner.assert(
		&mut result,
		id::remove("Host") == bpa::SUCCESS
			&& id::set_case_sensitive(false) == bpa::SUCCESS
			&& !id::is_case_sensitive()
			&& id::get("HOST") == 2001,
		"enable ASCII case-insensitive lookup",
	);
	runner.assert(
		&mut result,
		id::set("HOST", 2002) == bpa::SET_SUCCESS
			&& id::get("host") == 2002
			&& id::register("Ä", 6001) == bpa::SET_SUCCESS
			&& id::register("ä", 6002) == bpa::SET_SUCCESS
			&& id::get("Ä") == 6001
			&& id::get("ä") == 6002,
		"fold ASCII only and preserve non-ASCII case distinctions",
	);
	runner.assert(
		&mut result,
		id::register("alias", 2002) == bpa::SET_SUCCESS
			&& id::unmap(2002) == bpa::SUCCESS
			&& id::get("host") == id::NOT_FOUND
			&& id::get("alias") == id::NOT_FOUND
			&& id::unmap_name("default_host") == bpa::SUCCESS,
		"remove all aliases by ID or one name",
	);

	id::gen_reset();
	runner.assert(
		&mut result,
		id::gen_next() == 0 && id::gen_next() == 1 && id::gen_next() == 2,
		"generate sequential IDs starting at zero",
	);
	id::gen_reset();
	runner.assert(
		&mut result,
		id::gen_next() == 0,
		"reset sequential ID generation",
	);
	runner.assert(
		&mut result,
		id::clear() == bpa::SUCCESS
			&& id::get("default_host") == id::NOT_FOUND
			&& !id::is_case_sensitive()
			&& id::register("after-clear", 5001) == bpa::SET_SUCCESS
			&& id::get("AFTER-CLEAR") == 5001,
		"clear mappings while preserving matching mode",
	);
	runner.assert(
		&mut result,
		id::shutdown() == bpa::SUCCESS
			&& id::get("after-clear") == id::NOT_FOUND
			&& id::register("after-shutdown", 5002) == bpa::SET_SUCCESS
			&& id::get("after-shutdown") == 5002
			&& id::shutdown() == bpa::SUCCESS
			&& id::set_case_sensitive(true) == bpa::SUCCESS,
		"shutdown and lazily reuse the registry",
	);
	runner.summary(&result);
}