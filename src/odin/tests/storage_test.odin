package tests

import "../clm"

storage_tests :: proc() -> i32 {
	result: test_result
	test_begin("CLM Storage")
	defer clm.clm_bpa_shutdown()
	v: i32
	w: i32 = 72
	assert_test(&result, clm.clm_storage_new_2d(100, clm.CLM_BPA_TYPE_INT, 0, 4, 3) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_new_2d(100, clm.CLM_BPA_TYPE_INT, 0, 4, 3) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_get_to(100, 0, 2, 3, &v, size_of(v)) == clm.CLM_BPA_GET_SUCCESS && v == 0)
	assert_test(&result, clm.clm_storage_set_from(100, 0, 1, 2, &w, size_of(w)) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(100, 0, 1, 2, &v, size_of(v)) == clm.CLM_BPA_GET_SUCCESS && v == 72)
	assert_test(&result, clm.clm_storage_get_to(100, 1, 1, 2, &v, size_of(v)) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_get_to(100, 0, 3, 2, &v, size_of(v)) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_set_from(100, 0, 1, 2, &w, 2) == clm.CLM_BPA_FAILURE)
	info: clm.clm_storage_info
	assert_test(&result, clm.clm_storage_get_info(100, &info) == clm.CLM_BPA_GET_SUCCESS && info.columns == 4 && info.rows == 3 && info.layers == 1)
	assert_test(&result, clm.clm_storage_slice(100, 0, 1, 1, 1, 2, 2, 101) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(101, 0, 0, 1, &v, size_of(v)) == clm.CLM_BPA_GET_SUCCESS && v == 72)
	w = 21
	assert_test(&result, clm.clm_storage_set_from(101, 0, 0, 1, &w, size_of(w)) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(100, 0, 1, 2, &v, size_of(v)) == clm.CLM_BPA_GET_SUCCESS && v == 72)
	assert_test(&result, clm.clm_storage_slice(100, 0, 2, 2, 1, 2, 2, 102) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_delete(101) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_delete(100) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_get_info(100, &info) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_new(110, clm.CLM_BPA_TYPE_BYTE, 0, 2, 3, 2) == clm.CLM_BPA_SUCCESS)
	x: u8 = 9
	r: u8
	assert_test(&result, clm.clm_storage_set_from(110, 1, 2, 1, &x, 1) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(110, 0, 2, 1, &r, 1) == clm.CLM_BPA_GET_SUCCESS && r == 0)
	assert_test(&result, clm.clm_storage_slice(110, 1, 1, 0, 1, 2, 2, 111) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(111, 0, 1, 1, &r, 1) == clm.CLM_BPA_GET_SUCCESS && r == 9)
	assert_test(&result, clm.clm_storage_delete(111) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_delete(110) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_atlas_new(120, clm.CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2) == clm.CLM_BPA_SUCCESS)
	frame := [4]u8{1, 2, 3, 4}
	read: [4]u8
	assert_test(&result, clm.clm_storage_atlas_set_frame_from(120, 0, 1, 1, &frame, 4) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_atlas_get_frame_to(120, 0, 1, 1, &read, 4) == clm.CLM_BPA_GET_SUCCESS && frame == read)
	assert_test(&result, clm.clm_storage_atlas_get_frame_to(120, 0, 0, 0, &read, 4) == clm.CLM_BPA_GET_SUCCESS && read[0] == 0 && read[3] == 0)
	assert_test(&result, clm.clm_storage_atlas_get_frame_to(120, 0, 2, 0, &read, 4) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_atlas_get_frame_to(120, 0, 1, 1, &read, 3) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_atlas_slice_frame(120, 0, 1, 1, 121) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(121, 0, 1, 1, &r, 1) == clm.CLM_BPA_GET_SUCCESS && r == 4)
	assert_test(&result, clm.clm_storage_delete(121) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_delete(120) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_atlas_new(122, clm.CLM_BPA_TYPE_BYTE, 0, 5, 4, 2, 2) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_new_2d(123, clm.CLM_BPA_TYPE_STRING, 0, 2, 2) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_new_2d(123, clm.CLM_BPA_TYPE_INT, 0, 0, 2) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_new(123, clm.CLM_BPA_TYPE_INT, 0, max(u32), max(u32), 2) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_atlas_new_3d(130, clm.CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2, 2) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_atlas_set_frame_from(130, 1, 0, 1, &frame, 4) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_atlas_get_frame_to(130, 1, 0, 1, &read, 4) == clm.CLM_BPA_GET_SUCCESS && frame == read)
	assert_test(&result, clm.clm_storage_get_to(130, 0, 0, 2, &r, 1) == clm.CLM_BPA_GET_SUCCESS && r == 0)
	assert_test(&result, clm.clm_storage_delete(130) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_new_2d(140, clm.CLM_BPA_TYPE_INT, 0, 2, 2) == clm.CLM_BPA_SUCCESS)
	v = 5
	assert_test(&result, clm.clm_storage_fill_from(140, &v, size_of(v)) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(140, 0, 1, 1, &w, size_of(w)) == clm.CLM_BPA_GET_SUCCESS && w == 5)
	assert_test(&result, clm.clm_bpa_create(141, clm.CLM_BPA_TYPE_INT) == clm.CLM_BPA_SUCCESS)
	v = 83
	assert_test(&result, clm.clm_bpa_set(141, &v, size_of(v)) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_set(140, 0, 0, 0, 141) == clm.CLM_BPA_SET_SUCCESS)
	assert_test(&result, clm.clm_storage_get(140, 0, 0, 0, 141) == clm.CLM_BPA_GET_SUCCESS)
	assert_test(&result, clm.clm_bpa_get(141, &w, size_of(w)) == clm.CLM_BPA_GET_SUCCESS && w == 83)
	assert_test(&result, clm.clm_bpa_create(142, clm.CLM_BPA_TYPE_BYTE) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_set(140, 0, 0, 0, 142) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_get(140, 0, 0, 0, 142) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_fill_from(140, nil, size_of(v)) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_fill_from(140, &v, 1) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_bpa_delete(142) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_bpa_delete(141) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_delete(140) == clm.CLM_BPA_SUCCESS)

	// Struct payloads retain exact widths and deep-copy semantics.
	pair :: struct { a, b: i32 }
	original := pair{7, 11}
	actual: pair
	assert_test(&result, clm.clm_storage_new_2d(150, clm.CLM_BPA_TYPE_STRUCT, size_of(pair), 2, 2) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_fill_from(150, &original, size_of(original)) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_get_to(150, 0, 1, 1, &actual, size_of(actual)) == clm.CLM_BPA_GET_SUCCESS && actual == original)
	assert_test(&result, clm.clm_storage_get_to(150, 0, 1, 1, nil, size_of(actual)) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_get_info(150, nil) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_delete(150) == clm.CLM_BPA_SUCCESS)
	assert_test(&result, clm.clm_storage_delete(150) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_new_2d(151, clm.CLM_BPA_TYPE_STRUCT, 0, 2, 2) == clm.CLM_BPA_FAILURE)
	assert_test(&result, clm.clm_storage_new_2d(151, clm.CLM_BPA_TYPE_INT, 1, 2, 2) == clm.CLM_BPA_FAILURE)
	test_summary_counts(result.passed, result.failed)
	return result.failed
}
