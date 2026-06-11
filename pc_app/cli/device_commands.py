"""CLI commands backed by SDK device modules."""

from __future__ import annotations

from typing import TYPE_CHECKING

from sdk.result import CommandResult

from .formatter import format_result
from .parser import ParsedCommand

if TYPE_CHECKING:
    from .shell import CliShell


def register_device_commands(shell: CliShell) -> None:
    shell.registry.register("ping", lambda command: show(shell, shell.device.ping()))
    shell.registry.register("info", lambda command: show(shell, shell.device.core.info()))
    shell.registry.register("usb", lambda command: show(shell, shell.device.core.usb()))
    shell.registry.register("program", lambda command: show(shell, shell.device.core.program()))
    shell.registry.register("chip", lambda command: chip(shell, command), "periferic", "sys", "system")
    shell.registry.register("bus", lambda command: bus(shell, command))
    shell.registry.register("keypad", lambda command: show(shell, shell.device.keypad.get()))
    shell.registry.register("light", lambda command: light(shell, command))
    shell.registry.register("reset", lambda command: reset(shell, command))
    shell.registry.register("rtc", lambda command: rtc(shell, command))


def show(shell: CliShell, result: CommandResult) -> bool:
    print(format_result(result, debug=shell.debug.enabled))
    return True


def rtc(shell: CliShell, command: ParsedCommand) -> bool:
    result = execute_rtc(shell, command.args)
    print(format_result(result, debug=shell.debug.enabled))
    return True


def chip(shell: CliShell, command: ParsedCommand) -> bool:
    args = [arg.lower() for arg in command.args]
    selector = args[0] if args else "list"
    if selector.startswith("chip:"):
        selector = selector.split(":", 1)[1]

    if selector == "core":
        return show(shell, shell.device.system.local())
    if selector == "list":
        return show(shell, shell.device.system.peripherals())
    if selector == "keypad":
        return show(shell, shell.device.keypad.get())
    if selector in {"bios", "cnio"}:
        return show(
            shell,
            cli_error(
                "chip",
                f"chip {selector} no esta soportado por el firmware actual",
            ),
        )
    return show(shell, cli_error("chip", "Uso: chip [core|keypad|bios|cnio|list]"))


def bus(shell: CliShell, command: ParsedCommand) -> bool:
    args = command.args
    if not args:
        return show(shell, shell.device.bus.get())

    action = args[0].lower()
    if action == "scan":
        return show(shell, shell.device.bus.scan())
    if action == "sensor":
        if len(args) == 1:
            return show(shell, shell.device.bus.sensors())
        return show(shell, shell.device.bus.sensor(args[1]))
    if action in {"timeread", "time_read"}:
        return show(shell, shell.device.bus.time_read(args[1] if len(args) > 1 else None))
    if action == "samples":
        if len(args) == 1:
            return show(shell, shell.device.bus.samples())
        try:
            return show(shell, shell.device.bus.samples(int(args[1])))
        except ValueError:
            return show(shell, cli_error("bus.samples", "samples debe ser un entero"))
    return show(shell, cli_error("bus", "Uso: bus [scan|sensor [sel]|time_read [value]|samples [value]]"))


def light(shell: CliShell, command: ParsedCommand) -> bool:
    args = command.args
    if not args:
        return show(shell, shell.device.light.get())

    action = args[0].lower()
    if action == "power":
        return show(shell, shell.device.light.power())
    if action in shell.device.light.editable_fields:
        if len(args) < 2:
            return show(shell, shell.device.light.get())
        return show(shell, shell.device.light.set(action, args[1]))
    return show(shell, cli_error("light", "Uso: light <power|field> [value]"))


def reset(shell: CliShell, command: ParsedCommand) -> bool:
    target = command.args[0].lower() if command.args else "peripherals"
    return show(shell, shell.device.reset.run(target))


def execute_rtc(shell: CliShell, args: list[str]) -> CommandResult:
    if not args:
        return shell.device.rtc.status()

    action = args[0].lower()
    if action == "status":
        return shell.device.rtc.status()

    if action == "get":
        if len(args) == 1:
            return shell.device.rtc.get()
        field = args[1].lower()
        if field == "date":
            return shell.device.rtc.get_date()
        if field == "time":
            return shell.device.rtc.get_time()

    if action == "set":
        if len(args) < 2:
            return cli_error("rtc.set", "Uso: rtc set <YYYY-MM-DDTHH:MM:SS|date DD/MM/YYYY|time HH:MM:SS>")
        if len(args) == 2:
            return shell.device.rtc.set(args[1])
        field = args[1].lower()
        value = args[2]
        if field == "date":
            return shell.device.rtc.set_date(value)
        if field == "time":
            return shell.device.rtc.set_time(value)

    if action in {"sync", "synk"}:
        return shell.device.rtc.sync()

    return cli_error("rtc", "Uso: rtc <status|get|set|synk> [value]")


def cli_error(command_name: str, error: str) -> CommandResult:
    return CommandResult(ok=False, command_name=command_name, raw_command="", error=error)
