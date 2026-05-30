"""Interactive DeskBridge serial shell CLI."""

from __future__ import annotations

import argparse
import shlex
import sys
from dataclasses import dataclass
from importlib.metadata import PackageNotFoundError, version

from lib.usb import DeskBridgePortNotFound, DeskBridgeUSB, DeskBridgeUSBError


APP_NAME = "DeskBridge PC Shell"
LOCAL_PROMPT = "pc# "
DEVICE_PROMPT = "db>"
LOCAL_PREFIX = "#"
LEGACY_LOCAL_PREFIX = ":"
DEFAULT_RESPONSE_TIMEOUT = 2.0


KNOWN_DEVICE_COMMANDS = (
    "ping",
    "info?",
    "usb?",
    "program?",
    "sys?",
    "sys local",
    "sys periferics",
    "sys peripherals",
    "bus?",
    "bus scan",
    "bus time_read",
    "bus time_read 1000",
    "bus samples",
    "bus samples 3",
    "sensor?",
    "sensor 0",
    "sensor ENS160",
    "rtc?",
    "rtc get",
    "rtc set 2026-05-30T18:45:00",
)


@dataclass(slots=True)
class TerminalState:
    usb: DeskBridgeUSB
    auto_read: bool = True
    response_timeout: float = DEFAULT_RESPONSE_TIMEOUT


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    state = TerminalState(
        usb=DeskBridgeUSB(port=args.port, timeout=args.serial_timeout),
        auto_read=not args.no_auto_read,
        response_timeout=args.response_timeout,
    )

    if not args.quiet:
        print_banner()
        if not args.command:
            print("Comandos del CLI empiezan por #. Prueba #help.")

    if not args.no_connect:
        connect(state, quiet=args.quiet)
        if state.usb.connected and not args.no_banner:
            print_device_text(read_device(state, timeout=args.banner_timeout))

    try:
        if args.command:
            run_direct_command(state, " ".join(args.command), quiet=args.quiet)
        else:
            repl(state)
    except KeyboardInterrupt:
        if not args.quiet:
            print("\nSaliendo.")
    finally:
        state.usb.close()

    return 0


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="CLI para la shell serial ASCII de DeskBridge."
    )
    parser.add_argument(
        "--port",
        help="Puerto serie concreto, por ejemplo COM5. Si se omite, busca VID/PID.",
    )
    parser.add_argument(
        "--serial-timeout",
        type=float,
        default=0.1,
        help="Timeout base del puerto serie en segundos.",
    )
    parser.add_argument(
        "--response-timeout",
        type=float,
        default=DEFAULT_RESPONSE_TIMEOUT,
        help="Tiempo maximo esperando la respuesta y el prompt db>.",
    )
    parser.add_argument(
        "--banner-timeout",
        type=float,
        default=0.8,
        help="Tiempo maximo esperando el mensaje inicial del firmware.",
    )
    parser.add_argument(
        "--no-connect",
        action="store_true",
        help="No conectar al iniciar; usa :connect dentro del CLI.",
    )
    parser.add_argument(
        "--no-banner",
        action="store_true",
        help="No leer el mensaje inicial despues de conectar.",
    )
    parser.add_argument(
        "--no-auto-read",
        action="store_true",
        help="No leer respuesta despues de enviar un comando.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Mostrar solo la respuesta del dispositivo.",
    )
    parser.add_argument(
        "command",
        nargs="*",
        help="Comando directo a enviar. Si se omite, abre el modo interactivo.",
    )
    return parser.parse_args(argv)


def repl(state: TerminalState) -> None:
    while True:
        try:
            line = input(LOCAL_PROMPT).strip()
        except EOFError:
            print()
            return

        if not line:
            print_device_text(read_device(state, timeout=0.1))
            continue

        if is_local_command(line):
            if not handle_local_command(state, strip_local_prefix(line)):
                return
            continue

        send_shell_command(state, line)
        if state.auto_read:
            print_device_text(read_device(state))


def run_direct_command(state: TerminalState, command: str, *, quiet: bool) -> None:
    send_shell_command(state, command)
    if state.auto_read:
        response = read_device(state)
        if quiet:
            response = strip_prompt(response)
        print_device_text(response)


def handle_local_command(state: TerminalState, command_line: str) -> bool:
    try:
        parts = shlex.split(command_line)
    except ValueError as exc:
        print(f"Comando invalido: {exc}")
        return True

    if not parts:
        return True

    command = parts[0].lower()
    args = parts[1:]

    if command in {"q", "quit", "exit"}:
        return False
    if command in {"h", "help", "?"}:
        print_help()
    elif command == "info":
        print_program_info(state)
    elif command == "commands":
        print_known_commands()
    elif command in {"connect", "open"}:
        port = args[0] if args else None
        connect(state, port=port)
        if state.usb.connected:
            print_device_text(read_device(state, timeout=0.8))
    elif command in {"disconnect", "close"}:
        state.usb.close()
        print("Desconectado.")
    elif command == "status":
        print_status(state)
    elif command == "read":
        seconds = parse_seconds(args, default=state.response_timeout)
        print_device_text(read_device(state, timeout=seconds))
    elif command == "auto":
        set_auto_read(state, args)
    elif command == "timeout":
        set_response_timeout(state, args)
    elif command == "send":
        if not args:
            print("Uso: :send comando")
        else:
            send_shell_command(state, " ".join(args))
            if state.auto_read:
                print_device_text(read_device(state))
    else:
        print(f"Comando local desconocido: {LOCAL_PREFIX}{command}. Usa #help.")

    return True


def connect(
    state: TerminalState,
    *,
    port: str | None = None,
    quiet: bool = False,
) -> None:
    if port is not None:
        state.usb.close()
        state.usb.port = port

    try:
        selected_port = state.usb.connect()
    except DeskBridgePortNotFound as exc:
        if not quiet:
            print(f"No se encontro el dispositivo: {exc}")
    except DeskBridgeUSBError as exc:
        if not quiet:
            print(f"No se pudo conectar: {exc}")
    else:
        if not quiet:
            print(f"USB conectado: {selected_port}")


def send_shell_command(state: TerminalState, command: str) -> None:
    try:
        state.usb.write_line(command)
    except UnicodeEncodeError:
        print("No enviado: la shell solo acepta texto ASCII.")
    except DeskBridgeUSBError as exc:
        print(f"No enviado: {exc}")


def is_local_command(line: str) -> bool:
    return line.startswith(LOCAL_PREFIX) or line.startswith(LEGACY_LOCAL_PREFIX)


def strip_local_prefix(line: str) -> str:
    return line[1:].strip()


def read_device(state: TerminalState, *, timeout: float | None = None) -> str:
    if not state.usb.connected:
        return ""

    try:
        return state.usb.read_until_prompt(
            DEVICE_PROMPT,
            timeout=state.response_timeout if timeout is None else timeout,
        )
    except DeskBridgeUSBError as exc:
        return f"No se pudo leer: {exc}\n"


def print_device_text(text: str) -> None:
    if not text:
        return
    print(normalize_newlines(text), end="" if text.endswith(("\n", "\r")) else "\n")


def normalize_newlines(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def strip_prompt(text: str) -> str:
    text = normalize_newlines(text)
    if text.rstrip().endswith(DEVICE_PROMPT):
        text = text.rstrip()[: -len(DEVICE_PROMPT)].rstrip()
    return text + ("\n" if text else "")


def parse_seconds(args: list[str], *, default: float) -> float:
    if not args:
        return default
    try:
        return max(0.0, float(args[0]))
    except ValueError:
        print(f"Tiempo invalido: {args[0]}. Usando {default}s.")
        return default


def set_auto_read(state: TerminalState, args: list[str]) -> None:
    if not args:
        print(f"auto-read: {'on' if state.auto_read else 'off'}")
        return

    value = args[0].lower()
    if value in {"on", "1", "true", "yes", "si"}:
        state.auto_read = True
    elif value in {"off", "0", "false", "no"}:
        state.auto_read = False
    else:
        print("Uso: :auto on|off")
        return

    print(f"auto-read: {'on' if state.auto_read else 'off'}")


def set_response_timeout(state: TerminalState, args: list[str]) -> None:
    if not args:
        print(f"timeout: {state.response_timeout}s")
        return
    state.response_timeout = parse_seconds(args, default=state.response_timeout)
    print(f"timeout: {state.response_timeout}s")


def print_status(state: TerminalState) -> None:
    status = "conectado" if state.usb.connected else "desconectado"
    port = state.usb.port or "<auto>"
    print("CLI")
    print(f"  estado       {status}")
    print(f"  puerto       {port}")
    print(f"  auto-read    {'on' if state.auto_read else 'off'}")
    print(f"  timeout      {state.response_timeout}s")


def print_program_info(state: TerminalState) -> None:
    pyserial_version = package_version("pyserial")
    serial_status = "conectado" if state.usb.connected else "desconectado"
    print(APP_NAME)
    print("  modo         shell ASCII USB")
    print(f"  comando CLI  {LOCAL_PREFIX}help, {LOCAL_PREFIX}exit, {LOCAL_PREFIX}info")
    print(f"  prompt chip  {DEVICE_PROMPT}")
    print(f"  pyserial     {pyserial_version}")
    print(f"  serial       {serial_status}")
    print(f"  puerto       {state.usb.port or '<auto>'}")


def package_version(package: str) -> str:
    try:
        return version(package)
    except PackageNotFoundError:
        return "no instalado"


def print_known_commands() -> None:
    print("Comandos del dispositivo")
    for command in KNOWN_DEVICE_COMMANDS:
        print(f"  {command}")


def print_help() -> None:
    print(
        """
Comandos del CLI
  #help                muestra esta ayuda
  #commands            lista comandos conocidos del firmware
  #info                muestra informacion del programa del PC
  #connect [COMx]      conecta por VID/PID o por puerto indicado
  #disconnect          cierra la conexion
  #status              muestra estado de conexion
  #read [segundos]     lee texto entrante hasta db> o timeout
  #auto on|off         activa/desactiva lectura despues de enviar
  #timeout [segundos]  muestra o cambia timeout de respuesta
  #send comando        envia un comando aunque empiece por #
  #exit                salir

Comandos del dispositivo
  Cualquier texto sin # se envia al firmware como ASCII terminado en \\n.
  Ejemplos: ping, info?, bus scan, sensor ENS160, rtc get

Nota
  El prefijo antiguo ":" todavia funciona, pero # sera el formato principal.
""".strip()
    )


def print_banner() -> None:
    print(f"{APP_NAME}")
    print("Shell local #  |  Shell dispositivo db>")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
