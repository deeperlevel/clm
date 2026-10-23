"""Run with: python -m unittest -v test_clm_container.py"""
import unittest
import clm_container as c


class ContainerTests(unittest.TestCase):
    def setUp(self):
        self.ids = []

    def tearDown(self):
        for container_id in self.ids:
            c.clm_container_destroy(container_id)

    def create(self, container_id, kind):
        self.ids.append(container_id)
        self.assertEqual(c.Container.create(container_id, kind), c.CONTAINER_SUCCESS)

    def test_every_variant_add_get_edit_delete(self):
        values = [
            (c.CONTAINER_TYPE_STRING, 'string', 'hello', 'world'),
            (c.CONTAINER_TYPE_BYTE, 'byte', -128, 127),
            (c.CONTAINER_TYPE_SHORT, 'short', -32768, 32767),
            (c.CONTAINER_TYPE_INTEGER, 'integer', -2147483648, 2147483647),
            (c.CONTAINER_TYPE_LONG_INTEGER, 'long', -(1 << 63), (1 << 63)-1),
            (c.CONTAINER_TYPE_FLOAT, 'float', 1.5, 2.5),
            (c.CONTAINER_TYPE_DOUBLE, 'double', 1.25, 2.75),
            (c.CONTAINER_TYPE_OBJECT, 'object', None, {'a': 1}),
        ]
        for i, (kind, suffix, before, after) in enumerate(values, 100):
            with self.subTest(suffix=suffix):
                self.create(i, kind)
                self.assertEqual(getattr(c.Container, 'add_'+suffix)(i, 2, before), c.CONTAINER_SUCCESS)
                self.assertEqual(getattr(c.Container, 'get_'+suffix)(i, 2), (c.CONTAINER_SUCCESS, before))
                self.assertEqual(getattr(c.Container, 'edit_'+suffix)(i, 2, after), c.CONTAINER_SUCCESS)
                self.assertEqual(getattr(c.Container, 'get_'+suffix)(i, 2), (c.CONTAINER_SUCCESS, after))
                self.assertEqual(c.Container.count(i), (c.CONTAINER_SUCCESS, 1))
                self.assertEqual(c.Container.contains(i, 2), (c.CONTAINER_SUCCESS, True))
                self.assertEqual(getattr(c.Container, 'delete_'+suffix)(i, 2), c.CONTAINER_SUCCESS)
                self.assertEqual(c.Container.count(i), (c.CONTAINER_SUCCESS, 0))

    def test_errors(self):
        self.create(200, c.CONTAINER_TYPE_INTEGER)
        self.assertEqual(c.Container.create(200, c.CONTAINER_TYPE_INTEGER), c.CONTAINER_ERROR_DUPLICATE_ID)
        self.assertEqual(c.Container.create(201, 0xAA), c.CONTAINER_ERROR_TYPE_NOT_SUPPORTED)
        self.assertEqual(c.Container.add_string(200, 1, 'x'), c.CONTAINER_ERROR_TYPE_NOT_MATCHED)
        self.assertEqual(c.Container.add_integer(9999, 1, 4), c.CONTAINER_ERROR_NOT_FOUND)
        self.assertEqual(c.Container.get_integer(200, 1), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.Container.add_integer(200, 1, 4), c.CONTAINER_SUCCESS)
        self.assertEqual(c.Container.add_integer(200, 1, 5), c.CONTAINER_ERROR_DUPLICATE_ID)
        self.assertEqual(c.Container.edit_integer(200, 7, 5), c.CONTAINER_ERROR_ID_NOT_FOUND)
        self.assertEqual(c.Container.delete_integer(200, 7), c.CONTAINER_ERROR_ID_NOT_FOUND)

    def test_type_bounds(self):
        self.create(300, c.CONTAINER_TYPE_BYTE)
        for value in (128, -129, True, '7'):
            self.assertEqual(c.Container.add_byte(300, 0, value), c.CONTAINER_ERROR_INVALID_VALUE)
        self.create(301, c.CONTAINER_TYPE_FLOAT)
        self.assertEqual(c.Container.add_float(301, 0, 1e100), c.CONTAINER_ERROR_INVALID_VALUE)

    def test_clear_destroy_reuse(self):
        self.create(400, c.CONTAINER_TYPE_STRING)
        c.Container.add_string(400, 0, 'x')
        self.assertEqual(c.Container.clear(400), c.CONTAINER_SUCCESS)
        self.assertEqual(c.Container.count(400), (c.CONTAINER_SUCCESS, 0))
        self.assertEqual(c.Container.destroy(400), c.CONTAINER_SUCCESS)
        self.assertEqual(c.Container.count(400), (c.CONTAINER_ERROR_NOT_FOUND, 0))
        self.assertEqual(c.Container.create(400, c.CONTAINER_TYPE_INTEGER), c.CONTAINER_SUCCESS)

    def test_iterator_forward_backward_insertion_order(self):
        self.create(500, c.CONTAINER_TYPE_OBJECT)
        for key, value in ((20, "b"), (5, None), (100, {"name": "Asia"})):
            self.assertEqual(c.Container.add_object(500, key, value), c.CONTAINER_SUCCESS)

        self.assertEqual(c.Container.next(500), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.Container.first(500), (c.CONTAINER_SUCCESS, 20))
        self.assertEqual(c.Container.next(500), (c.CONTAINER_SUCCESS, 5))
        self.assertEqual(c.Container.get_object(500, 5), (c.CONTAINER_SUCCESS, None))
        self.assertEqual(c.Container.next(500), (c.CONTAINER_SUCCESS, 100))
        self.assertEqual(c.Container.next(500), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.Container.previous(500), (c.CONTAINER_SUCCESS, 5))
        self.assertEqual(c.Container.last(500), (c.CONTAINER_SUCCESS, 100))
        self.assertEqual(c.Container.previous(500), (c.CONTAINER_SUCCESS, 5))
        self.assertEqual(c.Container.previous(500), (c.CONTAINER_SUCCESS, 20))
        self.assertEqual(c.Container.previous(500), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.Container.next(500), (c.CONTAINER_SUCCESS, 5))

    def test_iterator_empty_missing_and_independent_cursors(self):
        self.create(501, c.CONTAINER_TYPE_INTEGER)
        self.create(502, c.CONTAINER_TYPE_STRING)
        self.assertEqual(c.Container.first(501), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.Container.last(9999), (c.CONTAINER_ERROR_NOT_FOUND, None))
        for key in (7, 8):
            c.Container.add_integer(501, key, key)
        for key in (30, 40):
            c.Container.add_string(502, key, str(key))
        self.assertEqual(c.Container.first(501), (c.CONTAINER_SUCCESS, 7))
        self.assertEqual(c.Container.last(502), (c.CONTAINER_SUCCESS, 40))
        self.assertEqual(c.Container.next(501), (c.CONTAINER_SUCCESS, 8))
        self.assertEqual(c.Container.previous(502), (c.CONTAINER_SUCCESS, 30))

    def test_iterator_mutations_and_id_reuse(self):
        self.create(503, c.CONTAINER_TYPE_INTEGER)
        for key in (1, 2, 3):
            c.Container.add_integer(503, key, key)
        self.assertEqual(c.Container.first(503), (c.CONTAINER_SUCCESS, 1))
        c.Container.edit_integer(503, 1, 50)
        self.assertEqual(c.Container.next(503), (c.CONTAINER_SUCCESS, 2))
        c.Container.delete_integer(503, 2)
        self.assertEqual(c.Container.next(503), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        self.assertEqual(c.Container.first(503), (c.CONTAINER_SUCCESS, 1))
        c.Container.delete_integer(503, 1)
        c.Container.add_integer(503, 1, 99)
        self.assertEqual(c.Container.first(503), (c.CONTAINER_SUCCESS, 3))
        self.assertEqual(c.Container.last(503), (c.CONTAINER_SUCCESS, 1))
        c.Container.clear(503)
        self.assertEqual(c.Container.next(503), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))
        c.Container.destroy(503)
        c.Container.create(503, c.CONTAINER_TYPE_STRING)
        self.assertEqual(c.Container.first(503), (c.CONTAINER_ERROR_ID_NOT_FOUND, None))

    def test_docstrings_on_facade(self):
        self.assertEqual(c.Container.add_integer.__doc__, c.clm_container_add_integer.__doc__)


if __name__ == '__main__':
    unittest.main()
