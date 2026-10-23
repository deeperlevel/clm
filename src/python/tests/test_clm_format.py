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

from clm_format import LexRules, clm_lexer, clm_tree_build, clm_format_parse, clm_format_serialize

class TestFormat(TestBase):
    def test_lexer_quotes_and_comments(self):
        ts = clm_lexer('name="a#b" # comment', LexRules(("=",), "#"))
        self.assertEqual([t.text for t in ts if t.kind != "NEWLINE"], ["name", "=", '"a#b"'])
    def test_lexer_locations(self):
        t = clm_lexer('name=1', LexRules(("=",)))
        self.assertEqual((t[0].line, t[0].column), (1, 1))
    def test_tree_scalar_and_destroy(self):
        d = self.doc(clm_tree_build, 15)
        self.assertEqual(d.root.value, 15)
        clm_tree_destroy(d)
        clm_tree_destroy(d)
        with self.assertRaises(FormatError): clm_format_serialize('json', d)
    def test_dict_not_accepted(self):
        with self.assertRaises(FormatError): clm_tree_build({'name': 'bad'})
    def test_dispatch_json(self):
        d = self.doc(lambda s: clm_format_parse('json',s), '{"name":"UNDER"}')
        self.assertIn('UNDER', clm_format_serialize('json', d))
    def test_unsupported(self):
        with self.assertRaises(FormatError): clm_format_parse('xml', '<x/>')
