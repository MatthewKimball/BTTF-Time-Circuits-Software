"""
Object dictionary map for the BTTF Time Circuits CANopen node.

Mirrors CANopen/OD.h / OD.c in the firmware repo. Kept as plain data (not
generated) since the OD is small and changes rarely - if the firmware OD
changes, update this file to match.
"""

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Bit:
    mask: int
    name: str
    label: str
    kind: str  # "level" (persistent switch) or "oneshot" (auto-clears in firmware)


@dataclass(frozen=True)
class Field:
    subindex: int
    name: str
    label: str
    dtype: str  # "u8", "u16", "u32"
    access: str  # "ro" or "rw"
    bits: tuple[Bit, ...] = field(default_factory=tuple)
    min: int | None = None
    max: int | None = None
    unit: str = ""


@dataclass(frozen=True)
class Entry:
    index: int
    name: str
    label: str
    fields: tuple[Field, ...]


DTYPE_SIZES = {"u8": 1, "u16": 2, "u32": 4}

DATE_TIME_FIELDS = (
    Field(1, "day", "Day", "u8", "rw", min=1, max=31),
    Field(2, "month", "Month", "u8", "rw", min=1, max=12),
    Field(3, "year", "Year", "u16", "rw", min=0, max=9999),
    Field(4, "hour", "Hour", "u8", "rw", min=1, max=12),
    Field(5, "minute", "Minute", "u8", "rw", min=0, max=59),
    Field(6, "meridian", "AM/PM", "u8", "rw", min=1, max=2),
)

OBJECT_DICTIONARY: tuple[Entry, ...] = (
    Entry(0x2000, "destinationTime", "Destination Time", DATE_TIME_FIELDS),
    Entry(0x2001, "presentTime", "Present Time", DATE_TIME_FIELDS),
    Entry(0x2002, "lastDepartedTime", "Last Time Departed", DATE_TIME_FIELDS),
    Entry(
        0x2100,
        "buttonsState",
        "Buttons / Switches State",
        (
            Field(
                0,
                "buttonsState",
                "Buttons State",
                "u8",
                "ro",
                bits=(
                    Bit(1 << 0, "glitch", "Glitch switch", "level"),
                    Bit(1 << 1, "keypadEnter", "Keypad Enter switch", "level"),
                    Bit(1 << 2, "mute", "Mute switch", "level"),
                    Bit(1 << 3, "timeTravel", "Time Travel switch", "level"),
                ),
            ),
        ),
    ),
    Entry(
        0x2101,
        "timeCircuitsState",
        "Time Circuits State (current)",
        (Field(0, "state", "State", "u8", "ro"),),
    ),
    Entry(
        0x2102,
        "requestedTimeCircuitsState",
        "Time Circuits State (requested)",
        (Field(0, "state", "State", "u8", "rw"),),
    ),
    Entry(
        0x2200,
        "functionControl",
        "Function Control",
        (
            Field(
                0,
                "functionControl",
                "Function Control",
                "u8",
                "rw",
                bits=(
                    Bit(1 << 0, "clearAllDisplays", "Clear All Displays", "oneshot"),
                    Bit(1 << 1, "updateAllDisplays", "Update All Displays", "oneshot"),
                    Bit(1 << 2, "setAllDisplays", "Set All Displays (push OD dates to displays)", "oneshot"),
                    Bit(1 << 3, "updateDestinationDate", "Update Destination Date", "oneshot"),
                    Bit(1 << 4, "updateRtc", "Update RTC (from present time)", "oneshot"),
                    Bit(1 << 5, "saveDates", "Save Dates to SD Card", "oneshot"),
                ),
            ),
        ),
    ),
    Entry(
        0x2300,
        "settingParameters",
        "Settings",
        (
            Field(1, "glitchPeriod", "Glitch Period", "u32", "rw", min=1, unit="ms"),
            Field(2, "imuMotionThreshold", "IMU Any-Motion Threshold", "u8", "rw", min=1, max=255),
            Field(3, "imuMotionDuration", "IMU Any-Motion Duration", "u8", "rw", min=0, max=3),
            Field(
                4,
                "settingBits",
                "Setting Bits",
                "u8",
                "rw",
                bits=(
                    Bit(1 << 0, "glitchEnable", "Glitch Enable", "level"),
                    Bit(1 << 1, "muteColonSound", "Mute Colon Tick Sound", "level"),
                    Bit(1 << 2, "muteAll", "Mute All Sound", "level"),
                    Bit(1 << 3, "applyImuSettings", "Apply IMU Settings", "oneshot"),
                    Bit(1 << 4, "applyGlitchSettings", "Apply Glitch Settings", "oneshot"),
                ),
            ),
        ),
    ),
)

TIME_CIRCUITS_STATE_NAMES = {
    0: "Idle",
    1: "Armed",
    2: "Travel",
    3: "Complete",
}


def find_entry(index: int) -> Entry | None:
    for entry in OBJECT_DICTIONARY:
        if entry.index == index:
            return entry
    return None


def find_field(index: int, subindex: int) -> tuple[Entry, Field] | None:
    entry = find_entry(index)
    if entry is None:
        return None
    for f in entry.fields:
        if f.subindex == subindex:
            return entry, f
    return None


def od_as_json() -> list[dict]:
    """Serialize the OD map for the frontend."""
    out = []
    for entry in OBJECT_DICTIONARY:
        out.append(
            {
                "index": entry.index,
                "name": entry.name,
                "label": entry.label,
                "fields": [
                    {
                        "subindex": f.subindex,
                        "name": f.name,
                        "label": f.label,
                        "dtype": f.dtype,
                        "access": f.access,
                        "min": f.min,
                        "max": f.max,
                        "unit": f.unit,
                        "bits": [
                            {
                                "mask": b.mask,
                                "name": b.name,
                                "label": b.label,
                                "kind": b.kind,
                            }
                            for b in f.bits
                        ],
                    }
                    for f in entry.fields
                ],
            }
        )
    return out
