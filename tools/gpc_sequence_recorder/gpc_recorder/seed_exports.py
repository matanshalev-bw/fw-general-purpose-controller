"""Seed default export HPP into user data (deb postinst / Windows first-run)."""

from __future__ import annotations

from pathlib import Path

EXPORT_NAME = "g474_gpc_config_memory.hpp"


def seed_exports_if_missing(data_dir: Path, default_src: Path) -> None:
    if not data_dir or not default_src.is_file():
        return

    exports_dir = data_dir / "exports"
    dest = exports_dir / EXPORT_NAME
    if dest.is_file():
        return

    exports_dir.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(default_src.read_bytes())
