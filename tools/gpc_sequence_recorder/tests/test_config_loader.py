"""Load and round-trip g474_gpc_config_memory.hpp."""

import re
from pathlib import Path

import pytest

from gpc_recorder.codegen.config_loader import load_config_hpp
from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.pack import pack_trigger_data, unpack_trigger_data
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.paths import REPO_ROOT
from gpc_recorder.schema.loader import get_schema


def _normalize(text: str) -> str:
    lines = [line.rstrip() for line in text.splitlines()]
    return "\n".join(lines) + "\n"


@pytest.fixture(scope="module")
def schema():
    return get_schema()


def test_unpack_trigger_data_round_trip(schema):
    field_values = {
        "brake_mode": "BRAKE_MODE_FULLY_RELEASED",
        "desired_brakes_position_in_percentage": 0,
    }
    data, data_size = pack_trigger_data(schema, "BrakesContinuousCommand", field_values)
    restored = unpack_trigger_data(schema, "BrakesContinuousCommand", data, data_size)
    assert restored["brake_mode"] == "BRAKE_MODE_FULLY_RELEASED"
    assert restored["desired_brakes_position_in_percentage"] == 0


def test_load_current_repo_config(schema):
    path = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    session = load_config_hpp(path, schema)
    assert session.config_name == "G474_GPC_CONFIG"
    assert session.component_id == "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"
    assert len(session.bindings) == 2
    assert session.bindings[0].payload_type == "BRAKES_CONTINUOUS_COMMAND"
    assert session.bindings[1].payload_type == "DRIVER_STATE_COMMAND"
    assert len(session.state_tick_steps["CONTROLLER_STATE_ENGAGED"]) == 4
    assert len(session.state_tick_steps["CONTROLLER_STATE_OPERATIONAL"]) == 4


def test_reload_round_trip_emits_equivalent_hpp(schema, tmp_path):
    path = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    session = load_config_hpp(path, schema)
    generated = emit_config_hpp(session.to_dict(), schema, tmp_path / "out.hpp", write=False)
    expected = _normalize(path.read_text(encoding="utf-8"))
    assert _normalize(generated) == expected


def test_repl_auto_reload_on_startup():
    engine = ReplEngine()
    assert len(engine.ctx.session.bindings) == 2
    assert engine.ctx._config_path.name == "g474_gpc_config_memory.hpp"
    assert "Reloaded from" in engine.startup_message


def test_repl_reload_command():
    engine = ReplEngine(auto_reload=False)
    out, cont = engine.execute("reload()")
    assert cont, out
    assert len(engine.ctx.session.bindings) == 2
    assert engine.ctx._config_path.name == "g474_gpc_config_memory.hpp"
