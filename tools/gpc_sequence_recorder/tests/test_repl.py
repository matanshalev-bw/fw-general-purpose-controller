"""REPL integration test building example binding."""

from gpc_recorder.dsl.repl import ReplEngine


def test_repl_builds_example_binding():
    engine = ReplEngine()
    lines = [
        'bind_command(DRIVE_COMMAND, DriveCommand(require_autonomous=False, desired_drive_mode=DRIVE_MODE_BRAKE_NEUTRAL))',
        "gpio_write(port=1, pin=5, value=1)",
        "adc_read(adc_instance=2, channel=0, var_index=0, store_raw=1)",
        "dac_write(dac_instance=1, use_var=1, var_index=0, literal_value=0)",
        "delay_ms(500)",
        "can_transmit(can_bus=1, id=0x12, dlc=4, data=[0x12, 0x34, 0x56, 0x78])",
        "end_binding()",
    ]
    for line in lines:
        out, cont = engine.execute(line)
        assert cont, out

    assert len(engine.ctx.session.bindings) == 1
    assert engine.ctx.session.bindings[0].payload_type == "DRIVE_COMMAND"
    assert len(engine.ctx.session.bindings[0].steps) == 5

    hpp = engine.preview_hpp()
    assert "DRIVE_COMMAND" in hpp
    assert "digital_gpio_write" in hpp
    assert "can_transmit" in hpp
    assert ".delay_ms = {500}" in hpp


def test_bind_command_partial_struct_fields():
    engine = ReplEngine()
    line = (
        "bind_command(trigger=BRAKES_CONTINUOUS_COMMAND,"
        "BrakesContinuousCommand(brake_mode=BRAKE_MODE_FULLY_RELEASED))"
    )
    out, cont = engine.execute(line)
    assert cont, out
    assert engine.ctx.session.current_binding.field_values["brake_mode"] == "BRAKE_MODE_FULLY_RELEASED"
    assert engine.ctx.session.current_binding.field_values["desired_brakes_position_in_percentage"] == 0


def test_bind_command_trigger_then_struct_syntax():
    engine = ReplEngine()
    line = (
        "bind_command(trigger=BRAKES_CONTINUOUS_COMMAND,"
        "BrakesContinuousCommand(brake_mode=BRAKE_MODE_ARMED, desired_brakes_position_in_percentage=20))"
    )
    out, cont = engine.execute(line)
    assert cont, out
    assert engine.ctx.session.current_binding is not None
    assert engine.ctx.session.current_binding.payload_type == "BRAKES_CONTINUOUS_COMMAND"
