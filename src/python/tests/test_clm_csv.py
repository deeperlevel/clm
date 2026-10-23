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

from clm_csv import clm_csv_parse, clm_csv_serialize
class TestCSV(TestBase):
    def test_quoted_roundtrip(self):
        d=self.doc(clm_csv_parse,'name,role\n"Ada, A.","Dev ""Lead"""\n')
        row=clm_tree_get(d.root,0)
        self.assertEqual(clm_tree_get(row,11).value,'Ada, A.')
        e=self.doc(clm_csv_parse,clm_csv_serialize(d))
        self.assertEqual(clm_tree_get(clm_tree_get(e.root,0),22).value,'Dev "Lead"')
    def test_multiline(self):
        d=self.doc(clm_csv_parse,'name,role\nAda,"Two\nlines"\n')
        self.assertEqual(clm_tree_get(clm_tree_get(d.root,0),22).value,'Two\nlines')
    def test_bad_headers_and_rows(self):
        for s in ['name,name\nx,y\n','name,role\nonly\n']:
            with self.subTest(s=s), self.assertRaises(FormatError): self.doc(clm_csv_parse,s)
    def test_empty(self):
        d=self.doc(clm_csv_parse,'')
        self.assertEqual(clm_csv_serialize(d),'')
