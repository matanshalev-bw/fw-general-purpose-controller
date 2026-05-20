"""Golden-file test: recreate example g474_gpc_config_memory.hpp."""

import re
from pathlib import Path

import pytest

from gpc_recorder.codegen.emitter import emit_config_hpp
from gpc_recorder.paths import REPO_ROOT
from gpc_recorder.schema.loader import get_schema


def _normalize(text: str) -> str:
    lines = []
    for line in text.splitlines():
        line = line.rstrip()
        lines.append(line)
    return "\n".join(lines) + "\n"


def _build_example_session() -> dict:
    return {
        "config_name": "G474_GPC_CONFIG",
        "component_id": "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER",
        "powerup_steps": [
            {
                "op_type": "MicroOpType::DIGITAL_GPIO_WRITE",
                "union_member": "digital_gpio_write",
                "values": {"port": 1, "pin": 1, "value": 1},
            },
            {
                "op_type": "MicroOpType::DIGITAL_GPIO_WRITE",
                "union_member": "digital_gpio_write",
                "values": {"port": 2, "pin": 1, "value": 0},
            },
        ],
        "bindings": [
            {
                "payload_type": "DRIVE_COMMAND",
                "struct_name": "DriveCommand",
                "field_values": {
                    "require_autonomous": False,
                    "desired_drive_mode": "DRIVE_MODE_BRAKE_NEUTRAL",
                },
                "data": [0, 1, 0, 0, 0, 0, 0, 0],
                "steps": [
                    {
                        "op_type": "MicroOpType::DIGITAL_GPIO_WRITE",
                        "union_member": "digital_gpio_write",
                        "values": {"port": 1, "pin": 5, "value": 1},
                    },
                    {
                        "op_type": "MicroOpType::ADC_READ",
                        "union_member": "adc_read",
                        "values": {
                            "adc_instance": 2,
                            "channel": 0,
                            "var_index": 0,
                            "store_raw": 1,
                        },
                    },
                    {
                        "op_type": "MicroOpType::DAC_WRITE",
                        "union_member": "dac_write",
                        "values": {
                            "dac_instance": 1,
                            "use_var": 1,
                            "var_index": 0,
                            "literal_value": 0,
                        },
                    },
                    {
                        "op_type": "MicroOpType::DELAY_MS",
                        "union_member": "delay_ms",
                        "values": {"delay_ms": 500},
                    },
                    {
                        "op_type": "MicroOpType::CAN_TRANSMIT",
                        "union_member": "can_transmit",
                        "values": {
                            "can_bus": 1,
                            "dlc": 4,
                            "id": 0x12,
                            "data": [0x12, 0x34, 0x56, 0x78],
                        },
                    },
                ],
            }
        ],
    }


@pytest.fixture(scope="module")
def schema():
    return get_schema()


def test_golden_matches_example(schema, tmp_path):
    expected_path = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    expected = _normalize(expected_path.read_text(encoding="utf-8"))

    generated = emit_config_hpp(
        _build_example_session(),
        schema,
        tmp_path / "out.hpp",
        write=False,
    )
    generated = _normalize(generated)

    assert generated == expected


def test_schema_loads_drive_command(schema):
    assert "DRIVE_COMMAND" in schema.payload_type_ids
    assert schema.payload_id_to_struct["DRIVE_COMMAND"] == "DriveCommand"
    assert "digital_gpio_write" in schema.micro_ops


def test_schema_loads_component_ids(schema):
    assert "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER" in schema.component_ids
