from gpc_recorder.dsl.normalize import normalize_line


def test_begin_binding_inserts_command_struct_keyword():
    src = (
        "begin_binding(trigger=BRAKES_CONTINUOUS_COMMAND,"
        "BrakesContinuousCommand(brake_mode=BRAKE_MODE_ARMED))"
    )
    out = normalize_line(src)
    assert "command_struct=BrakesContinuousCommand" in out
