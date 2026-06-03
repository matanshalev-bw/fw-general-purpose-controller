# GPC Programmer

Host-side programmer for the General Purpose Controller (STM32G474) over CAN or USB Bluelink.

## Transports

**CAN (default)** — positional arguments:

`[CAN_INTERFACE] [CONTROLLER_TYPE] [FLASH_TARGET] [BIN_FILE]`

**USB** — explicit transport flag:

`--transport usb [--port PATH] [CONTROLLER_TYPE] [FLASH_TARGET] [BIN_FILE]`

Defaults:
- CAN interface = `can1`
- USB port = `/dev/ttyACM0`
- Target controller type = `gpc` (`COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER`)
- Flash target area = `app`
- Binary file = `../../application_projects/application_g474/Debug/application_g474.bin`

Supported controller types:
- `gpc`

Flash target areas:
- `app` — Application area (`0x08014800`, 366 KB)
- `config` — Configuration area (`0x08070000`, 64 KB)
- `customized_config` — Select config bin from controller metadata (`config_type`)

Before flashing, the tool verifies the on-device bootloader version (major.minor) matches `BOOTLOADER_VERSION_MAJOR` and `BOOTLOADER_VERSION_MINOR` in `versions.hpp`.

Exit codes:
- `0` — Success
- `-1` — Transport initialization or argument error
- `-2` — No programming ACK from controller
- `-3` — Flashing or invalid bin file error
- `-4` — Failed to read config type from controller
- `-5` — No config bin for the reported config type
- `-6` — Bootloader version mismatch

## CAN setup

```bash
sudo bash can_setup.sh
ip -details link show can1
```

## Usage examples

```bash
./g474 --help
./g474
./g474 can1
./g474 can1 gpc
./g474 can1 gpc config /path/to/fw-config-g4.bin
./g474 can1 gpc app /path/to/application_g474.bin
./g474 can1 gpc customized_config
./g474 --transport usb --port /dev/ttyACM0 gpc app /path/to/application_g474.bin
./g474 --transport usb gpc app
```

## Build

```bash
cd g474
make
```

Or with CMake from this directory:

```bash
cmake -B build && cmake --build build
```
