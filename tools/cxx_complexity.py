#!/usr/bin/env python3
"""
Heuristic C++ function complexity estimator.
Counts control-flow tokens and boolean ops per function and reports nesting depth.

Usage: python tools/cxx_complexity.py src/runtime/Runtime.cpp
"""
import re
import sys
from pathlib import Path

CTRL_TOKENS = [
    r"\bif\b", r"\belse\s+if\b", r"\bfor\b", r"\bwhile\b",
    r"\bswitch\b", r"\bcase\b", r"\bdefault\b", r"\bcatch\b", r"\?", r"&&", r"\|\|",
]

SIG_RE = re.compile(r"^\s*(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w:\s\*&<>]*\s+([A-Za-z_][\w:]*)\s*\([^;]*\)\s*\{\s*$")

def strip_comments(code: str) -> str:
    # Remove // and /* */ comments conservatively
    def replacer(match):
        s = match.group(0)
        return "" if s.startswith('/') else s
    pattern = re.compile(
        r"//.*?$|/\*.*?\*/|\'(?:\\.|[^\\'])*\'|\"(?:\\.|[^\\\"])*\"",
        re.DOTALL | re.MULTILINE,
    )
    return re.sub(pattern, replacer, code)

def estimate_complexity(body: str) -> tuple[int, int]:
    complexity = 1
    for tok in CTRL_TOKENS:
        complexity += len(re.findall(tok, body))
    # estimate nesting via braces
    depth = 0
    max_depth = 0
    for ch in body:
        if ch == '{':
            depth += 1
            if depth > max_depth:
                max_depth = depth
        elif ch == '}':
            depth -= 1
    return complexity, max_depth

def main():
    if len(sys.argv) < 2:
        print("usage: cxx_complexity.py <file> [more files]", file=sys.stderr)
        sys.exit(2)
    for file in sys.argv[1:]:
        p = Path(file)
        src = p.read_text(encoding='utf-8', errors='ignore')
        clean = strip_comments(src)
        lines = clean.splitlines()
        out = []
        i = 0
        while i < len(lines):
            m = SIG_RE.match(lines[i])
            if not m:
                i += 1
                continue
            name = m.group(1)
            # capture body by brace matching
            brace = 0
            body_lines = []
            # include opening line '{'
            line = lines[i]
            brace += line.count('{') - line.count('}')
            j = i + 1
            while j < len(lines):
                body_lines.append(lines[j])
                brace += lines[j].count('{') - lines[j].count('}')
                j += 1
                if brace == 0:
                    break
            body = "\n".join(body_lines[:-1]) if body_lines else ""
            comp, depth = estimate_complexity(body)
            out.append((comp, depth, i + 1, name))
            i = j
        print(f"== {p} ==")
        for comp, depth, line_no, name in sorted(out, key=lambda t: (-t[0], -t[1]))[:40]:
            print(f"{comp:4d}  depth={depth:2d}  line={line_no:5d}  {name}")

if __name__ == '__main__':
    main()

