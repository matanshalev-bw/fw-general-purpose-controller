# GPC USB bluelink bridge

Host-side C++ CLI that sends bluelink packets over USB CDC serial to the GPC firmware, using the same bluelink SDK stack as LLC `scripts/boot`.

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
  -t 99 \
  -P 010501 \
  -q ack
```

### Options

| Flag | Description |
|------|-------------|
| `-p, --port` | Serial device (default `/dev/ttyACM0`) |
| `-d, --dst` | Destination component id (default `17` = GPC `0x11`) |
| `-s, --src` | Source id (default `2` = HLC) |
| `-t, --payload-type` | `PayloadTypeIds` numeric value, decimal or `0x` hex (required) |
| `-P, --payload` | Payload as hex bytes (optional; zero-filled to SDK payload size if omitted) |
| `-q, --qos` | `none` or `ack` |
| `-r, --retries` | ACK retransmit count (default 5) |
| `--timeout-ms` | ACK wait timeout (default 2000) |
| `--log` | Listen and print incoming bluelink packets (stdout, one line per packet) |

### Log mode

```bash
./gpc_usb_bluelink_x86_64 --log -p /dev/ttyACM0
```

Prints human-readable lines for telemetry, connectivity `LOG`, ACK/NACK, and other packets. Used by the GPC Sequence Recorder **USB Bluelink Log** panel.

### Python example

```python
import subprocess

subprocess.run([
    "tools/gpc_usb_bluelink/gpc_usb_bluelink_x86_64",
    "--port", "/dev/ttyACM0",
    "--payload-type", "99",
    "--payload", "010501",
    "--qos", "ack",
], check=True)
```
