"""Tests for bindMainTick / bindState / bindStateTick DSL commands."""

import pytest

from gpc_recorder.dsl.builtins import RecorderContext, build_namespace


def test_bind_main_tick_records_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindMainTick"]()
    ns["gpio_write"](port=1, pin=0, value=1)
    ns["endMainTick"]()
    assert len(ctx.session.main_tick_steps) == 1
    assert ctx.session.main_tick_steps[0].union_member == "digital_gpio_write"


def test_bind_state_records_one_shot_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindState"]("CONTROLLER_STATE_INIT")
    ns["delay_ms"](100)
    ns["endState"]()
    assert "CONTROLLER_STATE_INIT" in ctx.session.state_steps
    assert len(ctx.session.state_steps["CONTROLLER_STATE_INIT"]) == 1


def test_bind_state_tick_records_loop_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindStateTick"]("CONTROLLER_STATE_OPERATIONAL")
    ns["delay_ms"](100)
    ns["endStateTick"]()
    assert "CONTROLLER_STATE_OPERATIONAL" in ctx.session.state_tick_steps
    assert len(ctx.session.state_tick_steps["CONTROLLER_STATE_OPERATIONAL"]) == 1


def test_bind_state_rejects_tick_only_states():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    with pytest.raises(ValueError, match="one-shot"):
        ns["bindState"]("CONTROLLER_STATE_OPERATIONAL")


def test_bind_state_tick_rejects_one_shot_states():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    with pytest.raises(ValueError, match="looping tick"):
        ns["bindStateTick"]("CONTROLLER_STATE_INIT")


def test_tick_recording_mutual_exclusion():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindMainTick"]()
    try:
        ns["bindState"]("CONTROLLER_STATE_INIT")
        assert False, "expected RuntimeError"
    except RuntimeError:
        pass
