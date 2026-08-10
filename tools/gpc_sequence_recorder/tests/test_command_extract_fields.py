"""Tests for command binding field extraction and var_mul/var_add."""

from gpc_recorder.codegen.config_loader import load_config_hpp_text
from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.dsl.pack import pack_match_trigger_data
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.schema.loader import get_schema


def test_bind_command_extract_field_excludes_from_trigger():
    engine = ReplEngine(auto_reload=False)
    lines = [
        "bind_command("
        "trigger=BRAKES_CONTINUOUS_COMMAND,"
        "brake_mode=BRAKE_MODE_ARMED,"
        "desired_brakes_position_in_percentage_var_index=0"
        ")",
        "var_mul(dest_var_index=0, src_var_index=0, numerator=1, denominator=10)",
        "pwm_set(frequency_hz=40000, use_var=1, var_index=0)",
        "end_binding()",
    ]
    for line in lines:
        out, cont = engine.execute(line)
        assert cont, out

    binding = engine.ctx.session.bindings[0]
    assert binding.data_size == 1
    assert binding.data[0] != 0
    assert len(binding.extract_fields) == 1
    assert binding.extract_fields[0]["byte_offset"] == 1
    assert binding.extract_fields[0]["var_index"] == 0
    assert binding.steps[0].union_member == "var_mul"
    assert binding.steps[1].union_member == "pwm_set"


def test_pack_match_trigger_data_partial_match():
    schema = get_schema()
    data, data_size, extract_fields = pack_match_trigger_data(
        schema,
        "BrakesContinuousCommand",
        {"brake_mode": "BRAKE_MODE_ARMED"},
        {"desired_brakes_position_in_percentage": 0},
    )
    assert data_size == 1
    assert len(extract_fields) == 1
    assert extract_fields[0]["byte_offset"] == 1


def test_config_loader_reads_extract_fields():
    engine = ReplEngine(auto_reload=False)
    lines = [
        "bind_command("
        "trigger=BRAKES_CONTINUOUS_COMMAND,"
        "brake_mode=BRAKE_MODE_ARMED,"
        "desired_brakes_position_in_percentage_var_index=0"
        ")",
        "var_mul(dest_var_index=0, src_var_index=0, numerator=1, denominator=10)",
        "end_binding()",
    ]
    for line in lines:
        out, cont = engine.execute(line)
        assert cont, out

    text = emit_config_hpp(engine.ctx.session.to_dict(), engine.ctx.schema, write=False)
    session = load_config_hpp_text(text, engine.ctx.schema, source="generated.hpp")
    armed = next(
        b
        for b in session.bindings
        if b.payload_type == "BRAKES_CONTINUOUS_COMMAND" and b.data_size == 1
    )
    assert len(armed.extract_fields) == 1
    assert armed.extract_fields[0]["var_index"] == 0
    assert any(step.union_member == "var_mul" for step in armed.steps)


def test_bind_command_rejects_enum_extract_field():
    engine = ReplEngine(auto_reload=False)
    out, cont = engine.execute(
        "bind_command("
        "trigger=BRAKES_CONTINUOUS_COMMAND,"
        "brake_mode_var_index=0,"
        "desired_brakes_position_in_percentage_var_index=1"
        ")"
    )
    assert cont, out
    assert "brake_mode" in out
    assert "enum" in out.lower()


def test_emit_command_binding_does_not_generate_invalid_enum():
    engine = ReplEngine(auto_reload=False)
    lines = [
        "bind_command("
        "trigger=BRAKES_CONTINUOUS_COMMAND,"
        "brake_mode=BRAKE_MODE_ARMED,"
        "desired_brakes_position_in_percentage_var_index=1"
        ")",
        "pwm_set(frequency_hz=40000, duty_percent=0, use_var=1, var_index=1)",
        "end_binding()",
    ]
    for line in lines:
        out, cont = engine.execute(line)
        assert cont, out

    text = emit_config_hpp(engine.ctx.session.to_dict(), engine.ctx.schema, write=False)
    assert "bluelink::0" not in text
    assert "BRAKE_MODE_ARMED" in text


def test_schema_includes_var_mul_and_var_add():
    schema = get_schema()
    assert "var_mul" in schema.micro_ops
    assert "var_add" in schema.micro_ops
