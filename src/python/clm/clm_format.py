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

"""Format-independent CLM tree, lexer, tokens, scalar and inline parsing helpers.

Object names must be registered in clm_id before building/parsing documents.
Documents own their CLM containers; destroy them explicitly.
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


class Format:
    tokenize = staticmethod(clm_lexer)
    build = staticmethod(clm_tree_build)
    destroy = staticmethod(clm_tree_destroy)
    get = staticmethod(clm_tree_get)

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


def clm_format_parse(format_name: str, text: str) -> Document:
    """Dispatch by explicit format name; use individual codec modules for direct access."""
    name = format_name.lower()
    if name == "yml": name = "yaml"
    if name not in ("json", "yaml", "ini", "toml", "csv"):
        raise FormatError(f"Unsupported format: {format_name}")
    from importlib import import_module
    return getattr(import_module("clm_" + name), "clm_" + name + "_parse")(text)

def clm_format_serialize(format_name: str, document: Document) -> str:
    """Dispatch serialization to the format codec."""
    name = format_name.lower()
    if name == "yml": name = "yaml"
    if name not in ("json", "yaml", "ini", "toml", "csv"):
        raise FormatError(f"Unsupported format: {format_name}")
    from importlib import import_module
    return getattr(import_module("clm_" + name), "clm_" + name + "_serialize")(document)

class Formats(Format):
    parse = staticmethod(clm_format_parse)
    serialize = staticmethod(clm_format_serialize)
