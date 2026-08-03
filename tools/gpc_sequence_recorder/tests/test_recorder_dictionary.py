"""Recorder dictionary schema enrichment tests."""

from gpc_recorder.schema.recorder_dictionary import recorder_commands_dictionary


def test_config_component_param_has_component_id_enum():
    data = recorder_commands_dictionary()
    config_cmd = next(cmd for cmd in data["recorder_commands"] if cmd["name"] == "config")
    component_param = next(p for p in config_cmd["params"] if p["name"] == "component")
    enum_values = component_param.get("enum_values") or []
    names = [item["name"] for item in enum_values]
    assert "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER" in names
    assert "COMPONENT_ID_LLC" not in names
    assert "COMPONENT_ID_BOOTLOADER" not in names
    assert "COMPONENT_ID_HLC" not in names
    assert "COMPONENT_ID_BROADCAST" not in names
    assert enum_values == data["component_ids"]
