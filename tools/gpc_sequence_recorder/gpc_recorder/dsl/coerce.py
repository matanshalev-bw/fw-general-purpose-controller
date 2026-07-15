"""Coerce REPL/GUI values into micro-op field types."""

import json
from typing import Any, List, Optional

# Matches MicroOpsPayload::COMM_DATA_LENGTH / MicroVarStore LE pack from COMM RX.
_COMM_RX_MAX_BYTES = 8


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


def pack_le_bytes_to_uint64(bytes_list: List[int], *, name: str = "value") -> int:
    """Little-endian pack of 1..8 bytes into uint64 (same as GPC COMM RX → MicroVarStore)."""
    if not bytes_list:
        raise ValueError(f"{name} byte array must not be empty")
    if len(bytes_list) > _COMM_RX_MAX_BYTES:
        raise ValueError(f"{name} byte array length must be <= {_COMM_RX_MAX_BYTES}, got {len(bytes_list)}")
    value = 0
    for i, raw in enumerate(bytes_list):
        b = int(raw)
        if b < 0 or b > 0xFF:
            raise ValueError(f"{name}[{i}] must be a byte 0..255, got {raw!r}")
        value |= (b & 0xFF) << (8 * i)
    return value


def _looks_like_byte_array(value: str) -> bool:
    s = value.strip()
    if s.startswith("["):
        return True
    # Comma-separated bytes (e.g. "10, 3, 51") — not a single hex/decimal literal.
    return "," in s


def coerce_var_set_value(value: Any) -> int:
    """Accept a raw uint64, or a byte list packed LE like COMM RX into MicroVarStore."""
    if isinstance(value, bool):
        raise ValueError("value must be an integer or byte array, not bool")
    if isinstance(value, int):
        return value & 0xFFFFFFFFFFFFFFFF
    if isinstance(value, (list, tuple)):
        return pack_le_bytes_to_uint64(coerce_int_byte_list(value, name="value"))
    if isinstance(value, str):
        s = value.strip()
        if _looks_like_byte_array(s):
            return pack_le_bytes_to_uint64(coerce_int_byte_list(s, name="value"))
        try:
            return int(s, 0) & 0xFFFFFFFFFFFFFFFF
        except ValueError as e:
            raise ValueError(
                "value must be an integer (e.g. 3500 / 0x1234) or a byte array "
                "(e.g. [0x0A, 0x03, 0x33] or 10, 3, 51)"
            ) from e
    raise ValueError(f"value must be an integer or byte array, got {type(value).__name__}")
