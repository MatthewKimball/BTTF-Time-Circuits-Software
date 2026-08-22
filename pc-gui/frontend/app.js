const API = "";

let OD = null; // { entries: [...], stateNames: {...} }

// Plain fetch() never times out - if the underlying connection goes dead
// (e.g. an idle tunnel/proxy silently dropping it, which this dev setup is
// commonly accessed through), the browser can hang on it indefinitely rather
// than erroring, since it hasn't noticed the connection is gone yet. Without
// this, an awaited fetch that hangs forever means whatever code was waiting
// on it - a button's disabled state, a sync-in-progress flag - never reaches
// its finally block either, so it stays stuck until the page is fully
// reloaded (which throws away the dead connection and opens a new one).
// Wrapping every fetch with a bounded timeout means a dead connection always
// surfaces as a clear, recoverable error instead.
const FETCH_TIMEOUT_MS = 8000;

async function fetchWithTimeout(url, options = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } catch (err) {
    if (err.name === "AbortError") {
      throw new Error(`request timed out after ${FETCH_TIMEOUT_MS / 1000}s (connection may be stale - try reloading the page)`);
    }
    throw err;
  } finally {
    clearTimeout(timer);
  }
}

function log(msg, level = "info") {
  const el = document.getElementById("log");
  const time = new Date().toLocaleTimeString();
  const prefix = level === "error" ? "ERROR" : level === "warn" ? "WARN" : "OK";
  el.textContent = `[${time}] ${prefix}: ${msg}\n` + el.textContent;
}

async function apiRead(index, subindex) {
  const res = await fetchWithTimeout(`${API}/api/read?index=${index}&subindex=${subindex}`, { cache: "no-store" });
  if (!res.ok) {
    const body = await res.json().catch(() => ({}));
    throw new Error(body.detail || `read ${index.toString(16)}:${subindex} failed (${res.status})`);
  }
  const body = await res.json();
  return body.value;
}

async function apiWrite(index, subindex, value) {
  const res = await fetchWithTimeout(`${API}/api/write`, {
    method: "POST",
    cache: "no-store",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ index, subindex, value }),
  });
  if (!res.ok) {
    const body = await res.json().catch(() => ({}));
    throw new Error(body.detail || `write ${index.toString(16)}:${subindex} failed (${res.status})`);
  }
  return true;
}

async function setBit(index, subindex, mask, on, label) {
  try {
    const current = await apiRead(index, subindex);
    const next = on ? (current | mask) : (current & ~mask & 0xff);
    await apiWrite(index, subindex, next);
    log(`${label}: ${on ? "on" : "off"}`);
  } catch (err) {
    log(`${label}: ${err.message}`, "error");
  }
}

async function pulseBit(index, subindex, mask, label) {
  try {
    const current = await apiRead(index, subindex);
    await apiWrite(index, subindex, current | mask);
    log(`${label}: sent`);
  } catch (err) {
    log(`${label}: ${err.message}`, "error");
  }
}

// ---------- Connection status ----------

async function refreshHealth() {
  const dot = document.getElementById("conn-dot");
  const text = document.getElementById("conn-text");
  try {
    const res = await fetchWithTimeout(`${API}/api/health`, { cache: "no-store" });
    const body = await res.json();
    if (body.connected) {
      dot.className = "dot ok";
      text.textContent = `connected: ${body.port} @ ${body.bitrate} bps, node ${body.node_id}`;
    } else {
      dot.className = "dot";
      text.textContent = `not connected: ${body.error || "unknown error"}`;
    }
  } catch (err) {
    dot.className = "dot";
    text.textContent = `backend unreachable: ${err.message}`;
  }
}

document.getElementById("reconnect-btn").addEventListener("click", async () => {
  document.getElementById("conn-text").textContent = "reconnecting…";
  try {
    await fetchWithTimeout(`${API}/api/reconnect`, { method: "POST" });
  } catch (err) {
    log(`reconnect failed: ${err.message}`, "error");
  }
  await refreshHealth();
});

// ---------- Live status (WebSocket) ----------

function connectLiveSocket() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const ws = new WebSocket(`${proto}://${location.host}/ws/live`);

  ws.onmessage = (ev) => {
    const data = JSON.parse(ev.data);
    const state = data["0x2101:0"];
    const buttons = data["0x2100:0"];

    const stateDisplay = document.getElementById("state-display");
    if (state !== null && state !== undefined && OD) {
      stateDisplay.textContent = OD.stateNames[state] ?? `#${state}`;
    } else {
      stateDisplay.textContent = "—";
    }

    if (buttons !== null && buttons !== undefined) {
      const switchMasks = { glitch: 1, keypadEnter: 2, mute: 4, timeTravel: 8 };
      for (const [name, mask] of Object.entries(switchMasks)) {
        const el = document.querySelector(`.switch[data-name="${name}"]`);
        if (el) el.classList.toggle("on", (buttons & mask) !== 0);
      }
    }
  };

  ws.onclose = () => {
    setTimeout(connectLiveSocket, 2000);
  };
  ws.onerror = () => ws.close();
}

// ---------- State control ----------

document.querySelectorAll("#state-control-card button[data-state]").forEach((btn) => {
  btn.addEventListener("click", async () => {
    const value = parseInt(btn.dataset.state, 10);
    try {
      await apiWrite(0x2102, 0, value);
      log(`requested state: ${btn.textContent}`);
    } catch (err) {
      log(`request state failed: ${err.message}`, "error");
    }
  });
});

// ---------- Date/time cards (built from OD metadata) ----------

const DATETIME_ENTRIES = [0x2000, 0x2001, 0x2002];

const TZ_GROUPS = {
  "North America": [
    "America/New_York", "America/Chicago", "America/Denver", "America/Los_Angeles", "America/Anchorage",
  ],
  "Europe": [
    "Europe/London", "Europe/Dublin", "Europe/Paris", "Europe/Berlin", "Europe/Madrid",
    "Europe/Rome", "Europe/Amsterdam", "Europe/Warsaw", "Europe/Athens", "Europe/Moscow",
  ],
  "Asia / Pacific": [
    "Asia/Dubai", "Asia/Kolkata", "Asia/Shanghai", "Asia/Tokyo", "Asia/Seoul",
    "Asia/Singapore", "Australia/Sydney", "Australia/Brisbane", "Australia/Perth", "Pacific/Auckland",
  ],
  "Other": ["UTC"],
};

const TZ_OPTIONS_HTML = Object.entries(TZ_GROUPS)
  .map(
    ([group, zones]) =>
      `<optgroup label="${group}">` +
      zones.map((z) => `<option value="${z}" ${z === "America/New_York" ? "selected" : ""}>${z}</option>`).join("") +
      `</optgroup>`
  )
  .join("");

function buildDateTimeCards() {
  const grid = document.getElementById("datetime-grid");
  grid.innerHTML = "";

  for (const index of DATETIME_ENTRIES) {
    const entry = OD.entries.find((e) => e.index === index);
    if (!entry) continue;

    const card = document.createElement("section");
    card.className = "card";
    card.innerHTML = `<h2>${entry.label}</h2><div class="dt-fields"></div><div class="field-actions"></div>`;
    const fieldsEl = card.querySelector(".dt-fields");
    const actionsEl = card.querySelector(".field-actions");

    for (const f of entry.fields) {
      const label = document.createElement("label");
      if (f.name === "meridian") {
        label.innerHTML = `${f.label}
          <select data-subindex="${f.subindex}">
            <option value="1">AM</option>
            <option value="2">PM</option>
          </select>`;
      } else {
        label.innerHTML = `${f.label}
          <input type="number" data-subindex="${f.subindex}" min="${f.min ?? ""}" max="${f.max ?? ""}">`;
      }
      fieldsEl.appendChild(label);
    }

    const readBtn = document.createElement("button");
    readBtn.className = "secondary";
    readBtn.textContent = "Read";
    readBtn.addEventListener("click", async () => {
      try {
        for (const f of entry.fields) {
          const value = await apiRead(index, f.subindex);
          const input = fieldsEl.querySelector(`[data-subindex="${f.subindex}"]`);
          if (input) input.value = value;
        }
        log(`${entry.label}: read from device`);
      } catch (err) {
        log(`${entry.label} read failed: ${err.message}`, "error");
      }
    });

    // Writes every field's current input value to the device. Returns the
    // values written, since callers that then trigger an apply action need
    // to wait for this to actually finish (rather than firing writes and
    // the apply trigger concurrently, which could race on the CAN bus).
    async function writeAllFields() {
      const values = {};
      for (const f of entry.fields) {
        const input = fieldsEl.querySelector(`[data-subindex="${f.subindex}"]`);
        const value = parseInt(input.value, 10);
        if (Number.isNaN(value)) throw new Error(`${f.label} is not a number`);
        await apiWrite(index, f.subindex, value);
        values[f.name] = value;
      }
      return values;
    }

    const writeBtn = document.createElement("button");
    writeBtn.textContent = "Write";
    writeBtn.addEventListener("click", async () => {
      try {
        await writeAllFields();
        log(`${entry.label}: written to device`);
      } catch (err) {
        log(`${entry.label} write failed: ${err.message}`, "error");
      }
    });

    actionsEl.appendChild(readBtn);
    actionsEl.appendChild(writeBtn);

    if (index === 0x2000) {
      const applyBtn = document.createElement("button");
      applyBtn.textContent = "Update";
      applyBtn.title = "Write fields, then trigger Update Destination Date";
      applyBtn.addEventListener("click", async () => {
        try {
          await writeAllFields();
          await pulseBit(0x2200, 0, 1 << 3, "Update Destination Date");
        } catch (err) {
          log(`${entry.label} update failed: ${err.message}`, "error");
        }
      });
      actionsEl.appendChild(applyBtn);
    }

    if (index === 0x2001) {
      const applyBtn = document.createElement("button");
      applyBtn.textContent = "Update";
      applyBtn.title = "Write the fields above, then apply them to the display and RTC";
      applyBtn.addEventListener("click", async () => {
        try {
          await writeAllFields();
          await applyAllDisplaysFromOd();
          log(`${entry.label}: written and applied to display/RTC`);
        } catch (err) {
          log(`${entry.label} update failed: ${err.message}`, "error");
        }
      });
      actionsEl.appendChild(applyBtn);

      const syncGroup = document.createElement("div");
      syncGroup.className = "settings-group sync-group";
      syncGroup.innerHTML = `<h3>Sync to Timezone</h3>
        <div class="numeric-row">
          <span>Timezone</span>
          <select id="realtime-tz-select">${TZ_OPTIONS_HTML}</select>
        </div>`;
      const syncBtn = document.createElement("button");
      syncBtn.textContent = "Sync";
      syncBtn.title = "Fetch the current time for the selected timezone and apply it exactly on the next :00 mark";
      syncGroup.appendChild(syncBtn);
      card.appendChild(syncGroup);

      const localGroup = document.createElement("div");
      localGroup.className = "settings-group sync-group";
      localGroup.innerHTML = `<h3>Sync to My Timezone</h3>`;
      const detectBtn = document.createElement("button");
      detectBtn.textContent = "Sync";
      detectBtn.title = "Detect this browser's local timezone and sync to it exactly on the next :00 mark";
      localGroup.appendChild(detectBtn);
      card.appendChild(localGroup);

      const statusEl = document.createElement("div");
      statusEl.className = "hint";
      statusEl.style.marginTop = "0.5rem";
      card.appendChild(statusEl);

      syncBtn.addEventListener("click", () => {
        const tzSelect = document.getElementById("realtime-tz-select");
        syncPresentTimeToReal(fieldsEl, statusEl, syncBtn, tzSelect.value);
      });

      detectBtn.addEventListener("click", () => {
        const tz = Intl.DateTimeFormat().resolvedOptions().timeZone;
        const tzSelect = document.getElementById("realtime-tz-select");
        // Only updates the dropdown if the detected zone happens to be one
        // of the listed options - the sync itself always uses the exact
        // detected value regardless, shown in the status line below.
        tzSelect.value = tz;
        syncPresentTimeToReal(fieldsEl, statusEl, detectBtn, tz);
      });
    }

    if (index === 0x2002) {
      const applyBtn = document.createElement("button");
      applyBtn.textContent = "Update";
      applyBtn.title = "Write the fields above, then apply them to the displays " +
        "(the firmware has no last-departed-only trigger, so this refreshes all three displays)";
      applyBtn.addEventListener("click", async () => {
        try {
          await writeAllFields();
          await applyAllDisplaysFromOd();
          log(`${entry.label}: written and applied to displays`);
        } catch (err) {
          log(`${entry.label} update failed: ${err.message}`, "error");
        }
      });
      actionsEl.appendChild(applyBtn);
    }

    grid.appendChild(card);
  }
}

// Applies whatever is currently in the OD's destination/present/last-
// departed records to all three physical displays via two separate writes:
// SET_ALL_DISPLAYS (bit2, copies the OD records into the firmware's working
// config) then UPDATE_ALL_DISPLAYS (bit1, validates and redraws). Each is a
// read-modify-write (read the current byte, OR in the bit, write it back) -
// NOT a blind overwrite of the whole byte. The firmware's control loop only
// runs every ~20ms, and a round trip to the board is often faster than
// that, so the SET write frequently hasn't been processed yet by the time
// the UPDATE write is sent (confirmed live: reading the byte back
// immediately after the SET write showed it still pending about 40% of the
// time in testing). A blind overwrite for the second write would silently
// erase that still-pending SET bit before the firmware ever saw it -
// which read back correctly from the OD (since the write itself always
// succeeds) but never actually reached the display. Reading-before-writing
// means the two bits just end up combined in that case, and the firmware
// processes SET before UPDATE within the same control-loop iteration, so
// either arrival order (same tick or different) ends up applying cleanly.
//
// Used for present time too (not just a UPDATE_RTC-only apply) because the
// once-a-minute present-time refresh in
// timeCircuit_control_updatePresentDateTime() only redraws the physical
// display when the raw RTC minute value (0-59) differs from the last one
// seen - if a synced time's minute happens to coincide with whatever was
// last cached, the display silently never redraws even though the RTC
// chip is correctly updated. UPDATE_ALL_DISPLAYS redraws unconditionally.
// Also note the firmware only redraws if ALL THREE records currently hold
// a valid date - if destination or last-departed is somehow invalid, this
// silently no-ops.
async function applyAllDisplaysFromOd() {
  const fc1 = await apiRead(0x2200, 0);
  await apiWrite(0x2200, 0, fc1 | (1 << 2)); // SET_ALL_DISPLAYS

  const fc2 = await apiRead(0x2200, 0);
  await apiWrite(0x2200, 0, fc2 | (1 << 1)); // UPDATE_ALL_DISPLAYS
}

// ---------- Sync present time to real time ----------

// Matches the 24h->12h conversion in App/Time_Circuits/datetime_display.c's
// dateTime_setDateTimeHour(): 0 -> 12 AM, 1-11 -> AM, 12 -> 12 PM, 13-23 -> PM.
function to12Hour(hour24) {
  const meridian = hour24 < 12 ? 1 : 2; // 1=AM, 2=PM
  let hour12 = hour24 % 12;
  if (hour12 === 0) hour12 = 12;
  return { hour: hour12, meridian };
}

// Fetches the current time for a zone and parses the response's OWN
// year/month/day/hour/minute/second digits directly out of the ISO string
// (the backend already computed correct wall-clock fields for the target
// zone). Deliberately does NOT go through `new Date(...)` + .getHours() -
// a JS Date is just an absolute instant with no zone attached, and reading
// fields back out of one uses the BROWSER'S OWN system timezone, which may
// not match the requested zone at all.
async function fetchTimeParts(tz) {
  // Defense in depth against caching, even though a live diagnostic capture
  // showed a ~19 minute stale value arrive in a fast (393ms) round trip -
  // meaning the staleness was baked into the response itself, not caused by
  // this request being served from a browser/proxy cache. Left in anyway:
  // it's cheap, correct, and rules the caching class of bug out entirely.
  const res = await fetchWithTimeout(`/api/realtime?tz=${encodeURIComponent(tz)}&_=${Date.now()}`, { cache: "no-store" });
  if (!res.ok) throw new Error(`realtime lookup failed (${res.status})`);
  const body = await res.json();
  const m = body.iso.match(/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})/);
  if (!m) throw new Error(`unexpected time format from server: ${body.iso}`);
  return {
    year: parseInt(m[1], 10),
    month: parseInt(m[2], 10),
    day: parseInt(m[3], 10),
    hour: parseInt(m[4], 10),
    minute: parseInt(m[5], 10),
    second: parseInt(m[6], 10),
    source: body.source,
    error: body.error,
    // Date.parse() on the FULL iso string (with its explicit numeric UTC
    // offset) correctly resolves to an absolute instant - safe to use here
    // purely as an epoch value for a plausibility check below. This is a
    // different use than reading .getHours()/.getMinutes() back out of a
    // Date object, which re-interprets in the browser's own local zone and
    // was the earlier (real, but distinct) bug.
    epochMs: Date.parse(body.iso),
  };
}

// Fetches the current time for a zone. Deliberately does NOT cross-check
// the result against this browser's own clock: an earlier version did,
// and it turned out to reject perfectly correct fetched data whenever the
// machine running the browser had its own clock wrong (confirmed in this
// project's dev sandbox, whose system clock was independently found to be
// running ~18 minutes fast via the plain OS `date` command). The entire
// point of fetching online time is to work even when a local clock can't
// be trusted - the backend does its own (much more lenient) sanity check
// server-side instead, since the server is the actual source of truth in
// the target deployment (a Pi serving multiple browsers, none of whose
// clocks should be trusted as a reference). Logging is kept for
// visibility into what was actually fetched.
async function fetchPlausibleTimeParts(tz, statusEl) {
  statusEl.textContent = "Fetching accurate time…";

  const fetchStartedAt = new Date();
  const target = await fetchTimeParts(tz);
  const fetchFinishedAt = new Date();

  log(
    `Real-time sync fetch: requested tz=${tz} at browser-local ${fetchStartedAt.toISOString()} `
    + `- got back {source: ${target.source}, year: ${target.year}, month: ${target.month}, `
    + `day: ${target.day}, hour: ${target.hour}, minute: ${target.minute}, second: ${target.second}} `
    + `- response received at ${fetchFinishedAt.toISOString()} (round trip ${fetchFinishedAt - fetchStartedAt}ms)`
  );
  console.log("[TimeCircuits sync diagnostic]", {
    tz,
    fetchStartedAt: fetchStartedAt.toISOString(),
    fetchFinishedAt: fetchFinishedAt.toISOString(),
    roundTripMs: fetchFinishedAt - fetchStartedAt,
    target,
  });

  return target;
}

let realTimeSyncActive = false;
let realTimeSyncArmedButton = null; // whichever button actually armed the pending sync

async function syncPresentTimeToReal(fieldsEl, statusEl, button, tz) {
  if (realTimeSyncActive) {
    // Second click while armed = cancel, regardless of which of the two
    // sync buttons was clicked - reset whichever one actually armed it.
    realTimeSyncActive = false;
    statusEl.textContent = "Cancelled.";
    if (realTimeSyncArmedButton) realTimeSyncArmedButton.textContent = realTimeSyncArmedButton.dataset.idleLabel;
    realTimeSyncArmedButton = null;
    return;
  }

  realTimeSyncActive = true;
  realTimeSyncArmedButton = button;
  button.dataset.idleLabel = button.textContent;
  button.textContent = "Cancel Sync";
  statusEl.textContent = "Checking server clock…";

  try {
    // Wait until roughly the top of a minute - calibrated against the
    // SERVER's own clock, not this device's. An earlier version waited
    // for THIS device's local clock to hit :00, which is fine for a
    // device with an accurate clock, but a real phone was confirmed
    // (2026-08-22) running ~2s fast: it fired its local :00 trigger while
    // the server's clock was still 2s into the previous minute, silently
    // writing a value one minute behind what the phone's own (fast) clock
    // showed a moment later. A laptop synced at the same time, with an
    // accurate clock, showed no such gap. Calibrating the wait duration
    // against one cheap initial fetch of the server's clock - then using
    // the device's clock only to measure the RELATIVE/monotonic elapsed
    // time via performance.now(), never its absolute reading - makes this
    // immune to however wrong a given device's own clock is.
    statusEl.textContent = "Waiting for :00… (calibrating to server clock)";
    const calibrationStartedAt = performance.now();
    const calibration = await fetchTimeParts(tz);
    const calibrationRoundTripMs = performance.now() - calibrationStartedAt;
    log(
      `Real-time sync calibration: requested tz=${tz}, server reported second=${calibration.second} `
      + `(round trip ${calibrationRoundTripMs.toFixed(0)}ms)`
    );

    // The server's stated second value was true roughly half the round
    // trip ago, assuming roughly symmetric request/response latency.
    // "% 60" folds second=0 (we're already basically at a boundary) down
    // to a 0ms wait instead of a full extra minute.
    const secondsUntilBoundary = (60 - calibration.second) % 60;
    const msUntilBoundary = secondsUntilBoundary * 1000 - calibrationRoundTripMs / 2;
    const targetMonotonicMs = performance.now() + Math.max(0, msUntilBoundary);

    // This is deliberately NOT "fetch once, then trust an extrapolated
    // offset for up to 60s while waiting": if the tab gets throttled in
    // the background, or the machine sleeps, during that wait, an
    // extrapolated offset goes stale and silently writes the wrong time -
    // which is exactly the class of bug that caused this to be 18-30
    // minutes off for real users. Here, a throttled/late-firing wait just
    // means the final fetch below (the one actually used) happens later
    // than intended, still with a fresh, correct value - never a stale
    // extrapolation.
    await new Promise((resolve, reject) => {
      const poll = () => {
        if (!realTimeSyncActive) {
          reject(new Error("cancelled"));
          return;
        }
        const remainingMs = targetMonotonicMs - performance.now();
        statusEl.textContent = `Waiting for :00… (~${Math.max(0, Math.round(remainingMs / 1000))}s, calibrated to server clock)`;
        if (remainingMs <= 0) {
          resolve();
          return;
        }
        setTimeout(poll, 100);
      };
      poll();
    });

    // NOW do the one authoritative, freshly-fetched lookup - right at the
    // (server-calibrated) trigger moment, not up to a minute earlier.
    const target = await fetchPlausibleTimeParts(tz, statusEl);

    const { hour, meridian } = to12Hour(target.hour);
    const fields = {
      day: target.day,
      month: target.month,
      year: target.year,
      hour,
      minute: target.minute,
      meridian,
    };

    for (const f of OD.entries.find((e) => e.index === 0x2001).fields) {
      const value = fields[f.name];
      await apiWrite(0x2001, f.subindex, value);
      const input = fieldsEl.querySelector(`[data-subindex="${f.subindex}"]`);
      if (input) input.value = value;
    }

    await applyAllDisplaysFromOd();

    const targetStr = `${String(target.month).padStart(2, "0")}/${String(target.day).padStart(2, "0")}/${target.year} `
      + `${String(hour).padStart(2, "0")}:${String(target.minute).padStart(2, "0")} ${meridian === 1 ? "AM" : "PM"} (${tz})`;
    statusEl.textContent = `Synced to ${targetStr}.`;
    log(`Present time synced to real time: ${targetStr}`);
  } catch (err) {
    if (err.message !== "cancelled") {
      statusEl.textContent = `Sync failed: ${err.message}`;
      log(`Real-time sync failed: ${err.message}`, "error");
    }
  } finally {
    realTimeSyncActive = false;
    realTimeSyncArmedButton = null;
    button.textContent = button.dataset.idleLabel;
  }
}

// ---------- Function control (built from OD metadata) ----------

function buildFunctionControlButtons() {
  const entry = OD.entries.find((e) => e.index === 0x2200);
  const container = document.getElementById("function-control-buttons");
  container.innerHTML = "";
  for (const bit of entry.fields[0].bits) {
    const row = document.createElement("div");
    row.className = "toggle-row";
    const btn = document.createElement("button");
    btn.textContent = "Run";
    btn.addEventListener("click", () => pulseBit(0x2200, 0, bit.mask, bit.label));
    row.innerHTML = `<span>${bit.label}</span>`;
    row.appendChild(btn);
    container.appendChild(row);
  }
}

// ---------- Power relay + scheduler ----------

// Talks straight to /api/relay and /api/schedule rather than the OD - this
// is Pi-local infrastructure (mains power to the Time Circuits hardware),
// nothing to do with the STM32's CANopen object dictionary. Works
// identically whether or not real GPIO hardware is present: the backend's
// gpio_devices.py degrades to a no-op stand-in on a dev machine. Relay
// channel 2 exists in the backend as a spare but is deliberately not
// exposed here.
const POWER_RELAY_CHANNEL = 1;

async function refreshPowerStatus() {
  const btn = document.getElementById("power-toggle-btn");
  const statusDot = document.getElementById("power-status");
  try {
    const res = await fetchWithTimeout("/api/relay", { cache: "no-store" });
    if (!res.ok) throw new Error(`fetch failed (${res.status})`);
    const channels = await res.json();
    const on = channels[POWER_RELAY_CHANNEL]?.on ?? false;
    btn.textContent = on ? "Turn Off" : "Turn On";
    btn.dataset.on = on ? "1" : "0";
    statusDot?.classList.toggle("on", on);
  } catch (err) {
    btn.textContent = "Unavailable";
    statusDot?.classList.remove("on");
    log(`Power status failed: ${err.message}`, "error");
  }
}

function buildPowerCard() {
  const btn = document.getElementById("power-toggle-btn");
  btn.addEventListener("click", async () => {
    const nextOn = btn.dataset.on !== "1";
    btn.disabled = true;
    try {
      await fetchWithTimeout(`/api/relay/${POWER_RELAY_CHANNEL}`, {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ on: nextOn }),
      });
      log(`Time Circuits Power: ${nextOn ? "on" : "off"}`);
    } catch (err) {
      log(`Power toggle failed: ${err.message}`, "error");
    } finally {
      await refreshPowerStatus();
      btn.disabled = false;
    }
  });
  refreshPowerStatus();

  buildScheduler();
}

// ---------- Scheduler ----------

const SCHEDULE_DAYS = [
  { key: "mon", label: "M" },
  { key: "tue", label: "T" },
  { key: "wed", label: "W" },
  { key: "thu", label: "T" },
  { key: "fri", label: "F" },
  { key: "sat", label: "S" },
  { key: "sun", label: "S" },
];

function daysSummary(days) {
  const set = new Set(days);
  if (SCHEDULE_DAYS.every((d) => set.has(d.key))) return "Every day";
  const weekdays = ["mon", "tue", "wed", "thu", "fri"];
  const weekend = ["sat", "sun"];
  if (weekdays.every((d) => set.has(d)) && !weekend.some((d) => set.has(d))) return "Weekdays";
  if (weekend.every((d) => set.has(d)) && !weekdays.some((d) => set.has(d))) return "Weekends";
  return SCHEDULE_DAYS.filter((d) => set.has(d.key)).map((d) => d.label).join(" ");
}

function buildDayPicker() {
  const container = document.getElementById("schedule-days-input");
  container.innerHTML = "";
  for (const day of SCHEDULE_DAYS) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = day.label;
    btn.dataset.day = day.key;
    btn.className = "selected"; // every day selected by default
    btn.addEventListener("click", () => btn.classList.toggle("selected"));
    container.appendChild(btn);
  }
}

function selectedDays() {
  return [...document.querySelectorAll("#schedule-days-input button.selected")].map((b) => b.dataset.day);
}

async function refreshScheduleList() {
  const listEl = document.getElementById("schedule-list");
  listEl.innerHTML = "";
  try {
    const res = await fetchWithTimeout("/api/schedule", { cache: "no-store" });
    if (!res.ok) throw new Error(`fetch failed (${res.status})`);
    const entries = await res.json();
    if (entries.length === 0) {
      listEl.innerHTML = '<p class="hint">No scheduled times yet.</p>';
      return;
    }
    for (const entry of entries.sort((a, b) => a.time.localeCompare(b.time))) {
      const row = document.createElement("div");
      row.className = "schedule-row";
      row.innerHTML = `
        <span class="schedule-time">${entry.time}</span>
        <span class="schedule-summary">${entry.action === "on" ? "Turn On" : "Turn Off"} <span class="schedule-days">&middot; ${daysSummary(entry.days)}</span></span>
        <input type="checkbox" ${entry.enabled ? "checked" : ""} title="Enabled">
        <button class="secondary">Delete</button>
      `;
      row.querySelector("input").addEventListener("change", async (ev) => {
        try {
          await fetchWithTimeout(`/api/schedule/${entry.id}`, {
            method: "PATCH",
            cache: "no-store",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ enabled: ev.target.checked }),
          });
          log(`Schedule ${entry.time} ${entry.action}: ${ev.target.checked ? "enabled" : "disabled"}`);
        } catch (err) {
          ev.target.checked = !ev.target.checked;
          log(`Schedule update failed: ${err.message}`, "error");
        }
      });
      row.querySelector("button").addEventListener("click", async () => {
        try {
          await fetchWithTimeout(`/api/schedule/${entry.id}`, { method: "DELETE", cache: "no-store" });
          log(`Schedule entry removed: ${entry.time} ${entry.action}`);
          await refreshScheduleList();
        } catch (err) {
          log(`Schedule delete failed: ${err.message}`, "error");
        }
      });
      listEl.appendChild(row);
    }
  } catch (err) {
    listEl.innerHTML = `<p class="hint">Schedule unavailable: ${err.message}</p>`;
  }
}

function buildScheduler() {
  buildDayPicker();
  refreshScheduleList();

  document.getElementById("schedule-add-btn").addEventListener("click", async () => {
    const days = selectedDays();
    const time = document.getElementById("schedule-time-input").value;
    const action = document.getElementById("schedule-action-input").value;
    if (days.length === 0) {
      log("Pick at least one day for the schedule entry", "error");
      return;
    }
    if (!time) {
      log("Pick a time for the schedule entry", "error");
      return;
    }
    try {
      await fetchWithTimeout("/api/schedule", {
        method: "POST",
        cache: "no-store",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ days, time, action }),
      });
      log(`Schedule added: ${time} ${action}`);
      await refreshScheduleList();
    } catch (err) {
      log(`Schedule add failed: ${err.message}`, "error");
    }
  });
}

// ---------- Settings ----------

function buildSettingsCard() {
  const entry = OD.entries.find((e) => e.index === 0x2300);
  const settingBitsField = entry.fields.find((f) => f.name === "settingBits");
  const bitsByName = Object.fromEntries(settingBitsField.bits.map((b) => [b.name, b]));
  const glitchPeriod = entry.fields.find((f) => f.name === "glitchPeriod");
  const imuThreshold = entry.fields.find((f) => f.name === "imuMotionThreshold");
  const imuDuration = entry.fields.find((f) => f.name === "imuMotionDuration");

  const bodyEl = document.getElementById("settings-body");
  bodyEl.innerHTML = "";

  function toggleRow(bit) {
    const row = document.createElement("div");
    row.className = "toggle-row";
    row.innerHTML = `<span>${bit.label}</span><input type="checkbox" data-mask="${bit.mask}">`;
    row.querySelector("input").addEventListener("change", (ev) => {
      setBit(0x2300, 4, bit.mask, ev.target.checked, bit.label);
    });
    return row;
  }

  function numericRow(field, inputId) {
    const label = field.unit ? `${field.label} (${field.unit})` : field.label;
    const row = document.createElement("div");
    row.className = "numeric-row";
    row.innerHTML = `<span>${label}</span>
      <input type="number" id="${inputId}" min="${field.min ?? ""}" max="${field.max ?? ""}">`;
    return row;
  }

  // ---- Glitch ----
  const glitchGroup = document.createElement("div");
  glitchGroup.className = "settings-group";
  glitchGroup.innerHTML = "<h3>Glitch</h3>";
  glitchGroup.appendChild(toggleRow(bitsByName.glitchEnable));
  glitchGroup.appendChild(numericRow(glitchPeriod, "glitch-period-input"));
  const glitchApplyBtn = document.createElement("button");
  glitchApplyBtn.textContent = "Apply";
  glitchGroup.appendChild(glitchApplyBtn);
  bodyEl.appendChild(glitchGroup);

  // ---- Sound Effects ----
  const soundGroup = document.createElement("div");
  soundGroup.className = "settings-group";
  soundGroup.innerHTML = "<h3>Sound Effects</h3>";
  soundGroup.appendChild(toggleRow(bitsByName.muteAll));
  soundGroup.appendChild(toggleRow(bitsByName.muteColonSound));
  bodyEl.appendChild(soundGroup);

  // ---- IMU ----
  const imuGroup = document.createElement("div");
  imuGroup.className = "settings-group";
  imuGroup.innerHTML = "<h3>IMU</h3>";
  imuGroup.appendChild(numericRow(imuThreshold, "imu-threshold-input"));
  imuGroup.appendChild(numericRow(imuDuration, "imu-duration-input"));
  const imuApplyBtn = document.createElement("button");
  imuApplyBtn.textContent = "Apply";
  imuGroup.appendChild(imuApplyBtn);
  bodyEl.appendChild(imuGroup);

  glitchApplyBtn.addEventListener("click", async () => {
    try {
      const value = parseInt(document.getElementById("glitch-period-input").value, 10);
      await apiWrite(0x2300, glitchPeriod.subindex, value);
      await pulseBit(0x2300, 4, 1 << 4, "Apply Glitch Settings");
    } catch (err) {
      log(`glitch settings failed: ${err.message}`, "error");
    }
  });

  imuApplyBtn.addEventListener("click", async () => {
    try {
      const threshold = parseInt(document.getElementById("imu-threshold-input").value, 10);
      const duration = parseInt(document.getElementById("imu-duration-input").value, 10);
      await apiWrite(0x2300, imuThreshold.subindex, threshold);
      await apiWrite(0x2300, imuDuration.subindex, duration);
      await pulseBit(0x2300, 4, 1 << 3, "Apply IMU Settings");
    } catch (err) {
      log(`IMU settings failed: ${err.message}`, "error");
    }
  });

  // Populate current values.
  (async () => {
    try {
      const bits = await apiRead(0x2300, 4);
      for (const cb of bodyEl.querySelectorAll("input[type=checkbox]")) {
        cb.checked = (bits & parseInt(cb.dataset.mask, 10)) !== 0;
      }
      document.getElementById("glitch-period-input").value = await apiRead(0x2300, glitchPeriod.subindex);
      document.getElementById("imu-threshold-input").value = await apiRead(0x2300, imuThreshold.subindex);
      document.getElementById("imu-duration-input").value = await apiRead(0x2300, imuDuration.subindex);
    } catch (err) {
      log(`settings read failed: ${err.message}`, "error");
    }
  })();
}

// ---------- Movie-accurate defaults ----------

const MOVIE_DATE_RECORD_TO_INDEX = {
  destinationTime: 0x2000,
  presentTime: 0x2001,
  lastDepartedTime: 0x2002,
};

document.getElementById("movie-dates-btn").addEventListener("click", async () => {
  const statusEl = document.getElementById("movie-dates-status");
  const btn = document.getElementById("movie-dates-btn");
  btn.disabled = true;
  statusEl.textContent = "Fetching movie-accurate dates…";

  try {
    const res = await fetchWithTimeout("/api/movie-dates", { cache: "no-store" });
    if (!res.ok) {
      const body = await res.json().catch(() => ({}));
      throw new Error(body.detail || `fetch failed (${res.status})`);
    }
    const dates = await res.json();

    for (const [recordName, index] of Object.entries(MOVIE_DATE_RECORD_TO_INDEX)) {
      const record = dates[recordName];
      if (!record) throw new Error(`movie_dates.json is missing "${recordName}"`);
      const entry = OD.entries.find((e) => e.index === index);
      for (const f of entry.fields) {
        const value = record[f.name];
        if (value === undefined) throw new Error(`movie_dates.json's "${recordName}" is missing "${f.name}"`);
        await apiWrite(index, f.subindex, value);
      }
    }

    statusEl.textContent = "Applying to displays…";
    await applyAllDisplaysFromOd();

    statusEl.textContent = 'Applied. Click "Read" on each date/time card above to see the new values.';
    log("Movie-accurate dates applied to destination, present, and last-departed displays.");
  } catch (err) {
    statusEl.textContent = `Failed: ${err.message}`;
    log(`Movie-accurate dates failed: ${err.message}`, "error");
  } finally {
    btn.disabled = false;
  }
});

// ---------- Randomiser ----------

document.getElementById("randomiser-btn").addEventListener("click", async () => {
  const statusEl = document.getElementById("randomiser-status");
  const btn = document.getElementById("randomiser-btn");
  btn.disabled = true;
  statusEl.textContent = "Picking a moment in history…";

  try {
    const res = await fetchWithTimeout("/api/historical-dates", { cache: "no-store" });
    if (!res.ok) {
      const body = await res.json().catch(() => ({}));
      throw new Error(body.detail || `fetch failed (${res.status})`);
    }
    const { dates } = await res.json();
    if (!dates || dates.length === 0) throw new Error("historical_dates.json has no entries");

    const pick = dates[Math.floor(Math.random() * dates.length)];
    const entry = OD.entries.find((e) => e.index === 0x2000);
    for (const f of entry.fields) {
      const value = pick[f.name];
      if (value === undefined) throw new Error(`historical_dates.json entry is missing "${f.name}"`);
      await apiWrite(0x2000, f.subindex, value);
    }

    statusEl.textContent = "Applying to destination display…";
    await pulseBit(0x2200, 0, 1 << 3, "Update Destination Date");

    statusEl.textContent = `Set destination to: ${pick.label}.`;
    log(`Randomiser set destination time to: ${pick.label}`);
  } catch (err) {
    statusEl.textContent = `Failed: ${err.message}`;
    log(`Randomiser failed: ${err.message}`, "error");
  } finally {
    btn.disabled = false;
  }
});

// ---------- Advanced raw access ----------

document.getElementById("raw-read-btn").addEventListener("click", async () => {
  try {
    const index = parseInt(document.getElementById("raw-index").value, 16);
    const subindex = parseInt(document.getElementById("raw-subindex").value, 10);
    const value = await apiRead(index, subindex);
    document.getElementById("raw-value").value = value;
    log(`read ${index.toString(16)}:${subindex} = ${value}`);
  } catch (err) {
    log(`raw read failed: ${err.message}`, "error");
  }
});

document.getElementById("raw-write-btn").addEventListener("click", async () => {
  try {
    const index = parseInt(document.getElementById("raw-index").value, 16);
    const subindex = parseInt(document.getElementById("raw-subindex").value, 10);
    const value = parseInt(document.getElementById("raw-value").value, 10);
    await apiWrite(index, subindex, value);
    log(`wrote ${index.toString(16)}:${subindex} = ${value}`);
  } catch (err) {
    log(`raw write failed: ${err.message}`, "error");
  }
});

// ---------- Current display times (read-only status) ----------

const CURRENT_TIME_ELEMENTS = {
  0x2000: "current-time-destination",
  0x2001: "current-time-present",
  0x2002: "current-time-lastdeparted",
};

async function readDateTimeRecord(index) {
  const entry = OD.entries.find((e) => e.index === index);
  const values = {};
  for (const f of entry.fields) {
    values[f.name] = await apiRead(index, f.subindex);
  }
  return values;
}

// Same 3-letter abbreviations as monthDisplayChars[] in
// App/Time_Circuits/datetime_display.c, so the month column matches what
// the physical segment display actually shows.
const MONTH_ABBR = [
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
];

function bttfColumn(label, value) {
  return `<div class="bttf-col"><span class="bttf-label">${label}</span><span class="bttf-value">${value}</span></div>`;
}

// Renders a date/time record in the same layout as the physical Time
// Circuits display: MONTH/DAY/YEAR columns, an AM/PM indicator-lamp pair,
// then HOUR : MINUTE.
function formatDateTimeRecord(v) {
  const month = MONTH_ABBR[v.month - 1] ?? "---";
  const dd = String(v.day).padStart(2, "0");
  const hh = String(v.hour).padStart(2, "0");
  const min = String(v.minute).padStart(2, "0");
  const isAm = v.meridian === 1;
  return `<div class="bttf-readout">`
    + bttfColumn("Month", month)
    + bttfColumn("Day", dd)
    + bttfColumn("Year", v.year)
    + `<div class="bttf-meridian">`
    + `<div class="bttf-meridian-item"><span class="bttf-label">AM</span><span class="bttf-indicator${isAm ? " on" : ""}"></span></div>`
    + `<div class="bttf-meridian-item"><span class="bttf-label">PM</span><span class="bttf-indicator${isAm ? "" : " on"}"></span></div>`
    + `</div>`
    + bttfColumn("Hour", hh)
    + `<span class="bttf-value bttf-colon">:</span>`
    + bttfColumn("Min", min)
    + `</div>`;
}

async function refreshCurrentTimes() {
  if (!OD) return;
  for (const [index, elId] of Object.entries(CURRENT_TIME_ELEMENTS)) {
    const el = document.getElementById(elId);
    if (!el) continue;
    try {
      el.innerHTML = formatDateTimeRecord(await readDateTimeRecord(Number(index)));
    } catch (err) {
      el.textContent = "unavailable";
    }
  }
}

// ---------- Tabs ----------

const TAB_STORAGE_KEY = "tc-active-tab";

function initTabs() {
  const tabButtons = [...document.querySelectorAll(".tab-btn")];
  const panels = [...document.querySelectorAll(".tab-panel")];
  const validTabs = tabButtons.map((btn) => btn.dataset.tab);

  function activate(tab) {
    if (!validTabs.includes(tab)) tab = validTabs[0];
    for (const btn of tabButtons) btn.classList.toggle("active", btn.dataset.tab === tab);
    for (const panel of panels) panel.classList.toggle("active", panel.dataset.tab === tab);
    localStorage.setItem(TAB_STORAGE_KEY, tab);
  }

  for (const btn of tabButtons) {
    btn.addEventListener("click", () => activate(btn.dataset.tab));
  }

  activate(localStorage.getItem(TAB_STORAGE_KEY) || validTabs[0]);
}

// ---------- Init ----------

async function init() {
  initTabs();

  await refreshHealth();
  setInterval(refreshHealth, 5000);

  const res = await fetchWithTimeout(`${API}/api/od`, { cache: "no-store" });
  OD = await res.json();

  buildDateTimeCards();
  buildFunctionControlButtons();
  buildSettingsCard();
  buildPowerCard();
  connectLiveSocket();

  refreshCurrentTimes();
  setInterval(refreshCurrentTimes, 10000);

  // Keeps the Status page's power indicator current even if this tab is
  // never the one visited - e.g. the schedule turning the relay on/off in
  // the background, or another browser tab/device toggling it.
  setInterval(refreshPowerStatus, 10000);
}

init();
