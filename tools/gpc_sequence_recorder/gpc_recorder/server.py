"""FastAPI server with WebSocket REPL bridge."""

from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, HTMLResponse
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.usb_bridge import (
    UsbBridgeError,
    get_usb_session,
    list_serial_ports,
    controller_commands_catalog,
    micro_ops_catalog,
    send_controller_command,
    send_micro_command,
)
from gpc_recorder.paths import (
    DEFAULT_EXPORT_HEX_PATH,
    DEFAULT_EXPORT_PATH,
    FLASH_CONFIG_BYTES_SIZE,
    TOOL_DIR,
)

WEB_DIR = TOOL_DIR / "web"
_NO_CACHE = {"Cache-Control": "no-cache, no-store, must-revalidate"}
_APP_JS = WEB_DIR / "app.js"
_ASSET_VERSION = str(int(_APP_JS.stat().st_mtime)) if _APP_JS.is_file() else "0"

app = FastAPI(title="GPC Sequence Recorder")
_repl = ReplEngine()


@app.get("/")
async def index() -> HTMLResponse:
    html = (WEB_DIR / "index.html").read_text(encoding="utf-8")
    html = html.replace('src="/app.js"', f'src="/app.js?v={_ASSET_VERSION}"')
    return HTMLResponse(html, headers=_NO_CACHE)


@app.get("/app.js")
async def app_js() -> FileResponse:
    return FileResponse(_APP_JS, media_type="application/javascript", headers=_NO_CACHE)


@app.get("/api/preview")
async def preview() -> dict:
    return {"hpp": _repl.preview_hpp()}


@app.get("/api/usb/ports")
async def usb_ports() -> dict:
    return {"ports": list_serial_ports()}


@app.get("/api/usb/status")
async def usb_status() -> dict:
    return get_usb_session().status()


@app.get("/api/usb/micro-ops")
async def usb_micro_ops() -> dict:
    return {"micro_ops": micro_ops_catalog()}

@app.get("/api/usb/controller-commands")
async def usb_controller_commands() -> dict:
    return {"controller_commands": controller_commands_catalog()}


@app.post("/api/usb/open")
async def usb_open(body: dict) -> dict:
    port = body.get("port")
    if not port:
        return {"ok": False, "error": "Missing port"}
    try:
        get_usb_session().open(str(port))
        return {"ok": True, **get_usb_session().status()}
    except UsbBridgeError as e:
        return {"ok": False, "error": str(e)}


@app.post("/api/usb/close")
async def usb_close() -> dict:
    get_usb_session().close()
    return {"ok": True, **get_usb_session().status()}


@app.post("/api/usb/send-micro")
async def usb_send_micro(body: dict) -> dict:
    union_member = body.get("union_member")
    values = body.get("values") or {}
    qos = body.get("qos", "none")
    if not union_member:
        return {"ok": False, "error": "Missing union_member"}
    try:
        result = send_micro_command(
            str(union_member),
            values,
            qos=str(qos),
            retries=int(body.get("retries", 5)),
            timeout_ms=int(body.get("timeout_ms", 2000)),
        )
        return result
    except UsbBridgeError as e:
        return {"ok": False, "error": str(e)}

@app.post("/api/usb/send-controller")
async def usb_send_controller(body: dict) -> dict:
    payload_type = body.get("payload_type")
    values = body.get("values") or {}
    qos = body.get("qos", "none")
    if not payload_type:
        return {"ok": False, "error": "Missing payload_type"}
    try:
        result = send_controller_command(
            str(payload_type),
            values,
            qos=str(qos),
            retries=int(body.get("retries", 5)),
            timeout_ms=int(body.get("timeout_ms", 2000)),
        )
        return result
    except UsbBridgeError as e:
        return {"ok": False, "error": str(e)}


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
