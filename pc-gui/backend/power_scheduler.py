"""
Day-of-week + time-of-day scheduler for the relay-controlled Time Circuits
power outlet (relay channel 1). Entries persist to power_schedule.json,
read/written fresh on every call - same "edit takes effect immediately,
no restart needed" pattern as movie_dates.json/historical_dates.json.

A background loop (run_scheduler_loop, started from app.py) checks once a
minute and fires the relay when an enabled entry's day+time matches now,
using the server's own local clock (see the "Sync to Real Time" feature
elsewhere in this app for why - the server's OS clock, NTP-synced, is the
reliable source of truth, not any client's).
"""

import asyncio
import json
import threading
from datetime import datetime
from pathlib import Path

from gpio_devices import relay

DAYS = ("mon", "tue", "wed", "thu", "fri", "sat", "sun")
RELAY_CHANNEL = 1  # Time Circuits Power - the only channel the scheduler controls

SCHEDULE_PATH = Path(__file__).resolve().parent / "power_schedule.json"

_lock = threading.Lock()


def _load() -> dict:
    if not SCHEDULE_PATH.exists():
        return {"entries": []}
    with open(SCHEDULE_PATH, encoding="utf-8") as f:
        return json.load(f)


def _save(data: dict) -> None:
    with open(SCHEDULE_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def _validate(days: list[str], time_str: str, action: str) -> None:
    if not days or any(d not in DAYS for d in days):
        raise ValueError(f"days must be a non-empty subset of {DAYS}")
    try:
        datetime.strptime(time_str, "%H:%M")
    except ValueError as exc:
        raise ValueError('time must be "HH:MM"') from exc
    if action not in ("on", "off"):
        raise ValueError('action must be "on" or "off"')


def list_entries() -> list[dict]:
    with _lock:
        return _load()["entries"]


def add_entry(days: list[str], time_str: str, action: str, enabled: bool = True) -> dict:
    _validate(days, time_str, action)
    with _lock:
        data = _load()
        next_id = max((e["id"] for e in data["entries"]), default=0) + 1
        entry = {"id": next_id, "days": list(days), "time": time_str, "action": action, "enabled": enabled}
        data["entries"].append(entry)
        _save(data)
        return entry


def update_entry(entry_id: int, **fields) -> dict | None:
    """Partial update - only overwrites keys present in `fields`."""
    with _lock:
        data = _load()
        for entry in data["entries"]:
            if entry["id"] != entry_id:
                continue
            merged = {**entry, **fields}
            _validate(merged["days"], merged["time"], merged["action"])
            entry.update(fields)
            _save(data)
            return entry
        return None


def delete_entry(entry_id: int) -> bool:
    with _lock:
        data = _load()
        before = len(data["entries"])
        data["entries"] = [e for e in data["entries"] if e["id"] != entry_id]
        _save(data)
        return len(data["entries"]) < before


async def run_scheduler_loop():
    """
    Runs forever. Sleeps until the top of each minute, then checks every
    enabled entry once. A last-fired-minute guard (keyed on the full
    timestamp, not just HH:MM) means a slow tick or a backwards clock
    step can't cause the same entry to fire twice or fire stale.
    """
    last_checked_minute = None
    while True:
        now = datetime.now()
        await asyncio.sleep(max(1, 60 - now.second))
        now = datetime.now()
        minute_key = now.strftime("%Y-%m-%d %H:%M")
        if minute_key == last_checked_minute:
            continue
        last_checked_minute = minute_key

        day = DAYS[now.weekday()]
        hhmm = now.strftime("%H:%M")
        for entry in list_entries():
            if entry["enabled"] and day in entry["days"] and entry["time"] == hhmm:
                relay.set(RELAY_CHANNEL, entry["action"] == "on")
