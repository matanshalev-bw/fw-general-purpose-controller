# GPC Sequence Recorder

Local web GUI with a Python-like terminal REPL for authoring GPC micro-op sequences. Generates `configs/ConfigsTypes/g474_gpc_config_memory.hpp` from Bluelink command triggers and micro-op steps.

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

begin_powerup()
gpio_write(port=1, pin=5, value=1)
delay_ms(100)
end_powerup()

begin_binding(
  DRIVE_COMMAND,
  DriveCommand(require_autonomous=False, desired_drive_mode=DRIVE_MODE_BRAKE_NEUTRAL),
)
gpio_write(port=1, pin=5, value=1)
adc_read(adc_instance=2, channel=0, var_index=0, store_raw=1)
dac_write(dac_instance=1, use_var=1, var_index=0, literal_value=0)
delay_ms(500)
can_transmit(can_bus=1, id=0x12, dlc=4, data=[0x12, 0x34, 0x56, 0x78])
end_binding()

export()  # writes configs/ConfigsTypes/g474_gpc_config_memory.hpp
```

## Tests

```bash
cd tools/gpc_sequence_recorder
source .venv/bin/activate
export PYTHONPATH=.
pytest -v
```

## Layout

- `gpc_recorder/schema/` — parses PayloadTypes, commands, enums, micro-ops from firmware headers
- `gpc_recorder/dsl/` — REPL, packing, session builtins
- `gpc_recorder/codegen/` — Jinja2 emitter for config HPP
- `web/` — xterm.js terminal UI

Schema is loaded from the repo root (`3rd_party/bluelink_sdk/...`, `configs/...`); no duplicate schema database.
