"""Config hex export via STM32CubeIDE headless build."""

import pytest

from gpc_recorder.codegen.config_build import (
    ConfigBuildError,
    build_config_hex,
    find_stm32cubeide_headless,
)
from gpc_recorder.codegen.emitter import emit_config_hex, emit_config_hpp
from gpc_recorder.paths import DEFAULT_EXPORT_HEX_PATH, FLASH_CONFIG_ADDRESS, REPO_ROOT
from gpc_recorder.schema.loader import get_schema
from tests.test_golden import _build_example_session

def _stm32cubeide_available() -> bool:
    try:
        find_stm32cubeide_headless()
        return True
    except ConfigBuildError:
        return False


pytestmark = pytest.mark.skipif(
    not _stm32cubeide_available(),
    reason="STM32CubeIDE not installed (set STM32CUBEIDE or install under /opt/st/)",
)


def test_headless_build_produces_hex():
    schema = get_schema()
    session = _build_example_session()
    hpp = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    backup = hpp.read_text(encoding="utf-8")
    try:
        emit_config_hpp(session, schema, hpp, write=True)
        hex_path = build_config_hex(session, schema)
        text = hex_path.read_text(encoding="utf-8")
        assert ":10" in text or ":02000004" in text
        addr_line = f"{FLASH_CONFIG_ADDRESS:08X}".upper()
        assert addr_line[2:] in text.upper() or addr_line in text.upper()
    finally:
        hpp.write_text(backup, encoding="utf-8")


def test_export_writes_hex_to_default_path():
    schema = get_schema()
    session = _build_example_session()
    hpp = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    dest = DEFAULT_EXPORT_HEX_PATH
    backup = hpp.read_text(encoding="utf-8")
    try:
        emit_config_hpp(session, schema, hpp, write=True)
        emit_config_hex(session, schema, dest, write=True)
        assert dest.is_file()
        assert "powerup_sequence" in hpp.read_text()
        assert "digital_gpio_write" in hpp.read_text()
    finally:
        hpp.write_text(backup, encoding="utf-8")
