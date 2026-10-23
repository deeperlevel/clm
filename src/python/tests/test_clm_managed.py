import time
import unittest
from clm_container import (clm_container_create, clm_container_unreference,
                           clm_container_get_info, CONTAINER_TYPE_INTEGER, CONTAINER_SUCCESS)
from clm_storage import (clm_storage_create, clm_storage_unreference,
                         clm_storage_get_info, STORAGE_TYPE_INTEGER, STORAGE_SUCCESS)
from clm_depend import (clm_depend_initialize, clm_depend_destroy, clm_depend_register,
                        clm_depend_resolve, clm_depend_unreference, clm_depend_get_info,
                        DependLifetime, DependStatus, DependError)
from clm_managed import *


class ManagedTests(unittest.TestCase):
    def setUp(self):
        clm_managed_destroy()
        clm_depend_initialize()
        clm_managed_initialize()

    def tearDown(self):
        clm_managed_destroy()
        clm_depend_destroy()

    def test_default_disabled_and_validation(self):
        self.assertFalse(clm_managed_is_enabled())
        self.assertEqual(clm_managed_set_interval(0), ManagedStatus.INVALID_ARGUMENT)
        self.assertEqual(clm_managed_set_thread(999), ManagedStatus.INVALID_ARGUMENT)
        self.assertEqual(clm_managed_set_algorithm(999), ManagedStatus.INVALID_ARGUMENT)
        self.assertFalse(clm_managed_poll())

    def test_manual_collect_all_three(self):
        c, s = 0x8900001, 0x8900002
        self.assertEqual(clm_container_create(c, CONTAINER_TYPE_INTEGER), CONTAINER_SUCCESS)
        self.assertEqual(clm_storage_create(s, STORAGE_TYPE_INTEGER), STORAGE_SUCCESS)
        self.assertEqual(clm_depend_register(0x8900003, [], lambda d: {'ok': True}, DependLifetime.TRANSIENT), DependStatus.SUCCESS)
        iid = clm_depend_resolve(0x8900003)
        self.assertEqual(clm_managed_collect(), (0, 0, 0))
        clm_depend_unreference(iid)
        clm_container_unreference(c)
        clm_storage_unreference(s)
        self.assertEqual(clm_managed_collect(), (1, 1, 1))
        stats = clm_managed_get_stats()
        self.assertEqual((stats.cycles, stats.disposed_transients, stats.disposed_containers, stats.disposed_storages), (2, 1, 1, 1))
        self.assertNotEqual(clm_container_get_info(c)[0], CONTAINER_SUCCESS)
        self.assertNotEqual(clm_storage_get_info(s)[0], STORAGE_SUCCESS)

    def test_singleton_is_not_collected(self):
        self.assertEqual(clm_depend_register(0x8900010, [], lambda d: 'single', DependLifetime.EAGER), DependStatus.SUCCESS)
        iid = clm_depend_resolve(0x8900010)
        clm_depend_unreference(iid)
        self.assertEqual(clm_managed_collect(), (0, 0, 0))
        self.assertEqual(clm_depend_get_info(iid).references, 0)

    def test_caller_interval_poll(self):
        self.assertEqual(clm_managed_set_algorithm(ManagedAlgorithm.INTERVAL), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_set_interval(1), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_enable(), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_set_interval(2), ManagedStatus.ACTIVE)
        time.sleep(0.01)
        self.assertTrue(clm_managed_poll())
        self.assertEqual(clm_managed_get_stats().cycles, 1)
        self.assertEqual(clm_managed_disable(), ManagedStatus.SUCCESS)
        self.assertFalse(clm_managed_poll())

    def test_worker_and_stop(self):
        self.assertEqual(clm_managed_set_algorithm(ManagedAlgorithm.INTERVAL), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_set_thread(ManagedThread.WORKER), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_set_interval(10), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_enable(), ManagedStatus.SUCCESS)
        deadline = time.monotonic() + 1
        while clm_managed_get_stats().cycles == 0 and time.monotonic() < deadline:
            time.sleep(0.005)
        self.assertGreater(clm_managed_get_stats().cycles, 0)
        self.assertEqual(clm_managed_disable(), ManagedStatus.SUCCESS)
        recorded = clm_managed_get_stats().cycles
        time.sleep(0.03)
        self.assertEqual(clm_managed_get_stats().cycles, recorded)

    def test_failure_is_recorded(self):
        from unittest.mock import patch
        with patch('clm_managed.clm_depend_dispose', side_effect=RuntimeError('test failure')):
            with self.assertRaisesRegex(RuntimeError, 'test failure'):
                clm_managed_collect()
        self.assertEqual(clm_managed_get_stats().failures, 1)
        self.assertEqual(clm_managed_get_stats().last_error, 'test failure')


if __name__ == '__main__':
    unittest.main()
