"""Sugar names for GPC var slots (recorder-only; firmware stays index-based)."""

from __future__ import annotations

import json
import re
from typing import Any, List, Sequence, Union

from gpc_recorder.paths import MICRO_VAR_SLOT_COUNT
from gpc_recorder.validate import validate_var_index

_IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_VN_RE = re.compile(r"^v(\d+)$")
# HPP metadata comment (stripped before C++ parse; firmware-inert).
_HPP_VAR_NAMES_RE = re.compile(
    r"/\*\s*gpc-recorder:var_names\s*=\s*(\[[\s\S]*?\])\s*\*/",
)


def empty_var_names() -> List[str]:
    return [""] * MICRO_VAR_SLOT_COUNT


def normalize_var_names(names: Sequence[Any] | None) -> List[str]:
    out = empty_var_names()
    if not names:
        return out
    for i, raw in enumerate(names):
        if i >= MICRO_VAR_SLOT_COUNT:
            break
        out[i] = "" if raw is None else str(raw).strip()
    return out


def validate_sugar_name(name: str) -> str:
    """Return normalized sugar name, or raise ValueError."""
    cleaned = name.strip()
    if not cleaned:
        raise ValueError("var sugar name must be non-empty")
    if not _IDENT_RE.match(cleaned):
        raise ValueError(
            f"Invalid var sugar name {name!r}; use a Python identifier "
            f"([A-Za-z_][A-Za-z0-9_]*)"
        )
    if _VN_RE.match(cleaned):
        raise ValueError(
            f"Invalid var sugar name {cleaned!r}; v0..v{MICRO_VAR_SLOT_COUNT - 1} "
            "are reserved index aliases"
        )
    return cleaned


def set_var_name(var_names: List[str], var_index: int, name: str) -> None:
    """Set or clear sugar name for a slot in-place."""
    validate_var_index(int(var_index))
    idx = int(var_index)
    cleaned = name.strip() if isinstance(name, str) else str(name).strip()
    if not cleaned:
        var_names[idx] = ""
        return
    sugar = validate_sugar_name(cleaned)
    for i, existing in enumerate(var_names):
        if i != idx and existing == sugar:
            raise ValueError(f"Var sugar name {sugar!r} already used by slot {i}")
    var_names[idx] = sugar


def resolve_var_ref(value: Union[int, str], var_names: Sequence[str]) -> int:
    """Resolve an int, digit string, vN alias, or sugar name to a var index."""
    if isinstance(value, bool):
        raise ValueError(f"Invalid var reference: {value!r}")
    if isinstance(value, int):
        validate_var_index(value)
        return value

    if not isinstance(value, str):
        raise ValueError(f"Invalid var reference type {type(value).__name__}: {value!r}")

    text = value.strip()
    if not text:
        raise ValueError("Empty var reference")

    if text.isdigit():
        idx = int(text)
        validate_var_index(idx)
        return idx

    vn = _VN_RE.match(text)
    if vn:
        idx = int(vn.group(1))
        validate_var_index(idx)
        return idx

    for i, sugar in enumerate(var_names):
        if sugar and sugar == text:
            return i
    raise ValueError(f"Unknown var reference {value!r}")


def format_var_names_hpp_comment(var_names: Sequence[Any] | None) -> str:
    """Return a C comment with JSON var_names, or "" if all slots are empty."""
    names = normalize_var_names(var_names)
    if not any(names):
        return ""
    payload = json.dumps(names, separators=(",", ":"))
    return f"/* gpc-recorder:var_names={payload} */"


def parse_var_names_hpp_comment(text: str) -> List[str]:
    """Extract var_names from an HPP metadata comment, if present."""
    match = _HPP_VAR_NAMES_RE.search(text)
    if not match:
        return empty_var_names()
    try:
        data = json.loads(match.group(1))
    except json.JSONDecodeError:
        return empty_var_names()
    if not isinstance(data, list):
        return empty_var_names()
    return normalize_var_names(data)

# Live Expression representation prefs (recorder-only UI; firmware-inert).
_LIVE_EXPR_CASTS = frozenset(
    {"int", "hex", "str", "double", "array_int8", "array_uint8"}
)
_LEGACY_LIVE_EXPR_CASTS = {
    "dec": "int",
    "dec_array": "array_int8",
    "float_bits_hex": "double",
    "bin": "hex",
}
_HPP_LIVE_EXPR_CASTS_RE = re.compile(
    r"/\*\s*gpc-recorder:live_expr_casts\s*=\s*(\[[\s\S]*?\])\s*\*/",
)


def empty_live_expr_casts() -> List[str]:
    return ["int"] * MICRO_VAR_SLOT_COUNT


def normalize_live_expr_cast(raw: Any) -> str:
    text = "" if raw is None else str(raw).strip()
    text = _LEGACY_LIVE_EXPR_CASTS.get(text, text)
    return text if text in _LIVE_EXPR_CASTS else "int"


def normalize_live_expr_casts(casts: Sequence[Any] | None) -> List[str]:
    out = empty_live_expr_casts()
    if not casts:
        return out
    for i, raw in enumerate(casts):
        if i >= MICRO_VAR_SLOT_COUNT:
            break
        out[i] = normalize_live_expr_cast(raw)
    return out


def format_live_expr_casts_hpp_comment(casts: Sequence[Any] | None) -> str:
    """Return a C comment with JSON casts, or "" if all slots are default int."""
    normalized = normalize_live_expr_casts(casts)
    if all(c == "int" for c in normalized):
        return ""
    payload = json.dumps(normalized, separators=(",", ":"))
    return f"/* gpc-recorder:live_expr_casts={payload} */"


def parse_live_expr_casts_hpp_comment(text: str) -> List[str]:
    """Extract live_expr_casts from an HPP metadata comment, if present."""
    match = _HPP_LIVE_EXPR_CASTS_RE.search(text)
    if not match:
        return empty_live_expr_casts()
    try:
        data = json.loads(match.group(1))
    except json.JSONDecodeError:
        return empty_live_expr_casts()
    if not isinstance(data, list):
        return empty_live_expr_casts()
    return normalize_live_expr_casts(data)


def format_recorder_meta_hpp_comments(
    var_names: Sequence[Any] | None = None,
    live_expr_casts: Sequence[Any] | None = None,
) -> str:
    """Join non-empty recorder metadata comments for HPP emission."""
    parts = [
        format_var_names_hpp_comment(var_names),
        format_live_expr_casts_hpp_comment(live_expr_casts),
    ]
    return "\n".join(p for p in parts if p)
