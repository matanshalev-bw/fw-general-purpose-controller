"""Recording session state."""

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional


@dataclass
class MicroOpStepState:
    op_type: str
    union_member: str
    values: Dict[str, Any]


@dataclass
class BindingState:
    payload_type: str
    struct_name: str
    field_values: Dict[str, Any]
    data: List[int]
    steps: List[MicroOpStepState] = field(default_factory=list)


def _steps_to_dict(steps: List[MicroOpStepState]) -> List[Dict[str, Any]]:
    return [
        {
            "op_type": s.op_type,
            "union_member": s.union_member,
            "values": s.values,
        }
        for s in steps
    ]


@dataclass
class Session:
    config_name: str = "G474_GPC_CONFIG"
    component_id: str = "COMPONENT_ID_GENERAL_PURPOSE_CONTROLLER"
    bindings: List[BindingState] = field(default_factory=list)
    current_binding: Optional[BindingState] = None
    powerup_steps: List[MicroOpStepState] = field(default_factory=list)
    recording_powerup: bool = False
    main_tick_steps: List[MicroOpStepState] = field(default_factory=list)
    recording_main_tick: bool = False
    state_steps: Dict[str, List[MicroOpStepState]] = field(default_factory=dict)
    recording_state: Optional[str] = None
    state_tick_steps: Dict[str, List[MicroOpStepState]] = field(default_factory=dict)
    recording_state_tick: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        bindings = []
        for b in self.bindings:
            bindings.append(
                {
                    "payload_type": b.payload_type,
                    "struct_name": b.struct_name,
                    "field_values": b.field_values,
                    "data": b.data,
                    "steps": _steps_to_dict(b.steps),
                }
            )
        if self.current_binding:
            cb = self.current_binding
            bindings.append(
                {
                    "payload_type": cb.payload_type,
                    "struct_name": cb.struct_name,
                    "field_values": cb.field_values,
                    "data": cb.data,
                    "steps": _steps_to_dict(cb.steps),
                }
            )
        return {
            "config_name": self.config_name,
            "component_id": self.component_id,
            "bindings": bindings,
            "powerup_steps": _steps_to_dict(self.powerup_steps),
            "main_tick_steps": _steps_to_dict(self.main_tick_steps),
            "state_steps": {
                state: _steps_to_dict(steps) for state, steps in self.state_steps.items()
            },
            "state_tick_steps": {
                state: _steps_to_dict(steps) for state, steps in self.state_tick_steps.items()
            },
        }

    def is_recording(self) -> bool:
        return (
            self.recording_powerup
            or self.recording_main_tick
            or self.recording_state is not None
            or self.recording_state_tick is not None
            or self.current_binding is not None
        )
