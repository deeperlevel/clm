"""Extended CLM Container regression tests: python -m unittest -v test_clm_container_extended"""
import unittest
import clm_container as c


class ExtendedContainerTests(unittest.TestCase):
    def setUp(self):
        self.ids = []

    def tearDown(self):
        for cid in self.ids:
            c.clm_container_destroy(cid)

    def new(self, cid, kind=c.CONTAINER_TYPE_INTEGER):
        self.assertEqual(c.clm_container_create(cid, kind), c.CONTAINER_SUCCESS)
        self.ids.append(cid)

    def test_metadata_snapshot_and_initial_reference(self):
        self.new(850001)
        result, info = c.clm_container_get_info(850001)
        self.assertEqual(result, c.CONTAINER_SUCCESS)
        self.assertEqual((info.id, info.type, info.committed, info.reserved, info.references, info.index),
                         (850001, c.CONTAINER_TYPE_INTEGER, 0, -1, 1, -1))
        self.assertEqual(c.clm_container_add_integer(850001, 21, 99), c.CONTAINER_SUCCESS)
        self.assertEqual(info.committed, 0)  # snapshots are detached
        self.assertEqual(c.clm_container_get_info(850001)[1].committed, 1)
        with self.assertRaises(Exception):
            info.references = 999

    def test_cursor_position_and_key_independence(self):
        self.new(850002)
        for key in (900, 12, 500):
            c.clm_container_add_integer(850002, key, key + 1)
        self.assertEqual(c.clm_container_get_index(850002), (c.CONTAINER_SUCCESS, -1))
        self.assertEqual(c.clm_container_iterate(850002), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_get_index(850002), (c.CONTAINER_SUCCESS, 0))
        self.assertEqual(c.clm_container_get_key(850002), (c.CONTAINER_SUCCESS, 900))
        self.assertEqual(c.clm_container_get(850002), (c.CONTAINER_SUCCESS, 901))
        self.assertEqual(c.clm_container_is_first(850002), (c.CONTAINER_SUCCESS, True))
        self.assertEqual(c.clm_container_set_index(850002, 2), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_get_key(850002), (c.CONTAINER_SUCCESS, 500))
        self.assertEqual(c.clm_container_is_last(850002), (c.CONTAINER_SUCCESS, True))
        self.assertEqual(c.clm_container_move_next(850002), c.CONTAINER_ERROR_ID_NOT_FOUND)
        self.assertEqual(c.clm_container_get_index(850002), (c.CONTAINER_SUCCESS, 2))
        self.assertEqual(c.clm_container_iterate_backward(850002), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_move_previous(850002), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_get_key(850002), (c.CONTAINER_SUCCESS, 12))
        self.assertEqual(c.clm_container_get_count(850002), (c.CONTAINER_SUCCESS, 3))

    def test_position_bounds_and_empty(self):
        self.new(850003)
        self.assertEqual(c.clm_container_iterate(850003), c.CONTAINER_ERROR_ID_NOT_FOUND)
        self.assertEqual(c.clm_container_get(850003), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.clm_container_is_first(850003), (c.CONTAINER_SUCCESS, False))
        self.assertEqual(c.clm_container_set_index(850003, 0), c.CONTAINER_ERROR_INVALID_VALUE)
        c.clm_container_add_integer(850003, 1, 1)
        self.assertEqual(c.clm_container_set_index(850003, True), c.CONTAINER_ERROR_INVALID_VALUE)
        self.assertEqual(c.clm_container_set_index(850003, -1), c.CONTAINER_ERROR_INVALID_VALUE)
        self.assertEqual(c.clm_container_set_index(850003, 1), c.CONTAINER_ERROR_INVALID_VALUE)
        self.assertEqual(c.clm_container_set_index(9999999, 0), c.CONTAINER_ERROR_NOT_FOUND)
        self.assertEqual(c.clm_container_get_info(9999999), (c.CONTAINER_ERROR_NOT_FOUND, None))

    def test_slice_retains_keys_order_and_source_and_replaces_destination(self):
        self.new(850004, c.CONTAINER_TYPE_OBJECT)
        self.new(850005, c.CONTAINER_TYPE_OBJECT)
        source_object = {'mutable': True}
        for key, val in ((80, 'a'), (2, source_object), (99, 'c'), (7, None)):
            c.clm_container_add_object(850004, key, val)
        c.clm_container_add_object(850005, 555, 'old')
        c.clm_container_set_index(850004, 1)
        c.clm_container_set_index(850005, 0)
        self.assertEqual(c.clm_container_slice(850004, 850005, 1, 2), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_get_count(850005), (c.CONTAINER_SUCCESS, 2))
        self.assertEqual(c.clm_container_first(850005), (c.CONTAINER_SUCCESS, 2))
        self.assertIs(c.clm_container_get_object(850005, 2)[1], source_object)
        self.assertEqual(c.clm_container_next(850005), (c.CONTAINER_SUCCESS, 99))
        self.assertEqual(c.clm_container_get_object(850005, 555), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.clm_container_get_index(850004), (c.CONTAINER_SUCCESS, 1))
        self.assertEqual(c.clm_container_count(850004), (c.CONTAINER_SUCCESS, 4))
        self.assertEqual(c.clm_container_get_info(850005)[1].references, 1)
        self.assertEqual(c.clm_container_slice(850004, 850005, 4, 0), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_count(850005), (c.CONTAINER_SUCCESS, 0))

    def test_slice_failure_is_non_destructive(self):
        self.new(850006)
        self.new(850007)
        self.new(850008, c.CONTAINER_TYPE_STRING)
        c.clm_container_add_integer(850006, 10, 42)
        c.clm_container_add_integer(850007, 20, 99)
        for invalid in [(-1, 1), (0, 2), (2, 0), (0, -1), (True, 1)]:
            self.assertEqual(c.clm_container_slice(850006, 850007, *invalid), c.CONTAINER_ERROR_INVALID_VALUE)
            self.assertEqual(c.clm_container_get_integer(850007, 20), (c.CONTAINER_SUCCESS, 99))
        self.assertEqual(c.clm_container_slice(850006, 850008, 0, 1), c.CONTAINER_ERROR_TYPE_NOT_MATCHED)
        self.assertEqual(c.clm_container_slice(850006, 850006, 0, 1), c.CONTAINER_ERROR_INVALID_VALUE)
        self.assertEqual(c.clm_container_slice(850006, 9999999, 0, 1), c.CONTAINER_ERROR_NOT_FOUND)

    def test_reference_disposal_and_reuse(self):
        self.new(850009)
        self.new(850010)
        self.assertEqual(c.clm_container_reference(850009), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_get_info(850009)[1].references, 2)
        self.assertEqual(c.clm_container_unreference(850009), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_unreference(850009), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_unreference(850009), c.CONTAINER_ERROR_INVALID_VALUE)
        self.assertEqual(c.clm_container_count(850009), (c.CONTAINER_SUCCESS, 0))
        self.assertEqual(c.clm_container_dispose(), (c.CONTAINER_SUCCESS, 1))
        self.assertEqual(c.clm_container_get_info(850009), (c.CONTAINER_ERROR_NOT_FOUND, None))
        self.assertEqual(c.clm_container_get_info(850010)[1].references, 1)
        self.assertEqual(c.clm_container_create(850009, c.CONTAINER_TYPE_STRING), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_get_info(850009)[1].references, 1)
        self.assertEqual(c.clm_container_destroy(850009), c.CONTAINER_SUCCESS)
        self.assertEqual(c.clm_container_reference(850009), c.CONTAINER_ERROR_NOT_FOUND)
        self.assertEqual(c.clm_container_unreference(850009), c.CONTAINER_ERROR_NOT_FOUND)

    def test_compatibility_and_facade(self):
        self.new(850011, c.CONTAINER_TYPE_STRING)
        self.assertIs(c.Container.slice, c.clm_container_slice)
        self.assertEqual(c.Container.reference(850011), c.CONTAINER_SUCCESS)
        self.assertEqual(c.Container.unreference(850011), c.CONTAINER_SUCCESS)
        self.assertEqual(c.Container.move_first(850011), c.CONTAINER_ERROR_ID_NOT_FOUND)
        self.assertEqual(c.clm_container_create(850012, c.CONTAINER_TYPE_DYNAMIC), c.CONTAINER_ERROR_TYPE_NOT_SUPPORTED)

if __name__ == '__main__':
    unittest.main()
