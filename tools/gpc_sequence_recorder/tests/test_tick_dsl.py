"""Tests for bind_main_tick / bind_state / bind_state_tick DSL commands."""

import pytest

from gpc_recorder.dsl.builtins import RecorderContext, build_namespace


def test_bind_main_tick_records_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bind_main_tick"]()
    ns["gpio_write"](port=1, pin=0, value=1)
    ns["end_binding"]()
    assert len(ctx.session.main_tick_steps) == 1
    assert ctx.session.main_tick_steps[0].union_member == "digital_gpio_write"


def test_bind_state_records_one_shot_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bind_state"]("CONTROLLER_STATE_INIT")
    ns["delay_ms"](100)
    ns["end_binding"]()
    assert "CONTROLLER_STATE_INIT" in ctx.session.state_steps
    assert len(ctx.session.state_steps["CONTROLLER_STATE_INIT"]) == 1


def test_bind_state_tick_records_loop_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bind_state_tick"]("CONTROLLER_STATE_OPERATIONAL")
    ns["delay_ms"](100)
    ns["end_binding"]()
    assert "CONTROLLER_STATE_OPERATIONAL" in ctx.session.state_tick_steps
    assert len(ctx.session.state_tick_steps["CONTROLLER_STATE_OPERATIONAL"]) == 1


def test_bind_state_rejects_tick_only_states():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    with pytest.raises(ValueError, match="one-shot"):
        ns["bind_state"]("CONTROLLER_STATE_OPERATIONAL")


def test_bind_state_tick_rejects_one_shot_states():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    with pytest.raises(ValueError, match="looping tick"):
        ns["bind_state_tick"]("CONTROLLER_STATE_INIT")


def test_bind_state_tick_records_error_and_emergency_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    for state in ("CONTROLLER_STATE_ERROR", "CONTROLLER_STATE_EMERGENCY"):
        ns["bind_state_tick"](state)
        ns["delay_ms"](50)
        ns["end_binding"]()
        assert state in ctx.session.state_tick_steps
        assert len(ctx.session.state_tick_steps[state]) == 1


def test_end_binding_finishes_powerup():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bind_powerup"]()
    ns["delay_ms"](10)
    ns["end_binding"]()
    assert not ctx.session.recording_powerup
    assert len(ctx.session.powerup_steps) == 1


def test_tick_recording_mutual_exclusion():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bind_main_tick"]()
    try:
        ns["bind_state"]("CONTROLLER_STATE_INIT")
        assert False, "expected RuntimeError"
    except RuntimeError:
        pass
