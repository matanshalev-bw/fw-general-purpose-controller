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
    default_raw: Optional[str] = None  # from "= value" in header, if any


def _default_from_comment(comment: str) -> Optional[str]:
    if not comment:
        return None
    m = re.search(r"#\s*Default:\s*(.+?)\s*$", comment)
    return m.group(1).strip() if m else None


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


_TELEMETRY_STRUCT_TO_PAYLOAD_OVERRIDES: Dict[str, str] = {
    "RawSensorTelemetries": "RAW_SENSORS_TELEMETRY",
    "ControllerMetaData": "CONTROLLER_META_DATA_TELEMETRY",
    "CalibrationTelemetryData": "CALIBRATION_TELEMETRY",
    "ControlMetricsTelemetryData": "CONTROL_METRICS",
    "AutotuneTelemetryData": "AUTOTUNE_TELEMETRY",
    "LlcSystemStatusTelemetry": "LLC_SYSTEM_STATUS_TELEMETRY",
    "LlcSystemConfigTelemetry": "LLC_SYSTEM_CONFIG_TELEMETRY",
    "PowerPanelComponentTelemetry": "PPI_PP_TELEMETRY",
}


def _struct_name_to_telemetry_payload_id(
    struct_name: str, payload_type_ids: Dict[str, int]
) -> Optional[str]:
    if struct_name in _TELEMETRY_STRUCT_TO_PAYLOAD_OVERRIDES:
        pid = _TELEMETRY_STRUCT_TO_PAYLOAD_OVERRIDES[struct_name]
        if pid in payload_type_ids:
            return pid
    snake = re.sub(r"(?<!^)(?=[A-Z])", "_", struct_name).upper()
    if snake in payload_type_ids:
        return snake
    for suffix in ("Telemetries", "Telemetry", "Data"):
        if struct_name.endswith(suffix):
            base = struct_name[: -len(suffix)]
            candidate = re.sub(r"(?<!^)(?=[A-Z])", "_", base).upper() + "_TELEMETRY"
            if candidate in payload_type_ids:
                return candidate
    return None


class Schema:
    default_component_id: str = "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"

    def __init__(self) -> None:
        self._converter = CppToROSConverter()
        self.payload_type_ids: Dict[str, int] = {}
        self.command_structs: Dict[str, StructDef] = {}
        self.payload_id_to_struct: Dict[str, str] = {}
        self.telemetry_structs: Dict[str, StructDef] = {}
        self.telemetry_payload_id_to_struct: Dict[str, str] = {}
        self.telemetry_struct_sizes: Dict[str, int] = {}
        self.enums: Dict[str, EnumDef] = {}
        self.micro_ops: Dict[str, MicroOpDef] = {}
        self.component_ids: Dict[str, int] = {}
        self.constants: Dict[str, int] = {}

    def load(
        self,
        payload_types_path: Path,
        commands_path: Path,
        telemetry_path: Path,
        field_defs_path: Path,
        micro_ops_path: Path,
        component_id_path: Path,
    ) -> None:
        self._load_payload_types(payload_types_path.read_text(encoding="utf-8"))
        self._load_enums(field_defs_path.read_text(encoding="utf-8"))
        self._load_commands(commands_path.read_text(encoding="utf-8"))
        self._load_telemetry(telemetry_path.read_text(encoding="utf-8"))
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
                FieldDef(
                    cpp_type=t,
                    name=n,
                    array_size=a,
                    default_raw=_default_from_comment(c),
                )
                for t, n, c, a, _b in fields_raw
            ]
            self.command_structs[name] = StructDef(name=name, fields=fields)
            pid = _struct_name_to_payload_id(name)
            if pid and pid in self.payload_type_ids:
                self.payload_id_to_struct[pid] = name

    def _load_telemetry(self, content: str) -> None:
        from gpc_recorder.dsl.pack import struct_packed_size

        structs = self._converter.extract_structs_from_header(content, "TelemetryPayload")
        for name, body in structs.items():
            if "Union" in name:
                continue
            fields_raw = self._converter.parse_struct_fields(body, name)
            fields = [
                FieldDef(
                    cpp_type=t,
                    name=n,
                    array_size=a,
                    default_raw=_default_from_comment(c),
                )
                for t, n, c, a, _b in fields_raw
            ]
            struct_def = StructDef(name=name, fields=fields)
            try:
                size = struct_packed_size(self, struct_def)
            except ValueError:
                continue
            self.telemetry_structs[name] = struct_def
            self.telemetry_struct_sizes[name] = size
            pid = _struct_name_to_telemetry_payload_id(name, self.payload_type_ids)
            if pid:
                self.telemetry_payload_id_to_struct[pid] = name

    def _load_micro_ops(self, content: str) -> None:
        content = self._converter.remove_c_comments(content)
        for match in re.finditer(
            r"static\s+constexpr\s+\w+\s+(\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*;",
            content,
        ):
            name = match.group(1)
            raw = match.group(2)
            self.constants[name] = int(raw, 16) if raw.startswith("0x") else int(raw)

        op_type_match = re.search(
            r"enum\s+class\s+MicroOpType\s*:\s*uint8_t\s*\{([^}]+)\}",
            content,
            re.DOTALL,
        )
        op_types: Dict[str, int] = {}
        if op_type_match:
            op_types = _parse_enum_body(op_type_match.group(1))

        compare_match = re.search(
            r"enum\s+class\s+MicroCompareType\s*:\s*uint8_t\s*\{([^}]+)\}",
            content,
            re.DOTALL,
        )
        if compare_match:
            self.enums["MicroCompareType"] = EnumDef(
                name="MicroCompareType",
                values=_parse_enum_body(compare_match.group(1)),
            )

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
            "VAR_SET": "var_set",
            "IF_CONDITION": "if_condition",
            "MOVE_TO_ERROR_STATE": "move_to_error_state",
            "MOVE_TO_EMERGENCY_STATE": "move_to_emergency_state",
            "TRIGGER_SAFETY": "trigger_safety",
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
                    "var_set": "MicroVarSet",
                    "if_condition": "MicroIfCondition",
                    "move_to_error_state": "MicroMoveToErrorState",
                    "move_to_emergency_state": "MicroMoveToEmergencyState",
                    "trigger_safety": "MicroTriggerSafety",
                }
                struct_name = alt.get(member, struct_name)
            if struct_name not in structs:
                continue
            fields_raw = self._converter.parse_struct_fields(structs[struct_name], struct_name)
            fields = [
                FieldDef(
                    cpp_type=t,
                    name=n,
                    array_size=a,
                    default_raw=_default_from_comment(c),
                )
                for t, n, c, a, _b in fields_raw
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
