"""Thin wrapper around bluelink_to_ros_messages_generator parsing."""

import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from gpc_recorder.paths import REPO_ROOT

_REPO_SCRIPTS = REPO_ROOT / "3rd_party/bluelink_sdk/scripts"
if str(_REPO_SCRIPTS) not in sys.path:
    sys.path.insert(0, str(_REPO_SCRIPTS))

from bluelink_to_ros_messages_generator import CppToROSConverter  # noqa: E402


@dataclass
class FieldDef:
    cpp_type: str
    name: str
    array_size: Optional[str] = None


@dataclass
class StructDef:
    name: str
    fields: List[FieldDef]


@dataclass
class EnumDef:
    name: str
    values: Dict[str, int]  # NAME -> numeric


@dataclass
class MicroOpDef:
    op_type_name: str
    union_member: str
    struct_name: str
    fields: List[FieldDef]


def _parse_enum_body(body: str) -> Dict[str, int]:
    values: Dict[str, int] = {}
    current = -1
    for line in body.splitlines():
        line = line.strip().rstrip(",")
        if not line or line.startswith("//"):
            continue
        if "=" in line:
            name, val = line.split("=", 1)
            name = name.strip()
            val = val.strip()
            if val.startswith("0x"):
                current = int(val, 16)
            else:
                current = int(val)
            values[name] = current
        else:
            name = line.strip()
            if name:
                current += 1
                values[name] = current
    return values


def _struct_name_to_payload_id(struct_name: str) -> Optional[str]:
    if not struct_name.endswith("Command"):
        return None
    inner = struct_name[: -len("Command")]
    snake = re.sub(r"(?<!^)(?=[A-Z])", "_", inner).upper()
    return f"{snake}_COMMAND"


class Schema:
    default_component_id: str = "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"

    def __init__(self) -> None:
        self._converter = CppToROSConverter()
        self.payload_type_ids: Dict[str, int] = {}
        self.command_structs: Dict[str, StructDef] = {}
        self.payload_id_to_struct: Dict[str, str] = {}
        self.enums: Dict[str, EnumDef] = {}
        self.micro_ops: Dict[str, MicroOpDef] = {}
        self.component_ids: Dict[str, int] = {}

    def load(
        self,
        payload_types_path: Path,
        commands_path: Path,
        field_defs_path: Path,
        micro_ops_path: Path,
        component_id_path: Path,
    ) -> None:
        self._load_payload_types(payload_types_path.read_text(encoding="utf-8"))
        self._load_enums(field_defs_path.read_text(encoding="utf-8"))
        self._load_commands(commands_path.read_text(encoding="utf-8"))
        self._load_micro_ops(micro_ops_path.read_text(encoding="utf-8"))
        self._load_component_ids(component_id_path.read_text(encoding="utf-8"))

    def _load_payload_types(self, content: str) -> None:
        content = self._converter.remove_c_comments(content)
        m = re.search(
            r"enum\s+PayloadTypeIds\s*:\s*uint16_t\s*\{([^}]+)\}",
            content,
            re.DOTALL,
        )
        if not m:
            return
        self.payload_type_ids = _parse_enum_body(m.group(1))

    def _load_enums(self, content: str) -> None:
        content = self._converter.remove_c_comments(content)
        raw = self._converter.extract_enums_from_header(content)
        for name, body in raw.items():
            if isinstance(body, tuple):
                body = body[1]
            self.enums[name] = EnumDef(name=name, values=_parse_enum_body(body))

    def _load_commands(self, content: str) -> None:
        structs = self._converter.extract_structs_from_header(content, "CommandsPayload")
        for name, body in structs.items():
            if "Union" in name or name.startswith("Micro"):
                continue
            fields_raw = self._converter.parse_struct_fields(body, name)
            fields = [
                FieldDef(cpp_type=t, name=n, array_size=a)
                for t, n, _c, a, _b in fields_raw
            ]
            self.command_structs[name] = StructDef(name=name, fields=fields)
            pid = _struct_name_to_payload_id(name)
            if pid and pid in self.payload_type_ids:
                self.payload_id_to_struct[pid] = name

    def _load_micro_ops(self, content: str) -> None:
        content = self._converter.remove_c_comments(content)
        op_type_match = re.search(
            r"enum\s+class\s+MicroOpType\s*:\s*uint8_t\s*\{([^}]+)\}",
            content,
            re.DOTALL,
        )
        op_types: Dict[str, int] = {}
        if op_type_match:
            op_types = _parse_enum_body(op_type_match.group(1))

        structs = self._converter.extract_structs_from_header(content, "MicroOpsPayload")
        op_to_member = {
            "NOP": None,
            "DIGITAL_GPIO_WRITE": "digital_gpio_write",
            "DIGITAL_GPIO_READ": "digital_gpio_read",
            "ADC_READ": "adc_read",
            "DAC_WRITE": "dac_write",
            "PWM_SET": "pwm_set",
            "DELAY_MS": "delay_ms",
            "CAN_TRANSMIT": "can_transmit",
            "UART_TRANSMIT": "uart_transmit",
            "SPI_TRANSFER": "spi_transfer",
            "I2C_WRITE": "i2c_write",
        }
        for op_name, member in op_to_member.items():
            if not member:
                continue
            struct_name = "Micro" + "".join(
                p.capitalize() for p in member.split("_")
            )
            if struct_name not in structs:
                alt = {
                    "digital_gpio_write": "MicroDigitalGpioWrite",
                    "digital_gpio_read": "MicroDigitalGpioRead",
                    "adc_read": "MicroAdcRead",
                    "dac_write": "MicroDacWrite",
                    "pwm_set": "MicroPwmSet",
                    "delay_ms": "MicroDelayMs",
                    "can_transmit": "MicroCanTransmit",
                    "uart_transmit": "MicroUartTransmit",
                    "spi_transfer": "MicroSpiTransfer",
                    "i2c_write": "MicroI2cWrite",
                }
                struct_name = alt.get(member, struct_name)
            if struct_name not in structs:
                continue
            fields_raw = self._converter.parse_struct_fields(structs[struct_name], struct_name)
            fields = [
                FieldDef(cpp_type=t, name=n, array_size=a)
                for t, n, _c, a, _b in fields_raw
            ]
            self.micro_ops[member] = MicroOpDef(
                op_type_name=f"MicroOpType::{op_name}",
                union_member=member,
                struct_name=struct_name,
                fields=fields,
            )

    def _load_component_ids(self, content: str) -> None:
        content = self._converter.remove_c_comments(content)
        m = re.search(
            r"enum\s+ComponentId\s*(?::\s*[\w:]+)?\s*\{([^}]+)\}",
            content,
            re.DOTALL,
        )
        if not m:
            return
        self.component_ids = _parse_enum_body(m.group(1))
