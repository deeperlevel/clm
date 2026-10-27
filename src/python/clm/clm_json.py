# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by Deeperlevel, developed with community contributions.
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

"""JSON codec using the shared CLM format document/tree representation."""
from __future__ import annotations
from clm_format import (FormatError, Document, Node, _Object, _Array, _ValueParser, _assemble,
    _items, _quote, _unquote, _scalar, _strip_comment, _split_assignment, _field, _parse_scalar, _check_document)

def _to_json(node: Node) -> str:
    if node.kind == 'scalar': return _scalar(node.value)
    if node.kind == 'array': return '['+','.join(_to_json(v) for _,v in _items(node.container_id))+']'
    return '{'+','.join(_quote(f.name)+':'+_to_json(f.value) for _,f in _items(node.container_id))+'}'

# noinspection bad-argument-type

def clm_json_parse(text: str) -> Document:
    """Parse JSON into a CLM-owned document."""
    if not isinstance(text, str):
        raise TypeError("text must be str")
    return _assemble(_ValueParser(text, "json").parse())

def clm_json_serialize(document: Document) -> str:
    """Serialize a CLM document to the supported JSON syntax."""
    _check_document(document)
    return _to_json(document.root)

class JSON:
    parse = staticmethod(clm_json_parse)
    serialize = staticmethod(clm_json_serialize)
