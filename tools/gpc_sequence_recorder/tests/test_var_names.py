"""Tests for recorder-only var sugar names."""

import pytest

from gpc_recorder.codegen.config_loader import load_config_hpp_text
from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.dsl.var_names import (
    empty_live_expr_casts,
    format_live_expr_casts_hpp_comment,
    normalize_live_expr_casts,
    parse_live_expr_casts_hpp_comment,

    empty_var_names,
    format_var_names_hpp_comment,
    normalize_var_names,
    parse_var_names_hpp_comment,
    resolve_var_ref,
    set_var_name,
    validate_sugar_name,
)
from gpc_recorder.graph_api import build_context_from_graph, session_to_graph
from gpc_recorder.paths import MICRO_VAR_SLOT_COUNT


def test_empty_var_names_length():
    names = empty_var_names()
    assert len(names) == MICRO_VAR_SLOT_COUNT
    assert all(n == "" for n in names)


def test_validate_sugar_name_rejects_vN_and_invalid():
    assert validate_sugar_name("brake_pos") == "brake_pos"
    with pytest.raises(ValueError, match="reserved"):
        validate_sugar_name("v0")
    with pytest.raises(ValueError, match="identifier"):
        validate_sugar_name("brake-pos")
    with pytest.raises(ValueError, match="non-empty"):
        validate_sugar_name("  ")


def test_set_var_name_unique_and_clear():
    names = empty_var_names()
    set_var_name(names, 0, "brake_pos")
    assert names[0] == "brake_pos"
    with pytest.raises(ValueError, match="already used"):
        set_var_name(names, 1, "brake_pos")
    set_var_name(names, 0, "")
    assert names[0] == ""
    set_var_name(names, 1, "brake_pos")
    assert names[1] == "brake_pos"


def test_resolve_var_ref_int_vn_and_sugar():
    names = empty_var_names()
    set_var_name(names, 3, "adc_raw")
    assert resolve_var_ref(3, names) == 3
    assert resolve_var_ref("3", names) == 3
    assert resolve_var_ref("v3", names) == 3
    assert resolve_var_ref("adc_raw", names) == 3
    with pytest.raises(ValueError, match="Unknown"):
        resolve_var_ref("missing", names)
    with pytest.raises(ValueError):
        resolve_var_ref(MICRO_VAR_SLOT_COUNT, names)


def test_normalize_var_names_pads_and_strips():
    out = normalize_var_names(["  a  ", None, "b"])
    assert out[0] == "a"
    assert out[1] == ""
    assert out[2] == "b"
    assert len(out) == MICRO_VAR_SLOT_COUNT


def test_command_resolves_sugar_var_name():
    eng = ReplEngine(auto_reload=False)
    set_var_name(eng.ctx.session.var_names, 1, "brake_pos")
    assert eng.ctx.session.var_names[1] == "brake_pos"

    out, ok = eng.execute("bind_powerup()")
    assert ok, out
    out, ok = eng.execute('gpio_read(port=1, pin=5, var_index="brake_pos")')
    assert ok, out
    out, ok = eng.execute("end_binding()")
    assert ok, out

    step = eng.ctx.session.powerup_steps[0]
    assert step.values["var_index"] == 1


def test_graph_round_trip_preserves_var_names_and_resolves():
    eng = ReplEngine(auto_reload=False)
    set_var_name(eng.ctx.session.var_names, 2, "temp_c")
    eng.execute("bind_powerup()")
    eng.execute('gpio_read(port=2, pin=1, var_index="temp_c")')
    eng.execute("end_binding()")

    graph = session_to_graph(eng.ctx.session)
    assert graph["var_names"][2] == "temp_c"

    # Rebuild with sugar name still in step args
    rebuilt = build_context_from_graph(
        {
            "config": graph["config"],
            "var_names": graph["var_names"],
            "containers": [
                {
                    "type": "powerup",
                    "steps": [
                        {
                            "command": "gpio_read",
                            "args": {"port": 2, "pin": 1, "var_index": "temp_c"},
                        }
                    ],
                }
            ],
        }
    )
    assert rebuilt.session.var_names[2] == "temp_c"
    assert rebuilt.session.powerup_steps[0].values["var_index"] == 2


def test_hpp_comment_omitted_when_empty():
    assert format_var_names_hpp_comment(None) == ""
    assert format_var_names_hpp_comment(empty_var_names()) == ""
    assert parse_var_names_hpp_comment("// no meta") == empty_var_names()


def test_hpp_comment_format_and_parse():
    names = empty_var_names()
    set_var_name(names, 0, "brake_pos")
    set_var_name(names, 3, "temp_c")
    comment = format_var_names_hpp_comment(names)
    assert comment.startswith("/* gpc-recorder:var_names=")
    assert "brake_pos" in comment
    assert "temp_c" in comment
    parsed = parse_var_names_hpp_comment(f"#ifndef X\n{comment}\n#define X\n")
    assert parsed[0] == "brake_pos"
    assert parsed[3] == "temp_c"
    assert parsed[1] == ""


def test_hpp_emit_load_round_trip_preserves_var_names():
    eng = ReplEngine(auto_reload=False)
    set_var_name(eng.ctx.session.var_names, 1, "brake_pos")
    set_var_name(eng.ctx.session.var_names, 4, "adc_raw")
    eng.execute("bind_powerup()")
    eng.execute('gpio_read(port=1, pin=5, var_index="brake_pos")')
    eng.execute("end_binding()")

    text = emit_config_hpp(eng.ctx.session.to_dict(), eng.ctx.schema, write=False)
    assert "gpc-recorder:var_names=" in text
    assert "brake_pos" in text
    assert "adc_raw" in text

    reloaded = load_config_hpp_text(text, eng.ctx.schema, source="roundtrip.hpp")
    assert reloaded.var_names[1] == "brake_pos"
    assert reloaded.var_names[4] == "adc_raw"
    assert reloaded.powerup_steps[0].values["var_index"] == 1
    # Sugar still resolves after reload
    assert resolve_var_ref("brake_pos", reloaded.var_names) == 1
    assert resolve_var_ref("adc_raw", reloaded.var_names) == 4


def test_hpp_emit_without_names_has_no_metadata_comment():
    eng = ReplEngine(auto_reload=False)
    eng.execute("bind_powerup()")
    eng.execute("delay(ms=1)")
    eng.execute("end_binding()")
    text = emit_config_hpp(eng.ctx.session.to_dict(), eng.ctx.schema, write=False)
    assert "gpc-recorder:var_names" not in text


def test_live_expr_casts_normalize_and_legacy():
    out = normalize_live_expr_casts(["double", "dec", "dec_array", "nope"])
    assert out[0] == "double"
    assert out[1] == "int"
    assert out[2] == "array_int8"
    assert out[3] == "int"
    assert len(out) == len(empty_live_expr_casts())


def test_hpp_comment_omitted_when_casts_default():
    assert format_live_expr_casts_hpp_comment(None) == ""
    assert format_live_expr_casts_hpp_comment(empty_live_expr_casts()) == ""
    assert parse_live_expr_casts_hpp_comment("// no meta") == empty_live_expr_casts()


def test_hpp_emit_load_round_trip_preserves_live_expr_casts():
    from gpc_recorder.codegen.config_loader import load_config_hpp_text
    from gpc_recorder.codegen.emitter import emit_config_hpp
    from gpc_recorder.dsl.repl import ReplEngine

    eng = ReplEngine(auto_reload=False)
    eng.ctx.session.live_expr_casts[1] = "double"
    eng.ctx.session.live_expr_casts[4] = "hex"
    eng.execute("bind_powerup()")
    eng.execute("delay(ms=1)")
    eng.execute("end_binding()")

    text = emit_config_hpp(eng.ctx.session.to_dict(), eng.ctx.schema, write=False)
    assert "gpc-recorder:live_expr_casts=" in text
    assert '"double"' in text
    assert '"hex"' in text

    reloaded = load_config_hpp_text(text, eng.ctx.schema, source="roundtrip.hpp")
    assert reloaded.live_expr_casts[1] == "double"
    assert reloaded.live_expr_casts[4] == "hex"
    assert reloaded.live_expr_casts[0] == "int"


def test_graph_round_trip_preserves_live_expr_casts():
    from gpc_recorder.dsl.repl import ReplEngine
    from gpc_recorder.graph_api import build_context_from_graph, session_to_graph

    eng = ReplEngine(auto_reload=False)
    eng.ctx.session.live_expr_casts[2] = "array_uint8"
    graph = session_to_graph(eng.ctx.session)
    assert graph["live_expr_casts"][2] == "array_uint8"
    rebuilt = build_context_from_graph({"config": graph["config"], "live_expr_casts": graph["live_expr_casts"], "containers": []})
    assert rebuilt.session.live_expr_casts[2] == "array_uint8"
