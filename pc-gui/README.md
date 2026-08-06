# Time Circuits Control Panel

A local web GUI for controlling the Time Circuits board over CAN - either
via a USBtin (or any slcan-compatible) CAN-USB adapter from a dev machine,
or via SocketCAN (e.g. a PiCAN2 HAT) for a permanent Raspberry Pi
deployment (see [Raspberry Pi deployment](#raspberry-pi-deployment) below).

It talks to the board as a CANopen SDO client - it can read and write any
object in the OD (`CANopen/OD.h` / `OD.c` in the firmware repo), not just a
fixed set of hardcoded commands.

## Architecture

- `backend/` - a small Python (FastAPI) service that opens the CAN adapter
  with [python-can](https://python-can.readthedocs.io/) (`slcan` or
  `socketcan`, see `TC_CAN_INTERFACE` below) and speaks CANopen SDO
  (expedited transfers) to the board. Serves a JSON/WebSocket API on
  `localhost`, and also serves the frontend. `gpio_devices.py` additionally
  drives an optional relay + status LEDs on a Pi deployment, degrading to
  a no-op stand-in when there's no GPIO hardware (e.g. a dev machine).
- `frontend/` - a plain HTML/CSS/JS page (no build step) that talks to the
  backend and renders the controls. Open it via the backend's URL, not as a
  `file://` page (it needs the API).
- `deploy/` - systemd units and an install script for the Raspberry Pi
  deployment.

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

| Variable          | Default        | Meaning                                  |
|-------------------|----------------|-------------------------------------------|
| `TC_CAN_INTERFACE`| `slcan`        | `slcan` (USBtin) or `socketcan` (PiCAN2)  |
| `TC_CAN_PORT`     | `/dev/ttyACM0` | `slcan` only: serial device for the USBtin |
| `TC_CAN_CHANNEL`  | `can0`         | `socketcan` only: interface name          |
| `TC_CAN_BITRATE`  | `1000000`      | CAN bus bitrate (bps) - for `socketcan` this is informational only, the OS-level bitrate is set by `can0-up.service` |
| `TC_CAN_NODE_ID`  | `21`           | CANopen node ID of the Time Circuits board |

Example: `TC_CAN_PORT=/dev/ttyACM1 .venv/bin/uvicorn app:app --port 8420`

## Raspberry Pi deployment

For running this permanently (as a systemd service, starting on boot) on a
Raspberry Pi with a [PiCAN2](https://copperhilltech.com/pican-2-can-bus-board-for-raspberry-pi/)
HAT instead of a USBtin, plus an optional 2-channel relay for switching
power to the Time Circuits hardware (and a second device) and two status
LEDs.

**Hardware:**

| Signal | BCM GPIO | Physical pin | Notes |
|---|---|---|---|
| PiCAN2 SPI0 + INT | GPIO 8/9/10/11, 25 | - | Used by the HAT itself, not user-wired |
| Relay channel 1 (Time Circuits power) | GPIO5 | 29 | Active-high trigger by default (`TC_RELAY_ACTIVE_HIGH`) |
| Relay channel 2 (device 2 power) | GPIO6 | 31 | Same |
| LED: system alive (heartbeat) | GPIO13 | 33 | 1s on/1s off once the service is up |
| LED: CAN link | GPIO19 | 35 | On when the Time Circuits board is responding on CAN |

GPIO5/6/13/19 are adjacent on the header with ground pins nearby, chosen
to avoid everything the PiCAN2 uses. Verify the relay module's trigger
input is rated for 3.3V logic before wiring it directly to a Pi GPIO -
some relay boards expect 5V (Arduino-style) triggering and need a
buffer/level-shifter instead.

**Software**, run on the Pi itself:

```
git clone <this repo> ~/BTTF-Time-Circuits-Software
cd ~/BTTF-Time-Circuits-Software
sudo pc-gui/deploy/install.sh
sudo reboot   # only needed the first time, to activate the PiCAN2 overlay
```

`install.sh` installs everything via `apt` (no venv - avoids Debian's
PEP 668 externally-managed-environment restriction and keeps the systemd
unit simple), enables SPI, adds the `dtoverlay=mcp2515-can0,...` line to
`/boot/firmware/config.txt` if it isn't already there, and installs +
enables two systemd units:

- `can0-up.service` - brings `can0` up at 1 Mbit/s as soon as the PiCAN2's
  network device appears (`sys-subsystem-net-devices-can0.device`), with
  automatic bus-off recovery.
- `timecircuits-gui.service` - runs the backend with `TC_CAN_INTERFACE=socketcan`.
  Not gated on `can0-up.service` actually succeeding - the backend's own
  background health monitor retries the CAN connection on its own, so the
  web UI (and relay control) comes up regardless.

Re-running `install.sh` is safe (idempotent) if you change the deploy
files. After editing a `.service`/`.service.in` file, re-run it and then
`sudo systemctl restart timecircuits-gui.service` (or `can0-up.service`).

Oscillator frequency in the overlay (`oscillator=16000000`) matches a
16MHz PiCAN2 - if CAN never comes up and everything else checks out
(`ip link show can0` shows `UP`, `dmesg | grep mcp251x` shows no errors),
this is the first thing to check against your board's actual crystal.

## What the panel controls

- **Live Status**: the current time circuits state and the four physical
  switches, updated in real time over a WebSocket.
- **Power**: toggles the relay board's two power outlets (see
  [Raspberry Pi deployment](#raspberry-pi-deployment) above). Talks
  straight to `/api/relay`, not the CANopen OD - this is Pi-local
  infrastructure, unrelated to the STM32's object dictionary. Works the
  same with no relay wired up (a dev machine, or before the Pi's GPIO is
  connected) - the toggles just have no physical effect.
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
