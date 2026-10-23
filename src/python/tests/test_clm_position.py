# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by Deeperlevel, developed with community contributions.
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

"""Tests for the CLM Position reference dataclass, without added behavior."""

import unittest
from dataclasses import asdict, is_dataclass, replace

from clm_position import Position


class PositionTests(unittest.TestCase):
    """Verify representation, mutability, and native Python dataclass behavior."""

    def test_construct_and_read(self):
        """Both coordinate fields are retained in their declared order."""
        position = Position(12, 8)
        self.assertEqual((position.x, position.y), (12, 8))

    def test_keyword_construction(self):
        """Named arguments initialize the same two coordinates."""
        position = Position(y=5, x=3)
        self.assertEqual((position.x, position.y), (3, 5))

    # noinspection argument-list
    def test_both_coordinates_required(self):
        """The dataclass has no defaults for either field."""
        with self.assertRaises(TypeError):
            Position(1)
        with self.assertRaises(TypeError):
            Position()

    def test_fields_are_mutable(self):
        """Position is a writable representation, not a frozen record."""
        position = Position(1, 2)
        position.x = -3
        position.y = 0
        self.assertEqual((position.x, position.y), (-3, 0))

    def test_value_equality(self):
        """Python dataclasses generate field-wise equality."""
        self.assertEqual(Position(4, 5), Position(4, 5))
        self.assertNotEqual(Position(4, 5), Position(5, 4))

    def test_independent_instances(self):
        """Updating one position does not change another instance."""
        first = Position(1, 2)
        second = Position(1, 2)
        first.x = 99
        self.assertEqual(second.x, 1)

    def test_dataclass_conveniences(self):
        """Field conversion and replacement work on the Python reference type."""
        position = Position(7, 9)
        self.assertTrue(is_dataclass(position))
        self.assertEqual(asdict(position), {"x": 7, "y": 9})
        self.assertEqual(replace(position, x=10), Position(10, 9))
        self.assertIn("x=7", repr(position))

    # noinspection bad-argument-type
    def test_annotation_not_runtime_validator(self):
        """The original source does not enforce integer types at runtime."""
        position = Position("12", 8)
        self.assertEqual(position.x, "12")


if __name__ == "__main__":
    unittest.main()
