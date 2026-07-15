"""Recorder DSL command reference for the web UI."""

from __future__ import annotations

import inspect
from typing import Any, Dict, List

from gpc_recorder.dsl.builtins import RecorderContext


def _is_public_method(name: str, value: Any) -> bool:
    if name.startswith("_"):
        return False
    if not callable(value):
        return False
    return True


_EXAMPLES: Dict[str, str] = {
    "config": "config(name=\"G474_GPC_CONFIG\", component=COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER)",
    "bind_powerup": "bind_powerup()",
    "clear_powerup": "clear_powerup()",
    "bind_main_tick": "bind_main_tick()",
    "clear_main_tick": "clear_main_tick()",
    "bind_state": "bind_state(CONTROLLER_STATE_INIT)",
    "clear_state": "clear_state(CONTROLLER_STATE_INIT)",
    "bind_state_tick": "bind_state_tick(CONTROLLER_STATE_OPERATIONAL)",
    "clear_state_tick": "clear_state_tick(CONTROLLER_STATE_OPERATIONAL)",
    "bind_command": "bind_command(trigger=DRIVE_COMMAND, require_autonomous=False, desired_drive_mode=DRIVE_MODE_BRAKE_NEUTRAL)",
    "bind_telemetry": (
        "bind_telemetry(rate=1, trigger=HORN_TELEMETRY, "
        "requested_horn_time_var_index=1, remaining_horn_time_var_index=3)"
    ),
    "end_binding": "end_binding()",
    "clear_command": "clear_command()",
    "clear_telemetry": "clear_telemetry()",
    "undo": "undo()",
    "gpio_write": "gpio_write(port=1, pin=5, value=1)",
    "gpio_read": "gpio_read(port=1, pin=5, var_index=0)",
    "adc_read": "adc_read(adc_instance=1, channel=0, var_index=0, store_raw=1)",
    "dac_write": "dac_write(dac_instance=1, use_var=0, var_index=0, literal_value=2048)",
    "delay_ms": "delay_ms(100)",
    "can_transmit": "can_transmit(can_bus=1, id=0x12, dlc=4, data=[0x12, 0x34, 0x56, 0x78])",
    "pwm_set": "pwm_set(timer_instance=1, channel=1, use_var=0, var_index=0, literal_duty=1000)",
    "uart_transmit": 'uart_transmit(uart_instance=1, length=5, data=[0x48, 0x45, 0x4C, 0x4C, 0x4F])  # or USB data: "HELLO"',
    "spi_transfer": "spi_transfer(spi_instance=1, tx_len=3, tx_data=[0x9F, 0x00, 0x00])",
    "i2c_write": "i2c_write(i2c_instance=1, device_addr=0x50, length=2, data=[0x00, 0x01])",
    "can_receive": "can_receive(can_bus=1, id=0x12, dlc=4, var_index=0)",
    "uart_receive": "uart_receive(uart_instance=1, length=4, var_index=0)",
    "spi_receive": "spi_receive(spi_instance=1, rx_len=3, var_index=0)",
    "i2c_read": "i2c_read(i2c_instance=1, device_addr=0x50, length=2, var_index=0)",
    "var_set": "var_set(var_index=1, value=3343114)  # or value=[0x0A, 0x03, 0x33] (LE → uint64)",
    "if_condition": "if_condition(first_var_index=0, comparing_type=\">=\", second_var_index=1)",
    "end_condition": "end_condition()",
    "move_to_error_state": "move_to_error_state()",
    "move_to_emergency_state": "move_to_emergency_state()",
    "trigger_safety": "trigger_safety(safety_en=0)",
    "reload": "reload()",
    "show": "show()",
    "preview": "preview()",
    "export": "export()",
    "help": "help()  # or help(\"DRIVE_COMMAND\")",
}

_DESCRIPTIONS: Dict[str, str] = {
    "config": "Set the exported config struct name and target ComponentId.",
    "bind_powerup": "Resume or start recording the power-up micro-op sequence; appends to existing steps (max 15). Use clear_powerup() to wipe first.",
    "clear_powerup": "Clear the power-up sequence and stop power-up recording.",
    "bind_main_tick": "Resume or start recording the main tick sequence; appends to existing steps. Use clear_main_tick() to wipe first.",
    "clear_main_tick": "Clear the main tick sequence and stop main-tick recording.",
    "bind_state": "Resume or start a one-shot state sequence (INIT, DISENGAGEMENT, or POWER_UP_BIT); appends to existing steps. Use clear_state() to wipe first.",
    "clear_state": "Remove the saved one-shot sequence for the given state.",
    "bind_state_tick": "Resume or start a looping tick sequence for MANUAL, ENGAGED, OPERATIONAL, ERROR, or EMERGENCY; appends to existing steps. Use clear_state_tick() to wipe first.",
    "clear_state_tick": "Remove the saved tick sequence for the given state.",
    "bind_command": "Start a trigger binding: choose payload type and command fields, then append micro-op steps.",
    "bind_telemetry": (
        "Bind periodic telemetry (max 3, payload ≤8 bytes): set rate Hz, telemetry type, "
        "and {field}_var_index for each struct field. Saves immediately (no end_binding)."
    ),
    "end_binding": "Save the active recording (power-up, main tick, state, state tick, or trigger binding).",
    "clear_command": "Discard the in-progress trigger binding; saved bindings are not changed.",
    "clear_telemetry": "Remove all saved telemetry bindings.",
    "undo": "Remove the last micro-op step from the active recording, or peel back the last saved binding.",
    "gpio_write": "Append a digital GPIO write step to the active recording.",
    "gpio_read": "Append a digital GPIO read step; stores the pin value in var_index (0–19).",
    "adc_read": "Append an ADC read step; stores the sample in var_index (store_raw selects raw vs scaled). Pins: ADC1→PA0, ADC2→PB2.",
    "dac_write": "Append a DAC write step using a literal value or a var slot (use_var=1). Uses DAC1 CH1 on PA4.",
    "delay_ms": "Append a delay step (milliseconds).",
    "can_transmit": "Append a CAN transmit step (bus, id, dlc, and up to 8 data bytes).",
    "pwm_set": "Append a PWM duty step using a literal value or a var slot (use_var=1).",
    "uart_transmit": "Append a UART transmit step.",
    "spi_transfer": "Append a SPI transfer step (TX data only).",
    "i2c_write": "Append an I2C write step.",
    "can_receive": "Poll CAN RX for a matching ID (timeout DEFAULT_COMM_RX_TIMEOUT); store up to dlc bytes in var_index.",
    "uart_receive": "Poll UART RX for length bytes; store little-endian into var_index.",
    "spi_receive": "SPI master receive (zero TX clocking); store rx_len bytes into var_index.",
    "i2c_read": "I2C master read; store length bytes into var_index.",
    "var_set": (
        "Set a var slot (0–19) to a uint64: raw integer, or a byte array packed little-endian "
        "the same way COMM RX stores into the var (e.g. [0x0A, 0x03, 0x33] → 0x33030A)."
    ),
    "if_condition": "Start a conditional block; compare two var slots (==, !=, >, >=, <, <=).",
    "end_condition": "Close the active if_condition block and record its body step count.",
    "move_to_error_state": "Transition the controller to CONTROLLER_STATE_ERROR.",
    "move_to_emergency_state": "Transition the controller to CONTROLLER_STATE_EMERGENCY.",
    "trigger_safety": "Drive SAFETY_EN low (0) or high (1).",
    "reload": "Load power-up sequences, state sequences, and bindings from g474_gpc_config_memory.hpp.",
    "show": "Print session summary: config, all sequences, and trigger bindings.",
    "preview": "Render the config HPP in memory without writing files.",
    "export": "Write g474_gpc_config_memory.hpp and rebuild config_g474.bin (STM32CubeIDE headless build).",
    "help": "List DSL commands, or show fields for a payload, micro-op, or struct name.",
}


_RECORDER_COMMAND_GROUPS: List[tuple[str, List[str]]] = [
    ("", ["reload"]),
    (
        "bind commands",
        [
            "bind_powerup",
            "clear_powerup",
            "bind_main_tick",
            "clear_main_tick",
            "bind_state",
            "clear_state",
            "bind_state_tick",
            "clear_state_tick",
            "bind_command",
            "clear_command",
            "bind_telemetry",
            "clear_telemetry",
            "end_binding",
        ],
    ),
    (
        "micro commands",
        [
            "gpio_write",
            "gpio_read",
            "adc_read",
            "dac_write",
            "delay_ms",
            "can_transmit",
            "pwm_set",
            "uart_transmit",
            "spi_transfer",
            "i2c_write",
            "can_receive",
            "uart_receive",
            "spi_receive",
            "i2c_read",
            "var_set",
            "if_condition",
            "end_condition",
            "move_to_error_state",
            "move_to_emergency_state",
            "trigger_safety",
            "undo",
        ],
    ),
    (
        "config utils",
        [
            "config",
            "show",
            "preview",
            "export",
            "help",
        ],
    ),
]

_RECORDER_PUBLIC_COMMANDS = [name for _, names in _RECORDER_COMMAND_GROUPS for name in names]
_RECORDER_COMMAND_GROUP_BY_NAME = {
    name: group for group, names in _RECORDER_COMMAND_GROUPS for name in names
}


def _resolve_recorder_method(ctx: RecorderContext, name: str):
    return getattr(ctx, name, None)


def _param_to_dict(p: inspect.Parameter) -> Dict[str, Any]:
    has_default = p.default is not inspect._empty
    default = None if not has_default else p.default
    ann = None if p.annotation is inspect._empty else p.annotation
    ann_str = None
    if ann is not None:
        try:
            ann_str = ann.__name__  # type: ignore[attr-defined]
        except Exception:
            ann_str = str(ann).replace("typing.", "")
    kind = str(p.kind).split(".")[-1]
    is_list = False
    if ann_str:
        is_list = ann_str == "List" or ann_str.startswith("List[")
    return {
        "name": p.name,
        "kind": kind,
        "annotation": ann_str,
        "has_default": has_default,
        "default": default,
        "is_list": is_list,
    }


_LENGTH_PARAM_NAMES = frozenset({"length", "dlc", "tx_len", "rx_len"})
_DATA_PARAM_NAMES = frozenset({"data", "tx_data"})

# Explicit pin / channel maps shown next to param labels (same style as max-length hints).
_PARAM_HINTS: Dict[str, Dict[str, str]] = {
    "adc_read": {
        "adc_instance": "1=PA0 (ADC1_IN1), 2=PB2 (ADC2_IN12)",
        "channel": "use 0 (only buffer index)",
        "store_raw": "1=raw ADC counts, 0=millivolts",
    },
    "dac_write": {
        "dac_instance": "use 1 → PA4 (DAC1_OUT1)",
        "use_var": "1=value from var_index, 0=use literal_value",
        "literal_value": "12-bit code 0–4095",
    },
}


def _enrich_params_with_payload_limits(name: str, params: List[Dict[str, Any]]) -> None:
    """Attach max byte counts from the MicroOps C++ structs when available."""
    try:
        from gpc_recorder.dsl.pack import resolve_array_size
        from gpc_recorder.schema.loader import get_schema
    except Exception:
        return
    try:
        schema = get_schema()
    except Exception:
        return

    # var_set.value accepts a LE byte array (same packing as COMM RX → uint64).
    if name == "var_set":
        for p in params:
            if p["name"] == "value":
                p["accepts_byte_list"] = True
                p["max_len"] = schema.constants.get("COMM_DATA_LENGTH", 8)
        return

    op = schema.micro_ops.get(name)
    if op is None:
        return
    field_by_name = {f.name: f for f in op.fields}
    payload_max = None
    for field in op.fields:
        if not field.array_size:
            continue
        payload_max = resolve_array_size(schema, field.array_size)
        break
    if payload_max is None:
        return
    for p in params:
        field = field_by_name.get(p["name"])
        if field and field.array_size:
            p["max_len"] = resolve_array_size(schema, field.array_size)
        elif p["name"] in _LENGTH_PARAM_NAMES or p["name"] in _DATA_PARAM_NAMES:
            if p.get("is_list") or p["name"] in _DATA_PARAM_NAMES:
                p["max_len"] = payload_max
            else:
                p["max_value"] = payload_max


def _enrich_params_with_pin_hints(name: str, params: List[Dict[str, Any]]) -> None:
    hints = _PARAM_HINTS.get(name)
    if not hints:
        return
    for p in params:
        hint = hints.get(p["name"])
        if hint:
            p["hint"] = hint


def recorder_commands_dictionary() -> Dict[str, Any]:
    """
    Returns a stable list of DSL builtins available in the REPL.

    Format:
      { "recorder_commands": [...], "controller_states": [...], "controller_one_shot_states": [...], "controller_tick_states": [...] }
    """
    from gpc_recorder.paths import CONTROLLER_STATE_SEQUENCE_FIELDS, CONTROLLER_STATE_TICK_FIELDS

    ctx = RecorderContext()
    out: List[Dict[str, Any]] = []
    for name in _RECORDER_PUBLIC_COMMANDS:
        value = _resolve_recorder_method(ctx, name)
        if not _is_public_method(name, value):
            continue
        try:
            sig_obj = inspect.signature(value)
            sig = str(sig_obj)
        except (TypeError, ValueError):
            sig_obj = None
            sig = "()"
        doc = (inspect.getdoc(value) or "").strip()
        params: List[Dict[str, Any]] = []
        if sig_obj is not None:
            for p in sig_obj.parameters.values():
                if p.name == "self":
                    continue
                params.append(_param_to_dict(p))
        _enrich_params_with_payload_limits(name, params)
        _enrich_params_with_pin_hints(name, params)
        out.append(
            {
                "name": name,
                "group": _RECORDER_COMMAND_GROUP_BY_NAME.get(name, ""),
                "signature": sig,
                "description": _DESCRIPTIONS.get(name, ""),
                "doc": doc,
                "example": _EXAMPLES.get(name, f"{name}()"),
                "params": params,
            }
        )

    return {
        "recorder_commands": out,
        "controller_states": list(CONTROLLER_STATE_SEQUENCE_FIELDS.keys())
        + list(CONTROLLER_STATE_TICK_FIELDS.keys()),
        "controller_one_shot_states": list(CONTROLLER_STATE_SEQUENCE_FIELDS.keys()),
        "controller_tick_states": list(CONTROLLER_STATE_TICK_FIELDS.keys()),
    }
