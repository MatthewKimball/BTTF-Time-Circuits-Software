# Time Circuits Control Panel

A local web GUI for controlling the Time Circuits board over CAN, via a
USBtin (or any slcan-compatible) CAN-USB adapter.

It talks to the board as a CANopen SDO client - it can read and write any
object in the OD (`CANopen/OD.h` / `OD.c` in the firmware repo), not just a
fixed set of hardcoded commands.

## Architecture

- `backend/` - a small Python (FastAPI) service that opens the CAN adapter
  with [python-can](https://python-can.readthedocs.io/)'s `slcan` interface
  and speaks CANopen SDO (expedited transfers) to the board. Serves a JSON/
  WebSocket API on `localhost`, and also serves the frontend.
- `frontend/` - a plain HTML/CSS/JS page (no build step) that talks to the
  backend and renders the controls. Open it via the backend's URL, not as a
  `file://` page (it needs the API).

## One-time setup

The board's default CANopen node ID is **21**, and `Core/Src/can.c` is
configured for a **1 Mbit/s** bus - both are already the defaults below.

1. Give your user account access to the adapter's serial port (one-time,
   requires logging out/in - or run `newgrp dialout` in your current shell
   for it to take effect immediately without logging out):

   ```
   sudo usermod -aG dialout $USER
   ```

2. Create a virtualenv and install dependencies:

   ```
   cd pc-gui/backend
   python3 -m venv .venv
   .venv/bin/pip install -r requirements.txt
   ```

## Running

```
cd pc-gui/backend
.venv/bin/uvicorn app:app --host 127.0.0.1 --port 8420
```

Then open http://127.0.0.1:8420 in a browser.

Config via environment variables (defaults match the firmware):

| Variable         | Default        | Meaning                                  |
|------------------|----------------|-------------------------------------------|
| `TC_CAN_PORT`    | `/dev/ttyACM0` | Serial device for the USBtin              |
| `TC_CAN_BITRATE` | `1000000`      | CAN bus bitrate (bps)                     |
| `TC_CAN_NODE_ID` | `21`           | CANopen node ID of the Time Circuits board |

Example: `TC_CAN_PORT=/dev/ttyACM1 .venv/bin/uvicorn app:app --port 8420`

## What the panel controls

- **Live Status**: the current time circuits state and the four physical
  switches, updated in real time over a WebSocket.
- **Time Circuits State** (0x2102): request Idle/Armed/Travel/Complete. The
  firmware validates the transition (e.g. Armed requires a valid destination
  date) and silently ignores an invalid request rather than erroring.
- **Movie-Accurate Defaults**: resets destination, present, and last-
  departed time to movie-accurate dates and applies them to the displays
  and RTC in one click. Backed by `backend/movie_dates.json`, which is
  read fresh on every request - edit it to change the dates, no restart
  needed.
- **Randomiser**: sets just the destination time to a random historically
  significant moment, backed by `backend/historical_dates.json` (same
  "edit freely, no restart needed" behavior).
- **Date/Time** (0x2000/0x2001/0x2002): destination, present, and last-
  departed time records, each with **Read** / **Write** / **Update**
  buttons - Read pulls the current values from the board, Write pushes the
  edited fields into the OD, and Update does a Write followed by the
  matching firmware trigger (redraw + validate + save to SD). The present
  time card also has **Sync to Timezone** (pick any zone and sync to it)
  and **Sync to My Timezone** (uses the browser's detected zone) - both
  fetch the current time from the server's own OS clock (not the browser's)
  and apply it right as the clock's seconds roll over to `:00`.
- **Function Control** (0x2200): one-shot actions - clear/update/set
  displays, update destination date, update RTC from the present-time
  record, save dates to the SD card. Each button just sets a bit; the
  firmware acts on it and clears it automatically within one control loop
  iteration.
- **Settings** (0x2300), grouped into Glitch / Sound Effects / IMU:
  glitch enable, mute colon tick, and mute all are live switches (applied
  immediately). Glitch period and IMU any-motion threshold/duration are
  edited then applied via their own one-shot "Apply" bit, matching how the
  firmware only re-reads those values when told to. Note the physical Mute
  switch only silences the colon tick - "Mute All" here is the only way to
  mute everything.
- **Advanced: Raw OD Access**: generic index:subindex SDO read/write, for
  anything not covered by the named panels above.

## Notes

- SDO requests time out after 1s if the board doesn't respond (wrong node
  ID, bus not connected, board unpowered, etc.) - shown as an error in the
  panel's log rather than hanging. Separately, every browser-side `fetch()`
  has its own 8s timeout, so a stale/dead connection between the browser
  and this backend (e.g. over a tunneled dev setup) surfaces as a clear
  error instead of hanging a button forever.
- Only expedited SDO transfers (<=4 bytes) are implemented, since every
  object in this OD fits that. If a future OD entry needs more than 4 bytes,
  `backend/canopen_sdo.py` will need segmented transfer support added.
