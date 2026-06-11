"""System information commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class SystemModule(DeviceModule):
    def get(self) -> CommandResult:
        return self.local()

    def local(self) -> CommandResult:
        return self.command_result("system.local", "sys local")

    def peripherals(self) -> CommandResult:
        return self.command_result("system.peripherals", "sys peripherals")

    def periferics(self) -> CommandResult:
        return self.peripherals()
