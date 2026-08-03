"""
Local backend for the Time Circuits web control panel.

Talks to the Time Circuits CANopen node (node ID 21 by default) over a
USBtin (or any slcan-compatible) CAN-USB adapter, and exposes it to the
browser frontend over a small HTTP/WebSocket API on localhost.

Run with:
    .venv/bin/uvicorn app:app --host 127.0.0.1 --port 8420

Config via environment variables:
    TC_CAN_PORT      serial device for the adapter (default /dev/ttyACM0)
    TC_CAN_BITRATE   CAN bus bitrate in bps (default 1000000, matches can.c)
    TC_CAN_NODE_ID   CANopen node ID of the Time Circuits board (default 21)
"""

import asyncio
import json
import os
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

import can
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from canopen_sdo import CanopenSdoClient, SdoAbort, SdoTimeout
from object_dictionary import DTYPE_SIZES, find_field, od_as_json, TIME_CIRCUITS_STATE_NAMES

CAN_PORT = os.environ.get("TC_CAN_PORT", "/dev/ttyACM0")
CAN_BITRATE = int(os.environ.get("TC_CAN_BITRATE", "1000000"))
NODE_ID = int(os.environ.get("TC_CAN_NODE_ID", "21"))

FRONTEND_DIR = Path(__file__).resolve().parent.parent / "frontend"
MOVIE_DATES_PATH = Path(__file__).resolve().parent / "movie_dates.json"
HISTORICAL_DATES_PATH = Path(__file__).resolve().parent / "historical_dates.json"

app = FastAPI(title="Time Circuits Control Panel")


@app.middleware("http")
async def no_cache(request, call_next):
    # This is a local dev tool that changes frequently during development -
    # browser caching of the frontend static files (app.js/index.html/
    # style.css) has repeatedly caused stale pages after edits. Not worth
    # the tradeoff here, so just disable caching entirely.
    response = await call_next(request)
    response.headers["Cache-Control"] = "no-store"
    return response


_bus: can.BusABC | None = None
_sdo: CanopenSdoClient | None = None
_connect_error: str | None = None


def _connect():
    global _bus, _sdo, _connect_error
    try:
        _bus = can.Bus(
            interface="slcan",
            channel=CAN_PORT,
            bitrate=CAN_BITRATE,
            tty_baudrate=115200,
        )
        _sdo = CanopenSdoClient(_bus, NODE_ID)
        _connect_error = None
    except Exception as exc:  # noqa: BLE001 - surfaced to the UI, not swallowed
        _bus = None
        _sdo = None
        _connect_error = str(exc)


@app.on_event("startup")
def on_startup():
    _connect()


@app.on_event("shutdown")
def on_shutdown():
    if _bus is not None:
        _bus.shutdown()


def _require_sdo() -> CanopenSdoClient:
    if _sdo is None:
        raise HTTPException(
            status_code=503,
            detail=f"Not connected to CAN adapter on {CAN_PORT}: {_connect_error}",
        )
    return _sdo


@app.get("/api/health")
def health():
    return {
        "connected": _sdo is not None,
        "port": CAN_PORT,
        "bitrate": CAN_BITRATE,
        "node_id": NODE_ID,
        "error": _connect_error,
    }


@app.post("/api/reconnect")
def reconnect():
    if _bus is not None:
        _bus.shutdown()
    _connect()
    return health()


@app.get("/api/od")
def object_dictionary():
    return {"entries": od_as_json(), "stateNames": TIME_CIRCUITS_STATE_NAMES}


@app.get("/api/movie-dates")
def movie_dates():
    """
    Movie-accurate default date/times, read fresh from movie_dates.json on
    every call (no caching) so edits to that file take effect immediately -
    no server restart needed.
    """
    try:
        with open(MOVIE_DATES_PATH, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"{MOVIE_DATES_PATH} not found") from exc
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=500, detail=f"{MOVIE_DATES_PATH} is not valid JSON: {exc}") from exc


@app.get("/api/historical-dates")
def historical_dates():
    """
    Historically significant date/times for the Randomiser, read fresh from
    historical_dates.json on every call (no caching) so edits to that file
    take effect immediately - no server restart needed.
    """
    try:
        with open(HISTORICAL_DATES_PATH, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail=f"{HISTORICAL_DATES_PATH} not found") from exc
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=500, detail=f"{HISTORICAL_DATES_PATH} is not valid JSON: {exc}") from exc


@app.get("/api/read")
def read(index: int, subindex: int):
    sdo = _require_sdo()
    field_info = find_field(index, subindex)
    size_hint = DTYPE_SIZES[field_info[1].dtype] if field_info else None
    try:
        value = sdo.read(index, subindex, size_hint=size_hint)
    except SdoTimeout as exc:
        raise HTTPException(status_code=504, detail=str(exc)) from exc
    except SdoAbort as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc
    return {"index": index, "subindex": subindex, "value": value}


class WriteRequest(BaseModel):
    index: int
    subindex: int
    value: int


@app.post("/api/write")
def write(req: WriteRequest):
    sdo = _require_sdo()
    field_info = find_field(req.index, req.subindex)
    if field_info is None:
        raise HTTPException(status_code=404, detail="Unknown OD entry")
    entry, f = field_info
    if f.access != "rw":
        raise HTTPException(status_code=403, detail=f"{entry.name}.{f.name} is read-only")

    size = DTYPE_SIZES[f.dtype]
    try:
        sdo.write(req.index, req.subindex, req.value, size)
    except SdoTimeout as exc:
        raise HTTPException(status_code=504, detail=str(exc)) from exc
    except SdoAbort as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc
    return {"ok": True}


@app.get("/api/realtime")
def realtime(tz: str):
    """
    Current date/time for an IANA timezone (e.g. "America/New_York"),
    read from this server's own OS clock.

    This used to fetch from an external HTTP time API (timeapi.io /
    worldtimeapi.org) instead, on the theory that a local clock might not
    be trustworthy. That turned out to be backwards in practice: extensive
    live debugging (see git history / conversation log around this
    change) showed the external service returning a value consistently
    ~18-20 minutes off from multiple independent real-world references,
    while this machine's own OS clock matched those same references every
    time. The external service's unreliability couldn't be fixed or
    worked around from here. A properly NTP-synced host (systemd-timesyncd
    or chrony - what Raspberry Pi OS runs by default with network access)
    is the standard, reliable way to keep a Linux box's clock accurate;
    reinventing that via a third-party API added a failure mode without
    adding real accuracy.
    """
    try:
        local = datetime.now(ZoneInfo(tz))
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=400, detail=f"Unknown timezone {tz!r}: {exc}") from exc

    return {"source": "server-clock", "tz": tz, "iso": local.isoformat()}


# Objects polled for the live status panel: (index, subindex)
LIVE_POLL_OBJECTS = [
    (0x2100, 0),  # buttons state
    (0x2101, 0),  # current time circuits state
]


@app.websocket("/ws/live")
async def live_status(ws: WebSocket):
    await ws.accept()
    try:
        while True:
            payload = {}
            if _sdo is not None:
                for index, subindex in LIVE_POLL_OBJECTS:
                    try:
                        value = await asyncio.to_thread(_sdo.read, index, subindex)
                        payload[f"{index:#06x}:{subindex}"] = value
                    except (SdoTimeout, SdoAbort) as exc:
                        payload[f"{index:#06x}:{subindex}"] = None
                        payload.setdefault("_errors", []).append(str(exc))
            else:
                payload["_error"] = _connect_error or "not connected"
            await ws.send_json(payload)
            await asyncio.sleep(0.5)
    except WebSocketDisconnect:
        pass


app.mount("/", StaticFiles(directory=str(FRONTEND_DIR), html=True), name="frontend")
