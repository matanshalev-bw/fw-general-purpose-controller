"""Tests for bindMainTick / bindStateTick DSL commands."""

from gpc_recorder.dsl.builtins import RecorderContext, build_namespace


def test_bind_main_tick_records_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindMainTick"]()
    ns["gpio_write"](port=1, pin=0, value=1)
    ns["endMainTick"]()
    assert len(ctx.session.main_tick_steps) == 1
    assert ctx.session.main_tick_steps[0].union_member == "digital_gpio_write"


def test_bind_state_tick_records_steps():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindStateTick"]("CONTROLLER_STATE_OPERATIONAL")
    ns["delay_ms"](100)
    ns["endStateTick"]()
    assert "CONTROLLER_STATE_OPERATIONAL" in ctx.session.state_tick_steps
    assert len(ctx.session.state_tick_steps["CONTROLLER_STATE_OPERATIONAL"]) == 1


def test_tick_recording_mutual_exclusion():
    ctx = RecorderContext()
    ns = build_namespace(ctx)
    ns["bindMainTick"]()
    try:
        ns["bindStateTick"]("CONTROLLER_STATE_INIT")
        assert False, "expected RuntimeError"
    except RuntimeError:
        pass
