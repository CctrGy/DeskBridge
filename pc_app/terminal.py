"""Interactive DeskBridge serial shell CLI."""

from __future__ import annotations

import argparse
import configparser
import os
import subprocess
import shlex
import shutil
import sys
from dataclasses import dataclass
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path

from lib.usb import DeskBridgePortNotFound, DeskBridgeUSB, DeskBridgeUSBError


APP_NAME = "DeskBridge PC Shell"
DEVICE_PROMPT = "db>"
DEVICE_PREFIX = "$"
DEFAULT_RESPONSE_TIMEOUT = 2.0
PC_APP_DIR = Path(__file__).resolve().parent
PROJECT_DIR = PC_APP_DIR.parent
FIRMWARE_DIR = PROJECT_DIR / "chip_core"
PLATFORMIO_INI = FIRMWARE_DIR / "platformio.ini"


KNOWN_DEVICE_COMMANDS = (
    "help",
    "ping",
    "info?",
    "usb?",
    "program?",
    "sys?",
    "sys local",
    "sys periferics",
    "sys peripherals",
    "keypad?",
    "bus?",
    "bus scan",
    "bus time_read",
    "bus time_read 1000",
    "bus samples",
    "bus samples 3",
    "bus sensor",
    "bus sensor 0",
    "bus sensor ENS160",
    "rtc?",
    "rtc get",
    "rtc set 2026-05-30T18:45:00",
    "light?",
    "light enabled on",
    "light brightness 16384",
    "light kelvin 32768",
    "light power",
    "light sync",
    "reset keypad",
    "reset settings",
)


@dataclass(slots=True)
class TerminalState:
    usb: DeskBridgeUSB
    auto_read: bool = True
    response_timeout: float = DEFAULT_RESPONSE_TIMEOUT


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    direct_command = " ".join(args.command)
    direct_is_device = bool(direct_command and is_device_command(direct_command))
    state = TerminalState(
        usb=DeskBridgeUSB(port=args.port, timeout=args.serial_timeout),
        auto_read=not args.no_auto_read,
        response_timeout=args.response_timeout,
    )
    update_window_title(state)

    if not args.quiet:
        print_banner()
        if not args.command:
            print("Comandos locales sin prefijo. Comandos al dispositivo con $. Prueba help.")

    if not args.no_connect and (not args.command or direct_is_device):
        connect(state, quiet=args.quiet)
        if state.usb.connected and not args.no_banner:
            print_device_response(read_device(state, timeout=args.banner_timeout))

    try:
        if args.command:
            run_direct_command(state, direct_command, quiet=args.quiet)
        else:
            repl(state)
    except KeyboardInterrupt:
        if not args.quiet:
            print("\nSaliendo.")
    finally:
        state.usb.close()
        update_window_title(state)

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
        help="No conectar al iniciar; usa connect dentro del CLI.",
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
            line = input(prompt_text(state)).strip()
        except EOFError:
            print()
            return

        if not line:
            print_device_text(read_device(state, timeout=0.1))
            continue

        if is_device_command(line):
            send_shell_command(state, strip_device_prefix(line))
            if state.auto_read:
                print_device_response(read_device(state))
            continue

        if not handle_local_command(state, line):
            return


def run_direct_command(state: TerminalState, command: str, *, quiet: bool) -> None:
    if not is_device_command(command):
        handle_local_command(state, command)
        return

    send_shell_command(state, strip_device_prefix(command))
    if state.auto_read:
        response = read_device(state)
        if quiet:
            response = strip_prompt(response)
        print_device_response(response)


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
    elif command in {"clear", "cls"}:
        clear_terminal()
    elif command in {"firmware_upload", "fimeware_upload"}:
        firmware_upload(state, args)
    elif command in {"connect", "open"}:
        port = args[0] if args else None
        connect(state, port=port)
        if state.usb.connected:
            print_device_response(read_device(state, timeout=0.8))
    elif command in {"disconnect", "close"}:
        state.usb.close()
        update_window_title(state)
        print("Desconectado.")
    elif command == "status":
        print_status(state)
    elif command == "read":
        seconds = parse_seconds(args, default=state.response_timeout)
        print_device_response(read_device(state, timeout=seconds))
    elif command == "auto":
        set_auto_read(state, args)
    elif command == "timeout":
        set_response_timeout(state, args)
    elif command == "send":
        if not args:
            print("Uso: send comando")
        else:
            send_shell_command(state, " ".join(args))
            if state.auto_read:
                print_device_response(read_device(state))
    else:
        print(f"Comando local desconocido: {command}. Usa help.")

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
        update_window_title(state)
        if not quiet:
            print(f"USB conectado: {selected_port}")


def send_shell_command(state: TerminalState, command: str) -> None:
    try:
        state.usb.write_line(command)
    except UnicodeEncodeError:
        print("No enviado: la shell solo acepta texto ASCII.")
    except DeskBridgeUSBError as exc:
        print(f"No enviado: {exc}")


def is_device_command(line: str) -> bool:
    return line.startswith(DEVICE_PREFIX)


def strip_device_prefix(line: str) -> str:
    return line[1:].strip()


def prompt_text(state: TerminalState) -> str:
    if state.usb.connected and state.usb.port:
        return f"DESK/{state.usb.port}:>"
    return "DESK:>"


def update_window_title(state: TerminalState) -> None:
    title = APP_NAME
    if state.usb.connected and state.usb.port:
        title = f"{title} {state.usb.port}"
    set_window_title(title)


def set_window_title(title: str) -> None:
    if os.name == "nt":
        try:
            import ctypes

            ctypes.windll.kernel32.SetConsoleTitleW(title)
            return
        except OSError:
            pass
    print(f"\33]0;{title}\a", end="", flush=True)


def clear_terminal() -> None:
    os.system("cls" if os.name == "nt" else "clear")


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


def print_device_response(text: str) -> None:
    print_device_text(strip_prompt(text))


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
        print("Uso: auto on|off")
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
    print("  comando CLI  help, exit, info")
    print(f"  comando chip {DEVICE_PREFIX}ping, {DEVICE_PREFIX}info?")
    print(f"  prompt chip  {DEVICE_PROMPT}")
    print(f"  pyserial     {pyserial_version}")
    print(f"  serial       {serial_status}")
    print(f"  puerto       {state.usb.port or '<auto>'}")
    print(f"  firmware     {FIRMWARE_DIR}")


def package_version(package: str) -> str:
    try:
        return version(package)
    except PackageNotFoundError:
        return "no instalado"


def print_known_commands() -> None:
    print("Comandos del dispositivo")
    for command in KNOWN_DEVICE_COMMANDS:
        print(f"  {command}")


def firmware_upload(state: TerminalState, args: list[str]) -> None:
    environments = platformio_environments()

    if not args:
        print("Uso: firmware_upload <chip>")
        if environments:
            print("Chips configurados:")
            for environment in environments:
                print(f"  {environment}")
        return

    chip = args[0]
    if environments and chip not in environments:
        print(f"Chip no configurado: {chip}")
        print("Chips disponibles:")
        for environment in environments:
            print(f"  {environment}")
        return

    pio = find_platformio_command()
    if pio is None:
        print("No encuentro PlatformIO. Instala PlatformIO o agrega pio/platformio al PATH.")
        return

    if not PLATFORMIO_INI.exists():
        print(f"No existe PlatformIO config: {PLATFORMIO_INI}")
        return

    was_connected = state.usb.connected
    if was_connected:
        print("Cerrando USB serial antes de subir firmware.")
        state.usb.close()
        update_window_title(state)

    command = [*pio, "run", "-e", chip, "-t", "upload"]
    print(f"PlatformIO upload: {chip}")
    print(f"Directorio: {FIRMWARE_DIR}")

    try:
        completed = subprocess.run(command, cwd=FIRMWARE_DIR, check=False)
    except OSError as exc:
        print(f"No se pudo ejecutar PlatformIO: {exc}")
        return

    if completed.returncode == 0:
        print("Upload completado.")
    else:
        print(f"Upload fallo con codigo {completed.returncode}.")

    if was_connected:
        print("Intentando reconectar USB serial.")
        connect(state)
        if state.usb.connected:
            print_device_response(read_device(state, timeout=0.8))


def platformio_environments() -> list[str]:
    if not PLATFORMIO_INI.exists():
        return []

    parser = configparser.ConfigParser()
    parser.optionxform = str
    parser.read(PLATFORMIO_INI)

    environments = []
    for section in parser.sections():
        if section.startswith("env:"):
            environments.append(section.split(":", 1)[1])
    return environments


def find_platformio_command() -> list[str] | None:
    pio = shutil.which("pio")
    if pio:
        return [pio]

    platformio = shutil.which("platformio")
    if platformio:
        return [platformio]

    return None


def print_help() -> None:
    print(
        """
Comandos del CLI
  help                 muestra esta ayuda
  commands             lista comandos conocidos del firmware
  info                 muestra informacion del programa del PC
  firmware_upload <chip>
                       sube firmware con PlatformIO
  connect [COMx]       conecta por VID/PID o por puerto indicado
  disconnect           cierra la conexion
  status               muestra estado de conexion
  read [segundos]      lee texto entrante hasta db> o timeout
  auto on|off          activa/desactiva lectura despues de enviar
  timeout [segundos]   muestra o cambia timeout de respuesta
  send comando         envia un comando al dispositivo
  clear                limpia la terminal
  exit                 salir

Comandos del dispositivo
  Empiezan con $ y se envia al firmware sin ese simbolo, terminado en \\n.
  Ejemplos: $help, $ping, $info?, $bus scan, $bus sensor ENS160, $rtc get

Nota
  fimeware_upload tambien funciona como alias de firmware_upload.
""".strip()
    )


def print_banner() -> None:
    print(f"{APP_NAME}")
    print("Local DESK:>  |  Dispositivo $comando")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
