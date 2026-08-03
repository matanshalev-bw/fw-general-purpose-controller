"""Selectable bluelink::ComponentId values for the recorder UI and DSL."""

from __future__ import annotations

from typing import Any, Dict, List

EXCLUDED_SELECTABLE_COMPONENT_IDS = frozenset(
    {
        "COMPONENT_ID_BOOTLOADER",
        "COMPONENT_ID_HLC",
        "COMPONENT_ID_LLC",
        "COMPONENT_ID_BROADCAST",
    }
)


def selectable_component_ids(schema: Any) -> Dict[str, int]:
    return {
        name: value
        for name, value in schema.component_ids.items()
        if name not in EXCLUDED_SELECTABLE_COMPONENT_IDS
    }


def selectable_component_id_enum_values(schema: Any) -> List[Dict[str, Any]]:
    return [
        {"name": member, "value": value}
        for member, value in sorted(selectable_component_ids(schema).items(), key=lambda item: item[1])
    ]
