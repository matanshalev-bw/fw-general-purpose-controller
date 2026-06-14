# GPC Sequence Recorder

Local web GUI with a Python-like terminal REPL for authoring GPC micro-op sequences. Generates `configs/ConfigsTypes/g474_gpc_config_memory.hpp` (designated initializers) and `config_g474.bin` (packed flash image) for programmer-ready config flash.

## Quick start

```bash
cd tools/gpc_sequence_recorder
./run.sh
```

If venv creation fails, install the venv package and retry:

```bash
sudo apt install python3-venv python3-pip
rm -rf .venv
./run.sh
```

`run.sh` falls back to system Python when the virtualenv cannot be created.

Opens http://127.0.0.1:8765/ in your browser (terminal + live HPP preview).

Stdin REPL (no web UI):

```bash
cd tools/gpc_sequence_recorder
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
export PYTHONPATH=.
python -m gpc_recorder --repl
```

## Example session

```python
config(name="G474_GPC_CONFIG", component=COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER)

bind_powerup()
gpio_write(port=1, pin=5, value=1)
delay_ms(100)
end_binding()

bind_command(
  DRIVE_COMMAND,
  DriveCommand(require_autonomous=False, desired_drive_mode=DRIVE_MODE_BRAKE_NEUTRAL),
)
gpio_write(port=1, pin=5, value=1)
adc_read(adc_instance=2, channel=0, var_index=0, store_raw=1)
dac_write(dac_instance=1, use_var=1, var_index=0, literal_value=0)
delay_ms(500)
can_transmit(can_bus=1, id=0x12, dlc=4, data=[0x12, 0x34, 0x56, 0x78])
end_binding()

export()  # writes .hpp + .bin (CMake arm-none-eabi build of config_g474; CubeIDE fallback)
```

## Config bin build (CMake)

`export()` writes `g474_gpc_config_memory.hpp`, then builds `config_g474.bin` with CMake using the `config-g474.Debug` preset (same layout as fw-llc CI builds). Requires `cmake`, `make`, and `gcc-arm-none-eabi`:

```bash
sudo apt install cmake gcc-arm-none-eabi
./build_all.sh config-g474 debug
```

Output: `out/build/config-g474/Debug/config_projects/config_g474/config_g474.bin` (also copied to `config_projects/config_g474/Debug/config_g474.bin` on export).

CMake auto-detects STM32CubeIDE's bundled GNU Tools 13.x under `/opt/st/stm32cubeide_*` when apt's gcc-arm-none-eabi is too old for designated initializers. Optional: copy `cmake/toolchain-paths.cmake.in` to `cmake/toolchain-paths.cmake` for a custom toolchain path.

### STM32CubeIDE fallback

If CMake/arm-none-eabi is unavailable, export falls back to STM32CubeIDE headless build. Set `STM32CUBEIDE` to your install directory if it is not under `/opt/st/stm32cubeide_*`.

## Debian package

Build a local `.deb` that installs the recorder, USB bridge, programmer, and firmware tree:

```bash
# Requires fw-llc at ../fw-llc (for serial/unix sources vendored into the package)
./packaging/build_deb.sh
sudo apt install ./packaging/out/gpc-recorder_*_amd64.deb
```

Installed commands: `gpc-recorder`, `gpc-recorder-repl`, `gpc-recorder-build-config`, `prog-gpc-g4`, `gpc-usb-bluelink`.

Tree lives under `/opt/gpc-recorder/repo`.

## Tests

```bash
cd tools/gpc_sequence_recorder
source .venv/bin/activate
export PYTHONPATH=.
pytest -v
```

Bin export tests require gcc-arm-none-eabi and are skipped when it is not installed.

## Layout

- `gpc_recorder/schema/` — parses PayloadTypes, commands, enums, micro-ops from firmware headers
- `gpc_recorder/dsl/` — REPL, packing, session builtins
- `gpc_recorder/codegen/` — Jinja2 emitter for config HPP
- `web/` — xterm.js terminal UI

Schema is loaded from the repo root (`3rd_party/bluelink_sdk/...`, `configs/...`); no duplicate schema database.
