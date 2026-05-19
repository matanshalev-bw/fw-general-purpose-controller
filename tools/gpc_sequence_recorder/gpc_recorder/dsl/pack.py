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


def pack_struct(
    schema: Schema,
    struct_def: StructDef,
    field_values: Dict[str, Any],
) -> bytes:
    out = bytearray()
    for field in struct_def.fields:
        if field.name not in field_values:
            raise ValueError(f"Missing field {field.name!r} for {struct_def.name}")
        out.extend(_pack_field(schema, field, field_values[field.name]))
    return bytes(out)


def pack_trigger_data(
    schema: Schema,
    struct_name: str,
    field_values: Dict[str, Any],
    size: int = 8,
) -> Tuple[List[int], List[str]]:
    """Returns (data bytes padded to size, list of C++ literal fragments for non-trivial enums)."""
    struct_def = schema.command_structs[struct_name]
    raw = pack_struct(schema, struct_def, field_values)
    if len(raw) > size:
        raise ValueError(
            f"{struct_name} packs to {len(raw)} bytes, max trigger data is {size}"
        )
    padded = list(raw) + [0] * (size - len(raw))
    return padded, []
