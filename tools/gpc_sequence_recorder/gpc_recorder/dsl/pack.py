"""Pack C++-like struct field values into bytes (packed layout)."""

import re
import struct
from dataclasses import dataclass
from typing import Any, Dict, List, Tuple

from gpc_recorder.schema.cpp_parser import FieldDef, Schema, StructDef


@dataclass
class FieldValue:
    name: str
    value: Any


def _normalize_type(cpp_type: str) -> str:
    t = cpp_type.strip()
    t = t.split("::")[-1]
    t = re.sub(r"\s+", " ", t)
    return t


def _enum_numeric(schema: Schema, enum_type: str, value: Any) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        name = value.split("::")[-1]
        if enum_type in schema.enums:
            ev = schema.enums[enum_type]
            if name in ev.values:
                return ev.values[name]
        for ev in schema.enums.values():
            if name in ev.values:
                return ev.values[name]
        raise ValueError(f"Unknown enum value {value!r} for type {enum_type}")
    raise ValueError(f"Invalid enum value {value!r}")


def _field_size(schema: Schema, field: FieldDef) -> int:
    t = _normalize_type(field.cpp_type)
    if field.array_size:
        base = _field_size(schema, FieldDef(t, "x"))
        size_str = field.array_size.strip()
        if size_str.isdigit():
            return base * int(size_str)
        return base * 8
    sizes = {
        "bool": 1,
        "uint8_t": 1,
        "int8_t": 1,
        "char": 1,
        "unsigned char": 1,
        "uint16_t": 2,
        "int16_t": 2,
        "uint32_t": 4,
        "int32_t": 4,
        "float": 4,
    }
    if t in sizes:
        return sizes[t]
    if t in schema.enums:
        return 1
    raise ValueError(f"Unsupported field type {field.cpp_type!r}")


def _pack_field(schema: Schema, field: FieldDef, value: Any) -> bytes:
    t = _normalize_type(field.cpp_type)
    if field.array_size:
        arr = list(value)
        size_str = field.array_size.strip()
        n = int(size_str) if size_str.isdigit() else len(arr)
        parts = []
        for i in range(n):
            item = arr[i] if i < len(arr) else 0
            parts.append(_pack_field(schema, FieldDef(t, "elem"), item))
        return b"".join(parts)
    if t == "bool":
        return bytes([1 if value else 0])
    if t in schema.enums:
        return bytes([_enum_numeric(schema, t, value) & 0xFF])
    if t == "uint8_t" or t == "int8_t" or t == "char":
        return struct.pack("<b", int(value)) if t == "int8_t" else bytes([int(value) & 0xFF])
    if t == "uint16_t":
        return struct.pack("<H", int(value) & 0xFFFF)
    if t == "int16_t":
        return struct.pack("<h", int(value))
    if t == "uint32_t":
        return struct.pack("<I", int(value) & 0xFFFFFFFF)
    if t == "int32_t":
        return struct.pack("<i", int(value))
    if t == "float":
        return struct.pack("<f", float(value))
    raise ValueError(f"Cannot pack type {field.cpp_type!r}")


def _parse_default_literal(schema: Schema, field: FieldDef, raw: str) -> Any:
    raw = raw.strip().rstrip(",")
    if "::" in raw:
        raw = raw.split("::")[-1].strip()
    if raw in ("false", "true"):
        return raw == "true"
    if raw.startswith("0x"):
        return int(raw, 16)
    try:
        if "." in raw:
            return float(raw)
        return int(raw)
    except ValueError:
        pass
    t = _normalize_type(field.cpp_type)
    if t in schema.enums:
        return raw
    return raw


def _zero_default(schema: Schema, field: FieldDef) -> Any:
    t = _normalize_type(field.cpp_type)
    if t == "bool":
        return False
    if t in schema.enums:
        return 0
    if t == "float":
        return 0.0
    if t in ("uint8_t", "int8_t", "char", "uint16_t", "int16_t", "uint32_t", "int32_t"):
        return 0
    return 0


def _resolve_field_value(
    schema: Schema, field: FieldDef, value: Any | None = None
) -> Any:
    if value is not None:
        return value
    if field.default_raw:
        return _parse_default_literal(schema, field, field.default_raw)
    return _zero_default(schema, field)


def fill_struct_fields(
    schema: Schema,
    struct_def: StructDef,
    field_values: Dict[str, Any],
) -> Dict[str, Any]:
    """Merge user fields with C++ header defaults (or zero) for any omitted members."""
    merged: Dict[str, Any] = {}
    for field in struct_def.fields:
        merged[field.name] = _resolve_field_value(
            schema, field, field_values.get(field.name)
        )
    return merged


def pack_struct(
    schema: Schema,
    struct_def: StructDef,
    field_values: Dict[str, Any],
) -> bytes:
    merged = fill_struct_fields(schema, struct_def, field_values)
    out = bytearray()
    for field in struct_def.fields:
        out.extend(_pack_field(schema, field, merged[field.name]))
    return bytes(out)


def pack_trigger_data(
    schema: Schema,
    struct_name: str,
    field_values: Dict[str, Any],
    size: int = 8,
) -> Tuple[List[int], int]:
    """Returns (data bytes padded to size, packed wire byte count)."""
    struct_def = schema.command_structs[struct_name]
    raw = pack_struct(schema, struct_def, field_values)
    if len(raw) > size:
        raise ValueError(
            f"{struct_name} packs to {len(raw)} bytes, max trigger data is {size}"
        )
    padded = list(raw) + [0] * (size - len(raw))
    return padded, len(raw)


def _unpack_field(schema: Schema, field: FieldDef, chunk: bytes) -> Any:
    t = _normalize_type(field.cpp_type)
    if t == "bool":
        return bool(chunk[0])
    if t in schema.enums:
        val = chunk[0]
        enum_def = schema.enums[t]
        for name, num in enum_def.values.items():
            if num == val:
                return name
        return val
    if t in ("uint8_t", "char", "unsigned char"):
        return chunk[0]
    if t == "int8_t":
        return struct.unpack("<b", chunk[:1])[0]
    if t == "uint16_t":
        return struct.unpack("<H", chunk[:2])[0]
    if t == "int16_t":
        return struct.unpack("<h", chunk[:2])[0]
    if t == "uint32_t":
        return struct.unpack("<I", chunk[:4])[0]
    if t == "int32_t":
        return struct.unpack("<i", chunk[:4])[0]
    if t == "float":
        return struct.unpack("<f", chunk[:4])[0]
    raise ValueError(f"Cannot unpack type {field.cpp_type!r}")


def unpack_trigger_data(
    schema: Schema,
    struct_name: str,
    data: List[int],
    data_size: int,
) -> Dict[str, Any]:
    """Reverse pack_trigger_data: bytes back to named struct field values."""
    struct_def = schema.command_structs[struct_name]
    raw = bytes(data[:data_size])
    field_values: Dict[str, Any] = {}
    offset = 0
    for field in struct_def.fields:
        sz = _field_size(schema, field)
        chunk = raw[offset : offset + sz]
        if len(chunk) < sz:
            chunk = chunk + bytes(sz - len(chunk))
        if field.array_size:
            size_str = field.array_size.strip()
            n = int(size_str) if size_str.isdigit() else len(chunk)
            base_sz = sz // n if n else 1
            field_values[field.name] = [
                _unpack_field(schema, FieldDef(_normalize_type(field.cpp_type), "elem"), chunk[i : i + base_sz])
                for i in range(0, len(chunk), base_sz)
            ][:n]
        else:
            field_values[field.name] = _unpack_field(schema, field, chunk)
        offset += sz
    return field_values
