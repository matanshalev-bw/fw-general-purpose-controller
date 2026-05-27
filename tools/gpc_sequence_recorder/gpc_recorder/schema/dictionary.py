"""BlueLink command / micro-op struct reference for the web UI."""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from gpc_recorder.schema.loader import get_schema
from gpc_recorder.usb_bridge import MICRO_OP_TO_PAYLOAD_TYPE

# Payload types whose wire struct differs from the default *Command name mapping.
_STRUCT_TO_PAYLOAD_OVERRIDE: Dict[str, str] = {
    "ControlCommand": "POWER_COMMAND",
}


def _normalize_type(cpp_type: str) -> str:
    return cpp_type.strip().split("::")[-1]


def _enum_values_for_type(schema: Any, cpp_type: str) -> Optional[List[Dict[str, Any]]]:
    name = _normalize_type(cpp_type)
    if name not in schema.enums:
        return None
    return [
        {"name": member, "value": value}
        for member, value in sorted(schema.enums[name].values.items(), key=lambda item: item[1])
    ]


def _field_entry(field: Any, schema: Any) -> Dict[str, Any]:
    default: Any = None
    if field.default_raw:
        if field.array_size:
            default = []
        elif field.cpp_type in schema.enums:
            default = field.default_raw.split("::")[-1]
        else:
            try:
                default = (
                    int(field.default_raw, 0)
                    if field.default_raw.startswith("0x")
                    else int(field.default_raw)
                )
            except ValueError:
                default = field.default_raw
    entry: Dict[str, Any] = {
        "name": field.name,
        "type": field.cpp_type,
        "array_size": field.array_size,
        "default": default,
    }
    enum_values = _enum_values_for_type(schema, field.cpp_type)
    if enum_values is not None:
        entry["enum_values"] = enum_values
    return entry


def bluelink_commands_dictionary() -> Dict[str, List[Dict[str, Any]]]:
    schema = get_schema()
    struct_to_payload = dict(_STRUCT_TO_PAYLOAD_OVERRIDE)
    for payload_type, struct_name in schema.payload_id_to_struct.items():
        struct_to_payload.setdefault(struct_name, payload_type)

    commands: List[Dict[str, Any]] = []
    for struct_name, struct_def in sorted(schema.command_structs.items()):
        payload_type: Optional[str] = struct_to_payload.get(struct_name)
        payload_type_id: Optional[int] = (
            schema.payload_type_ids.get(payload_type) if payload_type else None
        )
        commands.append(
            {
                "struct_name": struct_name,
                "payload_type": payload_type,
                "payload_type_id": payload_type_id,
                "fields": [_field_entry(f, schema) for f in struct_def.fields],
            }
        )

    micro_ops: List[Dict[str, Any]] = []
    for member, op in sorted(schema.micro_ops.items()):
        payload_type = MICRO_OP_TO_PAYLOAD_TYPE.get(member)
        payload_type_id = (
            schema.payload_type_ids.get(payload_type) if payload_type else None
        )
        micro_ops.append(
            {
                "union_member": member,
                "struct_name": op.struct_name,
                "payload_type": payload_type,
                "payload_type_id": payload_type_id,
                "op_type": op.op_type_name,
                "fields": [_field_entry(f, schema) for f in op.fields],
            }
        )

    return {"commands": commands, "micro_ops": micro_ops}
