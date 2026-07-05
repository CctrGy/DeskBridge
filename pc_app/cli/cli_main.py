"""CLI entry point."""

from __future__ import annotations

import argparse

from sdk import DeviceSDK
from sdk.config_file import AppConfig
from sdk.identity import USB_ID
from sdk.protocol import ProtocolHeader, split_header
from tui.tui_main import run_tui
from gui.gui_main import DeskBridgeGui, ensure_web_assets
from backend.backend_main import main as backend_main

from .parser import ParsedCommand, parse_command
from .shell import CliShell
from .device_commands import is_device_help_request
from . import theme


DEVICE_ROOT_COMMANDS = {
    "ping",
    "info",
    "usb",
    "program",
    "display",
    "chip",
    "periferic",
    "sys",
    "system",
    "bus",
    "keypad",
    "light",
    "reset",
    "rtc",
    "wireless",
    "wireless?",
}


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if args.gui_self_test:
        AppConfig.load()
        DeviceSDK()
        ensure_web_assets()
        print("DeskBridge GUI self-test OK")
        return 0

    if args.backend_self_test:
        return backend_main(["--self-test"])

    if args.install_backend:
        return backend_main(["--install-startup"])

    if args.uninstall_backend:
        return backend_main(["--uninstall-startup"])

    if args.backend:
        backend_args = []
        if args.no_autoconnect:
            backend_args.append("--no-autoconnect")
        return backend_main(backend_args)

    if args.gui or (not args.cli and not args.tui and not args.command):
        return DeskBridgeGui(auto_connect=not args.no_autoconnect, port=args.port).run()

    device = DeviceSDK()

    if args.port:
        device.set_port(args.port)

    if args.tui:
        return run_tui(device, auto_connect=not args.no_autoconnect)

    shell = CliShell(device)

    if not args.command:
        return shell.run(auto_connect=not args.no_autoconnect)

    command_line = normalize_command_line(" ".join(args.command))
    command = parse_command(command_line)

    if command.name in DEVICE_ROOT_COMMANDS:
        if is_device_help_request(command):
            shell.execute_line(command.raw, channel=ProtocolHeader.COMMAND)
            return 0
        return run_single_device_command(shell, command)

    shell.history.add(command_line)
    shell.execute_line(command_line)
    return 0


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="DeskBridge")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--gui",
        action="store_true",
        help="Abre la interfaz grafica. Es el modo por defecto si no se indican argumentos.",
    )
    mode.add_argument(
        "--cli",
        action="store_true",
        help="Abre o ejecuta el CLI.",
    )
    mode.add_argument(
        "--tui",
        action="store_true",
        help="Abre la base del futuro Text User Interface.",
    )
    mode.add_argument(
        "--backend",
        action="store_true",
        help="Ejecuta el backend residente con icono de bandeja.",
    )
    parser.add_argument(
        "--no-autoconnect",
        action="store_true",
        help="No abre automaticamente el dispositivo DeskBridge al arrancar la terminal.",
    )
    parser.add_argument(
        "--port",
        help=f"Puerto COM manual. Si no se indica, se busca por VID/PID {USB_ID}.",
    )
    parser.add_argument("--gui-self-test", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--backend-self-test", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--install-backend", action="store_true", help="Instala el backend al inicio de Windows.")
    parser.add_argument("--uninstall-backend", action="store_true", help="Elimina el backend del inicio de Windows.")
    parser.add_argument("command", nargs="*")
    return parser.parse_args(argv)


def normalize_command_line(line: str) -> str:
    header, payload = split_header(line)
    if header in {ProtocolHeader.COMMAND.value, ProtocolHeader.SHELL.value} and payload:
        return payload
    return line


def run_single_device_command(shell: CliShell, command: ParsedCommand) -> int:
    try:
        shell.device.connect()
        shell.update_window_title()
    except Exception as exc:
        print(f"{theme.error('No se pudo conectar')}: {exc}")
        return 2

    try:
        shell.execute_line(command.raw, channel=ProtocolHeader.COMMAND)
        return 0
    finally:
        shell.device.disconnect()
        shell.update_window_title()


if __name__ == "__main__":
    raise SystemExit(main())
