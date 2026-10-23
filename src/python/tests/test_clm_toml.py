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

from clm_toml import clm_toml_parse, clm_toml_serialize
class TestTOML(TestBase):
    def test_table_roundtrip(self):
        d=self.doc(clm_toml_parse,'[game]\nplayers = 2\nflag = true\n')
        self.assertEqual(clm_tree_get(clm_tree_get(d.root,20),21).value,2)
        e=self.doc(clm_toml_parse,clm_toml_serialize(d))
        self.assertEqual(clm_tree_get(clm_tree_get(e.root,20),16).value,True)
    def test_array(self):
        d=self.doc(clm_toml_parse,'rows = [1,2,3]\n')
        self.assertEqual(clm_tree_get(clm_tree_get(d.root,15),2).value,3)
    def test_unsupported(self):
        with self.assertRaises(FormatError): self.doc(clm_toml_parse,'[[game]]\n')
