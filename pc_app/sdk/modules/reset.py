"""Reset commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class ResetModule(DeviceModule):
    targets = {"peripherals", "periferics", "keypad", "settings", "core", "mcu"}

    def run(self, target: str = "peripherals") -> CommandResult:
        if target not in self.targets:
            return CommandResult(
                ok=False,
                command_name="reset",
                raw_command=f"reset {target}",
                error="Uso: reset [peripherals|keypad|settings|core]",
            )
        return self.command_result(f"reset.{target}", f"reset {target}")

    def peripherals(self) -> CommandResult:
        return self.run("peripherals")

    def keypad(self) -> CommandResult:
        return self.run("keypad")

    def settings(self) -> CommandResult:
        return self.run("settings")

    def core(self) -> CommandResult:
        return self.run("core")
