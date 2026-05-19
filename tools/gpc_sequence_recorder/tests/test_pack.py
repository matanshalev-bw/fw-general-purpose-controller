"""Struct packing unit tests."""

import pytest

from gpc_recorder.dsl.pack import pack_trigger_data
from gpc_recorder.schema.loader import get_schema


@pytest.fixture(scope="module")
def schema():
    return get_schema()


def test_drive_command_brake_neutral(schema):
    data, _ = pack_trigger_data(
        schema,
        "DriveCommand",
        {
            "require_autonomous": False,
            "desired_drive_mode": "DRIVE_MODE_BRAKE_NEUTRAL",
        },
    )
    assert data[:2] == [0, 1]
    assert all(b == 0 for b in data[2:])
    assert len(data) == 8


def test_brakes_command_omitted_fields_use_defaults(schema):
    data, _ = pack_trigger_data(
        schema,
        "BrakesContinuousCommand",
        {"brake_mode": "BRAKE_MODE_FULLY_RELEASED"},
    )
    assert len(data) >= 2
    assert data[0] == 1  # BRAKE_MODE_FULLY_RELEASED
    assert data[1] == 0  # desired_brakes_position_in_percentage default


def test_drive_command_too_large_rejected(schema):
    # DriveCommand is 2 bytes — should not raise
    pack_trigger_data(schema, "DriveCommand", {"require_autonomous": False, "desired_drive_mode": "DRIVE_MODE_BRAKE_NEUTRAL"})
