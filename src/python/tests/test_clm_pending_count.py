"""Regression tests for CLM's pending-disposal counters."""
import unittest
import clm_container as c
import clm_storage as s

class PendingCountTests(unittest.TestCase):
    def test_container_pending_count(self):
        ids = (993001, 993002)
        baseline = c.clm_container_pending_count()
        try:
            for cid in ids:
                self.assertEqual(c.clm_container_create(cid, c.CONTAINER_TYPE_STRING), c.CONTAINER_SUCCESS)
            self.assertEqual(c.clm_container_pending_count(), baseline)
            self.assertEqual(c.clm_container_unreference(ids[0]), c.CONTAINER_SUCCESS)
            self.assertEqual(c.clm_container_pending_count(), baseline + 1)
            self.assertEqual(c.Container.pending_count(), baseline + 1)
            self.assertEqual(c.clm_container_reference(ids[0]), c.CONTAINER_SUCCESS)
            self.assertEqual(c.clm_container_pending_count(), baseline)
            self.assertEqual(c.clm_container_unreference(ids[1]), c.CONTAINER_SUCCESS)
            self.assertEqual(c.clm_container_pending_count(), baseline + 1)
            self.assertEqual(c.clm_container_destroy(ids[1]), c.CONTAINER_SUCCESS)
            self.assertEqual(c.clm_container_pending_count(), baseline)
        finally:
            for cid in ids: c.clm_container_destroy(cid)

    def test_storage_pending_count(self):
        ids = (994001, 994002)
        baseline = s.clm_storage_pending_count()
        try:
            for sid in ids:
                self.assertEqual(s.clm_storage_create(sid, s.STORAGE_TYPE_BYTE), s.STORAGE_SUCCESS)
            self.assertEqual(s.clm_storage_pending_count(), baseline)
            self.assertEqual(s.clm_storage_unreference(ids[0]), s.STORAGE_SUCCESS)
            self.assertEqual(s.clm_storage_pending_count(), baseline + 1)
            self.assertEqual(s.Storage.pending_count(), baseline + 1)
            self.assertEqual(s.clm_storage_reference(ids[0]), s.STORAGE_SUCCESS)
            self.assertEqual(s.clm_storage_pending_count(), baseline)
            self.assertEqual(s.clm_storage_unreference(ids[1]), s.STORAGE_SUCCESS)
            self.assertEqual(s.clm_storage_pending_count(), baseline + 1)
            self.assertEqual(s.clm_storage_destroy(ids[1]), s.STORAGE_SUCCESS)
            self.assertEqual(s.clm_storage_pending_count(), baseline)
        finally:
            for sid in ids: s.clm_storage_destroy(sid)

if __name__ == '__main__': unittest.main()
