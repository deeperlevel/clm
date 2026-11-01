from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).parent
CLM = ROOT / "clm"


@dataclass
class Function:
    start: int
    open_brace: int
    end: int
    name: str
    signature: str
    body: str


def mask_comments_and_literals(text: str) -> str:
    chars = list(text)
    i = 0
    state = "code"
    while i < len(text):
        ch = text[i]
        next_ch = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if ch == "/" and next_ch == "*":
                chars[i] = chars[i + 1] = " "
                state = "block"
                i += 2
                continue
            if ch == "/" and next_ch == "/":
                chars[i] = chars[i + 1] = " "
                state = "line"
                i += 2
                continue
            if ch == '"':
                chars[i] = " "
                state = "string"
            elif ch == "'":
                chars[i] = " "
                state = "char"
        elif state == "block":
            if ch == "*" and next_ch == "/":
                chars[i] = chars[i + 1] = " "
                state = "code"
                i += 2
                continue
            if ch != "\n":
                chars[i] = " "
        elif state == "line":
            if ch == "\n":
                state = "code"
            else:
                chars[i] = " "
        else:
            if ch == "\\" and i + 1 < len(text):
                chars[i] = " "
                if text[i + 1] != "\n":
                    chars[i + 1] = " "
                i += 2
                continue
            if (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                chars[i] = " "
                state = "code"
            elif ch != "\n":
                chars[i] = " "
        i += 1
    return "".join(chars)


def matching(text: str, opening: int, left: str, right: str) -> int:
    depth = 0
    for i in range(opening, len(text)):
        if text[i] == left:
            depth += 1
        elif text[i] == right:
            depth -= 1
            if depth == 0:
                return i
    raise ValueError(f"unmatched {left} at offset {opening}")


def find_inline_definitions(text: str) -> list[Function]:
    masked = mask_comments_and_literals(text)
    result: list[Function] = []
    pattern = re.compile(r"\bstatic\s+inline\b")
    position = 0
    while match := pattern.search(masked, position):
        opening = masked.find("(", match.end())
        if opening < 0:
            break
        between = masked[match.end():opening]
        name_match = re.search(r"([A-Za-z_]\w*)\s*$", between)
        if name_match is None:
            position = match.end()
            continue
        closing = matching(masked, opening, "(", ")")
        body_open = closing + 1
        while body_open < len(masked) and masked[body_open].isspace():
            body_open += 1
        if body_open >= len(masked) or masked[body_open] != "{":
            position = closing + 1
            continue
        body_close = matching(masked, body_open, "{", "}")
        signature = text[match.start():body_open].rstrip()
        result.append(Function(
            match.start(), body_open, body_close + 1,
            name_match.group(1), signature, text[body_open:body_close + 1]))
        position = body_close + 1
    return result


def implementation_blocks(text: str) -> list[tuple[int, int, str]]:
    lines = text.splitlines(keepends=True)
    offsets = []
    total = 0
    for line in lines:
        offsets.append(total)
        total += len(line)
    masked = mask_comments_and_literals(text)
    directives = []
    for index, line in enumerate(lines):
        line_masked = masked[offsets[index]:offsets[index] + len(line)]
        if re.search(r"^\s*#\s*(?:ifdef|if)\b.*(?:CLM_[A-Za-z0-9_]*IMPLEMENTATION|CLM_IMPLEMENTATION)", line_masked):
            depth = 1
            for end_index in range(index + 1, len(lines)):
                candidate = masked[offsets[end_index]:offsets[end_index] + len(lines[end_index])]
                if re.match(r"^\s*#\s*(?:if|ifdef|ifndef)\b", candidate):
                    depth += 1
                elif re.match(r"^\s*#\s*endif\b", candidate):
                    depth -= 1
                    if depth == 0:
                        start = offsets[index]
                        end = offsets[end_index] + len(lines[end_index])
                        inner_start = start + len(line)
                        inner_end = offsets[end_index]
                        directives.append((start, end, text[inner_start:inner_end]))
                        break
            else:
                raise ValueError(f"unterminated implementation guard at line {index + 1}")
    return directives


def condition_expression(text: str, position: int) -> str | None:
    prefix = text[:position]
    masked = mask_comments_and_literals(prefix)
    stack: list[dict[str, list[str] | str]] = []
    for line in masked.splitlines():
        match = re.match(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$", line)
        if not match:
            continue
        directive, expression = match.group(1), match.group(2).strip()
        if directive == "if":
            stack.append({"branches": [expression], "current": f"({expression})"})
        elif directive == "ifdef":
            value = f"defined({expression})"
            stack.append({"branches": [value], "current": value})
        elif directive == "ifndef":
            value = f"!defined({expression})"
            stack.append({"branches": [value], "current": value})
        elif directive == "elif" and stack:
            branches = stack[-1]["branches"]
            assert isinstance(branches, list)
            prior = " || ".join(f"({item})" for item in branches)
            branches.append(expression)
            stack[-1]["current"] = f"!({prior}) && ({expression})"
        elif directive == "else" and stack:
            branches = stack[-1]["branches"]
            assert isinstance(branches, list)
            prior = " || ".join(f"({item})" for item in branches)
            branches.append("1")
            stack[-1]["current"] = f"!({prior})"
        elif directive == "endif" and stack:
            stack.pop()
    if not stack:
        return None
    return " && ".join(f"({frame['current']})" for frame in stack)


def external_signature(signature: str) -> str:
    return re.sub(r"\bstatic\s+inline\s+", "", signature, count=1).rstrip()


def is_private(name: str) -> bool:
    return name.startswith("clm_private_") or "_internal_" in name


def prototype_names(text: str) -> set[str]:
    masked = mask_comments_and_literals(text)
    return set(re.findall(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;", masked, re.S))


def apply_replacements(text: str, replacements: list[tuple[int, int, str]]) -> str:
    for start, end, value in sorted(replacements, reverse=True):
        text = text[:start] + value + text[end:]
    return text


def extract_static_objects(text: str) -> tuple[str, str]:
    masked = mask_comments_and_literals(text)
    replacements = []
    objects = []
    depth = 0
    line_start = 0
    i = 0
    while i < len(masked):
        if masked[i] == "\n":
            line_start = i + 1
        if depth == 0 and i == line_start:
            match = re.match(r"[ \t]*static\s+const\b", masked[i:])
            if match:
                start = i + len(match.group(0)) - len(match.group(0).lstrip())
                equal = masked.find("=", i + match.end())
                semi = masked.find(";", i + match.end())
                if equal >= 0 and semi >= 0 and equal < semi:
                    brace_depth = 0
                    end = semi
                    for cursor in range(equal + 1, len(masked)):
                        if masked[cursor] == "{":
                            brace_depth += 1
                        elif masked[cursor] == "}":
                            brace_depth -= 1
                        elif masked[cursor] == ";" and brace_depth == 0:
                            end = cursor
                            break
                    objects.append(text[start:end + 1])
                    replacements.append((start, end + 1, ""))
                    i = end + 1
                    continue
        if masked[i] == "{":
            depth += 1
        elif masked[i] == "}":
            depth -= 1
        i += 1
    return apply_replacements(text, replacements), "\n\n".join(objects)


def convert_header(path: Path, write: bool) -> tuple[int, int, int]:
    original = path.read_text(encoding="utf-8")
    blocks = implementation_blocks(original)
    inline_functions = find_inline_definitions(original)
    block_ranges = [(start, end) for start, end, _ in blocks]
    in_block = lambda position: any(start < position < end for start, end in block_ranges)

    header_replacements: list[tuple[int, int, str]] = []
    source_functions: list[str] = []
    generated_declarations: list[str] = []

    for function in inline_functions:
        if in_block(function.start):
            continue
        condition = condition_expression(original, function.start)
        declaration = external_signature(function.signature) + ";"
        source_definition = external_signature(function.signature) + " " + function.body
        if condition:
            declaration = f"#if {condition}\n{declaration}\n#endif"
            source_definition = f"#if {condition}\n{source_definition}\n#endif"
        header_replacements.append((function.start, function.end, declaration))
        source_functions.append(source_definition)

    # Replace implementation-macro regions with declarations for non-private
    # inline APIs; their state, types, and bodies move into the module source.
    source_blocks = []
    for start, end, inner in blocks:
        block_functions = find_inline_definitions(inner)
        existing = prototype_names(original[:start] + original[end:])
        replacements = []
        for function in block_functions:
            private = is_private(function.name) and function.name not in existing
            if private:
                continue
            sig = external_signature(function.signature) + ";"
            if function.name not in existing:
                generated_declarations.append(sig)
        # The source keeps the implementation block intact so its nested
        # platform branches and implementation-only declarations stay paired.
        block_source = inner
        for function in reversed(block_functions):
            if not (is_private(function.name) and function.name not in existing):
                prefix = function.signature
                replacement = external_signature(prefix)
                block_source = block_source[:function.start] + replacement + block_source[function.open_brace:]
        source_blocks.append(block_source)
        header_replacements.append((start, end, "\n".join(generated_declarations)))
        generated_declarations.clear()

    header = apply_replacements(original, header_replacements)

    # Move module-private file-scope constants out of headers.
    header, static_objects = extract_static_objects(header)

    # Turn remaining declarations into ordinary external declarations.
    masked_header = mask_comments_and_literals(header)
    prototype_replacements = []
    for match in re.finditer(r"\bstatic\s+inline\s+", masked_header):
        prototype_replacements.append((match.start(), match.end(), ""))
    header = apply_replacements(header, prototype_replacements)

    source = f'#include "{path.name}"\n\n'
    if static_objects:
        source += static_objects + "\n\n"
    if source_blocks:
        source += "\n\n".join(block.strip() for block in source_blocks) + "\n\n"
    if source_functions:
        source += "\n\n".join(source_functions) + "\n"

    # Add generated APIs after the public types and existing declarations.
    if generated_declarations:
        header += "\n" + "\n".join(generated_declarations) + "\n"

    remaining = find_inline_definitions(header)
    if remaining:
        raise ValueError(f"{path.name}: {len(remaining)} inline bodies remain")
    if write:
        path.write_text(header, encoding="utf-8", newline="")
        (path.parent / f"{path.stem}.c").write_text(source, encoding="utf-8", newline="")
    return len(inline_functions), len(blocks), len(source_functions) + sum(
        len(find_inline_definitions(block)) for _, _, block in blocks)


def main() -> None:
    write = "--write" in sys.argv
    headers = sorted(path for path in CLM.glob("*.h") if path.name != "math.h")
    totals = [0, 0, 0]
    for path in headers:
        result = convert_header(path, write)
        totals = [a + b for a, b in zip(totals, result)]
        if any(result):
            print(f"{path.name}: inline={result[0]}, impl-blocks={result[1]}, moved={result[2]}")
    print(f"TOTAL: inline={totals[0]}, impl-blocks={totals[1]}, moved={totals[2]}")
    print("DRY RUN ONLY; pass --write to modify files." if not write else "Wrote module sources and declaration headers.")


if __name__ == "__main__":
    main()
