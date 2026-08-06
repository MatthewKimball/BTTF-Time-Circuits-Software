#!/usr/bin/env bash
# Installs the Time Circuits web control panel as a systemd service on a
# Raspberry Pi with a PiCAN2 HAT. Run on the Pi itself (not from a dev
# machine), as root or with sudo. Idempotent - safe to re-run.
#
# Usage: sudo pc-gui/deploy/install.sh

set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root (sudo pc-gui/deploy/install.sh)" >&2
    exit 1
fi

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEPLOY_DIR="$REPO_DIR/pc-gui/deploy"
RUN_USER="${SUDO_USER:-$(whoami)}"

echo "==> Repo: $REPO_DIR"
echo "==> Service will run as: $RUN_USER"

echo "==> Installing apt dependencies"
apt-get update
apt-get install -y \
    python3-can python3-fastapi python3-uvicorn \
    python3-gpiozero python3-lgpio \
    python3-pydantic python3-websockets

echo "==> Adding $RUN_USER to gpio/spi/dialout groups (harmless if already a member)"
usermod -aG gpio,spi,dialout "$RUN_USER"

echo "==> Enabling SPI"
raspi-config nonint do_spi 0

CONFIG_TXT="/boot/firmware/config.txt"
[ -f "$CONFIG_TXT" ] || CONFIG_TXT="/boot/config.txt"
OVERLAY_LINE="dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25"
NEEDS_REBOOT=0
if ! grep -qxF "$OVERLAY_LINE" "$CONFIG_TXT"; then
    echo "==> Adding PiCAN2 overlay to $CONFIG_TXT"
    echo "$OVERLAY_LINE" >> "$CONFIG_TXT"
    NEEDS_REBOOT=1
else
    echo "==> PiCAN2 overlay already present in $CONFIG_TXT"
fi

echo "==> Installing systemd units"
sed "s|__REPO_DIR__|$REPO_DIR|g; s|__RUN_USER__|$RUN_USER|g" \
    "$DEPLOY_DIR/timecircuits-gui.service.in" > /etc/systemd/system/timecircuits-gui.service
cp "$DEPLOY_DIR/can0-up.service" /etc/systemd/system/can0-up.service

systemctl daemon-reload
systemctl enable can0-up.service
systemctl enable timecircuits-gui.service

echo
if [ "$NEEDS_REBOOT" = "1" ]; then
    echo "==> A reboot is needed for the SPI/PiCAN2 overlay to take effect."
    echo "    Run: sudo reboot"
    echo "    Both services are enabled and will start automatically on boot."
else
    echo "==> Overlay was already active - starting services now"
    systemctl restart can0-up.service || echo "    (can0-up.service failed - is the PiCAN2 board physically connected?)"
    systemctl restart timecircuits-gui.service
    echo "==> Done. Check status with: systemctl status timecircuits-gui.service"
fi
