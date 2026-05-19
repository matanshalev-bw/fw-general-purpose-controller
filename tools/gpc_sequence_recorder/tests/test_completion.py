"""Tab-completion tests."""

from gpc_recorder.dsl.builtins import build_namespace, RecorderContext
from gpc_recorder.dsl.completion import complete_line, longest_common_prefix
from gpc_recorder.schema.loader import get_schema


def test_complete_component_id_in_config():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    line = "config(component=COMPONENT_ID_GENERAL_PU"
    matches = complete_line(schema, keys, line)
    assert "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER" in matches


def test_complete_config_keyword():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    matches = complete_line(schema, keys, "config(comp")
    assert "component=" in matches


def test_begin_binding_trigger_keyword():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    assert "trigger=" in complete_line(schema, keys, "begin_binding(trig")


def test_begin_binding_trigger_value():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    matches = complete_line(schema, keys, "begin_binding(trigger=REVERSER_")
    assert "REVERSER_COMMAND" in matches


def test_begin_binding_open_paren():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    matches = complete_line(schema, keys, "begin_binding(")
    assert matches == ["trigger="]


def test_begin_binding_after_trigger_equals():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    matches = complete_line(schema, keys, "begin_binding(trigger=")
    assert "DRIVE_COMMAND" in matches
    assert "BRAKES_CONTINUOUS_COMMAND" in matches


def test_brakes_command_struct_fields():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    matches = complete_line(
        schema,
        keys,
        "begin_binding(trigger=BRAKES_CONTINUOUS_COMMAND, BrakesContinuousCommand(",
    )
    assert "brake_mode=" in matches
    assert "desired_brakes_position_in_percentage=" in matches
    assert "ACK_PACKET_RECEIVED" not in matches
    assert "DRIVE_COMMAND" not in matches


def test_begin_binding_command_struct():
    schema = get_schema()
    keys = sorted(build_namespace(RecorderContext()).keys())
    assert "BrakesContinuousCommand" in schema.command_structs
    matches = complete_line(
        schema,
        keys,
        "begin_binding(trigger=BRAKES_CONTINUOUS_COMMAND, Brakes",
    )
    assert matches == ["BrakesContinuousCommand"]


def test_common_prefix():
    matches = [
        "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER",
        "COMPONENT_ID_GENERAL_PURPOSE_TESTER",
    ]
    assert longest_common_prefix(matches) == "COMPONENT_ID_GENERAL_PURPOSE_"
