"""Restricted Python REPL for sequence recording."""

import ast
import io
import re
import traceback
from contextlib import redirect_stdout
from typing import List, Tuple

from gpc_recorder.dsl.builtins import RecorderContext, build_namespace


class ReplEngine:
    def __init__(self) -> None:
        self.ctx = RecorderContext()
        self._namespace = build_namespace(self.ctx)
        self._history: List[str] = []

    def execute(self, line: str) -> Tuple[str, bool]:
        line = line.strip()
        if not line:
            return "", True
        self._history.append(line)

        if line in ("exit", "quit"):
            return "Goodbye.", False

        try:
            tree = ast.parse(line, mode="exec")
        except SyntaxError as e:
            return f"SyntaxError: {e}", True

        if not self._is_safe(tree):
            return "Error: only expressions and safe calls allowed", True

        buf = io.StringIO()
        try:
            with redirect_stdout(buf):
                try:
                    code = compile(line, "<repl>", "eval")
                    result = eval(code, {"__builtins__": {}}, self._namespace)
                    if result is not None:
                        print(repr(result))
                except SyntaxError:
                    exec(compile(tree, "<repl>", "exec"), {"__builtins__": {}}, self._namespace)
        except Exception:
            return traceback.format_exc(), True

        out = buf.getvalue().rstrip()
        return out if out else "OK", True

    def complete(self, prefix: str) -> List[str]:
        m = re.search(r"[A-Za-z_][A-Za-z0-9_]*$", prefix)
        word = m.group(0) if m else prefix
        names = sorted(self._namespace.keys())
        return [n for n in names if n.startswith(word)]

    def preview_hpp(self) -> str:
        return self.ctx.preview()

    def _is_safe(self, tree: ast.AST) -> bool:
        for node in ast.walk(tree):
            if isinstance(node, (ast.Import, ast.ImportFrom, ast.Lambda, ast.FunctionDef, ast.ClassDef)):
                return False
            if isinstance(node, ast.Name) and node.id.startswith("__"):
                return False
            if isinstance(node, ast.Attribute) and node.attr.startswith("__"):
                return False
        return True
