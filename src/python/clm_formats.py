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

"""CLM configuration formats: a shared lexer, ID-mapped container tree and codecs.

Persistent tree nodes are backed exclusively by CLM object containers. No external format readers or serializers are used.
Requires only clm_container.py and clm_id.py.
"""
from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from itertools import count
from typing import Any

from clm_container import (
    CONTAINER_SUCCESS, CONTAINER_ERROR_DUPLICATE_ID, CONTAINER_TYPE_OBJECT,
    clm_container_create, clm_container_add_object, clm_container_get_object,
    clm_container_first, clm_container_next, clm_container_destroy,
)
from clm_id import clm_id_get

class FormatError(ValueError):
    """Invalid input, unsupported structure or missing field-ID registration."""

@dataclass(frozen=True)
class LexRules:
    """Reusable lexical settings. Syntax/grammar is handled by format codecs."""
    delimiters: tuple[str, ...] = (":", ",", "=", "[", "]", "{", "}")
    inline_comment: str | None = None
    indent_matters: bool = False

@dataclass(frozen=True)
class Token:
    """One lexical token with its 1-based source line and column."""
    kind: str
    text: str
    line: int
    column: int

JSON_RULES = LexRules((":", ",", "{", "}", "[", "]"))
YAML_RULES = LexRules((":", "-", "[", "]", "{", "}", ","), "#", True)
TOML_RULES = LexRules(("=", "[", "]", ",", "{", "}"), "#")
INI_RULES = LexRules(("=", ":", "[", "]"), ";")
CSV_RULES = LexRules((",",), None)

def clm_lexer(text: str, rules: LexRules) -> list[Token]:
    """Tokenize quotes, delimiters and comments without splitting quoted content.

    This is a reusable lexical utility, not a complete JSON/YAML/TOML grammar.
    Comments are recognized only outside quoted strings. Newlines are emitted.
    """
    if not isinstance(text, str):
        raise TypeError("text must be a string")
    symbols = sorted(rules.delimiters, key=len, reverse=True)
    result: list[Token] = []
    for line_number, line in enumerate(text.splitlines(), 1):
        offset = len(line) - len(line.lstrip(" "))
        if rules.indent_matters:
            result.append(Token("INDENT", str(offset), line_number, 1))
        pos = 0
        while pos < len(line):
            if line[pos].isspace():
                pos += 1
                continue
            if rules.inline_comment and line.startswith(rules.inline_comment, pos):
                break
            start = pos
            if line[pos] in ('"', "'"):
                quote = line[pos]
                pos += 1
                escaped = False
                while pos < len(line):
                    ch = line[pos]
                    pos += 1
                    if escaped:
                        escaped = False
                    elif ch == "\\":
                        escaped = True
                    elif ch == quote:
                        break
                else:
                    raise FormatError(f"Unterminated quote at line {line_number}")
                result.append(Token("STRING", line[start:pos], line_number, start + 1))
                continue
            matched = next((s for s in symbols if line.startswith(s, pos)), None)
            if matched:
                result.append(Token("SYMBOL", matched, line_number, pos + 1))
                pos += len(matched)
                continue
            while pos < len(line):
                if line[pos].isspace() or (rules.inline_comment and line.startswith(rules.inline_comment, pos)):
                    break
                if any(line.startswith(s, pos) for s in symbols):
                    break
                pos += 1
            # Unknown punctuation is still content; ensure progress.
            if pos == start:
                pos += 1
            result.append(Token("WORD", line[start:pos], line_number, start + 1))
        result.append(Token("NEWLINE", "\n", line_number, len(line) + 1))
    return result

@dataclass(frozen=True)
class Field:
    """Original spelling plus value; ID itself is the CLM container entry key."""
    name: str
    value: "Node"

@dataclass(frozen=True)
class Node:
    """A scalar or a reference to an owned CLM container node."""
    kind: str                          # scalar | object | array
    value: Any = None                 # scalar value only
    container_id: int | None = None  # object/array only

@dataclass
class Document:
    """Root node and all owned CLM container IDs; destroy when no longer needed."""
    root: Node
    owned: list[int] = field(default_factory=list)
    destroyed: bool = False

_next_container_id = count(0x50000000)

def _new_container(doc: Document) -> int:
    for _ in range(10000):
        identifier = next(_next_container_id)
        status = clm_container_create(identifier, CONTAINER_TYPE_OBJECT)
        if status == CONTAINER_SUCCESS:
            doc.owned.append(identifier)
            return identifier
        if status != CONTAINER_ERROR_DUPLICATE_ID:
            raise FormatError(f"Container creation failed: {status}")
    raise FormatError("Unable to allocate CLM container ID")

def _put(container_id: int, key: int, value: Any) -> None:
    status = clm_container_add_object(container_id, key, value)
    if status != CONTAINER_SUCCESS:
        raise FormatError(f"Container {container_id}: cannot insert key {key} (status {status})")

def _items(container_id: int):
    """Iterate through a CLM container in insertion order.

    CLM's iterator uses a shared cursor per container; don't nest traversal
    of the same container or mutate it during traversal.
    """
    status, key = clm_container_first(container_id)
    while status == CONTAINER_SUCCESS:
        assert key is not None
        item_status, value = clm_container_get_object(container_id, key)
        if item_status != CONTAINER_SUCCESS:
            raise FormatError("Container changed during traversal")
        yield key, value
        status, key = clm_container_next(container_id)

def clm_tree_build(value: Any) -> Document:
    """Build a CLM-owned tree from internal _Object/_Array nodes or scalars.

    This public operation does not accept Python dictionaries. The format
    decoders construct _Object/_Array nodes directly, then delegate here.
    """
    return _assemble(value)

def clm_tree_destroy(doc: Document) -> None:
    """Release all CLM containers owned by this document (idempotent)."""
    if doc.destroyed:
        return
    for identifier in reversed(doc.owned):
        clm_container_destroy(identifier)
    doc.owned.clear()
    doc.destroyed = True

def clm_tree_get(node: Node, key: int) -> Node:
    """Read child node by numeric field ID or zero-based array index."""
    if node.kind not in ("object", "array") or node.container_id is None:
        raise FormatError("Scalar has no children")
    status, item = clm_container_get_object(node.container_id, key)
    if status != CONTAINER_SUCCESS:
        raise KeyError(key)
    return item.value if node.kind == "object" else item

def _check_document(doc: Document) -> None:
    if doc.destroyed:
        raise FormatError("Document already destroyed")

# Source-format independent intermediate nodes. Neither is a dictionary.
@dataclass
class _Object:
    fields: list[tuple[str, Any]] = field(default_factory=list)

@dataclass
class _Array:
    values: list[Any] = field(default_factory=list)

def _assemble(value: Any) -> Document:
    """Construct a persistent CLM tree without an intermediate Python dictionary."""
    doc = Document(Node('scalar'))

    # noinspection shadowing-names
    def put(value: Any) -> Node:
        if isinstance(value, _Object):
            cid = _new_container(doc)
            for name, child in value.fields:
                try:
                    fid = clm_id_get(name)
                except KeyError as exc:
                    raise FormatError(f'Unregistered field: {name!r}') from exc
                _put(cid, fid, Field(name, put(child)))
            return Node('object', container_id=cid)
        if isinstance(value, _Array):
            cid = _new_container(doc)
            for i, child in enumerate(value.values):
                _put(cid, i, put(child))
            return Node('array', container_id=cid)
        if value is None or type(value) in (str, bool, int, float):
            return Node('scalar', value)
        raise FormatError(f'Unsupported value type: {type(value).__name__}')
    try:
        doc.root = put(value)
    except Exception:
        clm_tree_destroy(doc)
        raise
    return doc

def _quote(value: str) -> str:
    """Escape a string according to JSON-compatible double-quote rules."""
    out = ['"']
    for c in value:
        n = ord(c)
        if c == '"': out.append('\\"')
        elif c == '\\': out.append('\\\\')
        elif c == '\n': out.append('\\n')
        elif c == '\r': out.append('\\r')
        elif c == '\t': out.append('\\t')
        elif c == '\b': out.append('\\b')
        elif c == '\f': out.append('\\f')
        elif n < 32: out.append(f'\\u{n:04x}')
        else: out.append(c)
    return ''.join(out) + '"'

def _unquote(text: str) -> str:
    """Decode standard JSON string escapes, rejecting malformed escapes."""
    if len(text) < 2 or text[0] != '"' or text[-1] != '"':
        raise FormatError('Expected double-quoted string')
    result = []
    i = 1
    while i < len(text) - 1:
        ch = text[i]
        if ch != '\\':
            if ord(ch) < 32: raise FormatError('Unescaped control character')
            result.append(ch); i += 1; continue
        i += 1
        if i >= len(text) - 1: raise FormatError('Incomplete string escape')
        escape = text[i]
        if escape in '"\\/': result.append(escape)
        elif escape in 'bfnrt': result.append({'b':'\b','f':'\f','n':'\n','r':'\r','t':'\t'}[escape])
        elif escape == 'u':
            digits = text[i+1:i+5]
            if len(digits) != 4 or not all(c in '0123456789abcdefABCDEF' for c in digits):
                raise FormatError('Invalid Unicode escape')
            cp = int(digits, 16); i += 4
            if 0xD800 <= cp <= 0xDBFF:
                if text[i+1:i+3] != '\\u': raise FormatError('Missing low surrogate')
                lower = text[i+3:i+7]
                if len(lower) != 4 or not all(c in '0123456789abcdefABCDEF' for c in lower):
                    raise FormatError('Invalid low surrogate')
                low = int(lower, 16)
                if not (0xDC00 <= low <= 0xDFFF): raise FormatError('Invalid low surrogate')
                cp = 0x10000 + (cp - 0xD800) * 1024 + low - 0xDC00
                i += 6
            elif 0xDC00 <= cp <= 0xDFFF: raise FormatError('Unpaired surrogate')
            result.append(chr(cp))
        else: raise FormatError(f'Unknown string escape: \\{escape}')
        i += 1
    return ''.join(result)

@dataclass(frozen=True)
class _LexToken:
    kind: str
    text: str
    line: int
    column: int

def _tokenize(text: str, punctuation: str, comments: str | None = None) -> list[_LexToken]:
    """Shared character-by-character lexer for quoted text and structural tokens."""
    result = []; i = 0; line = 1; column = 1
    while i < len(text):
        c = text[i]
        if c == '\n': line += 1; column = 1; i += 1; continue
        if c.isspace(): i += 1; column += 1; continue
        if comments and c == comments:
            while i < len(text) and text[i] != '\n': i += 1
            continue
        start, col = i, column
        if c in ('"', "'"):
            quote = c; i += 1; column += 1
            while i < len(text):
                if text[i] == '\\' and quote == '"':
                    i += 2; column += 2; continue
                if text[i] == quote: i += 1; column += 1; break
                if text[i] == '\n': raise FormatError(f'Unterminated string at {line}:{col}')
                i += 1; column += 1
            else: raise FormatError(f'Unterminated string at {line}:{col}')
            result.append(_LexToken('STRING' if quote == '"' else 'SINGLE', text[start:i], line, col))
        elif c in punctuation:
            result.append(_LexToken('SYMBOL', c, line, column)); i += 1; column += 1
        else:
            while i < len(text) and not text[i].isspace() and text[i] not in punctuation and not (comments and text[i] == comments):
                i += 1; column += 1
            result.append(_LexToken('WORD', text[start:i], line, col))
    result.append(_LexToken('EOF', '', line, column))
    return result

_NUMBER = re.compile(r'-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\Z')

def _number(text: str) -> Any:
    if not _NUMBER.fullmatch(text): raise FormatError(f'Invalid number: {text}')
    return float(text) if any(c in text for c in '.eE') else int(text)

class _ValueParser:
    """Small recursive-descent parser for JSON and TOML-like inline values."""
    def __init__(self, text: str, mode: str = 'json'):
        self.mode = mode
        self.tokens = _tokenize(text, '{}[],:=' if mode != 'json' else '{}[],:')
        self.pos = 0
    def peek(self): return self.tokens[self.pos]
    def take(self, expected=None):
        t = self.peek()
        if expected is not None and t.text != expected:
            raise FormatError(f'Expected {expected!r} at {t.line}:{t.column}, got {t.text!r}')
        if t.kind == 'EOF': raise FormatError('Unexpected end of input')
        self.pos += 1
        return t
    def parse(self):
        value = self.value()
        if self.peek().kind != 'EOF': raise FormatError(f'Unexpected token {self.peek().text!r}')
        return value
    def value(self):
        t = self.take()
        if t.text == '{':
            obj = _Object(); seen = set()
            if self.peek().text == '}': self.take('}'); return obj
            while True:
                k = self.take()
                if self.mode == 'json' and k.kind != 'STRING':
                    raise FormatError('JSON object keys must be double quoted')
                if k.kind not in ('STRING', 'SINGLE', 'WORD'): raise FormatError('Invalid object key')
                name = _unquote(k.text) if k.kind == 'STRING' else k.text[1:-1] if k.kind == 'SINGLE' else k.text
                self.take(':' if self.mode == 'json' else '=')
                if name in seen: raise FormatError(f'Duplicate field: {name}')
                seen.add(name); obj.fields.append((name, self.value()))
                if self.peek().text == '}': self.take('}'); return obj
                self.take(',')
                if self.peek().text == '}':
                    if self.mode == 'json': raise FormatError('Trailing comma in JSON object')
                    self.take('}'); return obj
        if t.text == '[':
            arr = _Array()
            if self.peek().text == ']': self.take(']'); return arr
            while True:
                arr.values.append(self.value())
                if self.peek().text == ']': self.take(']'); return arr
                self.take(',')
                if self.peek().text == ']':
                    if self.mode == 'json': raise FormatError('Trailing comma in JSON array')
                    self.take(']'); return arr
        if t.kind == 'STRING': return _unquote(t.text)
        if self.mode != 'json' and t.kind == 'SINGLE': return t.text[1:-1]
        if t.kind != 'WORD': raise FormatError(f'Unexpected token {t.text!r}')
        if t.text == 'true': return True
        if t.text == 'false': return False
        if t.text == 'null' and self.mode == 'json': return None
        if _NUMBER.fullmatch(t.text): return _number(t.text)
        raise FormatError(f'Unsupported {self.mode} literal: {t.text!r}')

def _strip_comment(line: str, marker: str) -> str:
    """Strip line comments only when outside quoted strings."""
    quoted = None; escaped = False
    for i, c in enumerate(line):
        if escaped: escaped = False; continue
        if c == '\\' and quoted == '"': escaped = True; continue
        if c in ('"', "'"):
            if quoted == c: quoted = None
            elif quoted is None: quoted = c
        elif c == marker and quoted is None: return line[:i]
    return line

def _split_assignment(text: str, marker: str) -> tuple[str, str]:
    """Split a field assignment outside quoted strings."""
    quoted = None; escaped = False
    for i,c in enumerate(text):
        if escaped: escaped = False; continue
        if c == '\\' and quoted == '"': escaped = True; continue
        if c in ('"', "'"):
            if quoted == c: quoted = None
            elif quoted is None: quoted = c
        elif c == marker and quoted is None: return text[:i].strip(), text[i+1:].strip()
    raise FormatError(f'Missing assignment {marker!r}: {text!r}')

def _field(obj: _Object, name: str, value: Any) -> None:
    if any(n == name for n,_ in obj.fields): raise FormatError(f'Duplicate field: {name}')
    obj.fields.append((name, value))

def _parse_scalar(text: str, mode: str) -> Any:
    text = text.strip()
    if not text: return ''
    if text.startswith('"'): return _unquote(text)
    if text.startswith("'") and text.endswith("'"):
        return text[1:-1].replace("''", "'")
    if text[0] == '[' or text[0] == '{':
        return _ValueParser(text, 'json' if mode == 'yaml' else 'toml').parse()
    if mode == 'ini': return text
    if text == 'true': return True
    if text == 'false': return False
    if mode == 'yaml' and text in ('null', '~'): return None
    if _NUMBER.fullmatch(text): return _number(text)
    if mode == 'yaml': return text
    raise FormatError(f'Unsupported {mode} literal: {text}')

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

def _scalar(value: Any, mode: str='json') -> str:
    if value is None:
        if mode == 'toml': raise FormatError('TOML does not support null')
        return 'null'
    if type(value) is bool: return 'true' if value else 'false'
    if type(value) in (int,float):
        if type(value) is float and not math.isfinite(value): raise FormatError('Non-finite numbers unsupported')
        return repr(value) if type(value) is float else str(value)
    if type(value) is str: return _quote(value)
    raise FormatError('Unsupported scalar')

# noinspection bad-argument-type
def _to_json(node: Node) -> str:
    if node.kind == 'scalar': return _scalar(node.value)
    if node.kind == 'array': return '['+','.join(_to_json(v) for _,v in _items(node.container_id))+']'
    return '{'+','.join(_quote(f.name)+':'+_to_json(f.value) for _,f in _items(node.container_id))+'}'

# noinspection bad-argument-type
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

def clm_format_parse(format_name: str, text: str) -> Document:
    """Parse a supported format using CLM-owned logic and CLM-owned tree storage.

    JSON supports RFC 8259 values. YAML, TOML and INI support documented
    practical subsets. CSV supports quoted fields including multiline values.
    Object field names must be pre-registered with clm_id.
    """
    if not isinstance(text,str): raise TypeError('text must be str')
    name=format_name.lower()
    if name=='json': value=_ValueParser(text).parse()
    elif name in ('yaml','yml'): value=_yaml_decode(text)
    elif name=='toml': value=_toml_decode(text)
    elif name=='ini': value=_ini_decode(text)
    elif name=='csv': value=_csv_decode(text)
    else: raise FormatError(f'Unsupported format: {format_name}')
    return clm_tree_build(value)

# noinspection bad-argument-type
def clm_format_serialize(format_name: str, document: Document) -> str:
    """Serialize a CLM-owned tree without delegating to format libraries."""
    _check_document(document)
    name=format_name.lower()
    if name=='json': return _to_json(document.root)
    if name in ('yaml','yml'):
        node=document.root
        if node.kind=='scalar': return _scalar(node.value)+'\n'
        if not list(_items(node.container_id)):
            return ('{}' if node.kind=='object' else '[]')+'\n'
        return '\n'.join(_yaml_lines(node))+'\n'
    if name=='toml': return '\n'.join(_toml_lines(document.root)).rstrip()+'\n'
    if name=='ini': return '\n'.join(_ini_lines(document.root)).rstrip()+'\n'
    if name=='csv': return _csv_dump(document.root)
    raise FormatError(f'Unsupported format: {format_name}')

def clm_json_parse(text: str) -> Document:
    """Parse JSON into CLM object, array, or scalar nodes."""
    return clm_format_parse('json',text)

def clm_yaml_parse(text: str) -> Document:
    """Parse the supported block-oriented YAML subset into CLM nodes."""
    return clm_format_parse('yaml',text)

def clm_toml_parse(text: str) -> Document:
    """Parse TOML tables and basic scalar/array values into CLM nodes."""
    return clm_format_parse('toml',text)

def clm_ini_parse(text: str) -> Document:
    """Parse sectioned INI into CLM nodes."""
    return clm_format_parse('ini',text)

def clm_csv_parse(text: str) -> Document:
    """Parse CSV into a CLM array of row objects."""
    return clm_format_parse('csv',text)

def clm_json_serialize(document: Document) -> str:
    """Serialize document to JSON."""
    return clm_format_serialize('json',document)

def clm_yaml_serialize(document: Document) -> str:
    """Serialize document to supported YAML subset."""
    return clm_format_serialize('yaml',document)

def clm_toml_serialize(document: Document) -> str:
    """Serialize document to supported TOML subset."""
    return clm_format_serialize('toml',document)

def clm_ini_serialize(document: Document) -> str:
    """Serialize document to sectioned INI."""
    return clm_format_serialize('ini',document)

def clm_csv_serialize(document: Document) -> str:
    """Serialize document to CSV."""
    return clm_format_serialize('csv',document)

class Formats:
    """Optional convenience facade over procedural CLM format functions."""
    parse=staticmethod(clm_format_parse)
    serialize=staticmethod(clm_format_serialize)
    tokenize=staticmethod(clm_lexer)
    build=staticmethod(clm_tree_build)
    destroy=staticmethod(clm_tree_destroy)
    get=staticmethod(clm_tree_get)
