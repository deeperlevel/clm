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

from clm_yaml import clm_yaml_parse, clm_yaml_serialize
class TestYAML(TestBase):
    def test_nested_mapping_and_sequence(self):
        d=self.doc(clm_yaml_parse,'person:\n  age: 22\n  rows:\n    - one\n    - two\n')
        person=clm_tree_get(d.root,18)
        self.assertEqual(clm_tree_get(person,19).value,22)
        e=self.doc(clm_yaml_parse,clm_yaml_serialize(d))
        self.assertEqual(clm_tree_get(clm_tree_get(e.root,18),19).value,22)
    def test_comment_in_quotes(self):
        d=self.doc(clm_yaml_parse,'name: "a#b" # hi\n')
        self.assertEqual(clm_tree_get(d.root,11).value,'a#b')
    def test_invalid_indentation(self):
        with self.assertRaises(FormatError): self.doc(clm_yaml_parse,'person:\n age: 2\n  name: x\n')
    def test_empty(self):
        self.assertEqual(self.doc(clm_yaml_parse,'[]').root.kind,'array')
