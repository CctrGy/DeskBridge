"""OLED display configuration commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class DisplayModule(DeviceModule):
    def get(self) -> CommandResult:
        return self.command_result("display.get", "display?")

    def fps(self, value: int | str | None = None) -> CommandResult:
        if value is None:
            return self.command_result("display.fps", "display fps")
        return self.command_result("display.fps.set", f"display fps {value}")
