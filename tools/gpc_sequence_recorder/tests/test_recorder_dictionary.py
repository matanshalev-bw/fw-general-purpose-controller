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


def test_var_byte_assign_params_have_field_hints():
    data = recorder_commands_dictionary()
    cmd = next(c for c in data["recorder_commands"] if c["name"] == "var_byte_assign")
    hints = {p["name"]: p.get("hint") for p in cmd["params"]}
    assert "dest" in hints["dest_var_index"].lower()
    assert "0..7" in hints["byte_index"]
    assert "low byte" in hints["src_var_index"].lower()
