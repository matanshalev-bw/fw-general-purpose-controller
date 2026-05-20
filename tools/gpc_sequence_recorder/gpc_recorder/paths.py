from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parent.parent  # tools/gpc_sequence_recorder
REPO_ROOT = TOOL_DIR.parent.parent  # fw-general-purpose-controller repo root

BLUELINK_MSG = REPO_ROOT / "3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include"
PAYLOAD_STRUCTS = BLUELINK_MSG / "PayloadStructs"

DEFAULT_EXPORT_PATH = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
DEFAULT_EXPORT_HEX_PATH = (
    REPO_ROOT / "config_projects/config_g474/Debug/config_g474.hex"
)

STM32CUBEIDE_WORKSPACE = REPO_ROOT / "out/stm32cubeide-ws"
STM32CUBEIDE_ECLIPSE_CONFIG = REPO_ROOT / "out/stm32cubeide-eclipse-config"
STM32CUBEIDE_PROJECT_DIR = REPO_ROOT / "config_projects/config_g474"
STM32CUBEIDE_BUILD_CONFIG = "config_g474/Debug"

MICRO_SEQUENCE_MAX_STEPS = 15
MICRO_SEQUENCE_MAX_BINDINGS = 16
MICRO_VAR_SLOT_COUNT = 8
TRIGGER_DATA_SIZE = 8
FLASH_CONFIG_ADDRESS = 0x08070000
FLASH_CONFIG_BYTES_SIZE = 0x10000
