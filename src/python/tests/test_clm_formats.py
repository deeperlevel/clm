"""Behavior and round-trip tests for shared CLM lexer/tree and five format codecs."""
import csv
import io
import json
import tomllib
import unittest

from clm_id import clm_id_clear, clm_id_register, clm_id_set_case_sensitive
from clm_formats import (
    FormatError, JSON_RULES, YAML_RULES, clm_lexer, clm_tree_get,
    clm_tree_destroy, clm_tree_build, clm_format_parse, clm_format_serialize,
    clm_json_parse, clm_json_serialize, Formats,
)


class FormatsTests(unittest.TestCase):
    def setUp(self):
        clm_id_clear()
        clm_id_set_case_sensitive(True)
        for idx, name in enumerate(("WebApi", "Repository", "host", "port", "enabled", "tags",
                                     "name", "age", "note", "extra", "value", "left", "right"), 1):
            clm_id_register(name, idx)
        self.docs = []

    def tearDown(self):
        for doc in self.docs:
            clm_tree_destroy(doc)
        clm_id_clear()

    def parse(self, name, text):
        doc = clm_format_parse(name, text)
        self.docs.append(doc)
        return doc

    def test_lexer_quote_comment_and_positions(self):
        toks = clm_lexer('host: "abc#def" # comment\n  tags: ["x,y", 4]', YAML_RULES)
        self.assertIn('"abc#def"', [t.text for t in toks])
        self.assertNotIn('comment', [t.text for t in toks])
        self.assertIn('"x,y"', [t.text for t in toks])
        self.assertEqual([t.text for t in toks if t.kind == 'INDENT'], ['0', '2'])
        self.assertEqual(toks[0].line, 1)
        with self.assertRaises(FormatError):
            clm_lexer('"unfinished', JSON_RULES)

    def test_json_nesting_scalars_and_escape_roundtrip(self):
        raw = '{"WebApi":{"host":"a\\\"b","port":8080,"enabled":true,"tags":["x",null,3]},"Repository":{"host":"db"}}'
        doc = self.parse('json', raw)
        web = clm_tree_get(doc.root, 1)
        self.assertEqual(clm_tree_get(web, 3).value, 'a"b')
        self.assertEqual(clm_tree_get(web, 4).value, 8080)
        self.assertIs(clm_tree_get(web, 5).value, True)
        self.assertIsNone(clm_tree_get(clm_tree_get(web, 6), 1).value)
        self.assertEqual(json.loads(clm_format_serialize('json', doc)), json.loads(raw))
        self.assertEqual(clm_tree_get(clm_tree_get(doc.root, 2), 3).value, 'db')

    def test_yaml_nested_and_arrays_roundtrip(self):
        raw = 'WebApi:\n  host: "local#host"\n  port: 8080\n  tags:\n    - alpha\n    - beta\nRepository:\n  host: localhost\n'
        doc = self.parse('yaml', raw)
        encoded = clm_format_serialize('yaml', doc)
        again = self.parse('yaml', encoded)
        self.assertEqual(clm_tree_get(clm_tree_get(again.root, 1), 3).value, 'local#host')
        self.assertEqual(clm_tree_get(clm_tree_get(clm_tree_get(again.root, 1), 6), 1).value, 'beta')

    def test_toml_sections_arrays_types_roundtrip(self):
        raw = '[WebApi]\nhost = "localhost"\nport = 8080\nenabled = true\ntags = ["a", "b"]\n[Repository]\nhost = "db"\n'
        doc = self.parse('toml', raw)
        encoded = clm_format_serialize('toml', doc)
        self.assertEqual(tomllib.loads(encoded), tomllib.loads(raw))

    def test_ini_sections_roundtrip(self):
        raw = '[WebApi]\nhost = localhost\nport = 8080\n[Repository]\nhost = db\n'
        doc = self.parse('ini', raw)
        encoded = clm_format_serialize('ini', doc)
        doc2 = self.parse('ini', encoded)
        self.assertEqual(clm_tree_get(clm_tree_get(doc2.root, 2), 3).value, 'db')
        self.assertEqual(clm_tree_get(clm_tree_get(doc2.root, 1), 4).value, '8080')

    def test_csv_rows_columns_quotes_and_empty(self):
        raw = 'name,age,note\nAda,32,"hello, world"\nBen,27,"a\nb"\n'
        doc = self.parse('csv', raw)
        first = clm_tree_get(doc.root, 0)
        self.assertEqual(clm_tree_get(first, 7).value, 'Ada')
        second = clm_tree_get(doc.root, 1)
        self.assertEqual(clm_tree_get(second, 9).value, 'a\nb')
        encoded = clm_format_serialize('csv', doc)
        self.assertEqual(list(csv.reader(io.StringIO(encoded))), list(csv.reader(io.StringIO(raw))))

    def test_name_lookup_required_and_container_rollback(self):
        with self.assertRaisesRegex(FormatError, 'Unregistered field'):
            clm_format_parse('json', '{"missing": 1}')

    def test_missing_nested_id_and_duplicate_alias(self):
        clm_id_register('extra_alias', 3)
        with self.assertRaises(FormatError):
            clm_format_parse('json', '{"host": "a", "extra_alias": "b"}')

    def test_destroy_idempotent_and_invalid_access(self):
        doc = self.parse('json', '{"host":"local"}')
        clm_tree_destroy(doc)
        clm_tree_destroy(doc)
        with self.assertRaises(FormatError):
            clm_json_serialize(doc)

    def test_unsupported_structure_and_formats(self):
        doc = self.parse('json', '{"host":null}')
        with self.assertRaises(FormatError):
            clm_format_serialize('toml', doc)
        with self.assertRaises(FormatError):
            clm_format_serialize('csv', doc)
        with self.assertRaises(FormatError):
            clm_format_parse('xml', '<a/>')
        with self.assertRaises(FormatError):
            clm_format_parse('csv', 'name,age\nAda\n')

    def test_convenience_and_direct_builder(self):
        doc = clm_format_parse('json', '{"host": "example"}')
        self.docs.append(doc)
        self.assertEqual(Formats.get(doc.root, 3).value, 'example')
        self.assertEqual(json.loads(Formats.serialize('json', doc)), {'host': 'example'})

    def test_empty_structures_and_scalar(self):
        a = self.parse('json', '{}')
        b = self.parse('json', '[]')
        c = self.parse('json', 'null')
        self.assertEqual(clm_format_serialize('yaml', a), '{}\n')
        self.assertEqual(clm_format_serialize('yaml', b), '[]\n')
        self.assertEqual(clm_json_serialize(c), 'null')


if __name__ == '__main__':
    unittest.main()
