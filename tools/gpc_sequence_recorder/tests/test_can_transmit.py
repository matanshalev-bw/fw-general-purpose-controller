"""can_transmit data coercion and normalization."""

from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.normalize import normalize_line
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.schema.loader import get_schema


def test_can_transmit_accepts_python_list():
    engine = ReplEngine(auto_reload=False)
    engine.execute("bind_state(CONTROLLER_STATE_INIT)")
    out, ok = engine.execute("can_transmit(can_bus=1, id=55, dlc=3, data=[0, 1, 2])")
    assert ok, out
    data = engine.ctx.session.state_steps["CONTROLLER_STATE_INIT"][0].values["data"]
    assert data == [0, 1, 2]


def test_can_transmit_accepts_quoted_list_string():
    engine = ReplEngine(auto_reload=False)
    engine.execute("bind_state(CONTROLLER_STATE_INIT)")
    out, ok = engine.execute('can_transmit(can_bus=1, id=55, dlc=3, data="[ 0, 1, 2 ]")')
    assert ok, out
    data = engine.ctx.session.state_steps["CONTROLLER_STATE_INIT"][0].values["data"]
    assert data == [0, 1, 2]


def test_can_transmit_comma_separated_string():
    engine = ReplEngine(auto_reload=False)
    engine.execute("bind_state(CONTROLLER_STATE_INIT)")
    out, ok = engine.execute('can_transmit(can_bus=1, id=55, dlc=3, data="0, 1, 2")')
    assert ok, out
    data = engine.ctx.session.state_steps["CONTROLLER_STATE_INIT"][0].values["data"]
    assert data == [0, 1, 2]


def test_normalize_unwraps_quoted_data_list():
    src = 'can_transmit(can_bus=1, id=55, dlc=3, data="[0, 1, 2]")'
    assert normalize_line(src) == "can_transmit(can_bus=1, id=55, dlc=3, data=[0, 1, 2])"


def test_can_transmit_exports_after_quoted_data():
    engine = ReplEngine(auto_reload=False)
    engine.execute("bind_state(CONTROLLER_STATE_INIT)")
    engine.execute('can_transmit(can_bus=1, id=0x37, dlc=3, data="[0, 1, 2]")')
    engine.execute("end_binding()")
    hpp = emit_config_hpp(engine.ctx.session.to_dict(), get_schema(), write=False)
    assert "can_transmit" in hpp
    assert "0x37" in hpp
