"""Host USB BlueLink bridge via gpc_usb_bluelink CLI."""

from __future__ import annotations

import glob
import platform
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional

from gpc_recorder.dsl.pack import fill_struct_fields, pack_struct
from gpc_recorder.paths import REPO_ROOT, TOOL_DIR
from gpc_recorder.schema.cpp_parser import StructDef
from gpc_recorder.schema.loader import get_schema

USB_TOOL_DIR = TOOL_DIR.parent / "gpc_usb_bluelink"
GPC_COMPONENT_ID = 0x11
DEFAULT_SOURCE_ID = 0x02  # HLC_ADDRESS on wire

MICRO_OP_TO_PAYLOAD_TYPE: Dict[str, str] = {
    "digital_gpio_write": "MICRO_DIGITAL_GPIO_WRITE_COMMAND",
    "digital_gpio_read": "MICRO_DIGITAL_GPIO_READ_COMMAND",
    "adc_read": "MICRO_ADC_READ_COMMAND",
    "dac_write": "MICRO_DAC_WRITE_COMMAND",
    "pwm_set": "MICRO_PWM_SET_COMMAND",
    "delay_ms": "MICRO_DELAY_MS_COMMAND",
    "can_transmit": "MICRO_CAN_TRANSMIT_COMMAND",
    "uart_transmit": "MICRO_UART_TRANSMIT_COMMAND",
    "spi_transfer": "MICRO_SPI_TRANSFER_COMMAND",
    "i2c_write": "MICRO_I2C_WRITE_COMMAND",
}


def payload_type_id(payload_type_name: str) -> int:
    schema = get_schema()
    if payload_type_name not in schema.payload_type_ids:
        raise UsbBridgeError(f"Payload type {payload_type_name} not in schema")
    return schema.payload_type_ids[payload_type_name]


class UsbBridgeError(Exception):
    pass


class UsbSession:
    def __init__(self) -> None:
        self.port: Optional[str] = None
        self.opened: bool = False

    def open(self, port: str) -> None:
        if port not in list_serial_ports():
            raise UsbBridgeError(f"Port not found: {port}")
        self.port = port
        self.opened = True

    def close(self) -> None:
        self.port = None
        self.opened = False

    def status(self) -> Dict[str, Any]:
        return {"opened": self.opened, "port": self.port}


_usb_session = UsbSession()


def get_usb_session() -> UsbSession:
    return _usb_session


def usb_bluelink_binary() -> Path:
    name = (
        "gpc_usb_bluelink_aarch64"
        if platform.machine().lower() in ("aarch64", "arm64")
        else "gpc_usb_bluelink_x86_64"
    )
    path = USB_TOOL_DIR / name
    if not path.is_file():
        raise UsbBridgeError(
            f"USB bridge binary not found at {path}. "
            f"Build with: cd {USB_TOOL_DIR} && make CC=g++ all"
        )
    return path


def list_serial_ports() -> List[str]:
    patterns = [
        "/dev/ttyACM*",
        "/dev/ttyUSB*",
        "/dev/serial/by-id/*",
    ]
    ports: List[str] = []
    for pattern in patterns:
        ports.extend(glob.glob(pattern))
    return sorted(set(ports))


def micro_ops_catalog() -> List[Dict[str, Any]]:
    schema = get_schema()
    catalog: List[Dict[str, Any]] = []
    for member, op in sorted(schema.micro_ops.items()):
        payload_type = MICRO_OP_TO_PAYLOAD_TYPE.get(member)
        if payload_type is None:
            continue
        fields = []
        for field in op.fields:
            default: Any = 0
            if field.default_raw:
                if field.array_size:
                    default = []
                elif field.cpp_type in schema.enums:
                    default = field.default_raw.split("::")[-1]
                else:
                    try:
                        default = int(field.default_raw, 0) if field.default_raw.startswith("0x") else int(field.default_raw)
                    except ValueError:
                        default = field.default_raw
            fields.append(
                {
                    "name": field.name,
                    "type": field.cpp_type,
                    "array_size": field.array_size,
                    "default": default,
                }
            )
        catalog.append(
            {
                "union_member": member,
                "payload_type": payload_type,
                "payload_type_id": payload_type_id(payload_type),
                "op_type": op.op_type_name,
                "fields": fields,
            }
        )
    return catalog


def pack_micro_op_hex(union_member: str, values: Dict[str, Any]) -> str:
    schema = get_schema()
    if union_member not in schema.micro_ops:
        raise UsbBridgeError(f"Unknown micro-op: {union_member}")
    op = schema.micro_ops[union_member]
    struct_def = StructDef(name=op.struct_name, fields=op.fields)
    merged = fill_struct_fields(schema, struct_def, values)
    raw = pack_struct(schema, struct_def, merged)
    return "".join(f"{b:02x}" for b in raw)


def send_micro_command(
    union_member: str,
    values: Dict[str, Any],
    *,
    port: Optional[str] = None,
    qos: str = "none",
    retries: int = 5,
    timeout_ms: int = 2000,
) -> Dict[str, Any]:
    session = get_usb_session()
    use_port = port or session.port
    if not use_port:
        raise UsbBridgeError("USB port not open — select a port and click Open")
    if port is None and not session.opened:
        raise UsbBridgeError("USB port not open — click Open first")

    payload_type_name = MICRO_OP_TO_PAYLOAD_TYPE.get(union_member)
    if payload_type_name is None:
        raise UsbBridgeError(f"No BlueLink payload type for micro-op {union_member}")

    type_id = payload_type_id(payload_type_name)
    payload_hex = pack_micro_op_hex(union_member, values)
    binary = usb_bluelink_binary()

    cmd = [
        str(binary),
        "-p",
        use_port,
        "-d",
        str(GPC_COMPONENT_ID),
        "-s",
        str(DEFAULT_SOURCE_ID),
        "-t",
        str(type_id),
        "-P",
        payload_hex,
        "-q",
        qos,
    ]
    if qos == "ack":
        cmd.extend(["-r", str(retries), "--timeout-ms", str(timeout_ms)])

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=max(5, timeout_ms / 1000 + 3),
            cwd=str(REPO_ROOT),
        )
    except subprocess.TimeoutExpired as exc:
        raise UsbBridgeError("USB send timed out") from exc
    except OSError as exc:
        raise UsbBridgeError(f"Failed to run USB bridge: {exc}") from exc

    stdout = (result.stdout or "").strip()
    stderr = (result.stderr or "").strip()
    if result.returncode != 0:
        detail = stderr or stdout or f"exit code {result.returncode}"
        raise UsbBridgeError(detail)

    return {
        "ok": True,
        "port": use_port,
        "payload_type": payload_type_name,
        "payload_type_id": type_id,
        "payload_hex": payload_hex,
        "output": stdout,
    }
