from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).parent / "clm"
IDENT = re.compile(r"[A-Za-z_]\w*")


def mask_comments(text: str) -> str:
    def blank(match: re.Match[str]) -> str:
        return "".join("\n" if character == "\n" else " "
                       for character in match.group(0))
    return re.sub(r"/\*.*?\*/|//[^\n]*", blank, text, flags=re.S)


def matching_paren(text: str, opening: int) -> int:
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"unclosed parameter list at offset {opening}")


def split_args(args: str) -> list[str]:
    if not args.strip() or args.strip() == "void":
        return []
    result = []
    start = 0
    depth = 0
    for index, character in enumerate(args):
        if character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
        elif character == "," and depth == 0:
            result.append(args[start:index].strip())
            start = index + 1
    result.append(args[start:].strip())
    return result


def param_name(declaration: str) -> str | None:
    callback = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", declaration)
    if callback:
        return callback.group(1)
    declaration = re.sub(r"\[[^]]*\]", " ", declaration)
    words = IDENT.findall(declaration)
    words = [word for word in words if word not in {
        "const", "volatile", "restrict", "struct", "enum", "register"
    }]
    return words[-1] if len(words) >= 2 else None


def param_description(name: str, declaration: str, summary: str) -> str:
    lowered = name.lower()
    summary_lower = summary.lower()
    if lowered.startswith("out_"):
        return f"Receives the {lowered[4:].replace('_', ' ')}."
    if lowered in {"out", "buffer", "destination_buffer"}:
        return "Writable output buffer."
    if "(" in declaration and "*" in declaration:
        return "Callback invoked by the operation."
    if "*" in declaration:
        if "const" in declaration:
            return "Input data or buffer; not modified."
        return "Writable output or in/out pointer."
    if lowered.endswith("_id") or lowered == "id":
        resource = lowered.removesuffix("_id").replace("_", " ")
        return f"CLM ID identifying the {resource or 'object or resource'}."
    hints = {
        "count": "Number of elements or items.",
        "size": "Size in bytes or elements, as specified above.",
        "length": "Length in bytes or elements, as specified above.",
        "capacity": "Available capacity.",
        "index": "Zero-based element or worker index.",
        "offset": "Offset into the input data.",
        "depth": "Current nesting depth.",
        "state": "State value to inspect or update.",
        "encoding": "Encoding used for input or output bytes.",
        "callback": "Callback to invoke.",
        "user": "Caller-provided callback context.",
        "text": "Text to process.",
        "source": "Source value or resource.",
        "destination": "Destination resource.",
        "value": "Value to process.",
        "angle": "Angle in radians." if "radian" in summary_lower else "Angle to process.",
        "radians": "Angle in radians.",
        "degrees": "Angle in degrees.",
        "t": "Interpolation factor; see summary for range behavior.",
        "first": "First operand or source.",
        "second": "Second operand or source.",
        "a": "First input value.",
        "b": "Second input value.",
        "x": "Input value or coordinate.",
        "y": "Input value or coordinate.",
        "z": "Input value or coordinate.",
    }
    return hints.get(lowered, "Input parameter described above.")


def return_description(name: str, return_type: str, summary: str) -> str:
    compact = " ".join(return_type.split())
    summary = " ".join(summary.split()).strip()
    if summary.endswith("."):
        summary = summary[:-1]
    if summary.lower().startswith("returns "):
        returned = summary[8:]
        return returned[0].upper() + returned[1:] + "." if returned else "Result described above."
    if compact.endswith("bool") or compact == "bool":
        if summary.lower().startswith("tests whether "):
            return "True if " + summary[14:] + "; otherwise false."
        if summary.lower().startswith("returns whether "):
            return summary[8:] + "; otherwise false."
        return "Whether the condition described above holds."
    if "status" in compact:
        return "Success or error status, as described in the summary above."
    if compact in {"clm_int", "int"}:
        return "Operation status or value described in the summary above."
    if compact in {"size_t", "clm_uint", "clm_ulong", "uint64_t", "uint32_t"}:
        return "Computed size, count, or value described in the summary above."
    if compact in {"double", "float", "clm_double", "clm_float"}:
        lowered = name.lower()
        operations = {
            "sin": "Sine of the input angle, in radians.",
            "cos": "Cosine of the input angle, in radians.",
            "tan": "Tangent of the input angle, in radians.",
            "asin": "Inverse sine of the input value, in radians.",
            "acos": "Inverse cosine of the input value, in radians.",
            "atan2": "Angle in radians for the input coordinates.",
            "sqrt": "Square root of the input value.",
            "pow": "Input value raised to the specified power.",
            "exp": "Exponential of the input value.",
            "floor": "Input rounded toward negative infinity.",
            "ceil": "Input rounded toward positive infinity.",
            "abs": "Absolute value of the input.",
            "lerp": "Interpolated value.",
            "degrees": "Angle converted to degrees.",
            "radians": "Angle converted to radians.",
        }
        for suffix, description in operations.items():
            if lowered.endswith("_" + suffix) or lowered == "clm_math_" + suffix:
                return description
    if "*" in compact:
        return "Result pointer, or NULL when no result is available."
    if summary.lower().startswith("returns "):
        return summary[8:] + "."
    return "Computed result described in the summary above."


def doc_summary(doc: str) -> str:
    lines = []
    for line in doc.splitlines():
        line = re.sub(r"^\s*\*?\s?", "", line).strip()
        if line and not line.startswith("@"):
            lines.append(line)
    return " ".join(lines)


def scan_header(path: Path, write: bool) -> tuple[int, list[tuple[str, str]]]:
    text = path.read_text(encoding="utf-8")
    masked = mask_comments(text)
    changes = []
    previews = []
    comment_pattern = re.compile(r"/\*\*(?P<doc>.*?)\*/", re.S)
    for comment in comment_pattern.finditer(text):
        tail_start = comment.end()
        tail = masked[tail_start:]
        semicolon = tail.find(";")
        brace = tail.find("{")
        if semicolon < 0 or (brace >= 0 and brace < semicolon):
            continue
        declaration = tail[:semicolon + 1]
        name_match = re.search(r"\b(clm_[A-Za-z0-9_]+)\s*\(", declaration)
        if not name_match:
            continue
        name = name_match.group(1)
        if "internal" in name or "private" in name:
            continue
        if re.search(r"\btypedef\b", declaration):
            continue
        open_paren = declaration.find("(", name_match.start())
        close_paren = matching_paren(declaration, open_paren)
        args = split_args(declaration[open_paren + 1:close_paren])
        names = [(param_name(arg), arg) for arg in args]
        names = [(name, arg) for name, arg in names if name]
        doc = comment.group("doc")
        documented = set(re.findall(r"@param(?:\[[^]]+\])?\s+([A-Za-z_]\w*)", doc))
        summary = doc_summary(doc)
        lines = []
        for parameter, arg in names:
            if parameter not in documented:
                lines.append(f" * @param {parameter} {param_description(parameter, arg, summary)}")
        prefix = declaration[:name_match.start()].strip()
        is_void = re.search(r"\bvoid\s*$", prefix) is not None
        if not is_void and not re.search(r"@(?:return|retval)\b", doc):
            lines.append(f" * @return {return_description(name, prefix, summary)}")
        if not lines:
            continue
        closing_start = comment.end() - 2
        line_start = text.rfind("\n", comment.start(), closing_start) + 1
        if line_start > comment.start():
            insertion = line_start
            addition = "\n".join(lines) + "\n"
        else:
            insertion = closing_start
            addition = "\n" + "\n".join(lines) + "\n "
        changes.append((insertion, addition))
        updated_comment = text[comment.start():insertion] + addition + text[insertion:comment.end()]
        previews.append((comment.group(0), updated_comment))
    for index, addition in reversed(changes):
        text = text[:index] + addition + text[index:]
    if write and changes:
        path.write_text(text, encoding="utf-8", newline="")
    return len(changes), previews


def main() -> None:
    arguments = __import__("sys").argv
    write = "--write" in arguments
    preview = "--preview" in arguments
    total = 0
    previewed = 0
    for path in sorted(ROOT.glob("*.h")):
        count, examples = scan_header(path, write)
        if count:
            print(f"{path.name}: {count} docblocks need tags")
            total += count
            if preview and path.name in {"async.h", "math.h", "string.h", "tree.h"}:
                for before, after in examples[:2]:
                    print(f"--- {path.name} preview ---\n{before}\nbecomes:\n{after}")
                    previewed += 1
    print(f"TOTAL: {total} docblocks {'updated' if write else 'would be updated'}")


if __name__ == "__main__":
    main()
