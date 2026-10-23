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

"""TOML codec using the shared CLM format document/tree representation."""
from __future__ import annotations
from clm_format import (FormatError, Document, Node, _Object, _Array, _ValueParser, _assemble,
    _items, _quote, _unquote, _scalar, _strip_comment, _split_assignment, _field, _parse_scalar, _check_document)

def _toml_decode(text: str) -> _Object:
    """Parse TOML tables, dotted headers and basic scalars/arrays."""
    root = _Object(); current = root
    for ln, original in enumerate(text.splitlines(),1):
        line = _strip_comment(original, '#').strip()
        if not line: continue
        if line.startswith('['):
            if not line.endswith(']') or line.startswith('[['): raise FormatError(f'Unsupported TOML table header at line {ln}')
            path = [p.strip().strip('"') for p in line[1:-1].split('.')]
            current = root
            for name in path:
                if not name: raise FormatError('Empty table path')
                match = next((v for k,v in current.fields if k == name), None)
                if match is None: match = _Object(); _field(current,name,match)
                if not isinstance(match,_Object): raise FormatError('Table collides with scalar')
                current = match
        else:
            key, raw = _split_assignment(line,'=')
            if not key or '.' in key: raise FormatError('Dotted TOML keys unsupported')
            key = key.strip('"')
            _field(current,key,_parse_scalar(raw,'toml'))
    return root

def _toml_value(node: Node) -> str:
    if node.kind == 'scalar': return _scalar(node.value,'toml')
    if node.kind == 'array': return '['+', '.join(_toml_value(v) for _,v in _items(node.container_id))+']'
    raise FormatError('TOML objects must be tables')


def _toml_lines(root: Node) -> list[str]:
    if root.kind != 'object': raise FormatError('TOML root must be an object')
    lines=[]

    # noinspection bad-argument-type
    def visit(node: Node, path: tuple[str,...]):
        if path: lines.append('['+'.'.join(_quote(p) for p in path)+']')
        entries=list(_items(node.container_id))
        for _,f in entries:
            if f.value.kind != 'object': lines.append(f'{_quote(f.name)} = {_toml_value(f.value)}')
        for _,f in entries:
            if f.value.kind == 'object':
                if lines: lines.append('')
                visit(f.value,path+(f.name,))
    visit(root,())
    return lines

# noinspection bad-argument-type

def clm_toml_parse(text: str) -> Document:
    """Parse TOML into a CLM-owned document."""
    if not isinstance(text, str):
        raise TypeError("text must be str")
    return _assemble(_toml_decode(text))

def clm_toml_serialize(document: Document) -> str:
    """Serialize a CLM document to the supported TOML syntax."""
    _check_document(document)
    return "\n".join(_toml_lines(document.root)).rstrip() + "\n"

class TOML:
    parse = staticmethod(clm_toml_parse)
    serialize = staticmethod(clm_toml_serialize)
