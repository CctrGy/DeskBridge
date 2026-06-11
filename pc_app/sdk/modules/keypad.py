"""Keypad peripheral commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class KeypadModule(DeviceModule):
    def get(self) -> CommandResult:
        return self.command_result("keypad.get", "keypad?")
