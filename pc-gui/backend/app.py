"""
Local backend for the Time Circuits web control panel.

Talks to the Time Circuits CANopen node (node ID 21 by default) over CAN,
and exposes it to the browser frontend over a small HTTP/WebSocket API.
Two CAN interfaces are supported: a USBtin (or any slcan-compatible)
USB-CAN adapter for dev-machine use (the default), or SocketCAN for a
CAN HAT like the PiCAN2 on a Raspberry Pi deployment.

Run with:
    .venv/bin/uvicorn app:app --host 127.0.0.1 --port 8420

Config via environment variables:
    TC_CAN_INTERFACE  "slcan" (default) or "socketcan"
    TC_CAN_PORT       slcan only: serial device for the adapter (default /dev/ttyACM0)
    TC_CAN_CHANNEL    socketcan only: interface name (default can0)
    TC_CAN_BITRATE    CAN bus bitrate in bps (default 1000000, matches can.c) -
                       for socketcan this must already be set on the OS side
                       (see pc-gui/deploy/can0-up.service), python-can doesn't
                       set it for socketcan interfaces
    TC_CAN_NODE_ID    CANopen node ID of the Time Circuits board (default 21)
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

import power_scheduler
from canopen_sdo import CanopenSdoClient, SdoAbort, SdoTimeout
from gpio_devices import relay, status_leds
from object_dictionary import DTYPE_SIZES, find_field, od_as_json, TIME_CIRCUITS_STATE_NAMES

CAN_INTERFACE = os.environ.get("TC_CAN_INTERFACE", "slcan")
CAN_PORT = os.environ.get("TC_CAN_PORT", "/dev/ttyACM0")
CAN_CHANNEL = os.environ.get("TC_CAN_CHANNEL", "can0")
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
        if CAN_INTERFACE == "socketcan":
            # Bitrate isn't a python-can/socketcan Bus() argument - it's set
            # on the OS side when the interface is brought up (see
            # pc-gui/deploy/can0-up.service), same as any other socketcan
            # device.
            _bus = can.Bus(interface="socketcan", channel=CAN_CHANNEL)
        else:
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


_health_monitor_task: asyncio.Task | None = None
_scheduler_task: asyncio.Task | None = None


async def _health_monitor_loop():
    """
    Runs independently of any connected browser (unlike the frontend's own
    5s health polling), so the CAN-link LED and reconnect-on-drop behavior
    work on a headless Pi deployment with nobody watching the page.

    Does a real SDO read rather than just checking `_sdo is not None` -
    the underlying device (a USB serial adapter, a SocketCAN interface)
    can disappear without the python-can Bus object itself immediately
    erroring, so object existence alone isn't a reliable liveness signal.
    """
    while True:
        try:
            if _sdo is None:
                _connect()
            if _sdo is not None:
                await asyncio.to_thread(_sdo.read, 0x2100, 0)  # cheap 1-byte read
                status_leds.set_can_link(True)
            else:
                status_leds.set_can_link(False)
        except Exception:  # noqa: BLE001 - this loop must never die
            status_leds.set_can_link(False)
        await asyncio.sleep(3)


@app.on_event("startup")
async def on_startup():
    global _health_monitor_task, _scheduler_task
    _connect()
    status_leds.start()
    _health_monitor_task = asyncio.create_task(_health_monitor_loop())
    _scheduler_task = asyncio.create_task(power_scheduler.run_scheduler_loop())


@app.on_event("shutdown")
async def on_shutdown():
    if _health_monitor_task is not None:
        _health_monitor_task.cancel()
    if _scheduler_task is not None:
        _scheduler_task.cancel()
    if _bus is not None:
        _bus.shutdown()


_CAN_CHANNEL_DESC = CAN_CHANNEL if CAN_INTERFACE == "socketcan" else CAN_PORT


def _require_sdo() -> CanopenSdoClient:
    if _sdo is None:
        raise HTTPException(
            status_code=503,
            detail=f"Not connected to CAN adapter on {_CAN_CHANNEL_DESC}: {_connect_error}",
        )
    return _sdo


@app.get("/api/health")
def health():
    return {
        "connected": _sdo is not None,
        "interface": CAN_INTERFACE,
        "port": _CAN_CHANNEL_DESC,
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


RELAY_CHANNEL_NAMES = {1: "Time Circuits Power", 2: "Device 2 Power"}


@app.get("/api/relay")
def relay_state():
    state = relay.state()
    return {ch: {"name": RELAY_CHANNEL_NAMES.get(ch, f"Channel {ch}"), "on": on} for ch, on in state.items()}


class RelayRequest(BaseModel):
    on: bool


@app.post("/api/relay/{channel}")
def relay_set(channel: int, req: RelayRequest):
    try:
        relay.set(channel, req.on)
    except ValueError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return relay_state()


class ScheduleEntryRequest(BaseModel):
    days: list[str]
    time: str
    action: str
    enabled: bool = True


class ScheduleEntryUpdate(BaseModel):
    days: list[str] | None = None
    time: str | None = None
    action: str | None = None
    enabled: bool | None = None


@app.get("/api/schedule")
def schedule_list():
    return power_scheduler.list_entries()


@app.post("/api/schedule")
def schedule_add(req: ScheduleEntryRequest):
    try:
        return power_scheduler.add_entry(req.days, req.time, req.action, req.enabled)
    except ValueError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc


@app.patch("/api/schedule/{entry_id}")
def schedule_update(entry_id: int, req: ScheduleEntryUpdate):
    fields = {k: v for k, v in req.model_dump().items() if v is not None}
    try:
        entry = power_scheduler.update_entry(entry_id, **fields)
    except ValueError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc
    if entry is None:
        raise HTTPException(status_code=404, detail="No such schedule entry")
    return entry


@app.delete("/api/schedule/{entry_id}")
def schedule_delete(entry_id: int):
    if not power_scheduler.delete_entry(entry_id):
        raise HTTPException(status_code=404, detail="No such schedule entry")
    return {"ok": True}


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
