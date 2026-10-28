package tests

import "../clm"

id_tests :: proc() -> i32 {
	r: test_result
	test_begin("CLM - ID")
	S :: clm.CLM_BPA_SUCCESS
	F :: clm.CLM_BPA_FAILURE
	SET :: clm.CLM_BPA_SET_SUCCESS
	GET :: clm.CLM_BPA_GET_SUCCESS
	NF :: clm.CLM_ID_NOT_FOUND
	value := NF
	assert_test(&r, clm.clm_id_is_case_sensitive(), "default case-sensitive")
	assert_test(&r, clm.clm_id_clear() == S, "initialize registry")
	assert_test(&r, clm.clm_id_register("host", 1001) == SET && clm.clm_id_get("host") == 1001, "register name")
	assert_test(&r, clm.clm_id_map("mapped-name", 1002) == SET && clm.clm_id_get("mapped-name") == 1002, "map alias")
	assert_test(&r, clm.clm_id_register("default_host", 1001) == SET && clm.clm_id_get("default_host") == 1001, "numeric aliases")
	assert_test(&r, clm.clm_id_register("host", 1002) == F && clm.clm_id_get("host") == 1001, "duplicate does not overwrite")
	for malformed in ([8]string{"\xc0\xaf", "\x80", "\xc2", "\xe0\x80\x80", "\xed\xa0\x80", "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80"}) {
		assert_test(&r, clm.clm_id_register(malformed, 1003) == F, "reject malformed UTF-8")
	}
	assert_test(&r, clm.clm_id_register("", 1) == F && clm.clm_id_get("") == NF && clm.clm_id_remove("") == F, "reject empty names")
	assert_test(&r, clm.clm_id_get("HOST") == NF, "case distinction")
	assert_test(&r, clm.clm_id_try_get("host", &value) == GET && value == 1001 &&
		clm.clm_id_try_get("missing", &value) == F && value == 1001 &&
		clm.clm_id_try_get("host", nil) == F, "status lookup preserves output on failure")
	assert_test(&r, clm.clm_id_set("host", 2001) == SET && clm.clm_id_get("host") == 2001 &&
		clm.clm_id_get("default_host") == 1001, "update leaves aliases unchanged")
	assert_test(&r, clm.clm_id_set("new_name", 3001) == SET && clm.clm_id_get("new_name") == 3001, "set inserts")
	assert_test(&r, clm.clm_id_remove("new_name") == S && clm.clm_id_get("new_name") == NF, "remove")
	assert_test(&r, clm.clm_id_remove("missing") == F, "remove unknown")
	assert_test(&r, clm.clm_id_register("Host", 4001) == SET && clm.clm_id_set_case_sensitive(false) == F &&
		clm.clm_id_is_case_sensitive() && clm.clm_id_get("Host") == 4001 &&
		clm.clm_id_get("host") == 2001, "case collision rollback")
	assert_test(&r, clm.clm_id_remove("Host") == S && clm.clm_id_set_case_sensitive(false) == S &&
		!clm.clm_id_is_case_sensitive() && clm.clm_id_get("HOST") == 2001, "enable ASCII folding")
	assert_test(&r, clm.clm_id_set("HOST", 2002) == SET && clm.clm_id_get("host") == 2002, "folded update")
	assert_test(&r, clm.clm_id_set_case_sensitive(true) == S && clm.clm_id_get("HOST") == NF &&
		clm.clm_id_get("host") == 2002 && clm.clm_id_set_case_sensitive(false) == S, "retain registered spelling")
	assert_test(&r, clm.clm_id_register("Ä", 6001) == SET && clm.clm_id_register("ä", 6002) == SET &&
		clm.clm_id_get("Ä") == 6001 && clm.clm_id_get("ä") == 6002, "non-ASCII case unchanged")
	assert_test(&r, clm.clm_id_register("alias", 2002) == SET && clm.clm_id_unmap(2002) == S &&
		clm.clm_id_get("host") == NF && clm.clm_id_get("alias") == NF && clm.clm_id_unmap(2002) == F, "unmap all aliases")
	assert_test(&r, clm.clm_id_unmap_name("default_host") == S && clm.clm_id_get("default_host") == NF, "unmap one name")
	assert_test(&r, clm.clm_id_register("sentinel", NF) == SET &&
		clm.clm_id_try_get("sentinel", &value) == GET && value == NF, "sentinel is a valid mapped ID")
	assert_test(&r, clm.clm_id_internal_hash_name("costarring", false) == clm.clm_id_internal_hash_name("liquid", false) &&
		clm.clm_id_register("costarring", 8001) == SET && clm.clm_id_register("liquid", 8002) == SET &&
		clm.clm_id_get("costarring") == 8001 && clm.clm_id_get("liquid") == 8002, "resolve actual hash collisions")
	assert_test(&r, clm.clm_id_remove("costarring") == S && clm.clm_id_get("liquid") == 8002 &&
		clm.clm_id_register("costarring", 8003) == SET && clm.clm_id_get("liquid") == 8002 &&
		clm.clm_id_get("costarring") == 8003, "probe through and reuse tombstones")
	clm.clm_id_next_entry_value = NF - 1
	assert_test(&r, clm.clm_id_register("last-entry", 8101) == SET &&
		clm.clm_id_register("wrapped-entry", 8102) == SET &&
		clm.clm_id_get("last-entry") == 8101 && clm.clm_id_get("wrapped-entry") == 8102, "internal entry allocation wraps without collision")
	assert_test(&r, clm.clm_id_register("nul-name\x00ignored", 8201) == SET &&
		clm.clm_id_get("nul-name") == 8201, "C NUL-terminated name semantics")
	assert_test(&r, clm.clm_id_gen_next() == 0 && clm.clm_id_gen_next() == 1 && clm.clm_id_gen_next() == 2, "sequential generator")
	clm.clm_id_gen_reset()
	assert_test(&r, clm.clm_id_gen_next() == 0, "reset generator")
	clm.clm_id_generator_value = NF - 2
	assert_test(&r, clm.clm_id_gen_next() == NF - 2 && clm.clm_id_gen_next() == NF - 1 &&
		clm.clm_id_gen_next() == NF && clm.clm_id_gen_next() == NF && clm.clm_id_generator_exhausted, "generator exhaustion never wraps")
	clm.clm_id_gen_reset()
	assert_test(&r, clm.clm_id_gen_next() == 0, "reset exhausted generator")
	assert_test(&r, clm.clm_id_clear() == S && clm.clm_id_get("mapped-name") == NF &&
		!clm.clm_id_is_case_sensitive(), "clear preserves matching policy")
	assert_test(&r, clm.clm_id_register("after-clear", 5001) == SET && clm.clm_id_get("AFTER-CLEAR") == 5001, "reuse cleared registry")
	assert_test(&r, clm.clm_id_shutdown() == S && !clm.clm_bpa_is_existing(clm.ID_STORAGE_ID) &&
		!clm.clm_bpa_is_existing(clm.CLM_ID_NAME_BUFFER_ID) &&
		clm.clm_id_gen_next() == 1 && !clm.clm_id_is_case_sensitive(), "shutdown releases resources and retains generator/mode")
	assert_test(&r, clm.clm_id_get("after-clear") == NF && clm.clm_id_register("after-shutdown", 5002) == SET &&
		clm.clm_id_get("after-shutdown") == 5002 && clm.clm_id_shutdown() == S, "lazy recreation")
	assert_test(&r, clm.clm_bpa_create(clm.ID_STORAGE_ID, clm.CLM_BPA_TYPE_INT) == S &&
		clm.clm_id_register("blocked", 1) == F &&
		clm.clm_bpa_delete(clm.ID_STORAGE_ID) == S, "do not overwrite conflicting BPA resources")
	assert_test(&r, clm.clm_id_set_case_sensitive(true) == S, "restore default mode")
	assert_test(&r, clm.clm_id_shutdown() == S, "final registry cleanup")
	clm.clm_id_gen_reset()
	test_summary_counts(r.passed, r.failed)
	return 0 if r.failed == 0 else 1
}
