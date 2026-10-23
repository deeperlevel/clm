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

"""Tests for the CLM Size reference dataclass, without added behavior."""

import unittest
from dataclasses import asdict, is_dataclass, replace

from clm_size import Size


class SizeTests(unittest.TestCase):
    """Verify representation, mutability, and native Python dataclass behavior."""

    def test_construct_and_read(self):
        """Both dimensions are retained in their declared order."""
        size = Size(80, 24)
        self.assertEqual((size.width, size.height), (80, 24))

    def test_keyword_construction(self):
        """Named arguments set the intended dimension fields."""
        size = Size(height=40, width=100)
        self.assertEqual((size.width, size.height), (100, 40))

    # noinspection argument-list
    def test_both_dimensions_required(self):
        """The dataclass has no default dimensions."""
        with self.assertRaises(TypeError):
            Size(80)
        with self.assertRaises(TypeError):
            Size()

    def test_fields_are_mutable(self):
        """Size permits modifying both dimensions after creation."""
        size = Size(80, 24)
        size.width = 120
        size.height = 30
        self.assertEqual((size.width, size.height), (120, 30))

    def test_value_equality(self):
        """Python dataclasses generate field-wise equality."""
        self.assertEqual(Size(10, 20), Size(10, 20))
        self.assertNotEqual(Size(10, 20), Size(20, 10))

    def test_zero_negative_allowed_by_representation(self):
        """The source imposes no bounds or positive-dimension validation."""
        size = Size(0, -1)
        self.assertEqual((size.width, size.height), (0, -1))

    def test_independent_instances(self):
        """Mutation of one Size instance does not affect another."""
        first, second = Size(1, 2), Size(1, 2)
        first.height = 5
        self.assertEqual(second.height, 2)

    def test_dataclass_conveniences(self):
        """Dataclass helpers expose and copy the expected fields."""
        size = Size(640, 480)
        self.assertTrue(is_dataclass(size))
        self.assertEqual(asdict(size), {"width": 640, "height": 480})
        self.assertEqual(replace(size, height=720), Size(640, 720))
        self.assertIn("width=640", repr(size))

    # noinspection bad-argument-type
    def test_annotation_not_runtime_validator(self):
        """Python annotations do not automatically enforce integer values."""
        size = Size("large", 20)
        self.assertEqual(size.width, "large")


if __name__ == "__main__":
    unittest.main()
