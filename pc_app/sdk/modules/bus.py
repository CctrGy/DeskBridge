"""Sensor bus commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class BusModule(DeviceModule):
    def get(self) -> CommandResult:
        return self.command_result("bus.get", "bus?")

    def scan(self) -> CommandResult:
        return self.command_result("bus.scan", "bus scan")

    def time_read(self, value: str | int | float | None = None) -> CommandResult:
        if value is None:
            return self.command_result("bus.time_read", "bus time_read")
        return self.command_result("bus.time_read.set", f"bus time_read {value}")

    def samples(self, value: int | None = None) -> CommandResult:
        if value is None:
            return self.command_result("bus.samples", "bus samples")
        return self.command_result("bus.samples.set", f"bus samples {value}")

    def sensors(self) -> CommandResult:
        return self.command_result("bus.sensors", "bus sensor")

    def sensor(self, selector: str | int) -> CommandResult:
        return self.command_result("bus.sensor", f"bus sensor {selector}")
