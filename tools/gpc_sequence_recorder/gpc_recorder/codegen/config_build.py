"""Build config_g474.hex via STM32CubeIDE headless CLI.

Requires STM32CubeIDE on the host (see STM32CUBEIDE or /opt/st/stm32cubeide_*).
The exported g474_gpc_config_memory.hpp must already be on disk before building.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, Iterable, Optional

from gpc_recorder.paths import (
    DEFAULT_EXPORT_HEX_PATH,
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


def _run_debug_build(headless: Path) -> None:
    _ensure_project_imported(headless)
    _run_headless(headless, ["-build", STM32CUBEIDE_BUILD_CONFIG], label="build")


def _hex_search_paths() -> list[Path]:
    ws = STM32CUBEIDE_WORKSPACE
    proj = STM32CUBEIDE_PROJECT_DIR.name
    return [
        DEFAULT_EXPORT_HEX_PATH,
        STM32CUBEIDE_PROJECT_DIR / "Debug/config_g474.hex",
        ws / f"{proj}/Debug/config_g474.hex",
        ws / "fw-config-g4/Debug/config_g474.hex",
        ws / "config_g474/Debug/config_g474.hex",
    ]


def _locate_built_hex() -> Optional[Path]:
    seen: set[Path] = set()
    for path in _hex_search_paths():
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file() and resolved.stat().st_size > 0:
            return resolved
    return None


def build_config_hex(
    session: Dict[str, Any],
    schema: Schema,
    dest: Path | None = None,
    *,
    try_cmake: bool = False,
) -> Path:
    """Compile config_g474 via STM32CubeIDE and copy the generated Intel HEX."""
    del session, schema, try_cmake

    headless = find_stm32cubeide_headless()
    _run_debug_build(headless)

    built = _locate_built_hex()
    if built is None:
        raise ConfigBuildError(
            "STM32CubeIDE build finished but config_g474.hex was not found. "
            f"Expected under {DEFAULT_EXPORT_HEX_PATH} or the CubeIDE workspace."
        )

    out = Path(dest) if dest is not None else DEFAULT_EXPORT_HEX_PATH
    if built.resolve() != out.resolve():
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(built, out)
    return out
