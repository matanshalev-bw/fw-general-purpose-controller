"""Tests for if_condition / end_condition / var_set DSL commands."""

import pytest

from gpc_recorder.codegen.config_loader import load_config_hpp
from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.builtins import RecorderContext, build_namespace
from gpc_recorder.schema.loader import get_schema
from gpc_recorder.validate import validate_condition_blocks


@pytest.fixture
def ns():
    ctx = RecorderContext()
    return build_namespace(ctx), ctx


def test_var_set_if_condition_records_main_tick_steps(ns):
    namespace, ctx = ns
    namespace["bind_main_tick"]()
    namespace["gpio_read"](port=2, pin=15, var_index=0)
    namespace["var_set"](var_index=1, value=1)
    namespace["if_condition"](first_var_index=0, comparing_type="==", second_var_index=1)
    namespace["gpio_write"](port=2, pin=15, value=1)
    namespace["end_condition"]()
    namespace["delay_ms"](500)
    namespace["end_binding"]()

    assert len(ctx.session.main_tick_steps) == 5
    assert ctx.session.main_tick_steps[1].union_member == "var_set"
    assert ctx.session.main_tick_steps[1].values["value"] == 1
    assert ctx.session.main_tick_steps[2].union_member == "if_condition"
    assert ctx.session.main_tick_steps[2].values["compare_type"] == "EQ"
    assert ctx.session.main_tick_steps[2].values["step_count"] == 1
    assert ctx.session.main_tick_steps[3].union_member == "digital_gpio_write"


def test_compare_type_aliases(ns):
    namespace, ctx = ns
    namespace["bind_main_tick"]()
    namespace["if_condition"](first_var_index=0, comparing_type=">=", second_var_index=1)
    namespace["end_condition"]()
    namespace["end_binding"]()
    assert ctx.session.main_tick_steps[0].values["compare_type"] == "GE"
    assert ctx.session.main_tick_steps[0].values["step_count"] == 0


def test_unclosed_if_condition_raises(ns):
    namespace, _ctx = ns
    namespace["bind_main_tick"]()
    namespace["if_condition"](first_var_index=0, comparing_type="==", second_var_index=1)
    with pytest.raises(ValueError, match="Unclosed if_condition"):
        namespace["end_binding"]()


def test_orphan_end_condition_raises(ns):
    namespace, _ctx = ns
    namespace["bind_main_tick"]()
    with pytest.raises(ValueError, match="without matching if_condition"):
        namespace["end_condition"]()


def test_nested_if_blocks_validate(ns):
    namespace, ctx = ns
    namespace["bind_main_tick"]()
    namespace["if_condition"](first_var_index=0, comparing_type=">", second_var_index=1)
    namespace["if_condition"](first_var_index=2, comparing_type="<", second_var_index=3)
    namespace["delay_ms"](10)
    namespace["end_condition"]()
    namespace["delay_ms"](20)
    namespace["end_condition"]()
    namespace["end_binding"]()
    assert len(ctx.session.main_tick_steps) == 4
    assert ctx.session.main_tick_steps[0].values["step_count"] == 3
    assert ctx.session.main_tick_steps[1].values["step_count"] == 1


def test_export_includes_if_condition_step_count(ns, tmp_path):
    namespace, ctx = ns
    namespace["config"]()
    namespace["bind_main_tick"]()
    namespace["var_set"](var_index=1, value=3500)
    namespace["if_condition"](first_var_index=0, comparing_type=">=", second_var_index=1)
    namespace["gpio_write"](port=2, pin=15, value=1)
    namespace["end_condition"]()
    namespace["end_binding"]()

    text = emit_config_hpp(ctx.session.to_dict(), ctx.schema, tmp_path / "out.hpp", write=False)
    assert "MicroOpType::VAR_SET" in text
    assert "MicroOpType::IF_CONDITION" in text
    assert "MicroOpType::END_CONDITION" not in text
    assert "MicroCompareType::GE" in text
    assert ".if_condition = {" in text


def test_round_trip_if_condition_steps(ns, tmp_path):
    namespace, ctx = ns
    namespace["config"]()
    namespace["bind_main_tick"]()
    namespace["var_set"](var_index=1, value=3500)
    namespace["if_condition"](first_var_index=0, comparing_type=">=", second_var_index=1)
    namespace["gpio_write"](port=2, pin=15, value=1)
    namespace["end_condition"]()
    namespace["end_binding"]()

    out_path = tmp_path / "if_config.hpp"
    emit_config_hpp(ctx.session.to_dict(), ctx.schema, out_path, write=True)
    reloaded = load_config_hpp(out_path, ctx.schema)

    assert len(reloaded.main_tick_steps) == 3
    assert reloaded.main_tick_steps[0].union_member == "var_set"
    assert reloaded.main_tick_steps[0].values["value"] == 3500
    assert reloaded.main_tick_steps[1].union_member == "if_condition"
    assert reloaded.main_tick_steps[1].values["compare_type"] == "GE"
    assert reloaded.main_tick_steps[1].values["step_count"] == 1
    assert reloaded.main_tick_steps[2].union_member == "digital_gpio_write"


def test_schema_includes_new_micro_ops():
    schema = get_schema()
    assert "var_set" in schema.micro_ops
    assert "if_condition" in schema.micro_ops
    assert "end_condition" not in schema.micro_ops
    assert "move_to_error_state" in schema.micro_ops
    assert "move_to_emergency_state" in schema.micro_ops
    assert "GE" in schema.enums["MicroCompareType"].values


def test_move_to_error_state_records_step(ns):
    namespace, ctx = ns
    namespace["bind_main_tick"]()
    namespace["move_to_error_state"]()
    namespace["end_binding"]()
    assert len(ctx.session.main_tick_steps) == 1
    assert ctx.session.main_tick_steps[0].union_member == "move_to_error_state"


def test_move_to_error_state_export(ns, tmp_path):
    namespace, ctx = ns
    namespace["config"]()
    namespace["bind_main_tick"]()
    namespace["move_to_error_state"]()
    namespace["end_binding"]()
    text = emit_config_hpp(ctx.session.to_dict(), ctx.schema, tmp_path / "out.hpp", write=False)
    assert "MicroOpType::MOVE_TO_ERROR_STATE" in text


def test_move_to_error_state_round_trip(ns, tmp_path):
    namespace, ctx = ns
    namespace["config"]()
    namespace["bind_main_tick"]()
    namespace["move_to_error_state"]()
    namespace["end_binding"]()
    out_path = tmp_path / "error_state_config.hpp"
    emit_config_hpp(ctx.session.to_dict(), ctx.schema, out_path, write=True)
    reloaded = load_config_hpp(out_path, ctx.schema)
    assert len(reloaded.main_tick_steps) == 1
    assert reloaded.main_tick_steps[0].union_member == "move_to_error_state"


def test_move_to_emergency_state_records_step(ns):
    namespace, ctx = ns
    namespace["bind_main_tick"]()
    namespace["move_to_emergency_state"]()
    namespace["end_binding"]()
    assert len(ctx.session.main_tick_steps) == 1
    assert ctx.session.main_tick_steps[0].union_member == "move_to_emergency_state"


def test_move_to_emergency_state_export(ns, tmp_path):
    namespace, ctx = ns
    namespace["config"]()
    namespace["bind_main_tick"]()
    namespace["move_to_emergency_state"]()
    namespace["end_binding"]()
    text = emit_config_hpp(ctx.session.to_dict(), ctx.schema, tmp_path / "out.hpp", write=False)
    assert "MicroOpType::MOVE_TO_EMERGENCY_STATE" in text


def test_move_to_emergency_state_round_trip(ns, tmp_path):
    namespace, ctx = ns
    namespace["config"]()
    namespace["bind_main_tick"]()
    namespace["move_to_emergency_state"]()
    namespace["end_binding"]()
    out_path = tmp_path / "emergency_state_config.hpp"
    emit_config_hpp(ctx.session.to_dict(), ctx.schema, out_path, write=True)
    reloaded = load_config_hpp(out_path, ctx.schema)
    assert len(reloaded.main_tick_steps) == 1
    assert reloaded.main_tick_steps[0].union_member == "move_to_emergency_state"


def test_validate_condition_blocks_direct():
    from gpc_recorder.dsl.session import MicroOpStepState

    steps = [
        MicroOpStepState(
            op_type="MicroOpType::IF_CONDITION",
            union_member="if_condition",
            values={"step_count": 1},
        ),
        MicroOpStepState(
            op_type="MicroOpType::DIGITAL_GPIO_WRITE",
            union_member="digital_gpio_write",
            values={},
        ),
    ]
    validate_condition_blocks(steps)
