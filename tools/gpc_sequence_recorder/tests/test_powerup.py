"""Powerup sequence REPL and export tests."""

from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.schema.loader import get_schema


def test_powerup_repl_and_export():
    engine = ReplEngine(auto_reload=False)
    for line in (
        "bind_powerup()",
        "gpio_write(port=1, pin=5, value=1)",
        "delay_ms(50)",
        "end_binding()",
    ):
        out, cont = engine.execute(line)
        assert cont, out

    hpp = engine.preview_hpp()
    assert "powerup_sequence" in hpp
    assert "digital_gpio_write" in hpp

    schema = get_schema()
    text = emit_config_hpp(engine.ctx.session.to_dict(), schema, write=False)
    assert "powerup_sequence" in text


def test_bind_powerup_in_completion():
    from gpc_recorder.dsl.builtins import build_namespace, RecorderContext

    ns = build_namespace(RecorderContext())
    assert "bind_powerup" in ns
    assert "end_binding" in ns
