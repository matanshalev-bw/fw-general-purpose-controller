# GPC USB BlueLink bridge

Host-side C++ CLI that sends arbitrary BlueLink packets over USB CDC serial to the GPC firmware (same wire format as LLC `scripts/boot`).

Intended as a subprocess backend for the Python sequence-recorder GUI.

## Build

Requires `fw-llc` checked out next to this repo (`../fw-llc`) for the shared `serial` library.

```bash
cd tools/gpc_usb_bluelink
make CC=g++ all
# -> gpc_usb_bluelink_x86_64
```

## Usage

```bash
./gpc_usb_bluelink_x86_64 -p /dev/ttyACM0 \
  -t MICRO_DIGITAL_GPIO_WRITE_COMMAND \
  -P 010501 \
  -q ack
```

### Options

| Flag | Description |
|------|-------------|
| `-p, --port` | Serial device (default `/dev/ttyACM0`) |
| `-d, --dst` | Destination component id (default `17` = GPC `0x11`) |
| `-s, --src` | Source id (default `2` = HLC) |
| `-t, --payload-type` | `PayloadTypeIds` numeric value or name (required) |
| `-P, --payload` | Payload as hex bytes (optional; size from SDK if omitted) |
| `-q, --qos` | `none` or `ack` |
| `-r, --retries` | ACK retransmit count (default 5) |
| `--timeout-ms` | ACK wait timeout (default 2000) |

### Python example

```python
import subprocess

subprocess.run([
    "tools/gpc_usb_bluelink/gpc_usb_bluelink_x86_64",
    "--port", "/dev/ttyACM0",
    "--payload-type", "MICRO_DIGITAL_GPIO_WRITE_COMMAND",
    "--payload", "010501",
    "--qos", "ack",
], check=True)
```
