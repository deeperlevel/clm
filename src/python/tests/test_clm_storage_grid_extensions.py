"""Regression tests for CLM multidimensional cursor and slice operations."""
import unittest
from clm_storage import *


class GridExtensionTests(unittest.TestCase):
    def tearDown(self):
        for sid in range(8000, 8010):
            clm_storage_destroy(sid)

    def test_atlas3d_position_and_traversal(self):
        self.assertEqual(clm_storage_atlas3d_create(8000, 2, 2, 2, 2, 2, 2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_ERROR_INVALID_INDEX, None))
        coords = (1, 0, 1, 1, 0, 1)
        self.assertEqual(clm_storage_atlas3d_set_position(8000, *coords), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_SUCCESS, coords))
        self.assertEqual(clm_storage_grid_get_position(8000), (STORAGE_SUCCESS, coords))
        self.assertEqual(clm_storage_grid_set(8000, 1, 0, 77, z=1, column=1, row=0, layer=1), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_grid_get_current(8000), (STORAGE_SUCCESS, 77))
        self.assertEqual(clm_storage_atlas3d_set_position(8000, 99, 0, 0, 0, 0, 0), STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_SUCCESS, coords))
        self.assertEqual(clm_storage_atlas3d_move_first(8000), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_SUCCESS, (0, 0, 0, 0, 0, 0)))
        self.assertEqual(clm_storage_atlas3d_move_next(8000), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_SUCCESS, (1, 0, 0, 0, 0, 0)))
        self.assertEqual(clm_storage_atlas3d_move_last(8000), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_SUCCESS, (1, 1, 1, 1, 1, 1)))
        self.assertEqual(clm_storage_atlas3d_move_next(8000), STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_atlas3d_get_position(8000), (STORAGE_SUCCESS, (1, 1, 1, 1, 1, 1)))
        self.assertEqual(clm_storage_atlas3d_move_previous(8000), STORAGE_SUCCESS)

    def test_other_grid_kinds_and_wrong_kind(self):
        clm_storage_block_create(8000, 2, 2)
        clm_storage_block3d_create(8001, 2, 2, 2)
        clm_storage_atlas_create(8002, 2, 2, 2, 2)
        for sid, setter, getter, coords in (
            (8000, clm_storage_block_set_position, clm_storage_block_get_position, (1, 1)),
            (8001, clm_storage_block3d_set_position, clm_storage_block3d_get_position, (1, 1, 1)),
            (8002, clm_storage_atlas_set_position, clm_storage_atlas_get_position, (1, 1, 1, 1)),
        ):
            self.assertEqual(setter(sid, *coords), STORAGE_SUCCESS)
            self.assertEqual(getter(sid), (STORAGE_SUCCESS, coords))
        self.assertEqual(clm_storage_atlas3d_move_first(8000), STORAGE_ERROR_INCOMPATIBLE)
        self.assertEqual(clm_storage_grid_get_position(888888), (STORAGE_ERROR_NOT_FOUND, None))

    def test_reverse_slice_preserves_other_planes(self):
        clm_storage_atlas_create(8000, 2, 2, 1, 1)
        clm_storage_atlas3d_create(8001, 2, 2, 3, 1, 1, 2)
        clm_storage_grid_set(8000, 1, 0, 89)
        clm_storage_grid_set(8001, 1, 0, 12, z=0, layer=1)
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, layer=1, depth=2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_grid_get(8001, 1, 0, z=2, layer=1), (STORAGE_SUCCESS, 89))
        self.assertEqual(clm_storage_grid_get(8001, 1, 0, z=0, layer=1), (STORAGE_SUCCESS, 12))
        self.assertEqual(clm_storage_grid_get(8001, 1, 0, z=2, layer=0), (STORAGE_SUCCESS, 0))
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, layer=2), STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, layer=1, depth=3), STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, layer=1, depth=2, overflow=9), STORAGE_ERROR_INVALID_VALUE)

    def test_reverse_slice_overflow(self):
        clm_storage_atlas_create(8000, 3, 2, 2, 2)
        clm_storage_atlas3d_create(8001, 1, 1, 2, 1, 1, 1)
        clm_storage_grid_set(8000, 2, 1, 42, column=1, row=1)
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, 0), STORAGE_ERROR_OVERFLOW)
        self.assertEqual(clm_storage_get_info(8001)[1].width, 1)
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, 0, overflow=STORAGE_ATLAS_OVERFLOW_IGNORE), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_set_slice(8000, 8001, 0, overflow=STORAGE_ATLAS_OVERFLOW_EXTEND), STORAGE_SUCCESS)
        info = clm_storage_get_info(8001)[1]
        self.assertEqual((info.width, info.height, info.depth, info.rows, info.columns, info.layers), (3, 2, 2, 2, 2, 1))
        self.assertEqual(clm_storage_grid_get(8001, 2, 1, column=1, row=1), (STORAGE_SUCCESS, 42))
        clm_storage_atlas_create(8002, 1, 1, 1, 1, storage_type=STORAGE_TYPE_INTEGER)
        self.assertEqual(clm_storage_atlas3d_set_slice(8002, 8001, 0), STORAGE_ERROR_TYPE_NOT_MATCHED)


if __name__ == '__main__':
    unittest.main()
