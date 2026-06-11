"""Light strip commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class LightModule(DeviceModule):
    editable_fields = {
        "enabled",
        "mode",
        "brightness",
        "kelvin",
        "cold_min",
        "cold_max",
        "warm_min",
        "warm_max",
        "click",
        "long",
        "repeat",
        "brightness_step",
        "kelvin_step",
    }

    def get(self) -> CommandResult:
        return self.command_result("light.get", "light?")

    def power(self) -> CommandResult:
        return self.command_result("light.power", "light power")

    def sync(self) -> CommandResult:
        return self.command_result("light.sync", "light sync")

    def set(self, field: str, value: str | int | bool) -> CommandResult:
        if field not in self.editable_fields:
            return CommandResult(
                ok=False,
                command_name="light.set",
                raw_command=f"light {field} {value}",
                error=f"Campo light no soportado: {field}",
            )
        return self.command_result(f"light.{field}.set", f"light {field} {format_value(value)}")

    def enabled(self, value: str | bool | None = None) -> CommandResult:
        if value is None:
            return self.get()
        return self.set("enabled", value)

    def mode(self, value: str | int) -> CommandResult:
        return self.set("mode", value)

    def brightness(self, value: int) -> CommandResult:
        return self.set("brightness", value)

    def kelvin(self, value: int) -> CommandResult:
        return self.set("kelvin", value)

    def cold_min(self, value: int) -> CommandResult:
        return self.set("cold_min", value)

    def cold_max(self, value: int) -> CommandResult:
        return self.set("cold_max", value)

    def warm_min(self, value: int) -> CommandResult:
        return self.set("warm_min", value)

    def warm_max(self, value: int) -> CommandResult:
        return self.set("warm_max", value)

    def click(self, value: int) -> CommandResult:
        return self.set("click", value)

    def long(self, value: int) -> CommandResult:
        return self.set("long", value)

    def repeat(self, value: int) -> CommandResult:
        return self.set("repeat", value)

    def brightness_step(self, value: int) -> CommandResult:
        return self.set("brightness_step", value)

    def kelvin_step(self, value: int) -> CommandResult:
        return self.set("kelvin_step", value)


def format_value(value: str | int | bool) -> str:
    if isinstance(value, bool):
        return "on" if value else "off"
    return str(value)
