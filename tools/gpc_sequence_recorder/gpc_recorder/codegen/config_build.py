"""Build config_g474.bin via CMake (arm-none-eabi) or STM32CubeIDE headless CLI.

CMake is the default path and requires gcc-arm-none-eabi on PATH (or cmake/toolchain-paths.cmake).
STM32CubeIDE is used only when CMake is unavailable and CubeIDE is installed.
The exported g474_gpc_config_memory.hpp must already be on disk before building.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, Iterable, Optional

from gpc_recorder.paths import (
    CMAKE_CONFIG_BUILD_DIR,
    CMAKE_CONFIG_INSTALL_DIR,
    CMAKE_CONFIG_PRESET,
    DEFAULT_EXPORT_BIN_PATH,
    REPO_ROOT,
    STM32CUBEIDE_BUILD_CONFIG,
    STM32CUBEIDE_ECLIPSE_CONFIG,
    STM32CUBEIDE_PROJECT_DIR,
    STM32CUBEIDE_WORKSPACE,
)
from gpc_recorder.schema.cpp_parser import Schema

IMPORT_MARKER = STM32CUBEIDE_WORKSPACE / ".gpc_recorder_imported"
BUILD_TIMEOUT_S = 900


class ConfigBuildError(RuntimeError):
    pass


def _stm32_cube_gcc_candidates() -> list[Path]:
    opt_st = Path("/opt/st")
    if not opt_st.is_dir():
        return []
    return sorted(
        opt_st.glob(
            "stm32cubeide_*/plugins/com.st.stm32cube.ide.mcu.externaltools."
            "gnu-tools-for-stm32.*.linux64_*/tools/bin/arm-none-eabi-gcc"
        ),
        reverse=True,
    )


def arm_toolchain_available() -> bool:
    """Return True when arm-none-eabi-gcc is available for CMake cross-builds."""
    env_gcc = os.environ.get("STM32_CUBE_GCC_BIN", "").strip()
    if env_gcc and Path(env_gcc).is_file():
        return True
    if shutil.which("arm-none-eabi-gcc") is not None:
        return True
    toolchain_file = REPO_ROOT / "cmake/toolchain-paths.cmake"
    if toolchain_file.is_file():
        text = toolchain_file.read_text(encoding="utf-8")
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("#") or "TOOLCHAIN_PATH" not in stripped:
                continue
            path = stripped.split('"')
            if len(path) >= 2 and path[1]:
                gcc = Path(path[1]) / "arm-none-eabi-gcc"
                if gcc.is_file():
                    return True
    return any(path.is_file() for path in _stm32_cube_gcc_candidates())


def find_stm32cubeide_headless() -> Path:
    """Resolve headless-build.sh (STM32CUBEIDE env or /opt/st/stm32cubeide_*)."""
    env_root = os.environ.get("STM32CUBEIDE", "").strip()
    if env_root:
        candidate = Path(env_root) / "headless-build.sh"
        if candidate.is_file():
            return candidate

    opt_st = Path("/opt/st")
    if opt_st.is_dir():
        installs = sorted(opt_st.glob("stm32cubeide_*/headless-build.sh"), reverse=True)
        for script in installs:
            if script.is_file():
                return script

    raise ConfigBuildError(
        "STM32CubeIDE not found. Set STM32CUBEIDE to the install directory "
        "(e.g. /opt/st/stm32cubeide_1.19.0) or install under /opt/st/."
    )


def _headless_base_args(headless: Path) -> list[str]:
    STM32CUBEIDE_ECLIPSE_CONFIG.mkdir(parents=True, exist_ok=True)
    STM32CUBEIDE_WORKSPACE.mkdir(parents=True, exist_ok=True)
    return [
        str(headless),
        "-configuration",
        str(STM32CUBEIDE_ECLIPSE_CONFIG),
        "-data",
        str(STM32CUBEIDE_WORKSPACE),
    ]


def _run_headless(headless: Path, extra: Iterable[str], *, label: str) -> None:
    cmd = _headless_base_args(headless) + list(extra)
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=BUILD_TIMEOUT_S,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        raise ConfigBuildError(f"STM32CubeIDE {label} timed out after {BUILD_TIMEOUT_S}s") from e
    except FileNotFoundError as e:
        raise ConfigBuildError(f"Failed to run {headless}: {e}") from e

    if proc.returncode != 0:
        combined = (proc.stdout or "") + "\n" + (proc.stderr or "")
        lines = [ln for ln in combined.splitlines() if "error:" in ln.lower() or "fatal error:" in ln.lower()]
        if not lines and "Build Failed" in combined:
            lines = [ln for ln in combined.splitlines() if "Failed" in ln or "Error" in ln][-5:]
        detail = "\n".join(lines[-8:]) if lines else combined.strip()[-2000:]
        hint = ""
        if "Workspace already in use" in combined:
            hint = " Close other STM32CubeIDE instances or remove out/stm32cubeide-ws/.metadata/.lock."
        raise ConfigBuildError(
            f"STM32CubeIDE {label} failed (exit {proc.returncode}){hint}"
            + (f":\n{detail}" if detail else "")
        )


def _ensure_project_imported(headless: Path) -> None:
    if IMPORT_MARKER.is_file():
        return
    if not STM32CUBEIDE_PROJECT_DIR.is_dir():
        raise ConfigBuildError(f"Config project missing: {STM32CUBEIDE_PROJECT_DIR}")
    _run_headless(
        headless,
        ["-import", str(STM32CUBEIDE_PROJECT_DIR)],
        label="import",
    )
    IMPORT_MARKER.touch()


def _run_cubeide_debug_build(headless: Path) -> None:
    _ensure_project_imported(headless)
    _run_headless(headless, ["-build", STM32CUBEIDE_BUILD_CONFIG], label="build")


def _cmake_bin_search_paths() -> list[Path]:
    install_bin = CMAKE_CONFIG_INSTALL_DIR / "bin"
    subproject = CMAKE_CONFIG_BUILD_DIR / "config_projects/config_g474"
    return [
        subproject / "config_g474.bin",
        subproject / "fw-config-g4.bin",
        CMAKE_CONFIG_BUILD_DIR / "config_g474.bin",
        CMAKE_CONFIG_BUILD_DIR / "fw-config-g4.bin",
        install_bin / "config_g474.bin",
        install_bin / "fw-config-g4.bin",
        DEFAULT_EXPORT_BIN_PATH,
    ]


def _cubeide_bin_search_paths() -> list[Path]:
    ws = STM32CUBEIDE_WORKSPACE
    proj = STM32CUBEIDE_PROJECT_DIR.name
    return [
        DEFAULT_EXPORT_BIN_PATH,
        STM32CUBEIDE_PROJECT_DIR / "Debug/config_g474.bin",
        ws / f"{proj}/Debug/config_g474.bin",
        ws / "fw-config-g4/Debug/config_g474.bin",
        ws / "config_g474/Debug/config_g474.bin",
    ]


def _locate_built_bin(search_paths: list[Path]) -> Optional[Path]:
    seen: set[Path] = set()
    for path in search_paths:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file() and resolved.stat().st_size > 0:
            return resolved
    return None


def _run_cmake_build() -> None:
    try:
        configure = subprocess.run(
            ["cmake", "--preset", CMAKE_CONFIG_PRESET],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=BUILD_TIMEOUT_S,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        raise ConfigBuildError(f"CMake configure timed out after {BUILD_TIMEOUT_S}s") from e
    except FileNotFoundError as e:
        raise ConfigBuildError("cmake not found; install cmake or use STM32CubeIDE") from e

    if configure.returncode != 0:
        combined = (configure.stdout or "") + "\n" + (configure.stderr or "")
        raise ConfigBuildError(
            f"CMake configure failed (exit {configure.returncode}):\n{combined.strip()[-2000:]}"
        )

    try:
        build = subprocess.run(
            ["cmake", "--build", "--preset", CMAKE_CONFIG_PRESET],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=BUILD_TIMEOUT_S,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        raise ConfigBuildError(f"CMake build timed out after {BUILD_TIMEOUT_S}s") from e

    if build.returncode != 0:
        combined = (build.stdout or "") + "\n" + (build.stderr or "")
        lines = [ln for ln in combined.splitlines() if "error:" in ln.lower() or "fatal error:" in ln.lower()]
        detail = "\n".join(lines[-8:]) if lines else combined.strip()[-2000:]
        raise ConfigBuildError(
            f"CMake build failed (exit {build.returncode})"
            + (f":\n{detail}" if detail else "")
        )


def build_config_bin(
    session: Dict[str, Any],
    schema: Schema,
    dest: Path | None = None,
    *,
    try_cmake: bool = True,
) -> Path:
    """Compile config_g474 and copy the generated binary to dest."""
    del session, schema

    built: Optional[Path] = None

    if try_cmake and arm_toolchain_available():
        _run_cmake_build()
        built = _locate_built_bin(_cmake_bin_search_paths())

    if built is None:
        try:
            headless = find_stm32cubeide_headless()
        except ConfigBuildError as e:
            if try_cmake and not arm_toolchain_available():
                raise ConfigBuildError(
                    "No config build backend available. Install gcc-arm-none-eabi "
                    "(sudo apt install gcc-arm-none-eabi) or STM32CubeIDE."
                ) from e
            if built is None:
                raise ConfigBuildError(
                    "CMake build finished but config_g474.bin was not found. "
                    f"Expected under {CMAKE_CONFIG_BUILD_DIR}."
                ) from e
            raise
        _run_cubeide_debug_build(headless)
        built = _locate_built_bin(_cubeide_bin_search_paths())

    if built is None:
        raise ConfigBuildError(
            "Config build finished but config_g474.bin was not found. "
            f"Expected under {CMAKE_CONFIG_BUILD_DIR} or the CubeIDE workspace."
        )

    out = Path(dest) if dest is not None else DEFAULT_EXPORT_BIN_PATH
    if built.resolve() != out.resolve():
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(built, out)
    return out
