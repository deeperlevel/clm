"""Behavioral tests for clm_id's procedural API and facade."""

import unittest

from clm_id import (
    IdRegistry,
    clm_id_clear,
    clm_id_get,
    clm_id_is_case_sensitive,
    clm_id_register,
    clm_id_remove,
    clm_id_set,
    clm_id_set_case_sensitive,
)


class TestIdRegistry(unittest.TestCase):
    def setUp(self):
        clm_id_clear()
        clm_id_set_case_sensitive(True)

    def tearDown(self):
        clm_id_clear()
        clm_id_set_case_sensitive(True)

    def test_default_sensitive(self):
        self.assertTrue(clm_id_is_case_sensitive())
        clm_id_register("Host", 123)
        self.assertEqual(clm_id_get("Host"), 123)
        with self.assertRaises(KeyError):
            clm_id_get("host")
        clm_id_register("host", 456)
        self.assertEqual(clm_id_get("host"), 456)

    def test_insensitive_register_lookup_and_remove(self):
        IdRegistry.set_case_sensitive(False)
        self.assertFalse(IdRegistry.is_case_sensitive())
        IdRegistry.register("WebAPI.Host", 123)
        self.assertEqual(IdRegistry.get("webapi.host"), 123)
        self.assertEqual(IdRegistry.get("WEBAPI.HOST"), 123)
        with self.assertRaises(KeyError):
            IdRegistry.register("webapi.HOST", 456)
        IdRegistry.remove("WEBAPI.host")
        with self.assertRaises(KeyError):
            IdRegistry.get("WebAPI.Host")

    def test_switch_modes_preserves_spelling(self):
        clm_id_register("Host", 10)
        clm_id_set_case_sensitive(False)
        self.assertEqual(clm_id_get("HOST"), 10)
        clm_id_set_case_sensitive(True)
        self.assertEqual(clm_id_get("Host"), 10)
        with self.assertRaises(KeyError):
            clm_id_get("HOST")

    def test_switch_with_collisions_rolls_back(self):
        clm_id_register("Host", 1)
        clm_id_register("host", 2)
        with self.assertRaises(ValueError):
            clm_id_set_case_sensitive(False)
        self.assertTrue(clm_id_is_case_sensitive())
        self.assertEqual(clm_id_get("Host"), 1)
        self.assertEqual(clm_id_get("host"), 2)

    def test_unicode_casefold(self):
        clm_id_set_case_sensitive(False)
        clm_id_register("Straße", 77)
        self.assertEqual(clm_id_get("STRASSE"), 77)

    def test_set_add_and_remove_errors(self):
        clm_id_set("port", 8080)
        self.assertEqual(clm_id_get("port"), 8080)
        clm_id_set("port", 9090)
        self.assertEqual(clm_id_get("port"), 9090)
        with self.assertRaises(KeyError):
            clm_id_register("port", 7)
        clm_id_remove("port")
        with self.assertRaises(KeyError):
            clm_id_remove("port")

    def test_distinct_names_can_share_id(self):
        clm_id_register("host", 123)
        clm_id_register("hostname", 123)
        self.assertEqual(clm_id_get("hostname"), 123)

    def test_clear_retains_mode(self):
        clm_id_set_case_sensitive(False)
        clm_id_register("host", 1)
        IdRegistry.clear()
        self.assertFalse(IdRegistry.is_case_sensitive())
        with self.assertRaises(KeyError):
            clm_id_get("HOST")

    # noinspection bad-argument-type
    def test_validation(self):
        with self.assertRaises(ValueError):
            clm_id_register("", 1)
        with self.assertRaises(ValueError):
            clm_id_get(None)
        with self.assertRaises(TypeError):
            clm_id_set("x", True)
        with self.assertRaises(TypeError):
            clm_id_set_case_sensitive(0)

    def test_facade_shares_functions(self):
        self.assertIs(IdRegistry.get, clm_id_get)
        self.assertIs(IdRegistry.register, clm_id_register)
        self.assertIsNotNone(IdRegistry.get.__doc__)


if __name__ == "__main__":
    unittest.main()
