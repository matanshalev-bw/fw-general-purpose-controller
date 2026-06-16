import os
from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parent.parent  # tools/gpc_sequence_recorder
REPO_ROOT = TOOL_DIR.parent.parent  # fw-general-purpose-controller repo root

_INSTALLED_REPO = Path("/opt/gpc-recorder/repo")
_INSTALLED_BIN_DIR = Path("/opt/gpc-recorder/bin")

BLUELINK_MSG = REPO_ROOT / "3rd_party/bluelink_sdk/bluelink_messages/bluelink_messages_include"
PAYLOAD_STRUCTS = BLUELINK_MSG / "PayloadStructs"


def _user_data_root() -> Path:
    env = os.environ.get("GPC_RECORDER_DATA", "").strip()
    if env:
        return Path(env)
    xdg = os.environ.get("XDG_CACHE_HOME", "").strip()
    if xdg:
        return Path(xdg) / "gpc-recorder"
    return Path.home() / ".cache" / "gpc-recorder"


def _repo_is_installed_read_only() -> bool:
    try:
        return REPO_ROOT.resolve().is_relative_to(_INSTALLED_REPO.resolve())
    except AttributeError:
        repo = str(REPO_ROOT.resolve())
        prefix = str(_INSTALLED_REPO.resolve())
        return repo == prefix or repo.startswith(prefix + "/")


def _use_user_data_paths() -> bool:
    if os.environ.get("GPC_RECORDER_USE_REPO_PATHS", "").strip().lower() in ("1", "true", "yes"):
        return False
    if os.environ.get("GPC_RECORDER_DATA", "").strip():
        return True
    if _repo_is_installed_read_only():
        return True
    out_dir = REPO_ROOT / "out"
    if out_dir.is_dir() and os.access(out_dir, os.W_OK):
        return False
    return not os.access(REPO_ROOT, os.W_OK)


USE_USER_DATA_PATHS = _use_user_data_paths()
USER_DATA_ROOT = _user_data_root()

if USE_USER_DATA_PATHS:
    USER_DATA_ROOT.mkdir(parents=True, exist_ok=True)
    (USER_DATA_ROOT / "exports").mkdir(parents=True, exist_ok=True)
    (USER_DATA_ROOT / "venv").mkdir(parents=True, exist_ok=True)

    DEFAULT_EXPORT_PATH = USER_DATA_ROOT / "exports/g474_gpc_config_memory.hpp"
    DEFAULT_EXPORT_BIN_PATH = USER_DATA_ROOT / "exports/config_g474.bin"
    GPC_EXPORT_CONFIG_DIR = USER_DATA_ROOT / "exports"
    CMAKE_CONFIG_BUILD_DIR = USER_DATA_ROOT / "build/config-g474/Debug"
    CMAKE_CONFIG_INSTALL_DIR = USER_DATA_ROOT / "install/config-g474/Debug"
    VENV_DIR = USER_DATA_ROOT / "venv"
    STM32CUBEIDE_WORKSPACE = USER_DATA_ROOT / "stm32cubeide-ws"
    STM32CUBEIDE_ECLIPSE_CONFIG = USER_DATA_ROOT / "stm32cubeide-eclipse-config"
else:
    DEFAULT_EXPORT_PATH = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    DEFAULT_EXPORT_BIN_PATH = REPO_ROOT / "config_projects/config_g474/Debug/config_g474.bin"
    GPC_EXPORT_CONFIG_DIR = REPO_ROOT / "configs/ConfigsTypes"
    CMAKE_CONFIG_BUILD_DIR = REPO_ROOT / "out/build/config-g474/Debug"
    CMAKE_CONFIG_INSTALL_DIR = REPO_ROOT / "out/install/config-g474/Debug"
    VENV_DIR = TOOL_DIR / ".venv"
    STM32CUBEIDE_WORKSPACE = REPO_ROOT / "out/stm32cubeide-ws"
    STM32CUBEIDE_ECLIPSE_CONFIG = REPO_ROOT / "out/stm32cubeide-eclipse-config"

CMAKE_CONFIG_PRESET = "config-g474.Debug"

PROGRAMMER_DIR = _INSTALLED_BIN_DIR if _repo_is_installed_read_only() else REPO_ROOT / "programmer/g474"
PROGRAMMER_CMAKE_BUILD_DIR = REPO_ROOT / "programmer/build"

STM32CUBEIDE_PROJECT_DIR = REPO_ROOT / "config_projects/config_g474"
STM32CUBEIDE_BUILD_CONFIG = "config_g474/Debug"

MICRO_SEQUENCE_MAX_STEPS = 15
MICRO_SEQUENCE_MAX_BINDINGS = 16
MICRO_VAR_SLOT_COUNT = 8
MAX_TELEMETRY_BINDINGS = 3
TRIGGER_DATA_SIZE = 8

CONTROLLER_STATE_SEQUENCE_FIELDS = {
    "CONTROLLER_STATE_INIT": "init_state_sequence",
    "CONTROLLER_STATE_DISENGAGEMENT": "disengagement_state_sequence",
    "CONTROLLER_STATE_POWER_UP_BIT": "power_up_bit_state_sequence",
}

CONTROLLER_STATE_TICK_FIELDS = {
    "CONTROLLER_STATE_MANUAL": "manual_state_tick_sequence",
    "CONTROLLER_STATE_ENGAGED": "engaged_state_tick_sequence",
    "CONTROLLER_STATE_OPERATIONAL": "operational_state_tick_sequence",
}

FLASH_CONFIG_ADDRESS = 0x08070000
FLASH_CONFIG_BYTES_SIZE = 0x10000
