# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec for GPC Sequence Recorder (Windows).
# Run from repo root: pyinstaller packaging/gpc-recorder.spec

import sys
from pathlib import Path

REPO_ROOT = Path(SPECPATH).resolve().parent.parent
TOOL_DIR = REPO_ROOT / "tools" / "gpc_sequence_recorder"

block_cipher = None

_hidden = [
    "gpc_recorder",
    "gpc_recorder.server",
    "gpc_recorder.dsl.repl",
    "gpc_recorder.dsl.builtins",
    "gpc_recorder.codegen.emitter",
    "gpc_recorder.codegen.config_build",
    "gpc_recorder.usb_bridge",
    "gpc_recorder.programmer_flash",
    "serial.tools.list_ports",
    "uvicorn.logging",
    "uvicorn.loops",
    "uvicorn.loops.auto",
    "uvicorn.protocols",
    "uvicorn.protocols.http",
    "uvicorn.protocols.http.auto",
    "uvicorn.protocols.websockets",
    "uvicorn.protocols.websockets.auto",
    "uvicorn.protocols.websockets.websockets_impl",
    "uvicorn.lifespan",
    "uvicorn.lifespan.on",
    "uvicorn.lifespan.off",
    "uvicorn.importer",
    "websockets",
    "websockets.legacy",
    "websockets.legacy.server",
    "anyio._backends._asyncio",
]

_datas = [
    (str(TOOL_DIR / "web"), "gpc_recorder/web"),
    (str(TOOL_DIR / "gpc_recorder" / "codegen" / "templates"), "gpc_recorder/codegen/templates"),
]

_common = dict(
    pathex=[str(TOOL_DIR)],
    binaries=[],
    datas=_datas,
    hiddenimports=_hidden,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

a = Analysis([str(REPO_ROOT / "packaging" / "win_entry.py")], **_common)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="gpc-recorder",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

a_repl = Analysis([str(REPO_ROOT / "packaging" / "win_entry_repl.py")], **_common)
pyz_repl = PYZ(a_repl.pure, a_repl.zipped_data, cipher=block_cipher)

exe_repl = EXE(
    pyz_repl,
    a_repl.scripts,
    [],
    exclude_binaries=True,
    name="gpc-recorder-repl",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

coll = COLLECT(
    exe,
    exe_repl,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="gpc-recorder",
)
