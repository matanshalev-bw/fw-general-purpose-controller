"""Coerce REPL/GUI values into micro-op field types."""

from typing import Any, List


def coerce_int_byte_list(value: Any, *, name: str = "data") -> List[int]:
    """Accept list literals, comma-separated text, or quoted '[0, 1]' strings."""
    if isinstance(value, (list, tuple)):
        try:
            return [int(x, 0) if isinstance(x, str) else int(x) for x in value]
        except (TypeError, ValueError) as e:
            raise ValueError(f"{name} must be a list of integers, got {value!r}") from e

    if isinstance(value, str):
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
                f"{name} must be a list of integers (e.g. [0, 1, 2]), got {value!r}"
            ) from e

    raise ValueError(f"{name} must be a list or comma-separated integers, got {type(value).__name__}")
