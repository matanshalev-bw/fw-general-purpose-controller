"""Emit g474_gpc_config_memory.hpp from session model."""

import subprocess
from pathlib import Path
from typing import Any, Dict, List

from jinja2 import Environment, FileSystemLoader, select_autoescape

from gpc_recorder.codegen.config_build import ConfigBuildError, build_config_bin
from gpc_recorder.paths import (
    CONTROLLER_STATE_SEQUENCE_FIELDS,
    CONTROLLER_STATE_TICK_FIELDS,
    MICRO_SEQUENCE_MAX_STEPS,
    TOOL_DIR,
)


def _format_data_array(data: List[int], struct_name: str, field_values: Dict[str, Any], schema) -> str:
    """Format trigger data with static_cast for enum bytes where needed."""
    struct_def = schema.command_structs.get(struct_name)
    if not struct_def:
        return ", ".join(str(b) for b in data)

    parts: List[str] = []
    offset = 0
    for field in struct_def.fields:
        from gpc_recorder.dsl.pack import _field_size, _normalize_type

        sz = _field_size(schema, field)
        chunk = data[offset : offset + sz]
        offset += sz
        t = _normalize_type(field.cpp_type)
        if t in schema.enums and len(chunk) == 1:
            val = chunk[0]
            enum_val = field_values.get(field.name)
            if isinstance(enum_val, str):
                enum_name = enum_val.split("::")[-1]
            else:
                enum_name = str(enum_val)
            parts.append(f"static_cast<uint8_t>(bluelink::{enum_name})")
        else:
            parts.extend(str(b) for b in chunk)

    while len(parts) < 8:
        parts.append("0")
    return ", ".join(parts[:8])


def _format_scalar(v: Any) -> str:
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, str) and v.startswith("0x"):
        return v
    return str(int(v))


def _format_array_field(v: Any, size: int = 8, *, hex_bytes: bool = False) -> str:
    arr = list(v) if isinstance(v, (list, tuple)) else []
    padded = arr + [0] * (size - len(arr))
    if hex_bytes:
        parts = [f"0x{int(x):X}" if int(x) != 0 else "0" for x in padded[:size]]
        return "{" + ", ".join(parts) + "}"
    return "{" + ", ".join(str(x) for x in padded[:size]) + "}"


def _format_union_init(member: str, values: Dict[str, Any], schema=None) -> str:
    if schema and member in schema.micro_ops:
        parts: List[str] = []
        for field in schema.micro_ops[member].fields:
            v = values.get(field.name)
            if v is None:
                raise ValueError(f"Missing field {field.name!r} for {member}")
            if field.array_size:
                size = int(field.array_size.strip()) if field.array_size.strip().isdigit() else 8
                use_hex = member == "can_transmit" and field.name == "data"
                parts.append(_format_array_field(v, size, hex_bytes=use_hex))
            elif member == "can_transmit" and field.name == "id":
                parts.append(f"0x{int(v):X}")
            else:
                parts.append(_format_scalar(v))
        return "{" + ", ".join(parts) + "}"

    if member == "delay_ms":
        ms = values.get("delay_ms", values.get("delay", 0))
        return f"{{{ms}}}"
    ordered = []
    for v in values.values():
        if isinstance(v, (list, tuple)):
            ordered.append(_format_array_field(v))
        else:
            ordered.append(_format_scalar(v))
    return "{" + ", ".join(ordered) + "}"


def emit_config_hpp(
    session: Dict[str, Any],
    schema,
    output_path: Path | None = None,
    *,
    write: bool = True,
) -> str:
    template_dir = TOOL_DIR / "gpc_recorder/codegen/templates"
    env = Environment(
        loader=FileSystemLoader(str(template_dir)),
        autoescape=select_autoescape(),
        trim_blocks=True,
        lstrip_blocks=True,
    )
    def _steps_out(step_list: List[Dict[str, Any]]) -> List[Dict[str, str]]:
        if len(step_list) > MICRO_SEQUENCE_MAX_STEPS:
            raise ValueError(f"At most {MICRO_SEQUENCE_MAX_STEPS} steps per sequence")
        out = []
        for step in step_list:
            out.append(
                {
                    "op_type": step["op_type"],
                    "union_member": step["union_member"],
                    "init": _format_union_init(step["union_member"], step["values"], schema),
                }
            )
        return out

    powerup_steps_out = _steps_out(session.get("powerup_steps", []))
    main_tick_steps_out = _steps_out(session.get("main_tick_steps", []))

    state_steps_map = session.get("state_steps", {})
    state_sequence_fields_out = {}
    for state_name, field_name in CONTROLLER_STATE_SEQUENCE_FIELDS.items():
        steps_out = _steps_out(state_steps_map.get(state_name, []))
        state_sequence_fields_out[field_name] = {
            "step_count": len(steps_out),
            "steps": steps_out,
        }

    state_tick_steps_map = session.get("state_tick_steps", {})
    state_tick_fields_out = {}
    for state_name, field_name in CONTROLLER_STATE_TICK_FIELDS.items():
        steps_out = _steps_out(state_tick_steps_map.get(state_name, []))
        state_tick_fields_out[field_name] = {
            "step_count": len(steps_out),
            "steps": steps_out,
        }

    bindings_out = []
    for b in session["bindings"]:
        steps_out = _steps_out(b["steps"])
        bindings_out.append(
            {
                "payload_type": b["payload_type"],
                "data_init": _format_data_array(
                    b["data"], b["struct_name"], b["field_values"], schema
                ),
                "step_count": len(steps_out),
                "steps": steps_out,
            }
        )

    ctx = {
        "config_name": session["config_name"],
        "component_id": session["component_id"],
        "binding_count": len(bindings_out),
        "bindings": bindings_out,
        "powerup_step_count": len(powerup_steps_out),
        "powerup_steps": powerup_steps_out,
        "main_tick_step_count": len(main_tick_steps_out),
        "main_tick_steps": main_tick_steps_out,
        "state_sequence_fields": state_sequence_fields_out,
        "state_tick_fields": state_tick_fields_out,
    }
    env.filters["format_union"] = lambda m, v: _format_union_init(m, v, schema)

    template = env.get_template("g474_config_memory.hpp.j2")
    text = template.render(**ctx)
    if write and output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(text, encoding="utf-8")
    return text


def emit_config_bin(
    session: Dict[str, Any],
    schema,
    output_path: Path | None = None,
    *,
    write: bool = True,
    hpp_path: Path | None = None,
) -> str:
    """Write config_g474.bin via STM32CubeIDE (matches compiled g474_gpc_config_memory.hpp)."""
    del hpp_path
    dest = Path(output_path) if output_path is not None else None
    try:
        result = build_config_bin(session, schema, dest if write else None)
    except Exception as e:
        raise ConfigBuildError(f"Failed to build config bin: {e}") from e
    return str(result)
