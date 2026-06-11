"""Core shell commands: ping, info, usb and program."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class CoreModule(DeviceModule):
    def ping(self) -> CommandResult:
        return self.command_result("core.ping", "ping")

    def info(self) -> CommandResult:
        return self.command_result("core.info", "info?")

    def usb(self) -> CommandResult:
        return self.command_result("core.usb", "usb?")

    def program(self) -> CommandResult:
        return self.command_result("core.program", "program?")
