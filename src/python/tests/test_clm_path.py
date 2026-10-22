# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm
# Contact: deeparchive@outlook.com
#
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
#
# Free to use, modify, redistribute, and bundle for any purpose.
# Attribution and retention of this notice are appreciated, not required.
#
# Provided AS IS, without warranty or liability.
#
# Don't start from scratch, bro.
# =============================================================================

"""Tests for CLA Path procedural functions and their convenience facade."""

import os
import tempfile
import unittest

import clm_path


class PathTests(unittest.TestCase):
    def test_extension_and_filename(self):
        p = os.path.join('folder', 'archive.tar.gz')
        self.assertEqual(clm_path.cla_path_get_file_extension(p), '.gz')
        self.assertEqual(clm_path.cla_path_get_filename(p), 'archive.tar.gz')
        self.assertEqual(clm_path.cla_path_get_file_name_without_extension(p), 'archive.tar')
        self.assertEqual(clm_path.cla_path_change_extension(p, 'zip'), os.path.join('folder', 'archive.tar.zip'))
        self.assertEqual(clm_path.cla_path_change_extension(p, '.txt'), os.path.join('folder', 'archive.tar.txt'))
        self.assertEqual(clm_path.cla_path_change_extension(p, ''), os.path.join('folder', 'archive.tar'))
        self.assertFalse(clm_path.cla_path_has_extension('.gitignore'))
        self.assertTrue(clm_path.cla_path_has_extension(p))

    def test_join_directory_and_full_path(self):
        expected = os.path.join('parent', 'child.txt')
        self.assertEqual(clm_path.cla_path_join('parent', 'child.txt'), expected)
        self.assertEqual(clm_path.cla_path_get_directory_name(expected), 'parent')
        self.assertEqual(clm_path.cla_path_get_full_path('..'), os.path.abspath('..'))
        self.assertTrue(clm_path.cla_path_is_full_path(os.path.abspath('..')))
        self.assertFalse(clm_path.cla_path_is_full_path('relative.txt'))

    def test_relative_path(self):
        with tempfile.TemporaryDirectory() as root:
            target = os.path.join(root, 'sub', 'name.txt')
            self.assertEqual(clm_path.cla_path_get_relative_path(root, target), os.path.join('sub', 'name.txt'))

    def test_separator_and_root_preservation(self):
        sep = os.sep
        self.assertEqual(clm_path.cla_path_get_directory_separator(), sep)
        self.assertTrue(clm_path.cla_path_ends_in_directory_separator('folder' + sep))
        self.assertFalse(clm_path.cla_path_ends_in_directory_separator('folder'))
        self.assertEqual(clm_path.cla_path_trim_ending_separator(sep), sep)
        self.assertEqual(clm_path.cla_path_trim_ending_separator('folder' + sep * 3), 'folder')
        self.assertEqual(clm_path.cla_path_trim_ending_separator(''), '')
        self.assertEqual(clm_path.cla_path_get_path_root(sep + 'folder'), sep)
        self.assertEqual(clm_path.cla_path_get_path_root('relative/folder'), '')

    @unittest.skipUnless(os.name == 'nt', 'Windows-specific drive semantics')
    def test_windows_roots(self):
        self.assertEqual(clm_path.cla_path_get_path_root('C:\\some\\file'), 'C:\\')
        self.assertEqual(clm_path.cla_path_get_path_root('C:relative'), 'C:')
        self.assertEqual(clm_path.cla_path_trim_ending_separator('C:\\'), 'C:\\')

    def test_random_filename(self):
        first = clm_path.cla_path_get_random_filename('txt')
        second = clm_path.cla_path_get_random_filename('.txt')
        self.assertRegex(first, r'^[0-9a-f]{12}\.txt$')
        self.assertRegex(second, r'^[0-9a-f]{12}\.txt$')
        self.assertNotEqual(first, second)
        self.assertRegex(clm_path.cla_path_get_random_filename(), r'^[0-9a-f]{12}$')

    def test_exists_and_temporary_directory(self):
        with tempfile.TemporaryDirectory() as root:
            destination = os.path.join(root, 'new', 'nested')
            created = clm_path.cla_path_get_temporary_directory(destination)
            self.assertTrue(os.path.isdir(created))
            self.assertEqual(os.path.dirname(created), os.path.abspath(destination))
            self.assertTrue(clm_path.cla_path_exists(created))
            self.assertFalse(clm_path.cla_path_exists(os.path.join(root, 'missing')))

    def test_invalid_path_characters(self):
        chars = clm_path.cla_path_get_invalid_path_characters()
        self.assertIn('\x00', chars)
        if os.name == 'nt':
            self.assertIn('?', chars)
            self.assertIn('*', chars)
        else:
            self.assertEqual(chars, ['\x00'])

    def test_facade_matches_procedural_api(self):
        self.assertEqual(clm_path.Path.join('a', 'b'), clm_path.cla_path_join('a', 'b'))
        self.assertEqual(clm_path.Path.get_filename('a.txt'), clm_path.cla_path_get_filename('a.txt'))
        self.assertEqual(clm_path.Path.get_path_root(os.sep + 'a'), clm_path.cla_path_get_path_root(os.sep + 'a'))
        self.assertEqual(clm_path.Path.get_directory_separator(), os.sep)
        self.assertEqual(clm_path.Path.change_extension('a.txt', 'json'), 'a.json')
        self.assertEqual(
            clm_path.Path.get_relative_path('a', os.path.join('a', 'b')),
            clm_path.cla_path_get_relative_path('a', os.path.join('a', 'b'))
        )


if __name__ == '__main__':
    unittest.main()
