"""Wireless BLE commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class WirelessModule(DeviceModule):
    def get(self) -> CommandResult:
        return self.command_result("wireless.get", "wireless?")

    def state(self) -> CommandResult:
        return self.command_result("wireless.state", "wireless state")

    def on(self) -> CommandResult:
        return self.command_result("wireless.on", "wireless on")

    def off(self) -> CommandResult:
        return self.command_result("wireless.off", "wireless off")

    def pair(self) -> CommandResult:
        return self.command_result("wireless.pair", "wireless pair")

    def enabled(self, value: str | bool | None = None) -> CommandResult:
        if value is None:
            return self.command_result("wireless.enabled", "wireless enabled")
        return self.command_result("wireless.enabled.set", f"wireless enabled {format_bool(value)}")

    def name(self, value: str | None = None) -> CommandResult:
        if value is None:
            return self.command_result("wireless.name", "wireless name")
        return self.command_result("wireless.name.set", f"wireless name {value}")

    def apply(self) -> CommandResult:
        return self.command_result("wireless.apply", "wireless apply")

    def write(self, text: str) -> CommandResult:
        return self.command_result("wireless.write", f"wireless write {text}")

    def rx(self) -> CommandResult:
        return self.command_result("wireless.rx", "wireless rx")


def format_bool(value: str | bool) -> str:
    if isinstance(value, bool):
        return "on" if value else "off"
    return str(value)
