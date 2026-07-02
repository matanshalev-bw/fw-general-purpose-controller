"""Coerce REPL/GUI values into micro-op field types."""

import json
from typing import Any, List, Optional


def quoted_string_bytes(value: str) -> Optional[List[int]]:
    """Return ASCII byte values when value is a quoted string literal."""
    s = value.strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        try:
            text = json.loads(s)
        except json.JSONDecodeError:
            return None
        if isinstance(text, str):
            return [ord(c) & 0xFF for c in text]
        return None
    if len(s) >= 2 and s[0] == "'" and s[-1] == "'":
        return [ord(c) & 0xFF for c in s[1:-1]]
    return None


def coerce_int_byte_list(value: Any, *, name: str = "data") -> List[int]:
    """Accept list literals, comma-separated text, quoted strings, or '[0, 1]' strings."""
    if isinstance(value, (list, tuple)):
        if len(value) == 1 and isinstance(value[0], str):
            quoted = quoted_string_bytes(value[0])
            if quoted is not None:
                return quoted
        try:
            return [int(x, 0) if isinstance(x, str) else int(x) for x in value]
        except (TypeError, ValueError) as e:
            raise ValueError(f"{name} must be a list of integers, got {value!r}") from e

    if isinstance(value, str):
        quoted = quoted_string_bytes(value)
        if quoted is not None:
            return quoted
        s = value.strip()
        if s.startswith("[") and s.endswith("]"):
            s = s[1:-1]
        if not s:
            return []
        parts = [p.strip() for p in s.split(",") if p.strip()]
        try:
            return [int(p, 0) for p in parts]
        except ValueError as e:
            raise ValueError(
                f"{name} must be a list of integers or a quoted string (e.g. \"HELLO\"), got {value!r}"
            ) from e

    raise ValueError(f"{name} must be a list or comma-separated integers, got {type(value).__name__}")
