import random
import unittest
from clm_storage import *


class StorageTests(unittest.TestCase):
    def setUp(self):
        for sid in list(range(100, 300)):
            clm_storage_destroy(sid)

    def tearDown(self):
        for sid in list(range(100, 300)):
            clm_storage_destroy(sid)

    def test_create_and_capacity(self):
        self.assertEqual(clm_storage_create(100, STORAGE_TYPE_INTEGER, reserved=2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_get_info(100)[1].reserved, 2)
        for x in range(5): self.assertEqual(clm_storage_append(100, x), STORAGE_SUCCESS)
        info = clm_storage_get_info(100)[1]
        self.assertEqual((info.committed, info.references, info.index), (5, 1, -1))
        self.assertGreaterEqual(info.reserved, 5)
        self.assertEqual(clm_storage_create(100, STORAGE_TYPE_INTEGER), STORAGE_ERROR_DUPLICATE_ID)
        self.assertEqual(clm_storage_create(101, 0xEF), STORAGE_ERROR_TYPE_NOT_SUPPORTED)

    def test_types(self):
        for sid, t, good, bad in [(100, STORAGE_TYPE_BYTE, 127, 128),
                                  (101, STORAGE_TYPE_SHORT, -32768, 32768),
                                  (102, STORAGE_TYPE_INTEGER, 12, True),
                                  (103, STORAGE_TYPE_LONG_INTEGER, 2**50, 2**63),
                                  (104, STORAGE_TYPE_FLOAT, 1.25, 1e100),
                                  (105, STORAGE_TYPE_DOUBLE, 1.25, 3),
                                  (106, STORAGE_TYPE_STRING, 'abc', 'abcd'),
                                  (107, STORAGE_TYPE_DYNAMIC, b'abcd', b'ab'),
                                  (108, STORAGE_TYPE_OBJECT, {'a': 1}, None)]:
            size = 3 if t == STORAGE_TYPE_STRING else (4 if t == STORAGE_TYPE_DYNAMIC else None)
            self.assertEqual(clm_storage_create(sid, t, size=size), STORAGE_SUCCESS)
            self.assertEqual(clm_storage_append(sid, good), STORAGE_SUCCESS)
            if t != STORAGE_TYPE_OBJECT:
                self.assertEqual(clm_storage_append(sid, bad), STORAGE_ERROR_INVALID_VALUE)
        self.assertEqual(clm_storage_create(109, STORAGE_TYPE_STRING, size=1), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_append(109, 'é'), STORAGE_ERROR_INVALID_VALUE)

    def test_navigation(self):
        clm_storage_create(100, STORAGE_TYPE_INTEGER)
        for x in [10, 20, 30]: clm_storage_append(100, x)
        self.assertEqual(clm_storage_get(100)[0], STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_move_first(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_get(100), (STORAGE_SUCCESS, 10))
        self.assertEqual(clm_storage_move_next(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_get_index(100), (STORAGE_SUCCESS, 1))
        clm_storage_insert(100, 0, 5)
        self.assertEqual(clm_storage_get_index(100)[1], 1) # numeric cursor unchanged
        self.assertEqual(clm_storage_get(100)[1], 10)
        self.assertEqual(clm_storage_move_last(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_move_next(100), STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_get(100)[1], 30)
        clm_storage_remove(100, 3)
        self.assertEqual(clm_storage_get_index(100)[1], 2)
        self.assertEqual(clm_storage_set_index(100, 99), STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_is_last(100), (STORAGE_SUCCESS, True))

    def test_mutation_copy_bulk(self):
        clm_storage_create(100, STORAGE_TYPE_INTEGER)
        clm_storage_add_sampling(100, 4, 3)
        clm_storage_insert(100, 1, 8)
        self.assertEqual([clm_storage_get(100, i)[1] for i in range(4)], [4,8,4,4])
        self.assertEqual(clm_storage_replace_all(100, 4, 9), (STORAGE_SUCCESS,3))
        self.assertEqual(clm_storage_copy(100, 0, 1, 3), STORAGE_SUCCESS)
        self.assertEqual([clm_storage_get(100, i)[1] for i in range(4)], [9,9,8,9])
        self.assertEqual(clm_storage_delete(100, 9), (STORAGE_SUCCESS, 3))
        self.assertEqual(clm_storage_get(100, 0), (STORAGE_SUCCESS, 8))
        self.assertEqual(clm_storage_resize(100, 3), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_get(100, 2), (STORAGE_SUCCESS, 0))
        self.assertEqual(clm_storage_swap(100, 0, 2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_reverse(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_sort_desc(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_get(100, 0)[1], 8)

    def test_transfer_filter_and_shallow_object(self):
        for i in (100,101,102): clm_storage_create(i, STORAGE_TYPE_OBJECT)
        obj = {'data':[1]}
        clm_storage_append(100, obj)
        clm_storage_append(100, {'data':[2]})
        self.assertEqual(clm_storage_clone(100, 101), STORAGE_SUCCESS)
        self.assertIs(clm_storage_get(101,0)[1], obj)
        self.assertEqual(clm_storage_slice(100, 102, 1, 1), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_count(102), (STORAGE_SUCCESS,1))
        self.assertEqual(clm_storage_append_clone(100,102), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_count(102), (STORAGE_SUCCESS,3))
        self.assertEqual(clm_storage_filter(100, lambda x: x['data'][0] > 1, 101), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_count(101), (STORAGE_SUCCESS,1))
        self.assertEqual(clm_storage_sort(100), STORAGE_ERROR_INVALID_VALUE)
        self.assertEqual(clm_storage_sort(100, lambda a,b: a['data'][0]-b['data'][0]), STORAGE_SUCCESS)

    def test_lock_refs_disposal(self):
        clm_storage_create(100, STORAGE_TYPE_BYTE)
        self.assertEqual(clm_storage_lock(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_append(100,3), STORAGE_ERROR_LOCKED)
        self.assertEqual(clm_storage_unlock(100), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_append(100,3), STORAGE_SUCCESS)
        clm_storage_reference(100)
        clm_storage_unreference(100)
        self.assertEqual(clm_storage_dispose(), (STORAGE_SUCCESS,0))
        clm_storage_unreference(100)
        self.assertEqual(clm_storage_unreference(100), STORAGE_ERROR_REFERENCES)
        self.assertEqual(clm_storage_get(100,0), (STORAGE_SUCCESS,3))
        self.assertEqual(clm_storage_dispose(), (STORAGE_SUCCESS,1))
        self.assertEqual(clm_storage_get(100,0)[0], STORAGE_ERROR_NOT_FOUND)

    def test_draw_spend(self):
        clm_storage_create(100, STORAGE_TYPE_INTEGER)
        for i in range(52): clm_storage_append(100, i)
        drawn = [clm_storage_draw(100, random.Random(i))[1] for i in range(52)]
        self.assertEqual(set(drawn), set(range(52)))
        self.assertEqual(clm_storage_draw(100)[0], STORAGE_ERROR_EMPTY)
        self.assertEqual(clm_storage_get_info(100)[1].spent, 52)
        clm_storage_reset_spent(100)
        self.assertEqual(clm_storage_get_info(100)[1].spent, 0)
        clm_storage_shuffle(100, random.Random(1))
        self.assertEqual(clm_storage_spend(100, 0)[0], STORAGE_SUCCESS)

    def test_blocks_and_grids(self):
        self.assertEqual(clm_storage_block_create(100, 3, 2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_block_set(100, 2, 1, 12), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_block_get(100, 2, 1), (STORAGE_SUCCESS,12))
        self.assertEqual(clm_storage_block_get(100, 3, 1)[0], STORAGE_ERROR_INVALID_INDEX)
        self.assertEqual(clm_storage_block3d_create(101, 2, 2, 3), STORAGE_SUCCESS)
        clm_storage_block3d_set(101, 1, 1, 2, 99)
        self.assertEqual(clm_storage_block3d_get(101,1,1,2)[1],99)
        self.assertEqual(clm_storage_block_create(102, 0, 2), STORAGE_ERROR_DIMENSION)
        self.assertEqual(clm_storage_append(100,3), STORAGE_ERROR_INCOMPATIBLE)

    def test_atlas_transfer_and_overflow(self):
        clm_storage_atlas_create(100, 2, 2, 2, 3)
        clm_storage_block_create(101, 2, 2)
        clm_storage_block_set(101,1,1,77)
        self.assertEqual(clm_storage_atlas_set(100,2,1,101), STORAGE_SUCCESS)
        clm_storage_block_create(102,2,2)
        self.assertEqual(clm_storage_atlas_get(100,2,1,102), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_block_get(102,1,1)[1],77)
        clm_storage_block_create(103,1,1)
        self.assertEqual(clm_storage_atlas_get(100,2,1,103), STORAGE_ERROR_OVERFLOW)
        self.assertEqual(clm_storage_atlas_get(100,2,1,103,STORAGE_ATLAS_OVERFLOW_IGNORE), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas_get(100,2,1,103,STORAGE_ATLAS_OVERFLOW_EXTEND), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_get_info(103)[1].width,2)
        self.assertEqual(clm_storage_atlas_resize(100,3,3), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas_get(100,2,1,102), STORAGE_ERROR_OVERFLOW)
        self.assertEqual(clm_storage_atlas_get(100,2,1,102,STORAGE_ATLAS_OVERFLOW_EXTEND), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_block_get(102,1,1)[1],77)
        self.assertEqual(clm_storage_atlas_redefine(100,1,3),STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas_get(100,2,1,102),STORAGE_ERROR_INVALID_INDEX)

    def test_atlas3d_and_slice(self):
        self.assertEqual(clm_storage_atlas3d_create(100,2,2,3,2,2,2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_grid_set(100,1,0,91,z=2,column=1,row=0,layer=1), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas_create(101,2,2,2,2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_slice(100,101,layer=1,depth=2), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_grid_get(101,1,0,column=1,row=0), (STORAGE_SUCCESS,91))
        clm_storage_block3d_create(102,2,2,3)
        self.assertEqual(clm_storage_atlas3d_get(100,1,0,1,102), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_block3d_get(102,1,0,2)[1],91)
        self.assertEqual(clm_storage_atlas3d_redefine(100,2,2,1), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_slice(100,101,layer=1), STORAGE_ERROR_INVALID_INDEX)

    def test_clone_and_invalid_operations(self):
        clm_storage_atlas_create(100,2,2,1,1)
        clm_storage_atlas_create(101,1,1,2,2)
        clm_storage_grid_set(100,1,1,42)
        self.assertEqual(clm_storage_atlas_clone(100,101), STORAGE_SUCCESS)
        self.assertEqual(clm_storage_grid_get(101,1,1)[1],42)
        self.assertEqual(clm_storage_atlas_get(100,0,0,101), STORAGE_ERROR_INCOMPATIBLE)
        self.assertEqual(clm_storage_set_index(100,999), STORAGE_ERROR_INVALID_INDEX)


if __name__ == '__main__':
    unittest.main()
