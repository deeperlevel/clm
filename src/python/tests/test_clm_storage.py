"""Grid and atlas storage checks using the CLM test reporting style."""

import struct

from clm import bpa, storage
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def run_tests() -> int:
    """Run storage checks and return the number of failed assertions."""
    result = TestResult()
    clm_test_begin("CLM - Storage")

    grid_id = 100
    slice_id = 101
    bad_slice_id = 102
    status = storage.clm_storage_new_2d(grid_id, bpa.CLM_BPA_TYPE_INT, 0, 4, 3)
    duplicate = storage.clm_storage_new_2d(grid_id, bpa.CLM_BPA_TYPE_INT, 0, 4, 3)
    zero = storage.clm_storage_get_to(grid_id, 0, 2, 3, 4)
    set_value = storage.clm_storage_set_from(grid_id, 0, 1, 2, struct.pack("=i", 72))
    get_value = storage.clm_storage_get_to(grid_id, 0, 1, 2, 4)
    invalid_layer = storage.clm_storage_get_to(grid_id, 1, 1, 2, 4)
    invalid_row = storage.clm_storage_get_to(grid_id, 0, 3, 2, 4)
    invalid_width = storage.clm_storage_set_from(grid_id, 0, 1, 2, b"\x00\x00")
    info_status, info = storage.clm_storage_get_info(grid_id)
    clm_test_assert(
        result,
        status == bpa.CLM_BPA_SUCCESS
        and duplicate == bpa.CLM_BPA_FAILURE
        and zero == (bpa.CLM_BPA_GET_SUCCESS, b"\0" * 4)
        and set_value == bpa.CLM_BPA_SET_SUCCESS
        and get_value == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 72))
        and invalid_layer == (bpa.CLM_BPA_FAILURE, None)
        and invalid_row == (bpa.CLM_BPA_FAILURE, None)
        and invalid_width == bpa.CLM_BPA_FAILURE
        and info_status == bpa.CLM_BPA_GET_SUCCESS
        and info is not None
        and (info.columns, info.rows, info.layers, info.element_size) == (4, 3, 1, 4),
        "create 2D storage, validate coordinates, and read/write BPA values",
    )

    slice_status = storage.clm_storage_slice(grid_id, 0, 1, 1, 1, 2, 2, slice_id)
    slice_value = storage.clm_storage_get_to(slice_id, 0, 0, 1, 4)
    independent = storage.clm_storage_set_from(slice_id, 0, 0, 1, struct.pack("=i", 21))
    source_unchanged = storage.clm_storage_get_to(grid_id, 0, 1, 2, 4)
    invalid_slice = storage.clm_storage_slice(grid_id, 0, 2, 2, 1, 2, 2, bad_slice_id)
    clm_test_assert(
        result,
        slice_status == bpa.CLM_BPA_SUCCESS
        and slice_value == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 72))
        and independent == bpa.CLM_BPA_SET_SUCCESS
        and source_unchanged == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 72))
        and invalid_slice == bpa.CLM_BPA_FAILURE
        and storage.clm_storage_delete(slice_id) == bpa.CLM_BPA_SUCCESS
        and storage.clm_storage_delete(grid_id) == bpa.CLM_BPA_SUCCESS
        and storage.clm_storage_get_info(grid_id) == (bpa.CLM_BPA_FAILURE, None),
        "slice copies a bounded region into independent storage",
    )

    layered_id = 110
    layered_slice_id = 111
    layered_create = storage.clm_storage_new(layered_id, bpa.CLM_BPA_TYPE_BYTE, 0, 2, 3, 2)
    set_layer = storage.clm_storage_set_from(layered_id, 1, 2, 1, b"\x09")
    other_layer = storage.clm_storage_get_to(layered_id, 0, 2, 1, 1)
    layered_slice = storage.clm_storage_slice(layered_id, 1, 1, 0, 1, 2, 2, layered_slice_id)
    slice_cell = storage.clm_storage_get_to(layered_slice_id, 0, 1, 1, 1)
    clm_test_assert(
        result,
        layered_create == bpa.CLM_BPA_SUCCESS
        and set_layer == bpa.CLM_BPA_SET_SUCCESS
        and other_layer == (bpa.CLM_BPA_GET_SUCCESS, b"\0")
        and layered_slice == bpa.CLM_BPA_SUCCESS
        and slice_cell == (bpa.CLM_BPA_GET_SUCCESS, b"\x09")
        and storage.clm_storage_delete(layered_slice_id) == bpa.CLM_BPA_SUCCESS
        and storage.clm_storage_delete(layered_id) == bpa.CLM_BPA_SUCCESS,
        "3D grids use layer-row-column row-major addressing",
    )

    atlas_id = 120
    frame_storage_id = 121
    atlas_create = storage.clm_storage_atlas_new(atlas_id, bpa.CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2)
    frame = bytes((1, 2, 3, 4))
    frame_set = storage.clm_storage_atlas_set_frame_from(atlas_id, 0, 1, 1, frame)
    frame_get = storage.clm_storage_atlas_get_frame_to(atlas_id, 0, 1, 1, 4)
    zero_frame = storage.clm_storage_atlas_get_frame_to(atlas_id, 0, 0, 0, 4)
    bad_frame = storage.clm_storage_atlas_get_frame_to(atlas_id, 0, 2, 0, 4)
    undersized_frame = storage.clm_storage_atlas_get_frame_to(atlas_id, 0, 1, 1, 3)
    frame_slice = storage.clm_storage_atlas_slice_frame(atlas_id, 0, 1, 1, frame_storage_id)
    last_frame_value = storage.clm_storage_get_to(frame_storage_id, 0, 1, 1, 1)
    clm_test_assert(
        result,
        atlas_create == bpa.CLM_BPA_SUCCESS
        and frame_set == bpa.CLM_BPA_SET_SUCCESS
        and frame_get == (bpa.CLM_BPA_GET_SUCCESS, frame)
        and zero_frame == (bpa.CLM_BPA_GET_SUCCESS, b"\0" * 4)
        and bad_frame == (bpa.CLM_BPA_FAILURE, None)
        and undersized_frame == (bpa.CLM_BPA_FAILURE, None)
        and frame_slice == bpa.CLM_BPA_SUCCESS
        and last_frame_value == (bpa.CLM_BPA_GET_SUCCESS, b"\x04")
        and storage.clm_storage_delete(frame_storage_id) == bpa.CLM_BPA_SUCCESS
        and storage.clm_storage_delete(atlas_id) == bpa.CLM_BPA_SUCCESS,
        "atlas frames read, write, and copy into regular 2D storage",
    )

    atlas3d_id = 130
    atlas3d_create = storage.clm_storage_atlas_new_3d(atlas3d_id, bpa.CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2, 2)
    set_3d_frame = storage.clm_storage_atlas_set_frame_from(atlas3d_id, 1, 0, 1, frame)
    get_3d_frame = storage.clm_storage_atlas_get_frame_to(atlas3d_id, 1, 0, 1, 4)
    untouched_layer = storage.clm_storage_get_to(atlas3d_id, 0, 0, 2, 1)
    clm_test_assert(
        result,
        atlas3d_create == bpa.CLM_BPA_SUCCESS
        and set_3d_frame == bpa.CLM_BPA_SET_SUCCESS
        and get_3d_frame == (bpa.CLM_BPA_GET_SUCCESS, frame)
        and untouched_layer == (bpa.CLM_BPA_GET_SUCCESS, b"\0")
        and storage.clm_storage_delete(atlas3d_id) == bpa.CLM_BPA_SUCCESS,
        "layered atlases isolate frame updates by layer",
    )

    fill_id = 140
    block_id = 141
    fill_create = storage.clm_storage_new_2d(fill_id, bpa.CLM_BPA_TYPE_INT, 0, 2, 2)
    fill_status = storage.clm_storage_fill_from(fill_id, struct.pack("=i", 5))
    filled = storage.clm_storage_get_to(fill_id, 0, 1, 1, 4)
    bpa.clm_bpa_create(block_id, bpa.CLM_BPA_TYPE_INT)
    bpa.clm_bpa_set(block_id, struct.pack("=i", 83))
    set_from_block = storage.clm_storage_set(fill_id, 0, 0, 0, block_id)
    get_to_block = storage.clm_storage_get(fill_id, 0, 0, 0, block_id)
    block_value = bpa.clm_bpa_get(block_id, 4)
    clm_test_assert(
        result,
        fill_create == bpa.CLM_BPA_SUCCESS
        and fill_status == bpa.CLM_BPA_SUCCESS
        and filled == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 5))
        and set_from_block == bpa.CLM_BPA_SET_SUCCESS
        and get_to_block == bpa.CLM_BPA_GET_SUCCESS
        and block_value == (bpa.CLM_BPA_GET_SUCCESS, struct.pack("=i", 83))
        and storage.clm_storage_delete(fill_id) == bpa.CLM_BPA_SUCCESS
        and bpa.clm_bpa_delete(block_id) == bpa.CLM_BPA_SUCCESS,
        "fill grids and transfer elements through compatible BPA blocks",
    )

    clm_test_assert(
        result,
        storage.clm_storage_atlas_new(122, bpa.CLM_BPA_TYPE_BYTE, 0, 5, 4, 2, 2) == bpa.CLM_BPA_FAILURE
        and storage.clm_storage_new_2d(123, bpa.CLM_BPA_TYPE_STRING, 0, 2, 2) == bpa.CLM_BPA_FAILURE
        and storage.clm_storage_new_2d(124, bpa.CLM_BPA_TYPE_INT, 0, 0, 2) == bpa.CLM_BPA_FAILURE
        and storage.clm_storage_new(125, bpa.CLM_BPA_TYPE_INT, 0, 0xFFFFFFFF, 0xFFFFFFFF, 2) == bpa.CLM_BPA_FAILURE,
        "reject invalid atlas divisibility, unsupported string grids, and unsafe dimensions",
    )

    return clm_test_summary(result)
