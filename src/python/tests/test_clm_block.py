"""Contract tests for the procedural CLM Block value structure.

Run with: python -m unittest -v test_clm_block
No dimension validation is assumed: Block is currently a plain dataclass.
"""

import unittest
from dataclasses import asdict, astuple, fields, is_dataclass

from clm_block import Block


class TestBlock(unittest.TestCase):
    def test_is_dataclass(self):
        self.assertTrue(is_dataclass(Block))

    def test_positional_construction(self):
        block = Block(2, 3, 20, 10)
        self.assertEqual((block.x, block.y, block.width, block.height), (2, 3, 20, 10))

    def test_keyword_construction_and_field_order(self):
        block = Block(height=10, width=20, y=3, x=2)
        self.assertEqual([field.name for field in fields(Block)], ["x", "y", "width", "height"])
        self.assertEqual(astuple(block), (2, 3, 20, 10))
        self.assertEqual(asdict(block), {"x": 2, "y": 3, "width": 20, "height": 10})

    def test_value_equality(self):
        self.assertEqual(Block(1, 2, 3, 4), Block(1, 2, 3, 4))
        self.assertNotEqual(Block(1, 2, 3, 4), Block(1, 2, 3, 5))

    def test_fields_are_mutable(self):
        block = Block(0, 0, 1, 1)
        block.x = 5
        block.height = 8
        self.assertEqual(astuple(block), (5, 0, 1, 8))

    def test_instances_are_independent(self):
        first = Block(1, 2, 3, 4)
        second = Block(1, 2, 3, 4)
        first.width = 99
        self.assertEqual(second.width, 3)

    def test_zero_and_negative_values_are_currently_permitted(self):
        self.assertEqual(astuple(Block(-2, -3, 0, -4)), (-2, -3, 0, -4))

    def test_all_fields_required(self):
        with self.assertRaises(TypeError):
            Block(1, 2, 3)

    def test_repr_contains_field_names_and_values(self):
        self.assertIn("width=30", repr(Block(1, 2, 30, 40)))
        self.assertIn("height=40", repr(Block(1, 2, 30, 40)))


if __name__ == "__main__":
    unittest.main()
