"""Graph JSON <-> RecorderContext conversion for the visual editor."""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from gpc_recorder.codegen.config_loader import load_config_hpp
from gpc_recorder.dsl.builtins import RecorderContext
from gpc_recorder.dsl.session import MicroOpStepState, Session
from gpc_recorder.paths import (
    CONTROLLER_STATE_SEQUENCE_FIELDS,
    CONTROLLER_STATE_TICK_FIELDS,
    DEFAULT_EXPORT_PATH,
)
from gpc_recorder.schema.loader import get_schema

_UNION_TO_COMMAND: Dict[str, str] = {
    "digital_gpio_write": "gpio_write",
    "digital_gpio_read": "gpio_read",
    "adc_read": "adc_read",
    "dac_write": "dac_write",
    "pwm_set": "pwm_set",
    "delay_ms": "delay_ms",
    "can_transmit": "can_transmit",
    "uart_transmit": "uart_transmit",
    "spi_transfer": "spi_transfer",
    "i2c_write": "i2c_write",
    "var_set": "var_set",
    "if_condition": "if_condition",
    "move_to_error_state": "move_to_error_state",
    "move_to_emergency_state": "move_to_emergency_state",
    "trigger_safety": "trigger_safety",
}

_COMPARE_TYPE_TO_DSL: Dict[str, str] = {
    "EQ": "==",
    "NE": "!=",
    "GT": ">",
    "GE": ">=",
    "LT": "<",
    "LE": "<=",
}

_STATE_LABELS: Dict[str, str] = {
    "CONTROLLER_STATE_INIT": "INIT",
    "CONTROLLER_STATE_DISENGAGEMENT": "DISENGAGEMENT",
    "CONTROLLER_STATE_POWER_UP_BIT": "POWER-UP BIT",
    "CONTROLLER_STATE_MANUAL": "MANUAL",
    "CONTROLLER_STATE_ENGAGED": "ENGAGED",
    "CONTROLLER_STATE_OPERATIONAL": "OPERATIONAL",
    "CONTROLLER_STATE_ERROR": "ERROR",
    "CONTROLLER_STATE_EMERGENCY": "EMERGENCY",
}


def _step_to_command(step: MicroOpStepState) -> Optional[Dict[str, Any]]:
    command = _UNION_TO_COMMAND.get(step.union_member)
    if command is None:
        return None
    args = dict(step.values)
    # Drop internal-only fields that the DSL builtins auto-fill and do not accept as kwargs.
    args.pop("reserved", None)
    if command == "if_condition":
        compare = args.pop("compare_type", args.pop("comparing_type", "EQ"))
        args.pop("step_count", None)
        args["comparing_type"] = _COMPARE_TYPE_TO_DSL.get(str(compare), str(compare))
    return {"command": command, "args": args}


def _steps_to_commands(steps: List[MicroOpStepState]) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    i = 0
    while i < len(steps):
        step = steps[i]
        cmd = _step_to_command(step)
        if cmd is None:
            i += 1
            continue
        if cmd["command"] == "if_condition":
            out.append(cmd)
            body_count = int(step.values.get("step_count", 0))
            for j in range(body_count):
                body_idx = i + 1 + j
                if body_idx >= len(steps):
                    break
                body_cmd = _step_to_command(steps[body_idx])
                if body_cmd is not None:
                    out.append(body_cmd)
            out.append({"command": "end_condition", "args": {}})
            i += 1 + body_count
        else:
            out.append(cmd)
            i += 1
    return out


def _apply_steps(ctx: RecorderContext, steps: List[Dict[str, Any]]) -> None:
    for step in steps:
        command = step.get("command")
        if not command:
            raise ValueError("Step missing command")
        if command == "end_condition":
            ctx.end_condition()
            continue
        args = step.get("args") or {}
        method = getattr(ctx, command, None)
        if method is None or not callable(method):
            raise ValueError(f"Unknown recorder command: {command}")
        method(**args)


def _apply_container(ctx: RecorderContext, container: Dict[str, Any]) -> None:
    ctype = container.get("type")
    steps = container.get("steps") or []

    if ctype == "powerup":
        ctx.bind_powerup()
        _apply_steps(ctx, steps)
        ctx.end_binding()
        return

    if ctype == "main_tick":
        ctx.bind_main_tick()
        _apply_steps(ctx, steps)
        ctx.end_binding()
        return

    if ctype == "state":
        state = container.get("state")
        if not state:
            raise ValueError("State container missing state")
        ctx.bind_state(state)
        _apply_steps(ctx, steps)
        ctx.end_binding()
        return

    if ctype == "state_tick":
        state = container.get("state")
        if not state:
            raise ValueError("State tick container missing state")
        ctx.bind_state_tick(state)
        _apply_steps(ctx, steps)
        ctx.end_binding()
        return

    if ctype == "command":
        trigger = container.get("trigger")
        if not trigger:
            raise ValueError("Command container missing trigger")
        fields = container.get("fields") or {}
        ctx.bind_command(trigger, **fields)
        _apply_steps(ctx, steps)
        ctx.end_binding()
        return

    if ctype == "telemetry":
        trigger = container.get("trigger")
        rate = container.get("rate")
        if not trigger or rate is None:
            raise ValueError("Telemetry container missing trigger or rate")
        fields = container.get("fields") or {}
        ctx.bind_telemetry(int(rate), trigger, **fields)
        return

    raise ValueError(f"Unknown container type: {ctype}")


def build_context_from_graph(graph: Dict[str, Any]) -> RecorderContext:
    ctx = RecorderContext()
    config = graph.get("config") or {}
    if config.get("name") or config.get("component"):
        ctx.config(
            name=config.get("name") or "G474_GPC_CONFIG",
            component=config.get("component") or "",
        )
    for container in graph.get("containers") or []:
        _apply_container(ctx, container)
    return ctx


def export_graph(graph: Dict[str, Any]) -> str:
    ctx = build_context_from_graph(graph)
    return ctx.export(str(DEFAULT_EXPORT_PATH))


def session_to_graph(session: Session) -> Dict[str, Any]:
    containers: List[Dict[str, Any]] = []

    if session.powerup_steps:
        containers.append(
            {
                "id": "powerup",
                "type": "powerup",
                "label": "POWER UP",
                "steps": _steps_to_commands(session.powerup_steps),
            }
        )

    if session.main_tick_steps:
        containers.append(
            {
                "id": "main_tick",
                "type": "main_tick",
                "label": "MAIN",
                "steps": _steps_to_commands(session.main_tick_steps),
            }
        )

    for state in CONTROLLER_STATE_SEQUENCE_FIELDS:
        steps = session.state_steps.get(state)
        if steps:
            containers.append(
                {
                    "id": state,
                    "type": "state",
                    "state": state,
                    "label": _STATE_LABELS.get(state, state),
                    "steps": _steps_to_commands(steps),
                }
            )

    for state in CONTROLLER_STATE_TICK_FIELDS:
        steps = session.state_tick_steps.get(state)
        if steps:
            containers.append(
                {
                    "id": state,
                    "type": "state_tick",
                    "state": state,
                    "label": _STATE_LABELS.get(state, state),
                    "steps": _steps_to_commands(steps),
                }
            )

    for i, binding in enumerate(session.bindings):
        containers.append(
            {
                "id": f"command_{i}",
                "type": "command",
                "label": f"COMMAND: {binding.payload_type}",
                "trigger": binding.payload_type,
                "fields": binding.field_values,
                "steps": _steps_to_commands(binding.steps),
            }
        )

    schema = get_schema()
    for i, tb in enumerate(session.telemetry_bindings):
        struct_def = schema.telemetry_structs.get(tb.struct_name)
        struct_fields = struct_def.fields if struct_def else []
        fields = {}
        for idx, m in enumerate(tb.field_mappings):
            field_name = m.get("field_name")
            if not field_name and idx < len(struct_fields):
                field_name = struct_fields[idx].name
            if not field_name:
                field_name = f"field{idx}"
            fields[f"{field_name}_var_index"] = m["var_index"]
        containers.append(
            {
                "id": f"telemetry_{i}",
                "type": "telemetry",
                "label": f"TELEMETRY: {tb.payload_type}",
                "trigger": tb.payload_type,
                "rate": tb.rate_hz,
                "fields": fields,
                "steps": [],
            }
        )

    return {
        "config": {
            "name": session.config_name,
            "component": session.component_id,
        },
        "containers": containers,
        "one_shot_states": list(CONTROLLER_STATE_SEQUENCE_FIELDS.keys()),
        "tick_states": list(CONTROLLER_STATE_TICK_FIELDS.keys()),
    }


def load_graph_from_config(path: Optional[str] = None) -> Dict[str, Any]:
    schema = get_schema()
    src = DEFAULT_EXPORT_PATH if not path else path
    session = load_config_hpp(src, schema)
    return session_to_graph(session)
