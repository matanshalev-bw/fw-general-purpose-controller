from gpc_recorder.paths import (
    MICRO_SEQUENCE_MAX_BINDINGS,
    MICRO_SEQUENCE_MAX_STEPS,
    MICRO_VAR_SLOT_COUNT,
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


def validate_trigger_data(data: list) -> None:
    if len(data) != TRIGGER_DATA_SIZE:
        raise ValueError(f"Trigger data must be {TRIGGER_DATA_SIZE} bytes")
