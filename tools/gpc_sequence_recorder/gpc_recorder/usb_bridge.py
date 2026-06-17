"""Host USB bluelink bridge via gpc_usb_bluelink CLI."""

from __future__ import annotations

import asyncio
import glob
import platform
import select
import shutil
import subprocess
import threading
from pathlib import Path
from typing import Any, AsyncIterator, Dict, List, Optional

from gpc_recorder.dsl.pack import fill_struct_fields, pack_struct
from gpc_recorder.paths import REPO_ROOT, TOOL_DIR, _INSTALLED_BIN_DIR, _repo_is_installed_read_only
from gpc_recorder.schema.cpp_parser import StructDef
from gpc_recorder.schema.loader import get_schema

USB_TOOL_DIR = TOOL_DIR.parent / "gpc_usb_bluelink"
DEFAULT_DESTINATION_COMPONENT = "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"
DEFAULT_SOURCE_ID = 0x02  # HLC_ADDRESS on wire

USB_DESTINATION_COMPONENT_IDS: Dict[str, int] = {
    "COMPONENT_ID_REVERSER_DRIVER": 0x0A,
    "COMPONENT_ID_IMPLEMENT_DRIVER": 0x0B,
    "COMPONENT_ID_POWER_PANEL_DRIVER": 0x0C,
    "COMPONENT_ID_STEERING_DRIVER": 0x0D,
    "COMPONENT_ID_REVERSER_AUX": 0x0E,
    "COMPONENT_ID_POWER_PANEL_AUX": 0x0F,
    "COMPONENT_ID_POWER_PANEL_TESTER": 0x10,
    "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER": 0x11,
}

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

CONTROLLER_STATE_COMMAND_VALUES = (
    "CONTROLLER_STATE_DISENGAGEMENT",
    "CONTROLLER_STATE_INIT",
    "CONTROLLER_STATE_ENGAGED",
    "CONTROLLER_STATE_POWER_UP_BIT",
)

CONTROLLER_COMMANDS: List[Dict[str, str]] = [
    {"label": "controller_state", "payload_type": "CONTROLLER_STATE_COMMAND"},
    {"label": "steering", "payload_type": "STEERING_CONTINUOUS_COMMAND"},
    {"label": "throttle", "payload_type": "THROTTLE_CONTINUOUS_COMMAND"},
    {"label": "brakes", "payload_type": "BRAKES_CONTINUOUS_COMMAND"},
    {"label": "reverser", "payload_type": "REVERSER_COMMAND"},
    # POWER_COMMAND is serialized as ControlCommand in the SDK serializer.
    {"label": "power", "payload_type": "POWER_COMMAND", "struct_name": "ControlCommand"},
]


class UsbBridgeError(Exception):
    pass


def destination_component_ids_catalog() -> List[Dict[str, Any]]:
    return [
        {"name": name, "value": value}
        for name, value in USB_DESTINATION_COMPONENT_IDS.items()
    ]


def resolve_destination_component_id(
    *,
    name: Optional[str] = None,
    value: Optional[int] = None,
) -> int:
    if value is not None:
        try:
            numeric = int(value)
        except (TypeError, ValueError) as exc:
            raise UsbBridgeError(f"Invalid destination component id: {value}") from exc
        for candidate in USB_DESTINATION_COMPONENT_IDS.values():
            if candidate == numeric:
                return numeric
        raise UsbBridgeError(f"Unsupported destination component id: 0x{numeric:02x}")

    component_name = name or DEFAULT_DESTINATION_COMPONENT
    if component_name not in USB_DESTINATION_COMPONENT_IDS:
        raise UsbBridgeError(f"Unknown destination component: {component_name}")
    return USB_DESTINATION_COMPONENT_IDS[component_name]


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
        stop_usb_log()
        self.port = None
        self.opened = False

    def status(self) -> Dict[str, Any]:
        return {"opened": self.opened, "port": self.port, "log_running": is_usb_log_running()}


_usb_session = UsbSession()
_usb_log_proc: Optional[subprocess.Popen] = None
_log_io_lock = threading.Lock()


def is_usb_log_running() -> bool:
    global _usb_log_proc
    return _usb_log_proc is not None and _usb_log_proc.poll() is None


def stop_usb_log() -> None:
    global _usb_log_proc
    if _usb_log_proc is None:
        return
    _usb_log_proc.terminate()
    try:
        _usb_log_proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        _usb_log_proc.kill()
        _usb_log_proc.wait(timeout=1)
    _usb_log_proc = None


def send_via_log_process(
    *,
    type_id: int,
    dst: int,
    src: int,
    qos: str,
    payload_hex: str,
    timeout_s: float = 5.0,
) -> str:
    with _log_io_lock:
        proc = _usb_log_proc
        if proc is None or proc.poll() is not None:
            raise UsbBridgeError("USB log is not running")
        if proc.stdin is None or proc.stderr is None:
            raise UsbBridgeError("USB log process has no stdin/stderr")

        qos_val = 1 if qos == "ack" else 0
        cmd_line = f"SEND {type_id} {dst} {src} {qos_val} {payload_hex}\n"
        proc.stdin.write(cmd_line.encode())
        proc.stdin.flush()

        ready, _, _ = select.select([proc.stderr], [], [], timeout_s)
        if not ready:
            raise UsbBridgeError("USB send timed out")
        response = proc.stderr.readline().decode(errors="replace").strip()
        if response == ">>SENT ok":
            return "Sent via active log session"
        if response.startswith(">>SENT err"):
            detail = response[len(">>SENT err") :].strip()
            raise UsbBridgeError(detail or "send failed")
        raise UsbBridgeError(f"Unexpected log process response: {response}")


def get_usb_session() -> UsbSession:
    return _usb_session


def usb_bluelink_binary() -> Path:
    name = (
        "gpc_usb_bluelink_aarch64"
        if platform.machine().lower() in ("aarch64", "arm64")
        else "gpc_usb_bluelink_x86_64"
    )

    # Prefer a freshly built binary in the repo over an older system-wide install.
    search_bases: List[Path] = []
    if not _repo_is_installed_read_only():
        search_bases.append(USB_TOOL_DIR)
    search_bases.append(_INSTALLED_BIN_DIR)
    for base in search_bases:
        path = base / name
        if path.is_file():
            return path

    resolved = shutil.which("gpc-usb-bluelink")
    if resolved:
        return Path(resolved)

    raise UsbBridgeError(
        f"USB bridge binary not found ({name}). "
        f"Installed package: gpc-usb-bluelink. "
        f"Dev build: cd tools/gpc_usb_bluelink && make CC=g++ all"
    )


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


def build_usb_log_command(port: str) -> List[str]:
    port = str(port).strip()
    if not port:
        raise UsbBridgeError("Select a USB port first")
    binary = usb_bluelink_binary()
    cmd = [str(binary), "--log", "-p", port]
    if shutil.which("stdbuf"):
        return ["stdbuf", "-oL", *cmd]
    return cmd


async def usb_log_stream(port: str) -> AsyncIterator[Dict[str, Any]]:
    global _usb_log_proc
    stop_usb_log()
    cmd = build_usb_log_command(port)

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
            cwd=str(REPO_ROOT),
        )
    except OSError as exc:
        raise UsbBridgeError(f"Failed to run USB log: {exc}") from exc

    _usb_log_proc = proc
    try:
        assert proc.stdout is not None
        while True:
            line = await asyncio.to_thread(proc.stdout.readline)
            if not line:
                break
            yield {"type": "output", "text": line.decode(errors="replace")}
        return_code = proc.wait()
        if return_code not in (0, -15, -9):
            yield {"type": "error", "message": f"USB log exited with code {return_code}"}
            return
        yield {"type": "done", "ok": True}
    finally:
        if _usb_log_proc is proc:
            _usb_log_proc = None
        if proc.poll() is None:
            proc.kill()
            proc.wait()


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

def controller_commands_catalog() -> List[Dict[str, Any]]:
    schema = get_schema()
    catalog: List[Dict[str, Any]] = []
    for item in CONTROLLER_COMMANDS:
        payload_type = item["payload_type"]
        type_id = payload_type_id(payload_type)
        struct_name = item.get("struct_name") or schema.payload_id_to_struct.get(payload_type)
        if not struct_name:
            # Skip entries we cannot pack using the parsed CommandsPayload structs
            continue
        if struct_name not in schema.command_structs:
            continue
        struct_def = schema.command_structs[struct_name]

        fields = []
        for field in struct_def.fields:
            default: Any = 0
            enum_values: Optional[List[str]] = None
            enum_type = field.cpp_type.split("::")[-1]
            if enum_type in schema.enums:
                enum_values = sorted(schema.enums[enum_type].values.keys())
                if payload_type == "CONTROLLER_STATE_COMMAND":
                    enum_values = [
                        v for v in CONTROLLER_STATE_COMMAND_VALUES if v in enum_values
                    ]
            if field.default_raw:
                if field.array_size:
                    default = []
                elif field.cpp_type in schema.enums or enum_type in schema.enums:
                    default = field.default_raw.split("::")[-1]
                else:
                    try:
                        default = int(field.default_raw, 0) if field.default_raw.startswith("0x") else int(field.default_raw)
                    except ValueError:
                        default = field.default_raw
            entry: Dict[str, Any] = {
                "name": field.name,
                "type": field.cpp_type,
                "array_size": field.array_size,
                "default": default,
            }
            if enum_values is not None:
                entry["enum_values"] = enum_values
            fields.append(entry)

        catalog.append(
            {
                "label": item.get("label", payload_type.lower()),
                "payload_type": payload_type,
                "payload_type_id": type_id,
                "struct_name": struct_name,
                "fields": fields,
            }
        )
    return catalog


def pack_controller_command_hex(payload_type: str, values: Dict[str, Any]) -> str:
    schema = get_schema()
    struct_name = None
    for item in CONTROLLER_COMMANDS:
        if item["payload_type"] == payload_type:
            struct_name = item.get("struct_name")
            break
    if not struct_name:
        struct_name = schema.payload_id_to_struct.get(payload_type)
    if not struct_name or struct_name not in schema.command_structs:
        raise UsbBridgeError(f"No command struct mapped for payload type {payload_type}")

    struct_def = schema.command_structs[struct_name]
    merged = fill_struct_fields(schema, StructDef(name=struct_def.name, fields=struct_def.fields), values)
    raw = pack_struct(schema, struct_def, merged)
    return "".join(f"{b:02x}" for b in raw)


def _run_usb_send(
    *,
    use_port: str,
    type_id: int,
    payload_type_name: str,
    payload_hex: str,
    dest_id: int,
    qos: str,
    retries: int,
    timeout_ms: int,
) -> Dict[str, Any]:
    if is_usb_log_running():
        output = send_via_log_process(
            type_id=type_id,
            dst=dest_id,
            src=DEFAULT_SOURCE_ID,
            qos=qos,
            payload_hex=payload_hex,
            timeout_s=max(5.0, timeout_ms / 1000 + 3),
        )
        return {
            "ok": True,
            "port": use_port,
            "destination_component_id": dest_id,
            "payload_type": payload_type_name,
            "payload_type_id": type_id,
            "payload_hex": payload_hex,
            "output": output,
        }

    binary = usb_bluelink_binary()
    cmd = [
        str(binary),
        "-p",
        use_port,
        "-d",
        str(dest_id),
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
        "destination_component_id": dest_id,
        "payload_type": payload_type_name,
        "payload_type_id": type_id,
        "payload_hex": payload_hex,
        "output": stdout,
    }


def send_micro_command(
    union_member: str,
    values: Dict[str, Any],
    *,
    port: Optional[str] = None,
    destination_component_id: Optional[int] = None,
    destination_component: Optional[str] = None,
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
        raise UsbBridgeError(f"No bluelink payload type for micro-op {union_member}")

    type_id = payload_type_id(payload_type_name)
    payload_hex = pack_micro_op_hex(union_member, values)
    dest_id = resolve_destination_component_id(
        name=destination_component,
        value=destination_component_id,
    )
    return _run_usb_send(
        use_port=use_port,
        type_id=type_id,
        payload_type_name=payload_type_name,
        payload_hex=payload_hex,
        dest_id=dest_id,
        qos=qos,
        retries=retries,
        timeout_ms=timeout_ms,
    )


def send_controller_command(
    payload_type: str,
    values: Dict[str, Any],
    *,
    port: Optional[str] = None,
    destination_component_id: Optional[int] = None,
    destination_component: Optional[str] = None,
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

    type_id = payload_type_id(payload_type)
    payload_hex = pack_controller_command_hex(payload_type, values)
    dest_id = resolve_destination_component_id(
        name=destination_component,
        value=destination_component_id,
    )
    return _run_usb_send(
        use_port=use_port,
        type_id=type_id,
        payload_type_name=payload_type,
        payload_hex=payload_hex,
        dest_id=dest_id,
        qos=qos,
        retries=retries,
        timeout_ms=timeout_ms,
    )
