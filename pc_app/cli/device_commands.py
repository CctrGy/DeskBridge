"""CLI commands backed by SDK device modules."""

from __future__ import annotations

from typing import TYPE_CHECKING

from sdk.result import CommandResult

from .formatter import format_result
from .parser import ParsedCommand
from . import theme

if TYPE_CHECKING:
    from .shell import CliShell


def register_device_commands(shell: CliShell) -> None:
    shell.registry.register("ping", lambda command: basic(shell, command, "ping", "ping", shell.device.ping))
    shell.registry.register("info", lambda command: basic(shell, command, "info", "info", shell.device.core.info))
    shell.registry.register("usb", lambda command: basic(shell, command, "usb", "usb", shell.device.core.usb))
    shell.registry.register("program", lambda command: basic(shell, command, "program", "program", shell.device.core.program))
    shell.registry.register("display", lambda command: display(shell, command))
    shell.registry.register("chip", lambda command: chip(shell, command), "periferic", "sys", "system")
    shell.registry.register("bus", lambda command: bus(shell, command))
    shell.registry.register("keypad", lambda command: keypad(shell, command))
    shell.registry.register("light", lambda command: light(shell, command))
    shell.registry.register("reset", lambda command: reset(shell, command))
    shell.registry.register("rtc", lambda command: rtc(shell, command))
    shell.registry.register("wireless", lambda command: wireless(shell, command), "wireless?")


def show(shell: CliShell, result: CommandResult) -> bool:
    capture = getattr(shell, "capture_result", None)
    if callable(capture):
        capture(result)
    print(format_result(result, debug=shell.debug.enabled))
    return True


def basic(shell: CliShell, command: ParsedCommand, title: str, usage: str, action) -> bool:
    if command.args and is_help_arg(command.args[0]):
        return command_help(title, [(usage, "")])
    return show(shell, action())


def rtc(shell: CliShell, command: ParsedCommand) -> bool:
    if command.args and is_help_arg(command.args[0]):
        return rtc_help()
    result = execute_rtc(shell, command.args)
    capture = getattr(shell, "capture_result", None)
    if callable(capture):
        capture(result)
    print(format_result(result, debug=shell.debug.enabled))
    return True


def chip(shell: CliShell, command: ParsedCommand) -> bool:
    args = [arg.lower() for arg in command.args]
    if args and is_help_arg(args[0]):
        return command_help(
            "peripheral",
            [
                ("peripheral", "list internal chips"),
                ("peripheral <chip:core|keypad|bios|cnio|list>", "show selected chip state"),
            ],
        )
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
    if args and is_help_arg(args[0]):
        return command_help(
            "bus",
            [
                ("bus", "show sensor bus state"),
                ("bus scan", "scan sensor I2C bus"),
                ("bus sensor [sel]", "list sensors or show one sensor"),
                ("bus timeRead [value]", "show or set read interval"),
                ("bus samples [value]", "show or set sample count"),
            ],
        )
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


def display(shell: CliShell, command: ParsedCommand) -> bool:
    args = command.args
    if args and is_help_arg(args[0]):
        return command_help(
            "display",
            [
                ("display", "show OLED display configuration"),
                ("display fps", "show OLED frame rate"),
                ("display fps <15..50>", "set OLED frame rate"),
            ],
        )
    if not args:
        return show(shell, shell.device.display.get())

    action = args[0].lower()
    if action in {"fps", "frame", "framerate"}:
        if len(args) == 1:
            return show(shell, shell.device.display.fps())
        try:
            return show(shell, shell.device.display.fps(int(args[1])))
        except ValueError:
            return show(shell, cli_error("display.fps", "fps debe ser un entero entre 15 y 50"))

    return show(shell, cli_error("display", "Uso: display [fps [15..50]]"))


def light(shell: CliShell, command: ParsedCommand) -> bool:
    args = command.args
    if args and is_help_arg(args[0]):
        return command_help(
            "light",
            [
                ("light", "show light state"),
                ("light power", "show power manager state"),
                ("light <field> <value>", "set light field"),
                ("light sync", "sync light configuration"),
            ],
        )
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


def keypad(shell: CliShell, command: ParsedCommand) -> bool:
    args = command.args
    if args and is_help_arg(args[0]):
        return command_help(
            "keypad",
            [
                ("keypad", "show keypad state"),
                ("keypad action list", "list programmable KEY actions"),
                ("keypad action <0..4> <action>", "assign action to one KEY button"),
            ],
        )
    if not args:
        return show(shell, shell.device.keypad.get())

    action = args[0].lower()
    if action in {"action", "assign"}:
        if len(args) >= 2 and args[1].lower() == "list":
            return show(shell, shell.device.keypad.action_list())
        if len(args) < 3:
            return show(shell, cli_error("keypad.action", "Uso: keypad action <0..4> <action>"))
        try:
            return show(shell, shell.device.keypad.set_action(args[1], args[2]))
        except (TypeError, ValueError) as exc:
            return show(shell, cli_error("keypad.action", str(exc)))

    return show(shell, cli_error("keypad", "Uso: keypad [action list|action <0..4> <action>]"))


def reset(shell: CliShell, command: ParsedCommand) -> bool:
    if command.args and is_help_arg(command.args[0]):
        return command_help(
            "reset",
            [
                ("reset", "reset peripherals"),
                ("reset <peripherals|keypad|settings|core|mcu>", "controlled reset target"),
            ],
        )
    target = command.args[0].lower() if command.args else "peripherals"
    return show(shell, shell.device.reset.run(target))


def wireless(shell: CliShell, command: ParsedCommand) -> bool:
    args = command.args
    if args and is_help_arg(args[0]):
        return wireless_help()
    if not args:
        return show(shell, shell.device.wireless.get())

    action = args[0].lower()
    if action in {"?", "status"}:
        return show(shell, shell.device.wireless.get())
    if action == "state":
        return show(shell, shell.device.wireless.state())
    if action == "on":
        return show(shell, shell.device.wireless.on())
    if action == "off":
        return show(shell, shell.device.wireless.off())
    if action == "pair":
        return show(shell, shell.device.wireless.pair())
    if action == "enabled":
        return show(shell, shell.device.wireless.enabled(args[1] if len(args) > 1 else None))
    if action == "name":
        return show(shell, shell.device.wireless.name(" ".join(args[1:]) if len(args) > 1 else None))
    if action == "apply":
        return show(shell, shell.device.wireless.apply())
    if action == "write":
        if len(args) < 2:
            return show(shell, cli_error("wireless.write", "Uso: wireless write <text>"))
        return show(shell, shell.device.wireless.write(" ".join(args[1:])))
    if action == "rx":
        return show(shell, shell.device.wireless.rx())

    return show(shell, cli_error("wireless", "Uso: wireless <state|on|off|pair|enabled|name|apply|write|rx> [value]"))


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

    return cli_error("rtc", "Uso: rtc <status|get|set|sync> [value]")


def wireless_help() -> bool:
    return command_help(
        "wireless",
        [
            ("wireless", "show wireless state"),
            ("wireless state", "show wireless state"),
            ("wireless on", "enable wireless"),
            ("wireless off", "disable wireless"),
            ("wireless pair", "enable BLE device mode and advertising"),
            ("wireless enabled [on|off]", "show or set enabled state"),
            ("wireless name [name]", "show or set BLE name"),
            ("wireless apply", "apply wireless configuration"),
            ("wireless write <text>", "write text to BLE link"),
            ("wireless rx", "read pending BLE RX text"),
        ],
    )


def rtc_help() -> bool:
    return command_help(
        "rtc",
        [
            ("rtc", "show RTC status"),
            ("rtc status", "show RTC status"),
            ("rtc get", "show date and time"),
            ("rtc get date", "show date"),
            ("rtc get time", "show time"),
            ("rtc set <value>", "set date and time"),
            ("rtc set date <DD/MM/YYYY>", "set date"),
            ("rtc set time <HH:MM:SS>", "set time"),
            ("rtc sync", "sync from PC date and time"),
        ],
    )


def command_help(title: str, rows: list[tuple[str, str]]) -> bool:
    print(theme.title(f"{title} help"))
    width = max(len(usage) for usage, _ in rows)
    for usage, description in rows:
        rendered = theme.highlight_usage(usage)
        padding = " " * max(2, width - len(usage) + 2)
        print(f"  {rendered}{padding}{theme.muted(description)}")
    return True


def is_help_arg(value: str) -> bool:
    return value.lower() in {"/?", "?", "help", "--help", "-h"}


def is_device_help_request(command: ParsedCommand) -> bool:
    return bool(command.args and is_help_arg(command.args[0]))


def cli_error(command_name: str, error: str) -> CommandResult:
    return CommandResult(ok=False, command_name=command_name, raw_command="", error=error)
