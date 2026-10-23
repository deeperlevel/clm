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

from clm_json import clm_json_parse, clm_json_serialize
class TestJSON(TestBase):
    def test_nested_roundtrip(self):
        d = self.doc(clm_json_parse, '{"name":"UNDER", "rows":[1,2], "flag":true}')
        self.assertEqual(clm_tree_get(d.root,11).value, 'UNDER')
        e = self.doc(clm_json_parse, clm_json_serialize(d))
        self.assertEqual(clm_tree_get(clm_tree_get(e.root,15),1).value,2)
    def test_unicode(self):
        d = self.doc(clm_json_parse, r'{"name":"\uD83D\uDE00"}')
        self.assertEqual(clm_tree_get(d.root,11).value,'😀')
    def test_invalid(self):
        for s in ['{"name":,}', '{name:1}', '{"name":1,}', '{"name":1,"name":2}', '[1,]', 'true trailing']:
            with self.subTest(s=s), self.assertRaises(FormatError): self.doc(clm_json_parse,s)
    def test_missing_registration(self):
        with self.assertRaises(FormatError): self.doc(clm_json_parse,'{"unknown":1}')
    def test_scalar_root(self):
        self.assertEqual(self.doc(clm_json_parse,'123').root.value,123)
