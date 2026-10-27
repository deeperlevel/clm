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

"""YAML codec using the shared CLM format document/tree representation."""
from __future__ import annotations
from clm_format import (FormatError, Document, Node, _Object, _Array, _ValueParser, _assemble,
    _items, _quote, _unquote, _scalar, _strip_comment, _split_assignment, _field, _parse_scalar, _check_document)

def _yaml_decode(text: str) -> Any:
    """Parse a small block YAML subset (mappings, sequences, quoted/scalar values)."""
    lines = []
    for ln, original in enumerate(text.splitlines(),1):
        line = _strip_comment(original, '#').rstrip()
        if not line.strip() or line.strip() in ('---','...'): continue
        if '\t' in line[:len(line)-len(line.lstrip())]: raise FormatError('YAML indentation must use spaces')
        indent = len(line)-len(line.lstrip(' '))
        lines.append((indent, line.strip(), ln))
    if not lines: return None
    if len(lines) == 1 and lines[0][1] in ('{}', '[]'):
        return _Object() if lines[0][1] == '{}' else _Array()
    if len(lines) == 1 and not lines[0][1].startswith('-') and ':' not in lines[0][1]:
        return _parse_scalar(lines[0][1], 'yaml')

    # noinspection shadowing-names,unresolved-references,bad-argument-type,unbound-local-variable
    def block(pos: int, indent: int):
        if pos >= len(lines) or lines[pos][0] != indent: raise FormatError('Invalid YAML indentation')
        sequence = lines[pos][1] == '-' or lines[pos][1].startswith('- ')
        result = _Array() if sequence else _Object()
        while pos < len(lines) and lines[pos][0] == indent:
            _, content, lineno = lines[pos]
            if sequence:
                if not (content == '-' or content.startswith('- ')): raise FormatError(f'Mixed YAML layout at line {lineno}')
                rest = content[1:].strip()
            else:
                if content.startswith('- '): raise FormatError(f'Mixed YAML layout at line {lineno}')
                key, rest = _split_assignment(content, ':')
                key = _parse_scalar(key, 'ini') if not key.startswith('"') else _unquote(key)
            pos += 1
            if rest:
                child = _parse_scalar(rest, 'yaml')
                if pos < len(lines) and lines[pos][0] > indent:
                    raise FormatError(f'Unexpected indentation at line {lines[pos][2]}')
            elif pos < len(lines) and lines[pos][0] > indent:
                child, pos = block(pos, lines[pos][0])
            else:
                child = None
            if sequence: result.values.append(child)
            else: _field(result, key, child)
        if pos < len(lines) and lines[pos][0] > indent: raise FormatError(f'Unexpected indentation at line {lines[pos][2]}')
        return result, pos

    value, pos = block(0, lines[0][0])
    if pos != len(lines): raise FormatError(f'Invalid YAML indentation at line {lines[pos][2]}')
    return value

def _yaml_lines(node: Node, indent: int=0) -> list[str]:
    prefix=' '*indent; lines=[]
    if node.kind == 'scalar': return [prefix+_scalar(node.value)]
    if node.kind == 'object':
        for _,f in _items(node.container_id):
            v=f.value; key=_quote(f.name)
            if v.kind == 'scalar': lines.append(f'{prefix}{key}: {_scalar(v.value)}')
            elif not list(_items(v.container_id)):
                lines.append(f'{prefix}{key}: '+ ('{}' if v.kind == 'object' else '[]'))
            else:
                lines.append(f'{prefix}{key}:'); lines += _yaml_lines(v,indent+2)
    else:
        for _,v in _items(node.container_id):
            if v.kind == 'scalar': lines.append(f'{prefix}- {_scalar(v.value)}')
            elif not list(_items(v.container_id)):
                lines.append(f'{prefix}- '+ ('{}' if v.kind == 'object' else '[]'))
            else: lines.append(prefix+'-'); lines += _yaml_lines(v,indent+2)
    return lines

# noinspection bad-argument-type

def clm_yaml_parse(text: str) -> Document:
    """Parse YAML into a CLM-owned document."""
    if not isinstance(text, str):
        raise TypeError("text must be str")
    return _assemble(_yaml_decode(text))

def clm_yaml_serialize(document: Document) -> str:
    """Serialize a CLM document to the supported YAML syntax."""
    _check_document(document)
    return (_scalar(document.root.value) + "\n" if document.root.kind == "scalar" else
            (("{}" if document.root.kind == "object" else "[]") + "\n" if not list(_items(document.root.container_id)) else
             "\n".join(_yaml_lines(document.root)) + "\n"))

class YAML:
    parse = staticmethod(clm_yaml_parse)
    serialize = staticmethod(clm_yaml_serialize)
