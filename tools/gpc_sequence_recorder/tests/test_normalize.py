from gpc_recorder.dsl.normalize import normalize_line


def test_bind_command_inserts_command_struct_keyword():
    src = (
        "bind_command(trigger=BRAKES_CONTINUOUS_COMMAND,"
        "BrakesContinuousCommand(brake_mode=BRAKE_MODE_ARMED))"
    )
    out = normalize_line(src)
    assert "command_struct=BrakesContinuousCommand" in out
