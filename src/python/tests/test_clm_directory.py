import os
import tempfile
import unittest
from pathlib import Path
import clm_path as p
import clm_directory as d
from clm_storage import *

class DirectoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        p.clm_path_destroy()
        self.root = Path(self.temp.name)
        for i, name in ((10, self.root),(11, self.root/'a'),(12,self.root/'a'/'b'),(13,self.root/'moved')):
            p.clm_path_map(i, str(name))
        d.clm_directory_create(12)
        (self.root/'a'/'one.txt').write_text('1')
        (self.root/'a'/'b'/'two.txt').write_text('2')
        self.sid=95821
        self.assertEqual(clm_storage_create(self.sid, STORAGE_TYPE_STRING, size=1024, reserved=20), STORAGE_SUCCESS)
        self.addCleanup(lambda: clm_storage_destroy(self.sid))
        self.addCleanup(p.clm_path_destroy)

    def test_search_files_sorted_and_map(self):
        self.assertEqual(d.clm_directory_find_files(10, True, self.sid),2)
        status, a=clm_storage_get(self.sid,0); _, b=clm_storage_get(self.sid,1)
        self.assertEqual(sorted([a, b]), [a, b])
        self.assertEqual({Path(a).name, Path(b).name}, {'one.txt', 'two.txt'})
        p.clm_path_map_from_storage(20,self.sid,1)
        self.assertEqual(p.clm_path_get(20),b)
        self.assertEqual(d.clm_directory_find_directories(10,True,self.sid),2)
        self.assertEqual(d.clm_directory_find_entries(10,True,self.sid),4)

    def test_capacity_and_width_rejection_preserve_results(self):
        self.assertEqual(d.clm_directory_find_files(10, True, self.sid),2)
        before=[clm_storage_get(self.sid,i)[1] for i in range(2)]
        tiny=95822
        clm_storage_create(tiny,STORAGE_TYPE_STRING,size=1024,reserved=1)
        try:
            clm_storage_append(tiny,'sentinel')
            with self.assertRaises(d.DirectoryError): d.clm_directory_find_files(10,True,tiny)
            self.assertEqual(clm_storage_get(tiny,0)[1],'sentinel')
        finally: clm_storage_destroy(tiny)
        self.assertEqual([clm_storage_get(self.sid,i)[1] for i in range(2)],before)

    def test_move_remove_create_and_empty(self):
        self.assertFalse(d.clm_directory_is_directory_empty(11))
        d.clm_directory_move(12,13)
        self.assertTrue(d.clm_directory_is_directory_existing(13))
        with self.assertRaises(OSError): d.clm_directory_remove(13)
        d.clm_directory_remove(13,True)
        self.assertFalse(d.clm_directory_is_directory_existing(13))
        d.clm_directory_create(13,False)
        self.assertTrue(d.clm_directory_is_directory_empty(13))

    def test_timestamp_and_hidden(self):
        old=d.clm_directory_get_last_write_time(11)
        d.clm_directory_set_last_write_time(11, old-3000)
        self.assertAlmostEqual(d.clm_directory_get_last_write_time(11),old-3000,delta=1000)
        if os.name != 'nt':
            with self.assertRaises(d.UnsupportedFilesystemOperation): d.clm_directory_set_hidden(11,True)

    def test_current_directory(self):
        d.clm_directory_get_current_directory(22)
        self.assertEqual(os.path.abspath(p.clm_path_get(22)),os.getcwd())

if __name__ == '__main__': unittest.main()
