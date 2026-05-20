"""FastAPI server with WebSocket REPL bridge."""

from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, HTMLResponse
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.paths import (
    DEFAULT_EXPORT_HEX_PATH,
    DEFAULT_EXPORT_PATH,
    FLASH_CONFIG_BYTES_SIZE,
    TOOL_DIR,
)

WEB_DIR = TOOL_DIR / "web"

app = FastAPI(title="GPC Sequence Recorder")
_repl = ReplEngine()


@app.get("/")
async def index() -> HTMLResponse:
    html = (WEB_DIR / "index.html").read_text(encoding="utf-8")
    return HTMLResponse(html)


@app.get("/app.js")
async def app_js() -> FileResponse:
    return FileResponse(WEB_DIR / "app.js", media_type="application/javascript")


@app.get("/api/preview")
async def preview() -> dict:
    return {"hpp": _repl.preview_hpp()}


@app.post("/api/export")
async def export_config() -> dict:
    try:
        text = _repl.ctx.export(str(DEFAULT_EXPORT_PATH))
        return {
            "ok": True,
            "path": str(DEFAULT_EXPORT_PATH),
            "hex_path": str(DEFAULT_EXPORT_HEX_PATH),
            "hpp": text,
            "flash_note": (
                f"Also wrote {DEFAULT_EXPORT_HEX_PATH.name} (packed config image). "
                f"Flash region: {FLASH_CONFIG_BYTES_SIZE // 1024} KB at 0x08070000."
            ),
        }
    except Exception as e:
        return {"ok": False, "error": str(e)}


@app.websocket("/ws/repl")
async def repl_ws(websocket: WebSocket) -> None:
    await websocket.accept()
    try:
        while True:
            line = await websocket.receive_text()
            if line == "\t":
                full_line = await websocket.receive_text()
                matches = _repl.complete(full_line)
                common = _repl.complete_common_prefix(full_line)
                await websocket.send_json(
                    {"type": "complete", "matches": matches, "common_prefix": common}
                )
                continue

            output, cont = _repl.execute(line)
            preview = _repl.preview_hpp()
            await websocket.send_json(
                {
                    "type": "result",
                    "output": output,
                    "continue": cont,
                    "preview": preview,
                    "powerup": _repl.ctx.powerup_summary(),
                    "bindings": _repl.ctx.bindings_summary(),
                }
            )
            if not cont:
                break
    except WebSocketDisconnect:
        pass
