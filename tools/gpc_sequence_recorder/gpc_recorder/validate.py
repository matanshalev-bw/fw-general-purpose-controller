from gpc_recorder.paths import (
    MICRO_SEQUENCE_MAX_BINDINGS,
    MICRO_SEQUENCE_MAX_STEPS,
    MICRO_VAR_SLOT_COUNT,
    MAX_TELEMETRY_BINDINGS,
    TRIGGER_DATA_SIZE,
)


def validate_var_index(var_index: int) -> None:
    if var_index >= MICRO_VAR_SLOT_COUNT:
        raise ValueError(f"var_index must be < {MICRO_VAR_SLOT_COUNT}")


def validate_step_count(count: int, *, label: str = "Sequence") -> None:
    if count < 1:
        raise ValueError(f"{label} must have at least one step")
    if count > MICRO_SEQUENCE_MAX_STEPS:
        raise ValueError(f"Maximum {MICRO_SEQUENCE_MAX_STEPS} steps per {label.lower()}")


def validate_binding_step_count(count: int) -> None:
    validate_step_count(count, label="Binding")


def validate_powerup_step_count(count: int) -> None:
    validate_step_count(count, label="Powerup sequence")


def validate_tick_step_count(count: int, *, label: str = "Tick sequence") -> None:
    if count > MICRO_SEQUENCE_MAX_STEPS:
        raise ValueError(f"Maximum {MICRO_SEQUENCE_MAX_STEPS} steps per {label.lower()}")


def validate_binding_count(count: int) -> None:
    if count > MICRO_SEQUENCE_MAX_BINDINGS:
        raise ValueError(f"Maximum {MICRO_SEQUENCE_MAX_BINDINGS} bindings")


def validate_telemetry_binding_count(count: int) -> None:
    if count > MAX_TELEMETRY_BINDINGS:
        raise ValueError(f"Maximum {MAX_TELEMETRY_BINDINGS} telemetry bindings")


def validate_trigger_data(data: list) -> None:
    if len(data) != TRIGGER_DATA_SIZE:
        raise ValueError(f"Trigger data must be {TRIGGER_DATA_SIZE} bytes")


def validate_condition_blocks(steps, *, open_stack=None) -> None:
    if open_stack is not None and len(open_stack) > 0:
        raise ValueError(f"Unclosed if_condition block ({len(open_stack)} missing end_condition)")

    i = 0
    while i < len(steps):
        step = steps[i]
        member = step.union_member if hasattr(step, "union_member") else step.get("union_member")
        if member == "end_condition":
            raise ValueError(f"end_condition at step {i + 1} must not appear in emitted sequence")
        if member == "if_condition":
            values = step.values if hasattr(step, "values") else step.get("values", {})
            step_count = int(values.get("step_count", 0))
            if i + 1 + step_count > len(steps):
                raise ValueError(
                    f"if_condition at step {i + 1} has step_count {step_count} but only "
                    f"{len(steps) - i - 1} body step(s) follow"
                )
            i += 1 + step_count
        else:
            i += 1
