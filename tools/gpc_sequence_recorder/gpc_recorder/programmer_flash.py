"""Flash GPC config image via host programmer over USB."""

from __future__ import annotations

import asyncio
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, AsyncIterator, Dict, List, Optional

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
    for cmd in ("prog-gpc-g4",):
        resolved = shutil.which(cmd)
        if resolved:
            return Path(resolved)

    if sys.platform == "win32":
        makefile_names = ("g474.exe", "prog-gpc-g4.exe")
        cmake_names = ("prog-gpc-g4.exe",)
    else:
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
        f"Programmer binary not found (prog-gpc-g4, {PROGRAMMER_DIR}, or {PROGRAMMER_CMAKE_BUILD_DIR}). "
        f"Dev build: cd programmer/g474 && make CXX=g++ all"
    )


def resolve_config_bin(bin_path: Optional[Path] = None) -> Path:
    path = Path(bin_path) if bin_path is not None else DEFAULT_EXPORT_BIN_PATH
    if path.is_file() and path.stat().st_size > 0:
        return path.resolve()

    raise ProgrammerFlashError(
        f"Config binary not found at {path}. "
        f"Click Export first to build {DEFAULT_EXPORT_BIN_PATH.name}."
    )


def build_flash_command(
    port: str,
    *,
    bin_path: Optional[Path] = None,
    controller_type: str = "gpc",
) -> tuple[List[str], Path]:
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
    if shutil.which("stdbuf"):
        cmd = ["stdbuf", "-oL", *cmd]
    return cmd, image


async def flash_config_via_usb_stream(
    port: str,
    *,
    bin_path: Optional[Path] = None,
    controller_type: str = "gpc",
) -> AsyncIterator[Dict[str, Any]]:
    cmd, image = build_flash_command(
        port,
        bin_path=bin_path,
        controller_type=controller_type,
    )

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=str(REPO_ROOT),
        )
    except OSError as exc:
        raise ProgrammerFlashError(f"Failed to run programmer: {exc}") from exc

    output_parts: List[str] = []
    deadline = asyncio.get_running_loop().time() + FLASH_TIMEOUT_S
    try:
        assert proc.stdout is not None
        while True:
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0:
                proc.kill()
                await proc.wait()
                raise ProgrammerFlashError(f"Flash timed out after {FLASH_TIMEOUT_S}s")

            try:
                chunk = await asyncio.wait_for(proc.stdout.read(1024), timeout=remaining)
            except asyncio.TimeoutError as exc:
                proc.kill()
                await proc.wait()
                raise ProgrammerFlashError(f"Flash timed out after {FLASH_TIMEOUT_S}s") from exc

            if not chunk:
                break

            text = chunk.decode(errors="replace")
            output_parts.append(text)
            yield {"type": "output", "text": text}

        return_code = await proc.wait()
        output = "".join(output_parts).strip()
        if return_code != 0:
            detail = output or f"programmer exit code {return_code}"
            yield {"type": "error", "message": detail, "output": output}
            return

        yield {
            "type": "done",
            "ok": True,
            "port": port,
            "bin_path": str(image),
            "output": output,
        }
    finally:
        if proc.returncode is None:
            proc.kill()
            await proc.wait()


def flash_config_via_usb(
    port: str,
    *,
    bin_path: Optional[Path] = None,
    controller_type: str = "gpc",
) -> Dict[str, Any]:
    cmd, image = build_flash_command(
        port,
        bin_path=bin_path,
        controller_type=controller_type,
    )

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
