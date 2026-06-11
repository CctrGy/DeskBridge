"""Local CLI commands that do not represent firmware features."""

from __future__ import annotations

import configparser
import os
import shutil
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING

from sdk.event_log import DeskBridgeLogManager

from .parser import ParsedCommand
from . import theme

if TYPE_CHECKING:
    from .shell import CliShell


PC_APP_DIR = Path(__file__).resolve().parents[1]
PROJECT_DIR = PC_APP_DIR.parent
FIRMWARE_DIR = PROJECT_DIR / "chip_core"
PLATFORMIO_INI = FIRMWARE_DIR / "platformio.ini"


def register_local_commands(shell: CliShell) -> None:
    registry = shell.registry
    registry.register("help", lambda command: show_help(shell, command))
    registry.register("exit", lambda command: False, "quit", "q")
    registry.register("clear", lambda command: clear(), "cls")
    registry.register("settings", lambda command: settings(shell, command))
    registry.register("debug", lambda command: settings(shell, prefixed(command, "debug")))
    registry.register("log", lambda command: settings(shell, prefixed(command, "log")))
    registry.register("paths", lambda command: paths(shell, command))
    registry.register("connect", lambda command: connect(shell, command))
    registry.register("disconnect", lambda command: disconnect(shell), "close")
    registry.register("reconnect", lambda command: reconnect(shell, command))
    registry.register("status", lambda command: status(shell))
    registry.register("history", lambda command: history(shell, command))
    registry.register("firmware_upload", lambda command: firmware_upload(command), "fimeware_upload")


def prefixed(command: ParsedCommand, topic: str) -> ParsedCommand:
    return ParsedCommand(command.name, [topic, *command.args], command.raw)


def show_help(shell: CliShell, command: ParsedCommand) -> bool:
    scope = command.args[0].lower() if command.args else "all"
    if scope in {"local", "all"}:
        show_local_help(shell)
    if scope == "all":
        print()
    if scope in {"periferic", "peripheral", "device", "all"}:
        show_periferic_help(shell)
    if scope not in {"local", "periferic", "peripheral", "device", "all"}:
        print(f"Usage: {theme.highlight_usage('help <local|periferic>')}")
        return True
    print()
    print(theme.muted(shell.language.t("help.legend")))
    return True


def show_local_help(shell: CliShell) -> None:
    print(theme.title(shell.language.t("help.local.title")))
    print_help_header(shell)
    print_help_row("help", "<local|periferic>", shell.language.t("help.local.help"))
    print_help_row("connect", "[COMx]", shell.language.t("help.local.connect"))
    print_help_row("disconnect", "", shell.language.t("help.local.disconnect"))
    print_help_row("reconnect", "[COMx]", shell.language.t("help.local.reconnect"))
    print_help_row("status", "", shell.language.t("help.local.status"))
    print_help_row("settings", "<debug|log|language> [value]", shell.language.t("help.local.settings"))
    print_help_row("paths", "[tree]", shell.language.t("help.local.paths"))
    print_help_row("history", "<limit|visualSize> [value]", shell.language.t("help.local.history"))
    print_help_row("clear", "", shell.language.t("help.local.clear"))
    print_help_row("firmware_upload", "<chip>", shell.language.t("help.local.firmware_upload"))
    print_help_row("exit", "", shell.language.t("help.local.exit"))


def show_periferic_help(shell: CliShell) -> None:
    print(theme.title(shell.language.t("help.periferic.title")))
    print_help_header(shell)
    print_help_row("ping", "", shell.language.t("help.periferic.ping"))
    print_help_row("info", "", shell.language.t("help.periferic.info"))
    print_help_row("usb", "", shell.language.t("help.periferic.usb"))
    print_help_row("program", "", shell.language.t("help.periferic.program"))
    print_help_row("periferic", "<chip:core|keypad|bios|cnio|list>", shell.language.t("help.periferic.periferic"))
    print_help_row("bus", "<scan|sensor|timeRead|samples> [value]", shell.language.t("help.periferic.bus"))
    print_help_row("keypad", "", shell.language.t("help.periferic.keypad"))
    print_help_row("light", "<power|field> [value]", shell.language.t("help.periferic.light"))
    print_help_row("reset", "<peripherals|keypad|settings|core>", shell.language.t("help.periferic.reset"))
    print_help_row("rtc", "<status|get|set|synk> [value]", shell.language.t("help.periferic.rtc"))


def clear() -> bool:
    os.system("cls" if os.name == "nt" else "clear")
    return True


def settings(shell: CliShell, command: ParsedCommand) -> bool:
    if not command.args:
        print(theme.title("Settings"))
        print_status_row("debug", "on" if shell.debug.enabled else "off", positive=shell.debug.enabled)
        print_status_row("log", shell.device.log_manager.level_name)
        print_status_row("language", shell.language.current)
        print_status_row("languages", ", ".join(shell.language.names()))
        return True

    topic = command.args[0].lower()
    values = command.args[1:]
    if topic == "debug":
        return settings_debug(shell, values)
    if topic == "log":
        return settings_log(shell, values)
    if topic in {"language", "lang"}:
        return settings_language(shell, values)

    print(f"Usage: {theme.highlight_usage('settings <debug|log|language> [value]')}")
    return True


def settings_debug(shell: CliShell, values: list[str]) -> bool:
    if not values:
        print(f"{theme.variable('debug')} {theme.muted(':')} {format_on_off(shell.debug.enabled)}")
        return True

    value = values[0].lower()
    if value in {"on", "1", "true", "yes", "si"}:
        shell.debug.set(True)
    elif value in {"off", "0", "false", "no"}:
        shell.debug.set(False)
    else:
        print(f"Usage: {theme.highlight_command_line('settings debug <on|off>')}")
        return True

    print(f"{theme.variable('debug')} {theme.muted(':')} {format_on_off(shell.debug.enabled)}")
    return True


def settings_log(shell: CliShell, values: list[str]) -> bool:
    if not values:
        print_log_status(shell)
        return True

    try:
        level = shell.device.log_manager.set_level(values[0])
    except ValueError:
        print(f"{theme.error('Invalid level')}: {theme.value(values[0])}")
        print(f"Levels: {theme.value(DeskBridgeLogManager.levels_text())}")
        return True

    print(f"{theme.variable('log level')} {theme.muted(':')} {theme.value(level)}")
    return True


def settings_language(shell: CliShell, values: list[str]) -> bool:
    if not values:
        print(theme.title("Language"))
        print_status_row("selected", shell.language.current)
        print_status_row("available", ", ".join(shell.language.names()))
        return True

    try:
        shell.language.set_language(values[0])
    except ValueError:
        print(f"{theme.error('Unknown language')}: {theme.value(values[0])}")
        print(f"Available: {theme.value(', '.join(shell.language.names()))}")
        return True

    print(f"{theme.variable('language')} {theme.muted(':')} {theme.value(shell.language.current)}")
    return True


def print_log_status(shell: CliShell) -> None:
    print(theme.title("LOG"))
    print_status_row("level", shell.device.log_manager.level_name)
    print_status_row("file", shell.device.log_manager.relative_log_file)
    print(f"Levels: {theme.value(DeskBridgeLogManager.levels_text())}")


def paths(shell: CliShell, command: ParsedCommand) -> bool:
    if command.args and command.args[0].lower() == "tree":
        return paths_tree()

    print(theme.title("Paths"))
    print_status_row("cwd", Path.cwd())
    print_status_row("program", PC_APP_DIR)
    return True


def paths_tree() -> bool:
    print(theme.title("Project tree"))
    print(r"""U:\DeskBridge\pc_app\
|---data
|   |   data.config        // variables generales
|   |   README.md          // informacion del sistema y proyecto
|   |
|   |---logs
|   |       DD-MM-YYYY.log // log diario creado al arrancar
|   |
|   \---settings
|       |---languaje
|       |       shell_english.json
|       |       shell_español.json
|       |       languaje.config
|       |
|       \---terminal_palete.json
|
\---program.exe            // futuro ejecutable principal""")
    return True


def connect(shell: CliShell, command: ParsedCommand) -> bool:
    if command.args:
        shell.device.set_port(command.args[0])
    try:
        port = shell.device.connect()
    except Exception as exc:
        print(f"{theme.error('Could not connect')}: {exc}")
        return True
    shell.update_window_title()
    print(f"{theme.success('Connected')} {theme.muted(':')} {theme.value(port)}")
    return True


def disconnect(shell: CliShell) -> bool:
    shell.device.disconnect()
    shell.update_window_title()
    print(theme.warning("Disconnected."))
    return True


def reconnect(shell: CliShell, command: ParsedCommand) -> bool:
    shell.device.disconnect()
    return connect(shell, command)


def status(shell: CliShell) -> bool:
    print(theme.title("CLI"))
    print_status_row("connected", "yes" if shell.device.connected else "no", positive=shell.device.connected)
    print_status_row("port", shell.device.port or "<auto>")
    print_status_row("debug", "on" if shell.debug.enabled else "off", positive=shell.debug.enabled)
    print_status_row("log", shell.device.log_manager.level_name)
    print_status_row("log file", shell.device.log_manager.relative_log_file)
    print_status_row("language", shell.language.current)
    print_status_row("history limit", shell.history.limit)
    print_status_row("history visual", shell.history.visual_size)
    return True


def history(shell: CliShell, command: ParsedCommand | None = None) -> bool:
    args = command.args if command is not None else []
    if args:
        return history_settings(shell, args)

    if not shell.history.entries:
        print(theme.muted("No history."))
        return True

    visible = shell.history.entries[-shell.history.visual_size :]
    start_index = len(shell.history.entries) - len(visible) + 1
    if len(shell.history.entries) > len(visible):
        print(theme.muted(f"Showing last {len(visible)} of {len(shell.history.entries)} commands."))
    for offset, entry in enumerate(visible):
        index = start_index + offset
        print(f"{theme.muted(f'{index:>3}')}  {theme.highlight_command_line(entry)}")
    return True


def history_settings(shell: CliShell, args: list[str]) -> bool:
    key = args[0].lower()
    if len(args) == 1:
        if key == "limit":
            print_status_row("history limit", shell.history.limit)
            return True
        if key in {"visualsize", "visual_size"}:
            print_status_row("history visual", shell.history.visual_size)
            return True

    if len(args) != 2 or key not in {"limit", "visualsize", "visual_size"}:
        print(f"Usage: {theme.highlight_usage('history <limit|visualSize> [value]')}")
        return True

    try:
        value = int(args[1])
    except ValueError:
        print(f"{theme.error('Invalid value')}: {theme.value(args[1])}")
        return True

    if key == "limit":
        shell.history.set_limit(value)
        shell.device.log_manager.config.set_history_limit(shell.history.limit)
        print_status_row("history limit", shell.history.limit)
    else:
        shell.history.set_visual_size(value)
        shell.device.log_manager.config.set_history_visual_size(shell.history.visual_size)
        print_status_row("history visual", shell.history.visual_size)
    return True


def firmware_upload(command: ParsedCommand) -> bool:
    environments = platformio_environments()
    if not command.args:
        print(f"Usage: {theme.highlight_usage('firmware_upload <chip>')}")
        if environments:
            print(theme.title("Configured chips"))
            for environment in environments:
                print(f"  {theme.value(environment)}")
        return True

    chip = command.args[0]
    if environments and chip not in environments:
        print(f"{theme.error('Chip is not configured')}: {theme.value(chip)}")
        return True

    pio = find_platformio_command()
    if pio is None:
        print(theme.error("PlatformIO was not found in PATH."))
        return True

    print(f"{theme.title('PlatformIO upload')} {theme.muted(':')} {theme.value(chip)}")
    completed = subprocess.run([*pio, "run", "-e", chip, "-t", "upload"], cwd=FIRMWARE_DIR, check=False)
    print(theme.success("Upload completed.") if completed.returncode == 0 else f"{theme.error('Upload failed')}: {completed.returncode}")
    return True


def platformio_environments() -> list[str]:
    if not PLATFORMIO_INI.exists():
        return []
    parser = configparser.ConfigParser()
    parser.read(PLATFORMIO_INI)
    return [section.split(":", 1)[1] for section in parser.sections() if section.startswith("env:")]


def find_platformio_command() -> list[str] | None:
    pio = shutil.which("pio")
    if pio:
        return [pio]
    platformio = shutil.which("platformio")
    if platformio:
        return [platformio]
    return None


def print_help_header(shell: CliShell) -> None:
    print(
        f"  {theme.muted(shell.language.t('help.columns.command').ljust(17))}"
        f"{theme.muted(shell.language.t('help.columns.args').ljust(42))}"
        f"{theme.muted(shell.language.t('help.columns.description'))}"
    )
    print(f"  {theme.muted('-' * 92)}")


def print_help_row(command: str, args: str, description: str) -> None:
    print(
        f"  {pad_col(command, theme.command(command), 17)}"
        f"{pad_col(args, theme.highlight_usage(args) if args else '', 42)}"
        f"{theme.muted(description)}"
    )


def pad_col(raw: str, rendered: str, width: int) -> str:
    return rendered + (" " * max(0, width - len(raw)))


def print_status_row(name: str, raw_value: object, *, positive: bool | None = None) -> None:
    if positive is True:
        rendered = theme.success(raw_value)
    elif positive is False:
        rendered = theme.error(raw_value)
    else:
        rendered = theme.value(raw_value)
    print(f"  {theme.variable(f'{name:<15}')} {theme.muted(':')} {rendered}")


def format_on_off(enabled: bool) -> str:
    return theme.success("on") if enabled else theme.warning("off")
