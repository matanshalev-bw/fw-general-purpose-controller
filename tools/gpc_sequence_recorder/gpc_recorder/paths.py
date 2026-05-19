from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parent.parent  # tools/gpc_sequence_recorder
REPO_ROOT = TOOL_DIR.parent.parent  # fw-general-purpose-controller repo root

BLUELINK_MSG = REPO_ROOT / "3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include"
PAYLOAD_STRUCTS = BLUELINK_MSG / "PayloadStructs"

DEFAULT_EXPORT_PATH = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"

MICRO_SEQUENCE_MAX_STEPS = 15
MICRO_SEQUENCE_MAX_BINDINGS = 16
MICRO_VAR_SLOT_COUNT = 8
TRIGGER_DATA_SIZE = 8
FLASH_CONFIG_BYTES_SIZE = 0x10000
