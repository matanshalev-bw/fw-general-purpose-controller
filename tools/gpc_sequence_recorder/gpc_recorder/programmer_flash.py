"""Flash GPC config image via host programmer over USB."""

from __future__ import annotations

import platform
import subprocess
from pathlib import Path
from typing import Any, Dict, Optional

from gpc_recorder.paths import (
    DEFAULT_EXPORT_BIN_PATH,
    PROGRAMMER_CMAKE_BUILD_DIR,
    PROGRAMMER_DIR,
    REPO_ROOT,
)

FLASH_TIMEOUT_S = 180


class ProgrammerFlashError(Exception):
    pass


def programmer_binary() -> Path:
    is_arm = platform.machine().lower() in ("aarch64", "arm64")
    makefile_names = ("g474_aarch64", "g474_x86_64") if is_arm else ("g474_x86_64", "g474_aarch64")
    cmake_names = ("prog-gpc-g4_aarch64", "prog-gpc-g4_x86_64") if is_arm else (
        "prog-gpc-g4_x86_64",
        "prog-gpc-g4_aarch64",
    )

    for name in makefile_names:
        path = PROGRAMMER_DIR / name
        if path.is_file():
            return path

    for name in cmake_names:
        path = PROGRAMMER_CMAKE_BUILD_DIR / name
        if path.is_file():
            return path

    fallback = PROGRAMMER_DIR / "g474"
    if fallback.is_file():
        return fallback

    raise ProgrammerFlashError(
        f"Programmer binary not found under {PROGRAMMER_DIR} or {PROGRAMMER_CMAKE_BUILD_DIR}. "
        f"Build with: cd {PROGRAMMER_DIR} && make CC=g++ all"
    )


def resolve_config_bin(bin_path: Optional[Path] = None) -> Path:
    path = Path(bin_path) if bin_path is not None else DEFAULT_EXPORT_BIN_PATH
    if path.is_file() and path.stat().st_size > 0:
        return path.resolve()

    raise ProgrammerFlashError(
        f"Config binary not found at {path}. "
        f"Click Export first to build {DEFAULT_EXPORT_BIN_PATH.name}."
    )


def flash_config_via_usb(
    port: str,
    *,
    bin_path: Optional[Path] = None,
    controller_type: str = "gpc",
) -> Dict[str, Any]:
    port = str(port).strip()
    if not port:
        raise ProgrammerFlashError("Select a USB port first")

    image = resolve_config_bin(bin_path)
    binary = programmer_binary()

    cmd = [
        str(binary),
        "--transport",
        "usb",
        "--port",
        port,
        controller_type,
        "config",
        str(image),
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=FLASH_TIMEOUT_S,
            cwd=str(REPO_ROOT),
        )
    except subprocess.TimeoutExpired as exc:
        raise ProgrammerFlashError(f"Flash timed out after {FLASH_TIMEOUT_S}s") from exc
    except OSError as exc:
        raise ProgrammerFlashError(f"Failed to run programmer: {exc}") from exc

    stdout = (result.stdout or "").strip()
    stderr = (result.stderr or "").strip()
    output = "\n".join(part for part in (stdout, stderr) if part)

    if result.returncode != 0:
        detail = output or f"programmer exit code {result.returncode}"
        raise ProgrammerFlashError(detail)

    return {
        "ok": True,
        "port": port,
        "bin_path": str(image),
        "output": output,
    }
