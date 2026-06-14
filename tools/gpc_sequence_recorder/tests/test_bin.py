"""Config bin export via CMake (arm-none-eabi) or STM32CubeIDE headless build."""

import pytest

from gpc_recorder.codegen.config_build import (
    arm_toolchain_available,
    build_config_bin,
)
from gpc_recorder.codegen.emitter import emit_config_bin, emit_config_hpp
from gpc_recorder.paths import DEFAULT_EXPORT_BIN_PATH, FLASH_CONFIG_BYTES_SIZE, REPO_ROOT
from gpc_recorder.schema.loader import get_schema
from tests.test_golden import _build_example_session


pytestmark = pytest.mark.skipif(
    not arm_toolchain_available(),
    reason="arm-none-eabi-gcc not installed (sudo apt install gcc-arm-none-eabi)",
)


def test_cmake_build_produces_bin():
    schema = get_schema()
    session = _build_example_session()
    hpp = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    backup = hpp.read_text(encoding="utf-8")
    try:
        emit_config_hpp(session, schema, hpp, write=True)
        bin_path = build_config_bin(session, schema)
        data = bin_path.read_bytes()
        assert len(data) > 0
        assert len(data) <= FLASH_CONFIG_BYTES_SIZE
    finally:
        hpp.write_text(backup, encoding="utf-8")


def test_export_writes_bin_to_default_path():
    schema = get_schema()
    session = _build_example_session()
    hpp = REPO_ROOT / "configs/ConfigsTypes/g474_gpc_config_memory.hpp"
    dest = DEFAULT_EXPORT_BIN_PATH
    backup = hpp.read_text(encoding="utf-8")
    try:
        emit_config_hpp(session, schema, hpp, write=True)
        emit_config_bin(session, schema, dest, write=True)
        assert dest.is_file()
        assert dest.stat().st_size > 0
        assert "powerup_sequence" in hpp.read_text()
        assert "digital_gpio_write" in hpp.read_text()
    finally:
        hpp.write_text(backup, encoding="utf-8")
