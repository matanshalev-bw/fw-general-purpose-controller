"""Load g474_gpc_config_memory.hpp into a recording session."""

from __future__ import annotations

import re
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from gpc_recorder.dsl.pack import _field_size, _normalize_type, unpack_trigger_data
from gpc_recorder.dsl.session import BindingState, MicroOpStepState, Session
from gpc_recorder.paths import (
    CONTROLLER_STATE_SEQUENCE_FIELDS,
    CONTROLLER_STATE_TICK_FIELDS,
    DEFAULT_EXPORT_PATH,
)
from gpc_recorder.schema.cpp_parser import Schema

_SEQUENCE_FIELD_TO_STATE = {
    field: state for state, field in CONTROLLER_STATE_SEQUENCE_FIELDS.items()
}
_TICK_FIELD_TO_STATE = {field: state for state, field in CONTROLLER_STATE_TICK_FIELDS.items()}

_OP_TYPE_TO_MEMBER = {
    "DIGITAL_GPIO_WRITE": "digital_gpio_write",
    "DIGITAL_GPIO_READ": "digital_gpio_read",
    "ADC_READ": "adc_read",
    "DAC_WRITE": "dac_write",
    "PWM_SET": "pwm_set",
    "DELAY_MS": "delay_ms",
    "CAN_TRANSMIT": "can_transmit",
    "UART_TRANSMIT": "uart_transmit",
    "SPI_TRANSFER": "spi_transfer",
    "I2C_WRITE": "i2c_write",
}


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def _find_braced_block(text: str, open_brace: int) -> Tuple[str, int]:
    if open_brace >= len(text) or text[open_brace] != "{":
        raise ValueError(f"Expected '{{' at index {open_brace}")
    depth = 0
    start = open_brace + 1
    for i in range(open_brace, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start:i], i + 1
    raise ValueError("Unbalanced braces")


def _find_field_block(text: str, field_name: str) -> Optional[str]:
    pattern = rf"\.{re.escape(field_name)}\s*=\s*\{{"
    match = re.search(pattern, text)
    if not match:
        return None
    inner, _ = _find_braced_block(text, match.end() - 1)
    return inner


def _parse_scalar(token: str) -> Any:
    token = token.strip()
    if not token:
        raise ValueError("Empty scalar token")
    if token.startswith("0x") or token.startswith("0X"):
        return int(token, 16)
    if token in ("true", "false"):
        return token == "true"
    if "." in token:
        return float(token)
    return int(token)


def _split_init_list(inner: str) -> List[str]:
    parts: List[str] = []
    current: List[str] = []
    depth = 0
    for ch in inner:
        if ch == "{":
            depth += 1
            current.append(ch)
        elif ch == "}":
            depth -= 1
            current.append(ch)
        elif ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def _parse_data_byte(token: str, schema: Schema) -> int:
    token = token.strip()
    cast_match = re.match(
        r"static_cast<uint8_t>\(\s*bluelink::(\w+)\s*\)",
        token,
    )
    if cast_match:
        enum_name = cast_match.group(1)
        for enum_def in schema.enums.values():
            if enum_name in enum_def.values:
                return enum_def.values[enum_name]
        raise ValueError(f"Unknown enum value {enum_name!r} in trigger data")
    return int(_parse_scalar(token))


def _parse_data_array(inner: str, schema: Schema) -> List[int]:
    tokens = _split_init_list(inner)
    data = [_parse_data_byte(t, schema) for t in tokens]
    while len(data) < 8:
        data.append(0)
    return data[:8]


def _parse_step_values(
    schema: Schema,
    union_member: str,
    init_inner: str,
) -> Dict[str, Any]:
    if union_member not in schema.micro_ops:
        raise ValueError(f"Unknown micro-op union member {union_member!r}")
    op_def = schema.micro_ops[union_member]
    tokens = _split_init_list(init_inner)
    values: Dict[str, Any] = {}
    ti = 0
    for field in op_def.fields:
        if field.array_size:
            size_str = field.array_size.strip()
            size = int(size_str) if size_str.isdigit() else 8
            arr = [_parse_scalar(tokens[ti + i]) for i in range(size)]
            values[field.name] = arr
            ti += size
        else:
            if ti >= len(tokens):
                raise ValueError(f"Missing value for {field.name} in {union_member}")
            values[field.name] = _parse_scalar(tokens[ti])
            ti += 1
    return values


def _parse_steps_block(steps_inner: str, schema: Schema) -> List[MicroOpStepState]:
    steps: List[MicroOpStepState] = []
    for match in re.finditer(
        r"\.op_type\s*=\s*bluelink::MicroOpsPayload::MicroOpType::(\w+),\s*"
        r"\.(\w+)\s*=\s*\{([^}]*)\}",
        steps_inner,
    ):
        op_name = match.group(1)
        union_member = match.group(2)
        init_inner = match.group(3)
        expected = _OP_TYPE_TO_MEMBER.get(op_name)
        if expected and expected != union_member:
            raise ValueError(
                f"Micro-op {op_name} uses .{union_member}, expected .{expected}"
            )
        if union_member not in schema.micro_ops:
            union_member = expected or union_member
        op = schema.micro_ops[union_member]
        values = _parse_step_values(schema, union_member, init_inner)
        steps.append(
            MicroOpStepState(
                op_type=op.op_type_name,
                union_member=union_member,
                values=values,
            )
        )
    return steps


def _parse_sequence_block(block: str, schema: Schema) -> List[MicroOpStepState]:
    steps_match = re.search(r"\.steps\s*=\s*\{", block)
    if not steps_match:
        return []
    steps_inner, _ = _find_braced_block(block, steps_match.end() - 1)
    return _parse_steps_block(steps_inner, schema)


def _parse_bindings(block: str, schema: Schema) -> List[BindingState]:
    bindings: List[BindingState] = []
    for binding_match in re.finditer(r"\{\s*\.trigger\s*=\s*\{", block):
        binding_inner, _ = _find_braced_block(block, binding_match.start())
        payload_match = re.search(
            r"\.payload_type\s*=\s*bluelink::PayloadTypeIds::(\w+)",
            binding_inner,
        )
        size_match = re.search(r"\.size\s*=\s*(\d+)", binding_inner)
        data_match = re.search(r"\.data\s*=\s*\{([^}]*)\}", binding_inner)
        if not payload_match or not size_match or not data_match:
            raise ValueError("Malformed binding trigger block")
        payload_type = payload_match.group(1)
        data_size = int(size_match.group(1))
        data = _parse_data_array(data_match.group(1), schema)
        struct_name = schema.payload_id_to_struct.get(payload_type)
        if struct_name is None:
            raise ValueError(f"No command struct mapped for {payload_type}")
        field_values = unpack_trigger_data(schema, struct_name, data, data_size)
        seq_match = re.search(r"\.sequence\s*=\s*\{", binding_inner)
        if not seq_match:
            raise ValueError(f"Binding for {payload_type} missing sequence block")
        seq_inner, _ = _find_braced_block(binding_inner, seq_match.end() - 1)
        steps = _parse_sequence_block(seq_inner, schema)
        bindings.append(
            BindingState(
                payload_type=payload_type,
                struct_name=struct_name,
                field_values=field_values,
                data=data,
                data_size=data_size,
                steps=steps,
            )
        )
    return bindings


def load_config_hpp(path: Path, schema: Schema) -> Session:
    """Parse g474_gpc_config_memory.hpp into a Session."""
    text = _strip_comments(path.read_text(encoding="utf-8"))

    name_match = re.search(r'\.name\s*=\s*"([^"]+)"', text)
    component_match = re.search(r"ComponentId::(\w+)", text)
    if not name_match or not component_match:
        raise ValueError(f"Not a valid GPC config memory file: {path}")

    session = Session(
        config_name=name_match.group(1),
        component_id=component_match.group(1),
    )

    sequences_block = _find_field_block(text, "sequences_config")
    if sequences_block is None:
        raise ValueError(f"Missing sequences_config in {path}")

    session.powerup_steps = _parse_sequence_block(
        _find_field_block(sequences_block, "powerup_sequence") or "",
        schema,
    )
    session.main_tick_steps = _parse_sequence_block(
        _find_field_block(sequences_block, "main_tick_sequence") or "",
        schema,
    )

    for field_name, state_name in _SEQUENCE_FIELD_TO_STATE.items():
        block = _find_field_block(sequences_block, field_name)
        if block:
            steps = _parse_sequence_block(block, schema)
            if steps:
                session.state_steps[state_name] = steps

    for field_name, state_name in _TICK_FIELD_TO_STATE.items():
        block = _find_field_block(sequences_block, field_name)
        if block:
            steps = _parse_sequence_block(block, schema)
            if steps:
                session.state_tick_steps[state_name] = steps

    bindings_block = _find_field_block(sequences_block, "bindings")
    if bindings_block:
        session.bindings = _parse_bindings(bindings_block, schema)

    return session


def default_config_path() -> Path:
    return DEFAULT_EXPORT_PATH
