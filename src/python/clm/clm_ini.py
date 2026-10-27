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

"""INI codec using the shared CLM format document/tree representation."""
from __future__ import annotations
from clm_format import (FormatError, Document, Node, _Object, _Array, _ValueParser, _assemble,
    _items, _quote, _unquote, _scalar, _strip_comment, _split_assignment, _field, _parse_scalar, _check_document)

def _ini_decode(text: str) -> _Object:
    """Parse sectioned INI with string-only fields and no interpolation."""
    root = _Object(); current = None
    for ln, original in enumerate(text.splitlines(),1):
        line = original.strip()
        if not line or line.startswith(('#',';')): continue
        if line.startswith('['):
            if not line.endswith(']'): raise FormatError(f'Malformed INI section at {ln}')
            current = _Object(); _field(root,line[1:-1].strip(),current)
        else:
            if current is None: raise FormatError('INI assignments require a section')
            i = min((n for n in (line.find('='), line.find(':')) if n >= 0), default=-1)
            if i < 0: raise FormatError(f'Invalid INI line {ln}')
            _field(current,line[:i].strip(),line[i+1:].strip())
    return root

def _ini_lines(root: Node) -> list[str]:
    if root.kind != 'object': raise FormatError('INI requires section root')
    lines=[]
    for _,section in _items(root.container_id):
        if section.value.kind != 'object': raise FormatError('INI section must be object')
        if not section.name or any(c in section.name for c in '\r\n[]'): raise FormatError('Invalid INI section')
        if lines: lines.append('')
        lines.append(f'[{section.name}]')
        for _,f in _items(section.value.container_id):
            if f.value.kind != 'scalar' or any(c in f.name for c in '\r\n=:'):
                raise FormatError('Invalid INI entry')
            v = '' if f.value.value is None else str(f.value.value)
            if '\n' in v or '\r' in v: raise FormatError('INI multiline values unsupported')
            lines.append(f'{f.name} = {v}')
    return lines


def clm_ini_parse(text: str) -> Document:
    """Parse INI into a CLM-owned document."""
    if not isinstance(text, str):
        raise TypeError("text must be str")
    return _assemble(_ini_decode(text))

def clm_ini_serialize(document: Document) -> str:
    """Serialize a CLM document to the supported INI syntax."""
    _check_document(document)
    return "\n".join(_ini_lines(document.root)).rstrip() + "\n"

class INI:
    parse = staticmethod(clm_ini_parse)
    serialize = staticmethod(clm_ini_serialize)
