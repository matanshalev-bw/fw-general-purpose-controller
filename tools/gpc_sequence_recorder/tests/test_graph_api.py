"""Graph API round-trip and export tests."""

from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.graph_api import (
    build_context_from_graph,
    export_graph,
    session_to_graph,
    _steps_to_commands,
)
from gpc_recorder.dsl.session import MicroOpStepState
from gpc_recorder.schema.loader import get_schema


def test_steps_to_commands_if_block():
    steps = [
        MicroOpStepState(
            op_type="IF_CONDITION",
            union_member="if_condition",
            values={
                "first_var_index": 0,
                "compare_type": "GE",
                "second_var_index": 1,
                "step_count": 2,
            },
        ),
        MicroOpStepState(
            op_type="DIGITAL_GPIO_WRITE",
            union_member="digital_gpio_write",
            values={"port": 1, "pin": 5, "value": 1},
        ),
        MicroOpStepState(
            op_type="MOVE_TO_ERROR_STATE",
            union_member="move_to_error_state",
            values={"reserved": [0, 0, 0, 0]},
        ),
    ]
    cmds = _steps_to_commands(steps)
    assert cmds[0]["command"] == "if_condition"
    assert cmds[0]["args"]["comparing_type"] == ">="
    assert cmds[1]["command"] == "gpio_write"
    assert cmds[2]["command"] == "move_to_error_state"
    assert cmds[3]["command"] == "end_condition"


def test_steps_to_commands_strips_internal_fields():
    steps = [
        MicroOpStepState(
            op_type="VAR_SET",
            union_member="var_set",
            values={"var_index": 1, "reserved": [0, 0, 0], "value": 3500},
        ),
        MicroOpStepState(
            op_type="MOVE_TO_ERROR_STATE",
            union_member="move_to_error_state",
            values={"reserved": [0, 0, 0, 0]},
        ),
    ]
    cmds = _steps_to_commands(steps)
    assert cmds[0] == {"command": "var_set", "args": {"var_index": 1, "value": 3500}}
    assert cmds[1] == {"command": "move_to_error_state", "args": {}}
    # Rebuilding a context from these commands must not raise on internal kwargs.
    graph = {
        "config": {"name": "G474_GPC_CONFIG", "component": "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"},
        "containers": [{"id": "powerup", "type": "powerup", "steps": cmds}],
    }
    ctx = build_context_from_graph(graph)
    assert len(ctx.session.powerup_steps) == 2


def test_session_to_graph_telemetry_field_names():
    engine = ReplEngine(auto_reload=False)
    for line in (
        "bind_telemetry(rate=1, trigger=HORN_TELEMETRY, "
        "requested_horn_time_var_index=1, remaining_horn_time_var_index=3)",
    ):
        out, cont = engine.execute(line)
        assert cont, out

    # Round-trip through config_loader-style mappings (no field_name key present).
    for tb in engine.ctx.session.telemetry_bindings:
        for m in tb.field_mappings:
            m.pop("field_name", None)

    graph = session_to_graph(engine.ctx.session)
    tel = next(c for c in graph["containers"] if c["type"] == "telemetry")
    assert "requested_horn_time_var_index" in tel["fields"]
    assert "remaining_horn_time_var_index" in tel["fields"]


def test_build_context_from_graph_powerup():
    graph = {
        "config": {"name": "G474_GPC_CONFIG", "component": "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"},
        "containers": [
            {
                "id": "powerup",
                "type": "powerup",
                "steps": [
                    {"command": "gpio_write", "args": {"port": 1, "pin": 5, "value": 1}},
                    {"command": "delay_ms", "args": {"delay_ms": 50}},
                ],
            }
        ],
    }
    ctx = build_context_from_graph(graph)
    assert len(ctx.session.powerup_steps) == 2
    assert ctx.session.powerup_steps[0].union_member == "digital_gpio_write"


def test_session_to_graph_round_trip():
    engine = ReplEngine(auto_reload=False)
    for line in (
        "bind_powerup()",
        "gpio_write(port=1, pin=5, value=1)",
        "if_condition(first_var_index=0, comparing_type=\">=\", second_var_index=1)",
        "gpio_write(port=1, pin=6, value=0)",
        "end_condition()",
        "end_binding()",
    ):
        out, cont = engine.execute(line)
        assert cont, out

    graph = session_to_graph(engine.ctx.session)
    assert any(c["type"] == "powerup" for c in graph["containers"])
    powerup = next(c for c in graph["containers"] if c["type"] == "powerup")
    assert powerup["steps"][0]["command"] == "gpio_write"
    assert powerup["steps"][1]["command"] == "if_condition"
    assert powerup["steps"][2]["command"] == "gpio_write"
    assert powerup["steps"][3]["command"] == "end_condition"

    ctx2 = build_context_from_graph(graph)
    schema = get_schema()
    hpp1 = emit_config_hpp(engine.ctx.session.to_dict(), schema, write=False)
    hpp2 = emit_config_hpp(ctx2.session.to_dict(), schema, write=False)
    assert "powerup_sequence" in hpp1
    assert "digital_gpio_write" in hpp1
    assert "if_condition" in hpp1 or "IF_CONDITION" in hpp1
    assert hpp1 == hpp2


def test_export_graph_powerup_hpp(tmp_path, monkeypatch):
    from gpc_recorder import paths

    export_path = tmp_path / "g474_gpc_config_memory.hpp"
    bin_path = tmp_path / "config_g474.bin"
    monkeypatch.setattr(paths, "DEFAULT_EXPORT_PATH", export_path)
    monkeypatch.setattr(paths, "DEFAULT_EXPORT_BIN_PATH", bin_path)

    graph = {
        "config": {"name": "TEST_CONFIG", "component": "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"},
        "containers": [
            {
                "id": "powerup",
                "type": "powerup",
                "steps": [
                    {"command": "gpio_write", "args": {"port": 1, "pin": 5, "value": 1}},
                    {"command": "delay_ms", "args": {"delay_ms": 10}},
                ],
            }
        ],
    }

    try:
        text = export_graph(graph)
    except Exception as exc:
        if "cmake" in str(exc).lower() or "cubeide" in str(exc).lower():
            ctx = build_context_from_graph(graph)
            text = emit_config_hpp(ctx.session.to_dict(), get_schema(), write=False)
        else:
            raise

    assert "TEST_CONFIG" in text
    assert "powerup_sequence" in text
    assert "digital_gpio_write" in text
