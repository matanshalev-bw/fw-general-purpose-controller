"""FastAPI server with WebSocket REPL bridge."""

from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, HTMLResponse
from gpc_recorder.dsl.repl import ReplEngine
from gpc_recorder.programmer_flash import (
    ProgrammerFlashError,
    flash_config_via_usb,
    flash_config_via_usb_stream,
)
from gpc_recorder.usb_bridge import (
    UsbBridgeError,
    destination_component_ids_catalog,
    get_usb_session,
    list_serial_ports,
    controller_commands_catalog,
    micro_ops_catalog,
    send_controller_command,
    send_micro_command,
    stop_usb_log,
    usb_log_stream,
)
from gpc_recorder.schema.dictionary import bluelink_commands_dictionary
from gpc_recorder.schema.recorder_dictionary import recorder_commands_dictionary
from gpc_recorder.paths import (
    DEFAULT_EXPORT_BIN_PATH,
    DEFAULT_EXPORT_PATH,
    FLASH_CONFIG_BYTES_SIZE,
    TOOL_DIR,
)

WEB_DIR = TOOL_DIR / "web"
_NO_CACHE = {"Cache-Control": "no-cache, no-store, must-revalidate"}
_APP_JS = WEB_DIR / "app.js"
_LLC_STATE_MACHINE_PNG = WEB_DIR / "gen4_llc_state_machine.png"
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


@app.get("/assets/gen4_llc_state_machine.png")
async def gen4_llc_state_machine_png() -> FileResponse:
    return FileResponse(_LLC_STATE_MACHINE_PNG, media_type="image/png", headers=_NO_CACHE)


@app.get("/api/preview")
async def preview() -> dict:
    return {
        "hpp": _repl.preview_hpp(),
        "powerup": _repl.ctx.powerup_summary(),
        "bindings": _repl.ctx.bindings_summary(),
    }


@app.post("/api/preview/apply")
async def preview_apply(body: dict) -> dict:
    hpp = body.get("hpp")
    if hpp is None:
        return {"ok": False, "error": "Missing hpp"}
    try:
        summary = _repl.ctx.load_hpp(str(hpp))
        return {
            "ok": True,
            "output": summary,
            "preview": _repl.preview_hpp(),
            "powerup": _repl.ctx.powerup_summary(),
            "bindings": _repl.ctx.bindings_summary(),
        }
    except Exception as e:
        return {"ok": False, "error": str(e)}


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


@app.get("/api/usb/component-ids")
async def usb_component_ids() -> dict:
    return {"component_ids": destination_component_ids_catalog()}


@app.get("/api/schema/commands-dictionary")
async def schema_commands_dictionary() -> dict:
    return bluelink_commands_dictionary()

@app.get("/api/schema/recorder-dictionary")
async def schema_recorder_dictionary() -> dict:
    return recorder_commands_dictionary()


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
    stop_usb_log()
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
            destination_component=body.get("destination_component"),
            destination_component_id=body.get("destination_component_id"),
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
            destination_component=body.get("destination_component"),
            destination_component_id=body.get("destination_component_id"),
            qos=str(qos),
            retries=int(body.get("retries", 5)),
            timeout_ms=int(body.get("timeout_ms", 2000)),
        )
        return result
    except UsbBridgeError as e:
        return {"ok": False, "error": str(e)}


@app.post("/api/flash")
async def flash_config(body: dict) -> dict:
    port = body.get("port")
    if not port:
        return {"ok": False, "error": "Missing port"}
    try:
        return flash_config_via_usb(str(port))
    except ProgrammerFlashError as e:
        return {"ok": False, "error": str(e)}


@app.websocket("/ws/flash")
async def flash_ws(websocket: WebSocket) -> None:
    await websocket.accept()
    try:
        body = await websocket.receive_json()
        port = body.get("port")
        if not port:
            await websocket.send_json({"type": "error", "message": "Missing port"})
            return

        async for event in flash_config_via_usb_stream(str(port)):
            await websocket.send_json(event)
            if event["type"] in {"done", "error"}:
                break
    except WebSocketDisconnect:
        pass
    except ProgrammerFlashError as e:
        await websocket.send_json({"type": "error", "message": str(e)})


@app.websocket("/ws/usb-log")
async def usb_log_ws(websocket: WebSocket) -> None:
    await websocket.accept()
    try:
        body = await websocket.receive_json()
        port = body.get("port")
        if not port:
            await websocket.send_json({"type": "error", "message": "Missing port"})
            return

        async for event in usb_log_stream(str(port)):
            await websocket.send_json(event)
            if event["type"] in {"done", "error"}:
                break
    except WebSocketDisconnect:
        stop_usb_log()
    except UsbBridgeError as e:
        await websocket.send_json({"type": "error", "message": str(e)})


@app.post("/api/export")
async def export_config() -> dict:
    try:
        text = _repl.ctx.export(str(DEFAULT_EXPORT_PATH))
        return {
            "ok": True,
            "path": str(DEFAULT_EXPORT_PATH),
            "bin_path": str(DEFAULT_EXPORT_BIN_PATH),
            "hpp": text,
            "flash_note": (
                f"Also wrote {DEFAULT_EXPORT_BIN_PATH.name} (packed config image). "
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
