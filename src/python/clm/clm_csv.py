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

"""CSV codec using the shared CLM format document/tree representation."""
from __future__ import annotations
from clm_format import (FormatError, Document, Node, _Object, _Array, _ValueParser, _assemble,
    _items, _quote, _unquote, _scalar, _strip_comment, _split_assignment, _field, _parse_scalar, _check_document)

def _csv_records(text: str) -> list[list[str]]:
    """RFC 4180-style CSV state machine supporting quotes, commas and newlines."""
    rows=[]; row=[]; cell=[]; i=0; quoted=False; closed=False; at_start=True
    while i < len(text):
        c=text[i]
        if quoted:
            if c == '"':
                if i+1 < len(text) and text[i+1] == '"': cell.append('"'); i+=2; continue
                quoted=False; closed=True
            else: cell.append(c)
        elif c == '"':
            if not at_start: raise FormatError('Unexpected quote in CSV field')
            quoted=True; at_start=False
        elif c == ',':
            row.append(''.join(cell)); cell=[]; at_start=True; closed=False
        elif c in '\r\n':
            if c == '\r' and i+1 < len(text) and text[i+1] == '\n': i+=1
            row.append(''.join(cell)); rows.append(row)
            row=[]; cell=[]; at_start=True; closed=False
        else:
            if closed: raise FormatError('Unexpected text after quoted CSV value')
            cell.append(c); at_start=False
        i+=1
    if quoted: raise FormatError('Unterminated CSV quoted cell')
    if row or cell or not at_start:
        row.append(''.join(cell)); rows.append(row)
    return rows

def _csv_decode(text: str) -> _Array:
    rows = _csv_records(text)
    if not rows: return _Array()
    headers=rows[0]
    if not headers or any(not h for h in headers) or len(set(headers)) != len(headers):
        raise FormatError('CSV requires unique nonempty headers')
    values=_Array()
    for i, row in enumerate(rows[1:],2):
        if len(row)!=len(headers): raise FormatError(f'CSV row {i} has wrong column count')
        values.values.append(_Object(list(zip(headers,row))))
    return values

def _csv_cell(text: str) -> str:
    return '"'+text.replace('"','""')+'"' if any(c in text for c in ',"\r\n') else text

# noinspection bad-argument-type
def _csv_dump(root: Node) -> str:
    if root.kind != 'array': raise FormatError('CSV root must be an array')
    rows=[n for _,n in _items(root.container_id)]
    if not rows: return ''
    if any(n.kind!='object' for n in rows): raise FormatError('CSV rows must be objects')
    headings=[f.name for _,f in _items(rows[0].container_id)]
    if not headings: raise FormatError('CSV requires columns')
    out=[','.join(_csv_cell(x) for x in headings)]
    for row in rows:
        values=[f for _,f in _items(row.container_id)]
        if [f.name for f in values] != headings: raise FormatError('CSV row headings differ')
        if any(f.value.kind!='scalar' for f in values): raise FormatError('CSV cells must be scalars')
        out.append(','.join(_csv_cell('' if f.value.value is None else str(f.value.value)) for f in values))
    return '\n'.join(out)+'\n'


def clm_csv_parse(text: str) -> Document:
    """Parse CSV into a CLM-owned document."""
    if not isinstance(text, str):
        raise TypeError("text must be str")
    return _assemble(_csv_decode(text))

def clm_csv_serialize(document: Document) -> str:
    """Serialize a CLM document to the supported CSV syntax."""
    _check_document(document)
    return _csv_dump(document.root)

class CSV:
    parse = staticmethod(clm_csv_parse)
    serialize = staticmethod(clm_csv_serialize)
