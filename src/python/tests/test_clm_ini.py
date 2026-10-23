import unittest
from clm_id import clm_id_clear, clm_id_register
from clm_format import FormatError, clm_tree_get, clm_tree_destroy

class TestBase(unittest.TestCase):
    def setUp(self):
        clm_id_clear()
        for n, i in [("name", 11), ("value", 12), ("section", 13), ("host", 14), ("rows", 15), ("flag", 16), ("num", 17), ("person", 18), ("age", 19), ("game", 20), ("players", 21), ("role", 22), ("display", 23), ("width", 24)]:
            clm_id_register(n, i)
        self.docs = []

    def doc(self, parser, text):
        d = parser(text)
        self.docs.append(d)
        return d

    def tearDown(self):
        for d in self.docs: clm_tree_destroy(d)
        clm_id_clear()

from clm_ini import clm_ini_parse, clm_ini_serialize
class TestINI(TestBase):
    def test_section_and_roundtrip(self):
        d=self.doc(clm_ini_parse,'[display]\nwidth=800\n')
        self.assertEqual(clm_tree_get(clm_tree_get(d.root,23),24).value,'800')
        e=self.doc(clm_ini_parse,clm_ini_serialize(d))
        self.assertEqual(clm_tree_get(clm_tree_get(e.root,23),24).value,'800')
    def test_no_global_fields(self):
        with self.assertRaises(FormatError): self.doc(clm_ini_parse,'host=example.org')
    def test_duplicate(self):
        with self.assertRaises(FormatError): self.doc(clm_ini_parse,'[section]\nhost=a\nhost=b\n')
