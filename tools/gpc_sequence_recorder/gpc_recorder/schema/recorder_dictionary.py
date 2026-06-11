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
    "bind_command": "bind_command(DRIVE_COMMAND, DriveCommand(require_autonomous=False, desired_drive_mode=DRIVE_MODE_BRAKE_NEUTRAL))",
    "end_binding": "end_binding()",
    "clear_command": "clear_command()",
    "undo": "undo()",
    "gpio_write": "gpio_write(port=1, pin=5, value=1)",
    "gpio_read": "gpio_read(port=1, pin=5, var_index=0)",
    "adc_read": "adc_read(adc_instance=2, channel=0, var_index=0, store_raw=1)",
    "dac_write": "dac_write(dac_instance=1, use_var=0, var_index=0, literal_value=0)",
    "delay_ms": "delay_ms(100)",
    "can_transmit": "can_transmit(can_bus=1, id=0x12, dlc=4, data=[0x12, 0x34, 0x56, 0x78])",
    "pwm_set": "pwm_set(timer_instance=1, channel=1, use_var=0, var_index=0, literal_duty=1000)",
    "uart_transmit": "uart_transmit(uart_instance=1, length=3, data=[0x01, 0x02, 0x03])",
    "spi_transfer": "spi_transfer(spi_instance=1, tx_len=3, tx_data=[0x9F, 0x00, 0x00])",
    "i2c_write": "i2c_write(i2c_instance=1, device_addr=0x50, length=2, data=[0x00, 0x01])",
    "reload": "reload()",
    "show": "show()",
    "preview": "preview()",
    "export": "export()",
    "help": "help()  # or help(\"DRIVE_COMMAND\")",
}

_DESCRIPTIONS: Dict[str, str] = {
    "config": "Set config name and target component id for export.",
    "bind_powerup": "Start recording the power-up micro-op sequence.",
    "clear_powerup": "Clear the current power-up sequence.",
    "bind_main_tick": "Start recording the main tick micro-op sequence (runs in every state).",
    "clear_main_tick": "Clear the main tick sequence.",
    "bind_state": "Record a one-shot state sequence (init/disengagement/power_up_bit); auto-transitions when done.",
    "clear_state": "Clear a saved one-shot state sequence.",
    "bind_state_tick": "Start recording a looping state tick sequence (manual/engaged/operational).",
    "clear_state_tick": "Clear a saved state tick sequence.",
    "bind_command": "Start recording a binding for a trigger payload type and its command struct.",
    "end_binding": "Finish and save the current recording (powerup, tick, state, or trigger binding).",
    "clear_command": "Discard the in-progress trigger binding (does not touch saved bindings).",
    "undo": "Undo the last recorded step (power-up or binding).",
    "gpio_write": "Add an immediate GPIO write micro-op step.",
    "gpio_read": "Add an immediate GPIO read micro-op step (stores into var_index).",
    "adc_read": "Add an ADC read micro-op step (stores into var_index).",
    "dac_write": "Add a DAC write micro-op step (from literal or var).",
    "delay_ms": "Add a delay micro-op step.",
    "can_transmit": "Add a CAN transmit micro-op step.",
    "pwm_set": "Add a PWM set micro-op step (from literal or var).",
    "uart_transmit": "Add a UART transmit micro-op step.",
    "spi_transfer": "Add a SPI transfer micro-op step.",
    "i2c_write": "Add an I2C write micro-op step.",
    "reload": "Load sequences and bindings from the current g474_gpc_config_memory.hpp.",
    "show": "Print current session summary (power-up + bindings).",
    "preview": "Generate the current HPP preview (no files written).",
    "export": "Update the loaded config HPP + packed BIN output files.",
    "help": "Show DSL help or details for a payload/struct name.",
}


_RECORDER_PUBLIC_COMMANDS = [
    "reload",
    "show",
    "bind_command",
    "end_binding",
    "preview",
    "export",
    "config",
    "clear_command",
    "bind_powerup",
    "clear_powerup",
    "bind_main_tick",
    "clear_main_tick",
    "bind_state",
    "clear_state",
    "bind_state_tick",
    "clear_state_tick",
    "undo",
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
    "help",
]


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
        out.append(
            {
                "name": name,
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
