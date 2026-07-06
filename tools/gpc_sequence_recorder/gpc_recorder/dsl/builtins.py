"""DSL builtins exposed to the REPL namespace."""

from pathlib import Path
from typing import Any, Dict, List, Optional

from gpc_recorder.codegen.config_loader import load_config_hpp
from gpc_recorder.codegen.emitter import emit_config_bin, emit_config_hpp
from gpc_recorder.dsl.coerce import coerce_int_byte_list
from gpc_recorder.dsl.pack import fill_struct_fields, pack_trigger_data
from gpc_recorder.dsl.session import BindingState, MicroOpStepState, Session, TelemetryBindingState
from gpc_recorder.paths import CONTROLLER_STATE_SEQUENCE_FIELDS, CONTROLLER_STATE_TICK_FIELDS, DEFAULT_EXPORT_BIN_PATH, DEFAULT_EXPORT_PATH
from gpc_recorder.schema.loader import get_schema
from gpc_recorder.paths import MICRO_SEQUENCE_MAX_STEPS
from gpc_recorder.validate import (
    validate_binding_count,
    validate_binding_step_count,
    validate_condition_blocks,
    validate_powerup_step_count,
    validate_tick_step_count,
    validate_telemetry_binding_count,
    validate_var_index,
)


_COMPARE_TYPE_ALIASES = {
    "==": "EQ",
    "!=": "NE",
    ">": "GT",
    ">=": "GE",
    "<": "LT",
    "<=": "LE",
    "EQ": "EQ",
    "NE": "NE",
    "GT": "GT",
    "GE": "GE",
    "LT": "LT",
    "LE": "LE",
}


def _normalize_compare_type(comparing_type: str) -> str:
    key = comparing_type.strip()
    if key not in _COMPARE_TYPE_ALIASES:
        allowed = ", ".join(sorted(set(_COMPARE_TYPE_ALIASES.keys())))
        raise ValueError(f"Unknown comparing_type {comparing_type!r}; use one of: {allowed}")
    return _COMPARE_TYPE_ALIASES[key]


class RecorderContext:
    def __init__(self) -> None:
        self.session = Session()
        self.schema = get_schema()
        self._config_path: Path = DEFAULT_EXPORT_PATH
        self._last_export_path: Optional[str] = None
        self._open_condition_stack: List[int] = []

    def config(self, name: str = "G474_GPC_CONFIG", component: str = "") -> None:
        self.session.config_name = name
        if component:
            self.session.component_id = component.split("::")[-1]

    def _ensure_not_recording(self, action: str) -> None:
        if self.session.is_recording():
            raise RuntimeError(f"Call end_binding() before {action}")

    def bind_command(self, trigger: str, command_struct: Any = None, **kwargs: Any) -> None:
        self._ensure_not_recording("bind_command()")
        payload_type = trigger.split("::")[-1] if isinstance(trigger, str) else str(trigger)
        if payload_type not in self.schema.payload_type_ids:
            raise ValueError(f"Unknown PayloadTypeIds::{payload_type}")

        struct_name = self.schema.payload_id_to_struct.get(payload_type)
        if struct_name is None:
            raise ValueError(f"No command struct mapped for {payload_type}")

        field_values = _extract_struct_fields(command_struct, struct_name, kwargs)
        struct_def = self.schema.command_structs[struct_name]
        field_values = fill_struct_fields(self.schema, struct_def, field_values)
        data, data_size = pack_trigger_data(self.schema, struct_name, field_values)
        self.session.current_binding = BindingState(
            payload_type=payload_type,
            struct_name=struct_name,
            field_values=field_values,
            data=data,
            data_size=data_size,
        )
        print(f"Binding started: {payload_type} ({struct_name}), {data_size} bytes packed")

    def bind_telemetry(self, rate: int, trigger: str, **kwargs: Any) -> None:
        self._ensure_not_recording("bind_telemetry()")
        from gpc_recorder.dsl.pack import _field_size

        payload_type = trigger.split("::")[-1] if isinstance(trigger, str) else str(trigger)
        if payload_type not in self.schema.payload_type_ids:
            raise ValueError(f"Unknown PayloadTypeIds::{payload_type}")

        struct_name = self.schema.telemetry_payload_id_to_struct.get(payload_type)
        if struct_name is None:
            raise ValueError(f"No telemetry struct mapped for {payload_type}")

        struct_def = self.schema.telemetry_structs[struct_name]
        payload_size = self.schema.telemetry_struct_sizes[struct_name]
        if payload_size > 8:
            raise ValueError(f"{struct_name} is {payload_size} bytes, max telemetry size is 8")
        if rate <= 0:
            raise ValueError("rate must be > 0")

        validate_telemetry_binding_count(len(self.session.telemetry_bindings) + 1)

        field_mappings: List[Dict[str, Any]] = []
        offset = 0
        for field in struct_def.fields:
            kw = f"{field.name}_var_index"
            if kw not in kwargs:
                raise ValueError(f"Missing {kw} for {struct_name}")
            var_index = int(kwargs.pop(kw))
            validate_var_index(var_index)
            byte_size = _field_size(self.schema, field)
            field_mappings.append(
                {
                    "field_name": field.name,
                    "byte_offset": offset,
                    "byte_size": byte_size,
                    "var_index": var_index,
                }
            )
            offset += byte_size

        if kwargs:
            raise ValueError(f"Unknown kwargs: {sorted(kwargs.keys())}")

        self.session.telemetry_bindings.append(
            TelemetryBindingState(
                payload_type=payload_type,
                struct_name=struct_name,
                rate_hz=int(rate),
                payload_size=payload_size,
                field_mappings=field_mappings,
            )
        )
        print(
            f"Telemetry binding saved: {payload_type} ({struct_name}), "
            f"{payload_size} bytes at {rate} Hz. "
            f"Total telemetry bindings: {len(self.session.telemetry_bindings)}"
        )

    def clear_telemetry(self) -> None:
        self.session.telemetry_bindings = []
        print("Telemetry bindings cleared.")

    def end_binding(self) -> None:
        if self.session.recording_powerup:
            validate_condition_blocks(self.session.powerup_steps, open_stack=self._open_condition_stack)
            validate_powerup_step_count(len(self.session.powerup_steps))
            self.session.recording_powerup = False
            self._open_condition_stack = []
            print(f"Powerup saved ({len(self.session.powerup_steps)} steps).")
            return
        if self.session.recording_main_tick:
            validate_condition_blocks(self.session.main_tick_steps, open_stack=self._open_condition_stack)
            validate_tick_step_count(len(self.session.main_tick_steps), label="Main tick sequence")
            self.session.recording_main_tick = False
            self._open_condition_stack = []
            print(f"Main tick saved ({len(self.session.main_tick_steps)} steps).")
            return
        if self.session.recording_state is not None:
            state_name = self.session.recording_state
            validate_condition_blocks(self.session.state_steps.get(state_name, []), open_stack=self._open_condition_stack)
            validate_tick_step_count(len(self.session.state_steps.get(state_name, [])), label=f"{state_name} sequence")
            self.session.recording_state = None
            self._open_condition_stack = []
            print(f"State sequence saved for {state_name} ({len(self.session.state_steps[state_name])} steps).")
            return
        if self.session.recording_state_tick is not None:
            state_name = self.session.recording_state_tick
            validate_condition_blocks(
                self.session.state_tick_steps.get(state_name, []), open_stack=self._open_condition_stack
            )
            validate_tick_step_count(
                len(self.session.state_tick_steps.get(state_name, [])), label=f"{state_name} tick"
            )
            self.session.recording_state_tick = None
            self._open_condition_stack = []
            print(f"State tick saved for {state_name} ({len(self.session.state_tick_steps[state_name])} steps).")
            return
        if self.session.current_binding is None:
            raise RuntimeError("No recording in progress")
        b = self.session.current_binding
        validate_condition_blocks(b.steps, open_stack=self._open_condition_stack)
        validate_binding_step_count(len(b.steps))
        validate_binding_count(len(self.session.bindings) + 1)
        self.session.bindings.append(b)
        self.session.current_binding = None
        self._open_condition_stack = []
        print(f"Binding saved ({len(b.steps)} steps). Total bindings: {len(self.session.bindings)}")

    def bind_powerup(self) -> None:
        self._ensure_not_recording("bind_powerup()")
        existing = len(self.session.powerup_steps)
        self.session.recording_powerup = True
        if existing:
            print(f"Powerup recording resumed ({existing} existing steps).")
        else:
            print("Powerup recording started.")

    def bind_main_tick(self) -> None:
        self._ensure_not_recording("bind_main_tick()")
        existing = len(self.session.main_tick_steps)
        self.session.recording_main_tick = True
        if existing:
            print(f"Main tick recording resumed ({existing} existing steps).")
        else:
            print("Main tick recording started.")

    def clear_main_tick(self) -> None:
        self.session.main_tick_steps = []
        self.session.recording_main_tick = False
        print("Main tick sequence cleared.")

    def bind_state(self, state: str) -> None:
        self._ensure_not_recording("bind_state()")
        state_name = state.split("::")[-1] if isinstance(state, str) else str(state)
        if state_name not in CONTROLLER_STATE_SEQUENCE_FIELDS:
            allowed = ", ".join(sorted(CONTROLLER_STATE_SEQUENCE_FIELDS))
            raise ValueError(f"bind_state() supports one-shot states only: {allowed}")
        existing = len(self.session.state_steps.get(state_name, []))
        self.session.state_steps.setdefault(state_name, [])
        self.session.recording_state = state_name
        if existing:
            print(f"State sequence recording resumed for {state_name} ({existing} existing steps).")
        else:
            print(f"State sequence recording started for {state_name} (runs once, then auto-transitions).")

    def clear_state(self, state: str) -> None:
        state_name = state.split("::")[-1] if isinstance(state, str) else str(state)
        if state_name not in CONTROLLER_STATE_SEQUENCE_FIELDS:
            allowed = ", ".join(sorted(CONTROLLER_STATE_SEQUENCE_FIELDS))
            raise ValueError(f"clear_state() supports one-shot states only: {allowed}")
        self.session.state_steps.pop(state_name, None)
        if self.session.recording_state == state_name:
            self.session.recording_state = None
        print(f"State sequence cleared for {state_name}.")

    def bind_state_tick(self, state: str) -> None:
        self._ensure_not_recording("bind_state_tick()")
        state_name = state.split("::")[-1] if isinstance(state, str) else str(state)
        if state_name not in CONTROLLER_STATE_TICK_FIELDS:
            allowed = ", ".join(sorted(CONTROLLER_STATE_TICK_FIELDS))
            raise ValueError(f"bind_state_tick() supports looping tick states only: {allowed}")
        existing = len(self.session.state_tick_steps.get(state_name, []))
        self.session.state_tick_steps.setdefault(state_name, [])
        self.session.recording_state_tick = state_name
        if existing:
            print(f"State tick recording resumed for {state_name} ({existing} existing steps).")
        else:
            print(f"State tick recording started for {state_name}.")

    def clear_state_tick(self, state: str) -> None:
        state_name = state.split("::")[-1] if isinstance(state, str) else str(state)
        if state_name not in CONTROLLER_STATE_TICK_FIELDS:
            allowed = ", ".join(sorted(CONTROLLER_STATE_TICK_FIELDS))
            raise ValueError(f"clear_state_tick() supports looping tick states only: {allowed}")
        self.session.state_tick_steps.pop(state_name, None)
        if self.session.recording_state_tick == state_name:
            self.session.recording_state_tick = None
        print(f"State tick sequence cleared for {state_name}.")

    def clear_powerup(self) -> None:
        self.session.powerup_steps = []
        self.session.recording_powerup = False
        print("Powerup sequence cleared.")

    def clear_command(self) -> None:
        self.session.current_binding = None
        print("Current binding cleared.")

    def undo(self) -> None:
        if self.session.recording_powerup and self.session.powerup_steps:
            self.session.powerup_steps.pop()
            print("Removed last step from powerup.")
            return
        if self.session.recording_main_tick and self.session.main_tick_steps:
            self.session.main_tick_steps.pop()
            print("Removed last step from main tick.")
            return
        if self.session.recording_state is not None:
            steps = self.session.state_steps.get(self.session.recording_state, [])
            if steps:
                steps.pop()
                print(f"Removed last step from {self.session.recording_state} sequence.")
                return
        if self.session.recording_state_tick is not None:
            steps = self.session.state_tick_steps.get(self.session.recording_state_tick, [])
            if steps:
                steps.pop()
                print(f"Removed last step from {self.session.recording_state_tick} tick.")
                return
        target = self.session.current_binding
        if target is not None and target.steps:
            target.steps.pop()
            print("Removed last step from current binding.")
            return
        if self.session.bindings:
            b = self.session.bindings[-1]
            if b.steps:
                b.steps.pop()
                print("Removed last step from last saved binding.")
            else:
                self.session.bindings.pop()
                print("Removed empty binding.")
            return
        print("Nothing to undo.")

    def _current_step_target(self) -> List[MicroOpStepState]:
        if self.session.recording_powerup:
            return self.session.powerup_steps
        if self.session.recording_main_tick:
            return self.session.main_tick_steps
        if self.session.recording_state is not None:
            return self.session.state_steps[self.session.recording_state]
        if self.session.recording_state_tick is not None:
            return self.session.state_tick_steps[self.session.recording_state_tick]
        if self.session.current_binding is not None:
            return self.session.current_binding.steps
        raise RuntimeError(
            "Call bind_powerup(), bind_main_tick(), bind_state(), bind_state_tick(), or bind_command() first"
        )

    def _add_step(self, union_member: str, values: Dict[str, Any]) -> None:
        if self.session.recording_powerup:
            target = self.session.powerup_steps
            label = "powerup"
        elif self.session.recording_main_tick:
            target = self.session.main_tick_steps
            label = "main tick"
        elif self.session.recording_state is not None:
            target = self.session.state_steps[self.session.recording_state]
            label = f"{self.session.recording_state} sequence"
        elif self.session.recording_state_tick is not None:
            target = self.session.state_tick_steps[self.session.recording_state_tick]
            label = f"{self.session.recording_state_tick} tick"
        elif self.session.current_binding is not None:
            target = self.session.current_binding.steps
            label = "binding"
        else:
            raise RuntimeError(
                "Call bind_powerup(), bind_main_tick(), bind_state(), bind_state_tick(), or bind_command() first"
            )
        if union_member not in self.schema.micro_ops:
            raise ValueError(f"Unknown micro-op {union_member!r}")
        op = self.schema.micro_ops[union_member]
        if "var_index" in values:
            validate_var_index(int(values["var_index"]))
        if "first_var_index" in values:
            validate_var_index(int(values["first_var_index"]))
        if "second_var_index" in values:
            validate_var_index(int(values["second_var_index"]))
        if len(target) >= MICRO_SEQUENCE_MAX_STEPS:
            raise ValueError(f"Maximum {MICRO_SEQUENCE_MAX_STEPS} steps per {label}")
        target.append(
            MicroOpStepState(op_type=op.op_type_name, union_member=union_member, values=values)
        )

    def gpio_write(self, port: int, pin: int, value: int) -> None:
        self._add_step("digital_gpio_write", {"port": port, "pin": pin, "value": value})

    def gpio_read(self, port: int, pin: int, var_index: int) -> None:
        self._add_step("digital_gpio_read", {"port": port, "pin": pin, "var_index": var_index})

    def adc_read(
        self,
        adc_instance: int,
        channel: int,
        var_index: int,
        store_raw: int = 1,
    ) -> None:
        self._add_step(
            "adc_read",
            {
                "adc_instance": adc_instance,
                "channel": channel,
                "var_index": var_index,
                "store_raw": store_raw,
            },
        )

    def dac_write(
        self,
        dac_instance: int,
        use_var: int = 0,
        var_index: int = 0,
        literal_value: int = 0,
    ) -> None:
        self._add_step(
            "dac_write",
            {
                "dac_instance": dac_instance,
                "use_var": use_var,
                "var_index": var_index,
                "literal_value": literal_value,
            },
        )

    def delay_ms(self, delay_ms: int) -> None:
        self._add_step("delay_ms", {"delay_ms": delay_ms})

    def can_transmit(
        self,
        can_bus: int,
        id: int,
        dlc: int,
        data: List[int],
    ) -> None:
        self._add_step(
            "can_transmit",
            {"can_bus": can_bus, "id": id, "dlc": dlc, "data": coerce_int_byte_list(data, name="data")},
        )

    def pwm_set(
        self,
        timer_instance: int,
        channel: int,
        use_var: int = 0,
        var_index: int = 0,
        literal_duty: int = 0,
    ) -> None:
        self._add_step(
            "pwm_set",
            {
                "timer_instance": timer_instance,
                "channel": channel,
                "use_var": use_var,
                "var_index": var_index,
                "literal_duty": literal_duty,
            },
        )

    def uart_transmit(self, uart_instance: int, length: int, data: List[int]) -> None:
        self._add_step(
            "uart_transmit",
            {"uart_instance": uart_instance, "length": length, "data": coerce_int_byte_list(data, name="data")},
        )

    def spi_transfer(self, spi_instance: int, tx_len: int, tx_data: List[int]) -> None:
        self._add_step(
            "spi_transfer",
            {"spi_instance": spi_instance, "tx_len": tx_len, "tx_data": coerce_int_byte_list(tx_data, name="tx_data")},
        )

    def i2c_write(
        self,
        i2c_instance: int,
        device_addr: int,
        length: int,
        data: List[int],
    ) -> None:
        self._add_step(
            "i2c_write",
            {
                "i2c_instance": i2c_instance,
                "device_addr": device_addr,
                "length": length,
                "data": coerce_int_byte_list(data, name="data"),
            },
        )

    def var_set(self, var_index: int, value: int) -> None:
        validate_var_index(var_index)
        self._add_step(
            "var_set",
            {
                "var_index": var_index,
                "reserved": [0, 0, 0],
                "value": int(value),
            },
        )

    def if_condition(self, first_var_index: int, comparing_type: str, second_var_index: int) -> None:
        compare_type = _normalize_compare_type(comparing_type)
        validate_var_index(first_var_index)
        validate_var_index(second_var_index)
        self._add_step(
            "if_condition",
            {
                "first_var_index": first_var_index,
                "compare_type": compare_type,
                "second_var_index": second_var_index,
                "step_count": 0,
            },
        )
        self._open_condition_stack.append(len(self._current_step_target()) - 1)

    def end_condition(self) -> None:
        target = self._current_step_target()
        if not self._open_condition_stack:
            raise ValueError("end_condition without matching if_condition")
        if_index = self._open_condition_stack.pop()
        step_count = len(target) - if_index - 1
        if step_count > 255:
            raise ValueError(f"condition block exceeds maximum step count (255), got {step_count}")
        target[if_index].values["step_count"] = step_count

    def move_to_error_state(self) -> None:
        self._add_step("move_to_error_state", {"reserved": [0, 0, 0, 0]})

    def move_to_emergency_state(self) -> None:
        self._add_step("move_to_emergency_state", {"reserved": [0, 0, 0, 0]})

    def powerup_summary(self) -> dict:
        return {
            "step_count": len(self.session.powerup_steps),
            "in_progress": self.session.recording_powerup,
        }

    def bindings_summary(self) -> list:
        """List of {index, payload_type, step_count, in_progress} for UI."""
        out = []
        for i, b in enumerate(self.session.bindings):
            out.append(
                {"index": i, "payload_type": b.payload_type, "step_count": len(b.steps), "in_progress": False}
            )
        if self.session.current_binding:
            cb = self.session.current_binding
            out.append(
                {
                    "index": len(out),
                    "payload_type": cb.payload_type,
                    "step_count": len(cb.steps),
                    "in_progress": True,
                }
            )
        return out

    def show(self) -> str:
        d = self.session.to_dict()
        lines = [f"config={d['config_name']}, component={d['component_id']}"]
        pu = len(self.session.powerup_steps)
        tag = " (recording)" if self.session.recording_powerup else ""
        lines.append(f"  powerup: {pu} steps{tag}")
        mt = len(self.session.main_tick_steps)
        tag = " (recording)" if self.session.recording_main_tick else ""
        lines.append(f"  main_tick: {mt} steps{tag}")
        for state_name, steps in self.session.state_steps.items():
            tag = " (recording)" if self.session.recording_state == state_name else ""
            lines.append(f"  state[{state_name}]: {len(steps)} steps{tag}")
        for state_name, steps in self.session.state_tick_steps.items():
            tag = " (recording)" if self.session.recording_state_tick == state_name else ""
            lines.append(f"  state_tick[{state_name}]: {len(steps)} steps{tag}")
        summary = self.bindings_summary()
        for b in summary:
            tag = " (recording)" if b["in_progress"] else ""
            lines.append(f"  [{b['index']}] {b['payload_type']}: {b['step_count']} steps{tag}")
        for i, tb in enumerate(self.session.telemetry_bindings):
            lines.append(
                f"  telemetry[{i}] {tb.payload_type}: {tb.payload_size} bytes at {tb.rate_hz} Hz"
            )
        if (
            pu == 0
            and mt == 0
            and not self.session.state_steps
            and not self.session.state_tick_steps
            and not summary
            and not self.session.telemetry_bindings
        ):
            return "No powerup, tick sequences, bindings, or telemetry yet."
        return "\n".join(lines)

    def preview(self) -> str:
        if (
            not self.session.bindings
            and not self.session.current_binding
            and not self.session.powerup_steps
            and not self.session.main_tick_steps
            and not self.session.state_steps
            and not self.session.state_tick_steps
            and not self.session.telemetry_bindings
        ):
            return "// No powerup, tick sequences, bindings, or telemetry to preview"
        return emit_config_hpp(self.session.to_dict(), self.schema, write=False)

    def reload(self, path: str = "") -> str:
        self._ensure_not_recording("reload()")
        src = self._config_path if not path else Path(path)
        if not src.is_file():
            raise FileNotFoundError(f"Config file not found: {src}")
        self.session = load_config_hpp(src, self.schema)
        self._config_path = src.resolve()
        summary = self.show()
        print(f"Reloaded from {self._config_path}")
        return summary

    def export(self, path: str = "") -> str:
        out = self._config_path if not path else Path(path)
        if (
            not self.session.bindings
            and not self.session.powerup_steps
            and not self.session.main_tick_steps
            and not self.session.state_steps
            and not self.session.state_tick_steps
            and not self.session.telemetry_bindings
        ):
            raise RuntimeError("No powerup, tick sequences, bindings, or telemetry to export")
        if self.session.is_recording():
            raise RuntimeError("Call end_binding() before export")
        session = self.session.to_dict()
        text = emit_config_hpp(session, self.schema, out, write=True)
        bin_path = DEFAULT_EXPORT_BIN_PATH
        emit_config_bin(session, self.schema, bin_path, write=True)
        self._config_path = out.resolve()
        self._last_export_path = str(out)
        print(f"Updated {out}")
        print(f"Exported to {bin_path}")
        return text

    def help(self, name: str = "") -> str:
        if not name:
            return (
                "Commands: reload(), show(), bind_command(), bind_telemetry(), end_binding(), "
                "preview(), export(), config(), clear_command(), clear_telemetry(), "
                "bind_powerup(), clear_powerup(), "
                "bind_main_tick(), clear_main_tick(), "
                "bind_state(state), clear_state(state), "
                "bind_state_tick(state), clear_state_tick(state), "
                "gpio_write(), gpio_read(), adc_read(), var_set(), if_condition(), end_condition(), "
                "move_to_error_state(), move_to_emergency_state(), dac_write(), delay_ms(), can_transmit(), "
                "pwm_set(), uart_transmit(), spi_transfer(), i2c_write(), "
                "undo(), help()"
            )
        name = name.split("::")[-1]
        if name in self.schema.micro_ops:
            op = self.schema.micro_ops[name]
            lines = [f"{name}({', '.join(f.name for f in op.fields)})"]
            for f in op.fields:
                lines.append(f"  {f.cpp_type} {f.name}")
            return "\n".join(lines)
        if name in self.schema.telemetry_payload_id_to_struct:
            sn = self.schema.telemetry_payload_id_to_struct[name]
            st = self.schema.telemetry_structs[sn]
            size = self.schema.telemetry_struct_sizes[sn]
            lines = [f"{sn} for PayloadTypeIds::{name} ({size} bytes)"]
            for f in st.fields:
                lines.append(f"  {f.cpp_type} {f.name} -> {f.name}_var_index")
            return "\n".join(lines)
        if name in self.schema.payload_id_to_struct:
            sn = self.schema.payload_id_to_struct[name]
            st = self.schema.command_structs[sn]
            lines = [f"{sn} for PayloadTypeIds::{name}"]
            for f in st.fields:
                lines.append(f"  {f.cpp_type} {f.name}")
            return "\n".join(lines)
        if name in self.schema.command_structs:
            st = self.schema.command_structs[name]
            lines = [f"struct {name}"]
            for f in st.fields:
                lines.append(f"  {f.cpp_type} {f.name}")
            return "\n".join(lines)
        return f"Unknown: {name}"


def _extract_struct_fields(
    command_struct: Any,
    struct_name: str,
    kwargs: Dict[str, Any],
) -> Dict[str, Any]:
    if command_struct is not None and hasattr(command_struct, "_fields"):
        return dict(command_struct._fields)
    if command_struct is not None and isinstance(command_struct, dict):
        return command_struct
    if kwargs:
        return kwargs
    if command_struct is not None and hasattr(command_struct, "__dict__"):
        return {k: v for k, v in command_struct.__dict__.items() if not k.startswith("_")}
    raise ValueError(f"Provide {struct_name}(field=...) as kwargs or a struct instance")


class StructInstance:
    """Simple struct placeholder for bind_command(DRIVE_COMMAND, DriveCommand(...))."""

    def __init__(self, struct_name: str, **fields: Any):
        self._struct_name = struct_name
        self._fields = fields

    def __repr__(self) -> str:
        inner = ", ".join(f"{k}={v!r}" for k, v in self._fields.items())
        return f"{self._struct_name}({inner})"


def build_namespace(ctx: RecorderContext) -> Dict[str, Any]:
    schema = ctx.schema
    ns: Dict[str, Any] = {
        "reload": ctx.reload,
        "show": ctx.show,
        "bind_command": ctx.bind_command,
        "bind_telemetry": ctx.bind_telemetry,
        "clear_telemetry": ctx.clear_telemetry,
        "end_binding": ctx.end_binding,
        "preview": ctx.preview,
        "export": ctx.export,
        "config": ctx.config,
        "clear_command": ctx.clear_command,
        "bind_powerup": ctx.bind_powerup,
        "clear_powerup": ctx.clear_powerup,
        "bind_main_tick": ctx.bind_main_tick,
        "clear_main_tick": ctx.clear_main_tick,
        "bind_state": ctx.bind_state,
        "clear_state": ctx.clear_state,
        "bind_state_tick": ctx.bind_state_tick,
        "clear_state_tick": ctx.clear_state_tick,
        "undo": ctx.undo,
        "gpio_write": ctx.gpio_write,
        "gpio_read": ctx.gpio_read,
        "adc_read": ctx.adc_read,
        "dac_write": ctx.dac_write,
        "delay_ms": ctx.delay_ms,
        "can_transmit": ctx.can_transmit,
        "pwm_set": ctx.pwm_set,
        "uart_transmit": ctx.uart_transmit,
        "spi_transfer": ctx.spi_transfer,
        "i2c_write": ctx.i2c_write,
        "var_set": ctx.var_set,
        "if_condition": ctx.if_condition,
        "end_condition": ctx.end_condition,
        "move_to_error_state": ctx.move_to_error_state,
        "move_to_emergency_state": ctx.move_to_emergency_state,
        "help": ctx.help,
        "True": True,
        "False": False,
    }
    for pid in schema.payload_type_ids:
        ns[pid] = pid
    for ename, edef in schema.enums.items():
        for vname, vval in edef.values.items():
            ns[vname] = vname
    for cid in schema.component_ids:
        ns[cid] = cid
    for sname in schema.command_structs:

        def _make_factory(name: str):
            def _factory(**kw: Any) -> StructInstance:
                return StructInstance(name, **kw)

            return _factory

        ns[sname] = _make_factory(sname)
    return ns
