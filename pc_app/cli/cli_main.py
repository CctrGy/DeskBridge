"""CLI entry point."""

from __future__ import annotations

import argparse

from sdk import DeviceSDK

from .parser import ParsedCommand
from .shell import CliShell
from . import theme


DEVICE_ROOT_COMMANDS = {
    "ping",
    "info",
    "usb",
    "program",
    "chip",
    "periferic",
    "sys",
    "system",
    "bus",
    "keypad",
    "light",
    "reset",
    "rtc",
}


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    shell = CliShell(DeviceSDK())

    if args.port:
        shell.device.set_port(args.port)

    if not args.command:
        return shell.run(auto_connect=not args.no_autoconnect)

    command_line = " ".join(args.command)
    command = ParsedCommand(args.command[0].lower(), args.command[1:], command_line)

    if command.name in DEVICE_ROOT_COMMANDS:
        return run_single_device_command(shell, command)

    shell.history.add(command_line)
    shell.execute_line(command_line)
    return 0


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="DeskBridge CLI")
    parser.add_argument(
        "--cli",
        action="store_true",
        help="Abre o ejecuta el CLI. Se acepta para compatibilidad con el lanzador.",
    )
    parser.add_argument(
        "--no-autoconnect",
        action="store_true",
        help="No abre automaticamente el dispositivo DeskBridge al arrancar la terminal.",
    )
    parser.add_argument(
        "--port",
        help="Puerto COM manual. Si no se indica, se busca por VID/PID 1209:DB01.",
    )
    parser.add_argument("command", nargs="*")
    return parser.parse_args(argv)


def run_single_device_command(shell: CliShell, command: ParsedCommand) -> int:
    try:
        shell.device.connect()
        shell.device.command_channel = "CMD"
        shell.update_window_title()
    except Exception as exc:
        print(f"{theme.error('No se pudo conectar')}: {exc}")
        return 2

    try:
        shell.execute_line(command.raw)
        return 0
    finally:
        shell.device.disconnect()
        shell.update_window_title()


if __name__ == "__main__":
    raise SystemExit(main())
