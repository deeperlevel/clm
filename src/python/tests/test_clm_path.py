"""CLM Path ID registry and transformations integration tests."""
import os
import re
import tempfile
import unittest

import clm_path as p
from clm_storage import (STORAGE_SUCCESS, STORAGE_TYPE_STRING, clm_storage_create,
                         clm_storage_append, clm_storage_get_index,
                         clm_storage_set_index, clm_storage_destroy)


class PathTests(unittest.TestCase):
    def setUp(self):
        p.clm_path_destroy()
        p.clm_path_initialize()

    def tearDown(self):
        p.clm_path_destroy()

    def test_map_replace_unmap(self):
        p.clm_path_map(100, 'a')
        self.assertTrue(p.clm_path_is_mapped(100))
        p.clm_path_map(100, 'b')
        self.assertEqual(p.clm_path_get(100), 'b')
        p.clm_path_unmap(100)
        self.assertFalse(p.clm_path_is_mapped(100))
        with self.assertRaises(p.PathError): p.clm_path_get(100)

    def test_join_default_destination(self):
        p.clm_path_map(1, 'c:/media')
        p.clm_path_map(2, 'movies')
        p.clm_path_join(1, 2)
        self.assertEqual(p.clm_path_get(1), 'c:/media/movies')
        self.assertEqual(p.clm_path_get(2), 'movies')

    def test_join_destination_alias_second(self):
        p.clm_path_map(1, 'c:/media')
        p.clm_path_map(2, 'movies')
        p.clm_path_join(1, 2, destination=2)
        self.assertEqual(p.clm_path_get(1), 'c:/media')
        self.assertEqual(p.clm_path_get(2), 'c:/media/movies')

    def test_sanitize_drive_and_relative(self):
        p.clm_path_map(3, 'd:/programs/../movies/movie.mkv')
        p.clm_path_sanitize(3)
        self.assertEqual(p.clm_path_get(3), 'd:/movies/movie.mkv')
        p.clm_path_map(4, 'a/./b/../c')
        p.clm_path_sanitize(4, destination=5)
        self.assertEqual(p.clm_path_get(5), 'a/c')
        self.assertEqual(p.clm_path_get(4), 'a/./b/../c')

    def test_extension_with_mapped_fragment(self):
        p.clm_path_map(11, 'program.exx')
        p.clm_path_map(12, 'exe')
        p.clm_path_change_extension(11, 12)
        self.assertEqual(p.clm_path_get(11), 'program.exe')
        p.clm_path_map(12, '')
        p.clm_path_change_extension(11, 12, destination=13)
        self.assertEqual(p.clm_path_get(13), 'program')

    def test_extension_and_components(self):
        p.clm_path_map(1, 'folder/archive.tar.gz')
        p.clm_path_get_file_extension(1, 2)
        p.clm_path_get_filename(1, 3)
        p.clm_path_get_file_name_without_extension(1, 4)
        p.clm_path_get_directory_name(1, 5)
        self.assertEqual([p.clm_path_get(i) for i in (2,3,4,5)], ['.gz','archive.tar.gz','archive.tar','folder'])
        self.assertTrue(p.clm_path_has_extension(1))
        p.clm_path_map(6,'.gitignore')
        self.assertFalse(p.clm_path_has_extension(6))

    def test_map_from_storage_preserves_cursor(self):
        sid=543210
        self.assertEqual(clm_storage_create(sid, STORAGE_TYPE_STRING, size=512), STORAGE_SUCCESS)
        try:
            self.assertEqual(clm_storage_append(sid,'c:/one'), STORAGE_SUCCESS)
            self.assertEqual(clm_storage_append(sid,'c:/two'), STORAGE_SUCCESS)
            self.assertEqual(clm_storage_set_index(sid,1), STORAGE_SUCCESS)
            p.clm_path_map_from_storage(99,sid,0)
            self.assertEqual(p.clm_path_get(99),'c:/one')
            self.assertEqual(clm_storage_get_index(sid),(STORAGE_SUCCESS,1))
            self.assertEqual(clm_storage_destroy(sid), STORAGE_SUCCESS)
            self.assertEqual(p.clm_path_get(99),'c:/one')
        finally:
            clm_storage_destroy(sid)

    def test_map_from_storage_failures_are_atomic(self):
        p.clm_path_map(1,'original')
        with self.assertRaises(p.PathError): p.clm_path_map_from_storage(1,123456789,0)
        self.assertEqual(p.clm_path_get(1),'original')
        sid=543211
        clm_storage_create(sid, STORAGE_TYPE_STRING, size=512)
        try:
            with self.assertRaises(p.PathError): p.clm_path_map_from_storage(1,sid,0)
            self.assertEqual(p.clm_path_get(1),'original')
        finally: clm_storage_destroy(sid)

    def test_errors_do_not_change_sources(self):
        p.clm_path_map(1,'original.txt')
        p.clm_path_map(2,'bad/extension')
        with self.assertRaises(p.PathError): p.clm_path_change_extension(1,2)
        self.assertEqual(p.clm_path_get(1),'original.txt')
        with self.assertRaises(p.PathError): p.clm_path_join(1,404)
        self.assertEqual(p.clm_path_get(1),'original.txt')
        with self.assertRaises(p.PathError): p.clm_path_map(7,'bad\x00name')
        self.assertFalse(p.clm_path_is_mapped(7))

    def test_relative_absolute_and_roots(self):
        p.clm_path_map(1,'/tmp/a/b')
        p.clm_path_map(2,'/tmp/a/b/c.txt')
        p.clm_path_get_relative_path(1,2)
        self.assertEqual(p.clm_path_get(2),'c.txt')
        p.clm_path_map(3,'/tmp/x/')
        p.clm_path_get_path_root(3,4)
        self.assertEqual(p.clm_path_get(4),'/')
        self.assertTrue(p.clm_path_is_full_path(3))
        p.clm_path_trim_ending_separator(3)
        self.assertEqual(p.clm_path_get(3),'/tmp/x')

    def test_full_path(self):
        p.clm_path_map(1,'.')
        p.clm_path_get_full_path(1)
        self.assertEqual(p.clm_path_get(1),os.path.abspath('.').replace('\\','/'))

    def test_filesystem_and_temporary_dir(self):
        with tempfile.TemporaryDirectory() as root:
            p.clm_path_map(1, root)
            self.assertTrue(p.clm_path_exists(1))
            p.clm_path_get_temporary_directory(1,2)
            directory = p.clm_path_get(2)
            self.assertTrue(os.path.isdir(directory))
            os.rmdir(directory)

    def test_random_filename_and_facade(self):
        p.clm_path_map(1,'mkv')
        p.Path.get_random_filename(2,1)
        self.assertRegex(p.Path.get(2),r'^[0-9a-f]{12}\.mkv$')
        self.assertIn('\x00',p.clm_path_get_invalid_path_characters())

    def test_destroy_and_reinitialize(self):
        p.clm_path_map(1,'a')
        p.clm_path_destroy()
        self.assertFalse(p.clm_path_is_mapped(1))
        p.clm_path_map(1,'new')
        self.assertEqual(p.clm_path_get(1),'new')

if __name__=='__main__': unittest.main()
