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

"""Python standard-library tests for clm_platform."""
import os
import unittest
from unittest.mock import patch

import clm_platform as cp


class PlatformTests(unittest.TestCase):
    def test_public_names(self):
        self.assertEqual(len(cp.__all__), 9)
        for name in cp.__all__:
            self.assertTrue(callable(getattr(cp, name)))

    def test_windows(self):
        with patch('clm_platform.platform.system', return_value='Windows'), patch('clm_platform.sys.platform', 'win32'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'Windows')
            self.assertTrue(cp.clm_platform_is_windows())
            self.assertTrue(cp.clm_platform_is_pc())
            self.assertFalse(cp.clm_platform_is_mobile())
            self.assertFalse(cp.clm_platform_is_linux())

    def test_linux(self):
        with patch('clm_platform.platform.system', return_value='Linux'), patch('clm_platform.sys.platform', 'linux'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'Linux')
            self.assertTrue(cp.clm_platform_is_linux())
            self.assertTrue(cp.clm_platform_is_pc())

    def test_macos(self):
        with patch('clm_platform.platform.system', return_value='Darwin'), patch('clm_platform.sys.platform', 'darwin'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'macOS')
            self.assertTrue(cp.clm_platform_is_pc())

    def test_android(self):
        with patch('clm_platform.platform.system', return_value='Linux'), patch('clm_platform.sys.platform', 'android'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'Android')
            self.assertTrue(cp.clm_platform_is_mobile())
            self.assertFalse(cp.clm_platform_is_linux())
            self.assertFalse(cp.clm_platform_is_pc())

    def test_ios(self):
        with patch('clm_platform.platform.system', return_value='Darwin'), patch('clm_platform.sys.platform', 'ios'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'iOS')
            self.assertTrue(cp.clm_platform_is_mobile())
            self.assertFalse(cp.clm_platform_is_pc())

    def test_bsd(self):
        with patch('clm_platform.platform.system', return_value='FreeBSD'), patch('clm_platform.sys.platform', 'freebsd14'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'FreeBSD')
            self.assertTrue(cp.clm_platform_is_pc())

    def test_unknown(self):
        with patch('clm_platform.platform.system', return_value='Other'), patch('clm_platform.sys.platform', 'other'):
            self.assertEqual(cp.clm_platform_get_os_name(), 'Unknown')
            self.assertFalse(cp.clm_platform_is_pc())
            self.assertFalse(cp.clm_platform_is_mobile())

    def test_console_is_conservative(self):
        self.assertFalse(cp.clm_platform_is_console())

    def test_version(self):
        with patch('clm_platform.platform.system', return_value='Windows'), patch('clm_platform.platform.version', return_value='10.0.12345'):
            self.assertEqual(cp.clm_platform_get_os_version(), '10.0.12345')

    def test_version_release_fallback(self):
        with patch('clm_platform.platform.system', return_value='Linux'), patch('clm_platform.platform.version', return_value=''), patch('clm_platform.platform.release', return_value='6.10'):
            self.assertEqual(cp.clm_platform_get_os_version(), '6.10')

    def test_version_unknown_fallback(self):
        with patch('clm_platform.platform.version', return_value=''), patch('clm_platform.platform.release', return_value=''):
            self.assertEqual(cp.clm_platform_get_os_version(), 'Unknown')

    def test_env_existing_missing_and_default(self):
        with patch.dict(os.environ, {'CLM_PLATFORM_TEST': 'value'}, clear=False):
            self.assertEqual(cp.clm_platform_get_environment_variable('CLM_PLATFORM_TEST'), 'value')
            self.assertIsNone(cp.clm_platform_get_environment_variable('CLM_PLATFORM_NOT_DEFINED_EXAMPLE'))
            self.assertEqual(cp.clm_platform_get_environment_variable('CLM_PLATFORM_NOT_DEFINED_EXAMPLE', 'fallback'), 'fallback')

    def test_env_empty_is_not_missing(self):
        with patch.dict(os.environ, {'CLM_PLATFORM_TEST': ''}):
            self.assertEqual(cp.clm_platform_get_environment_variable('CLM_PLATFORM_TEST', 'fallback'), '')

    # noinspection bad-argument-type
    def test_env_validations(self):
        for name in ('', 'X=Y', 'X\x00Y'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                cp.clm_platform_get_environment_variable(name)
        with self.assertRaises(ValueError):
            cp.clm_platform_get_environment_variable(None)
        with self.assertRaises(TypeError):
            cp.clm_platform_get_environment_variable('NAME', 42)

    def test_environment_snapshot_is_detached(self):
        with patch.dict(os.environ, {'CLM_PLATFORM_TEST': 'original'}):
            snapshot = cp.clm_platform_get_environment_variables()
            self.assertEqual(snapshot['CLM_PLATFORM_TEST'], 'original')
            snapshot['CLM_PLATFORM_TEST'] = 'modified'
            self.assertEqual(os.environ['CLM_PLATFORM_TEST'], 'original')


if __name__ == '__main__':
    unittest.main()
