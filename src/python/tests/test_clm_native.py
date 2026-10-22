# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm
# Contact: deeparchive@outlook.com
#
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
#
# Free to use, modify, redistribute, and bundle for any purpose.
# Attribution and retention of this notice are appreciated, not required.
#
# Provided AS IS, without warranty or liability.
#
# Don't start from scratch, bro.
# =============================================================================

"""Additional conformance tests for dependency-free CLM format codecs."""
import unittest
from clm_id import clm_id_clear, clm_id_register
from clm_formats import (
    FormatError,
    clm_format_parse,
    clm_format_serialize,
    clm_tree_destroy,
    clm_tree_get,
    clm_tree_build
)

class NativeFormatsTests(unittest.TestCase):
    def setUp(self):
        clm_id_clear()
        for n,i in (('section',10),('text',11),('items',12),('flag',13),('num',14),('host',15),('name',16)):
            clm_id_register(n,i)
        self.documents=[]

    def tearDown(self):
        for document in self.documents: clm_tree_destroy(document)
        clm_id_clear()

    def parse(self,fmt,s):
        d=clm_format_parse(fmt,s); self.documents.append(d); return d

    def test_no_native_dictionary_input(self):
        with self.assertRaises(FormatError): clm_tree_build({'host':'localhost'})

    def test_json_scalar_and_unicode_escapes(self):
        d=self.parse('json','{"text":"\\uD83D\\uDE00 \\n\\t"}'.replace('\\n','\\\\n') if False else r'{"text":"\uD83D\uDE00\n\t"}')
        self.assertEqual(clm_tree_get(d.root,11).value,'😀\n\t')
        self.assertEqual(self.parse('json', clm_format_serialize('json',d)).root.kind,'object')

    def test_json_strict_errors(self):
        malformed=['{"text":}','{"text":1,}', '{text:1}','{"text":01}', '[1,]',
                   '{"text":"\\x"}', '{"text":true} garbage', '{"text":1,"text":2}']
        for value in malformed:
            with self.subTest(value=value), self.assertRaises(FormatError): self.parse('json',value)

    def test_yaml_empty_and_indentation(self):
        for literal, kind in [('{}','object'),('[]','array')]:
            self.assertEqual(self.parse('yaml',literal).root.kind,kind)
        with self.assertRaises(FormatError): self.parse('yaml','section:\n  host: localhost\n   text: wrong\n')

    def test_yaml_comments_inside_quotes_and_nested_array(self):
        d=self.parse('yaml','section:\n    text: "# preserved" # ignored\n    items:\n      - "x"\n      - "y"\n')
        nested=clm_tree_get(d.root,10)
        self.assertEqual(clm_tree_get(nested,11).value,'# preserved')
        self.assertEqual(clm_tree_get(clm_tree_get(nested,12),1).value,'y')

    def test_toml_values(self):
        d=self.parse('toml','[section]\nflag = true\nnum = 123\nitems = [1,2,3]\n')
        section=clm_tree_get(d.root,10)
        self.assertEqual(clm_tree_get(section,14).value,123)
        self.assertEqual(clm_tree_get(clm_tree_get(section,12),2).value,3)
        self.assertTrue(clm_tree_get(section,13).value)

    def test_csv_escaped_quotes(self):
        d=self.parse('csv','name,text\nAda,"She said ""hi"""\n')
        self.assertEqual(clm_tree_get(clm_tree_get(d.root,0),11).value,'She said "hi"')
        self.assertIn('""hi""',clm_format_serialize('csv',d))

    def test_ini_duplicate_fields(self):
        with self.assertRaises(FormatError): self.parse('ini','[section]\nhost=x\nhost=y\n')

if __name__=='__main__': unittest.main()
