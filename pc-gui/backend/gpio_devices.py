"""
GPIO devices for the Raspberry Pi deployment: the 2-channel power relay
and the two status LEDs. Not used at all on a dev machine with a USBtin -
the gpiozero import (and every device creation) is guarded so the rest of
the backend keeps working unmodified where there's no GPIO hardware, like
this dev sandbox.

Config via environment variables:
    TC_RELAY1_PIN         BCM pin for relay channel 1 (default 5)
    TC_RELAY2_PIN         BCM pin for relay channel 2 (default 6)
    TC_RELAY_ACTIVE_HIGH  "true" (default) or "false" - the SSR module's
                           trigger polarity. Flip this if it turns out to
                           be wired backwards once tested.
    TC_LED_ALIVE_PIN      BCM pin for the "system alive" LED (default 13)
    TC_LED_CAN_PIN         BCM pin for the "CAN link" LED (default 19)

Pin choices avoid everything a PiCAN2 HAT uses (SPI0: GPIO 8/9/10/11,
INT: GPIO25) and the HAT ID EEPROM pins (GPIO0/1).
"""

import logging
import os

logger = logging.getLogger("gpio_devices")

RELAY1_PIN = int(os.environ.get("TC_RELAY1_PIN", "5"))
RELAY2_PIN = int(os.environ.get("TC_RELAY2_PIN", "6"))
RELAY_ACTIVE_HIGH = os.environ.get("TC_RELAY_ACTIVE_HIGH", "true").strip().lower() != "false"
LED_ALIVE_PIN = int(os.environ.get("TC_LED_ALIVE_PIN", "13"))
LED_CAN_PIN = int(os.environ.get("TC_LED_CAN_PIN", "19"))

try:
    from gpiozero import LED, OutputDevice
    _GPIO_AVAILABLE = True
except ImportError:
    _GPIO_AVAILABLE = False


class _NoOpDevice:
    """Stand-in for a gpiozero OutputDevice/LED when GPIO hardware isn't
    available, so callers never need to branch on availability themselves."""

    def __init__(self, *_args, **_kwargs):
        self.value = 0

    def on(self) -> None:
        self.value = 1

    def off(self) -> None:
        self.value = 0

    def blink(self, *_args, **_kwargs) -> None:
        pass

    def close(self) -> None:
        pass


def _make_output(pin: int, active_high: bool):
    if _GPIO_AVAILABLE:
        try:
            return OutputDevice(pin, active_high=active_high, initial_value=False)
        except Exception:  # noqa: BLE001 - falls back to the no-op stand-in
            logger.warning("GPIO pin %d unavailable, falling back to no-op", pin)
    return _NoOpDevice()


def _make_led(pin: int):
    if _GPIO_AVAILABLE:
        try:
            return LED(pin)
        except Exception:  # noqa: BLE001
            logger.warning("GPIO pin %d unavailable, falling back to no-op", pin)
    return _NoOpDevice()


class RelayController:
    """Controls the 2-channel SSR relay board. Channel 1 is intended for
    the Time Circuits hardware's power port, channel 2 for a second
    general-purpose device port - this class doesn't care which is which,
    the frontend just labels them."""

    def __init__(self):
        self._channels = {
            1: _make_output(RELAY1_PIN, RELAY_ACTIVE_HIGH),
            2: _make_output(RELAY2_PIN, RELAY_ACTIVE_HIGH),
        }

    def set(self, channel: int, on: bool) -> None:
        if channel not in self._channels:
            raise ValueError(f"no such relay channel: {channel}")
        device = self._channels[channel]
        (device.on if on else device.off)()

    def state(self) -> dict[int, bool]:
        return {ch: bool(dev.value) for ch, dev in self._channels.items()}


class StatusLeds:
    """The two status LEDs: system_alive heartbeat-blinks continuously once
    the app is running, can_link tracks whether the board is currently
    responding on CAN (updated by app.py's background health monitor)."""

    def __init__(self):
        self.system_alive = _make_led(LED_ALIVE_PIN)
        self.can_link = _make_led(LED_CAN_PIN)

    def start(self) -> None:
        # 1s on/1s off - a simple "the service is up" heartbeat, distinct
        # from a solid-on LED so a hung process would be distinguishable
        # from one that's still actually alive and blinking.
        self.system_alive.blink(on_time=1, off_time=1, background=True)

    def set_can_link(self, connected: bool) -> None:
        (self.can_link.on if connected else self.can_link.off)()


relay = RelayController()
status_leds = StatusLeds()
