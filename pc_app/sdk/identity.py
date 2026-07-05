"""Shared DeskBridge identity and PC-side version identifiers."""

from __future__ import annotations

APP_NAME = "DeskBridge PC"
APP_VERSION = "0.3.0"

USB_VID = 0x1209
USB_PID = 0xDB01
USB_ID = f"{USB_VID:04X}:{USB_PID:04X}"

# Shell headers use protocol v3. The older framed binary helper keeps its own
# compatibility version because it is not the active CORE shell protocol.
USB_SERIAL_PROTOCOL = 0x03
LEGACY_FRAME_PROTOCOL = 0x01
