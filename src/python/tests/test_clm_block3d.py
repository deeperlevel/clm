"""Contract tests for the procedural CLM Block3D value structure.

Run with: python -m unittest -v test_clm_block3d
No dimension validation is assumed: Block3D is currently a plain dataclass.
"""

import unittest
from dataclasses import asdict, astuple, fields, is_dataclass

from clm_block3d import Block3D


class TestBlock3D(unittest.TestCase):
    def test_is_dataclass(self):
        self.assertTrue(is_dataclass(Block3D))

    def test_positional_construction(self):
        block = Block3D(2, 3, 4, 20, 10, 5)
        self.assertEqual((block.x, block.y, block.z), (2, 3, 4))
        self.assertEqual((block.width, block.height, block.depth), (20, 10, 5))

    def test_keyword_construction_and_field_order(self):
        block = Block3D(depth=5, height=10, width=20, z=4, y=3, x=2)
        self.assertEqual([field.name for field in fields(Block3D)],
                         ["x", "y", "z", "width", "height", "depth"])
        self.assertEqual(astuple(block), (2, 3, 4, 20, 10, 5))
        self.assertEqual(asdict(block), {
            "x": 2, "y": 3, "z": 4, "width": 20, "height": 10, "depth": 5
        })

    def test_value_equality(self):
        self.assertEqual(Block3D(1, 2, 3, 4, 5, 6), Block3D(1, 2, 3, 4, 5, 6))
        self.assertNotEqual(Block3D(1, 2, 3, 4, 5, 6), Block3D(1, 2, 3, 4, 5, 7))

    def test_fields_are_mutable(self):
        block = Block3D(0, 0, 0, 1, 1, 1)
        block.z = 8
        block.depth = 12
        self.assertEqual(astuple(block), (0, 0, 8, 1, 1, 12))

    def test_instances_are_independent(self):
        first = Block3D(1, 2, 3, 4, 5, 6)
        second = Block3D(1, 2, 3, 4, 5, 6)
        first.depth = 99
        self.assertEqual(second.depth, 6)

    def test_zero_and_negative_values_are_currently_permitted(self):
        self.assertEqual(astuple(Block3D(-1, -2, -3, 0, -4, -5)),
                         (-1, -2, -3, 0, -4, -5))

    def test_all_fields_required(self):
        with self.assertRaises(TypeError):
            Block3D(1, 2, 3, 4, 5)

    def test_repr_contains_field_names_and_values(self):
        representation = repr(Block3D(1, 2, 3, 4, 5, 6))
        self.assertIn("z=3", representation)
        self.assertIn("depth=6", representation)


if __name__ == "__main__":
    unittest.main()
