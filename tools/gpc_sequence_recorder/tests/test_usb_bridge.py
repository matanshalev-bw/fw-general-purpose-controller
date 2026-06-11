"""USB bridge destination ComponentId resolution."""

import pytest

from gpc_recorder.usb_bridge import (
    UsbBridgeError,
    destination_component_ids_catalog,
    resolve_destination_component_id,
)


def test_destination_component_ids_catalog():
    names = [item["name"] for item in destination_component_ids_catalog()]
    assert names[0] == "COMPONENT_ID_REVERSER_DRIVER"
    assert names[-1] == "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"


def test_resolve_destination_component_defaults_to_gpc():
    assert resolve_destination_component_id() == 0x11


def test_resolve_destination_component_by_name():
    assert (
        resolve_destination_component_id(name="COMPONENT_ID_STEERING_DRIVER")
        == 0x0D
    )


def test_resolve_destination_component_by_value():
    assert resolve_destination_component_id(value=0x0C) == 0x0C


def test_resolve_destination_component_rejects_unknown_name():
    with pytest.raises(UsbBridgeError, match="Unknown destination component"):
        resolve_destination_component_id(name="COMPONENT_ID_BOOTLOADER")


def test_resolve_destination_component_rejects_unknown_value():
    with pytest.raises(UsbBridgeError, match="Unsupported destination component id"):
        resolve_destination_component_id(value=0x07)
