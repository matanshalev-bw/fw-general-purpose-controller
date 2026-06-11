"""Restricted Python REPL for sequence recording."""

import ast
import io
import traceback
from contextlib import redirect_stdout
from typing import List, Tuple

from gpc_recorder.dsl.builtins import RecorderContext, build_namespace
from gpc_recorder.dsl.completion import complete_line, longest_common_prefix
from gpc_recorder.dsl.normalize import normalize_line


class ReplEngine:
    def __init__(self, *, auto_reload: bool = True) -> None:
        self.ctx = RecorderContext()
        self._namespace = build_namespace(self.ctx)
        self._history: List[str] = []
        self._startup_message = self._auto_reload() if auto_reload else ""

    def _auto_reload(self) -> str:
        buf = io.StringIO()
        try:
            with redirect_stdout(buf):
                self.ctx.reload()
        except FileNotFoundError as e:
            return f"Warning: {e}"
        except Exception as e:
            return f"Warning: failed to reload config: {e}"
        return buf.getvalue().rstrip()

    @property
    def startup_message(self) -> str:
        return self._startup_message

    def execute(self, line: str) -> Tuple[str, bool]:
        line = line.strip()
        if not line:
            return "", True
        self._history.append(line)

        if line in ("exit", "quit"):
            return "Goodbye.", False

        line = normalize_line(line)

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

    def complete(self, line: str) -> List[str]:
        return complete_line(
            self.ctx.schema,
            sorted(self._namespace.keys()),
            line,
        )

    def complete_common_prefix(self, line: str) -> str:
        matches = self.complete(line)
        from gpc_recorder.dsl.completion import _word_at_end

        word = _word_at_end(line)
        common = longest_common_prefix(matches)
        return common if len(common) > len(word) else ""

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
