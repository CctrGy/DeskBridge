"""Local CLI commands that do not represent firmware features."""

from __future__ import annotations

import configparser
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import TYPE_CHECKING

from backend.startup import install_startup_task, startup_enabled, uninstall_startup_task
from sdk.event_log import DeskBridgeLogManager
from sdk.paths import app_dir, project_dir

from .parser import ParsedCommand
from . import theme

if TYPE_CHECKING:
    from .shell import CliShell


PC_APP_DIR = app_dir()
PROJECT_DIR = project_dir()
FIRMWARE_DIR = PROJECT_DIR / "chip_core"
PLATFORMIO_INI = FIRMWARE_DIR / "platformio.ini"


def register_local_commands(shell: CliShell) -> None:
    registry = shell.registry
    registry.register("help", lambda command: show_help(shell, command))
    registry.register("exit", lambda command: False, "quit", "q")
    registry.register("clear", lambda command: clear(), "cls")
    registry.register("settings", lambda command: settings(shell, command), "config")
    registry.register("debug", lambda command: settings(shell, prefixed(command, "debug")))
    registry.register("log", lambda command: settings(shell, prefixed(command, "log")))
    registry.register("paths", lambda command: paths(shell, command))
    registry.register("connect", lambda command: connect(shell, command))
    registry.register("disconnect", lambda command: disconnect(shell), "close")
    registry.register("reconnect", lambda command: reconnect(shell, command))
    registry.register("status", lambda command: status(shell))
    registry.register("backend", lambda command: backend(command))
    registry.register("history", lambda command: history(shell, command))
    registry.register("firmware_upload", lambda command: firmware_upload(command))


def prefixed(command: ParsedCommand, topic: str) -> ParsedCommand:
    return ParsedCommand(command.name, [topic, *command.args], command.raw)


def show_help(shell: CliShell, command: ParsedCommand) -> bool:
    scope = command.args[0].lower() if command.args else "all"
    if is_help_arg(scope):
        print(f"Usage: {theme.highlight_usage('help <local|peripheral>')}")
        return True
    if scope in {"local", "all"}:
        show_local_help(shell)
    if scope == "all":
        print()
    if scope in {"periferic", "peripheral", "device", "all"}:
        if shell.device.connected:
            show_periferic_help(shell)
        elif scope != "all":
            print(theme.warning("Peripheral disconnected."))
            return True
    if scope not in {"local", "periferic", "peripheral", "device", "all"}:
        print(f"Usage: {theme.highlight_usage('help <local|peripheral>')}")
        return True
    print()
    print(theme.muted(shell.language.t("help.legend")))
    return True


def show_local_help(shell: CliShell) -> None:
    print(theme.title(shell.language.t("help.local.title")))
    print_help_header(shell)
    print_help_row("help", "<local|peripheral>", shell.language.t("help.local.help"))
    print_help_row("connect", "[COMx]", shell.language.t("help.local.connect"))
    print_help_row("disconnect", "", shell.language.t("help.local.disconnect"))
    print_help_row("reconnect", "[COMx]", shell.language.t("help.local.reconnect"))
    print_help_row("status", "", shell.language.t("help.local.status"))
    print_help_row("backend", "<start|stop|autorun|notify> [value]", shell.language.t("help.local.backend"))
    settings_args = "<debug|log|color|palete|language> [value]"
    if is_tui_shell(shell):
        settings_args = "<debug|log|color|functionKey|size|palete|language> [value]"
    print_help_row("settings", settings_args, shell.language.t("help.local.settings"))
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
    print_help_row("display", "[fps [15..50]]", shell.language.t("help.periferic.display"))
    print_help_row("peripheral", "<chip:core|keypad|bios|cnio|list>", shell.language.t("help.periferic.periferic"))
    print_help_row("bus", "<scan|sensor|timeRead|samples> [value]", shell.language.t("help.periferic.bus"))
    print_help_row("keypad", "[action list|action <0..4> <action>]", shell.language.t("help.periferic.keypad"))
    print_help_row("light", "<power|field> [value]", shell.language.t("help.periferic.light"))
    print_help_row("wireless", "<state|on|off|pair|enabled|name|apply|write|rx> [value]", shell.language.t("help.periferic.wireless"))
    print_help_row("reset", "<peripherals|keypad|settings|core>", shell.language.t("help.periferic.reset"))
    print_help_row("rtc", "<status|get|set|sync> [value]", shell.language.t("help.periferic.rtc"))


def clear() -> bool:
    os.system("cls" if os.name == "nt" else "clear")
    return True


def settings(shell: CliShell, command: ParsedCommand) -> bool:
    if not command.args:
        print(theme.title("Settings"))
        print_status_row("debug", "on" if shell.debug.enabled else "off", positive=shell.debug.enabled)
        print_status_row("log", shell.device.log_manager.level_name)
        print_status_row("language", shell.language.current)
        print_status_row("connect", shell.device.log_manager.config.connect_state)
        if is_tui_shell(shell):
            print_status_row("functionKey", "on" if shell.device.log_manager.config.function_key_active else "off")
            print_status_row("size", f"{shell.device.log_manager.config.terminal_cols}x{shell.device.log_manager.config.terminal_lines}")
        return True

    topic = command.args[0].lower()
    values = command.args[1:]
    if is_help_arg(topic):
        return settings_help(shell)
    if topic == "debug":
        if values and is_help_arg(values[0]):
            return settings_debug_help(shell)
        return settings_debug(shell, values)
    if topic == "log":
        if values and is_help_arg(values[0]):
            return settings_log_help(shell)
        return settings_log(shell, values)
    if topic == "color":
        if values and is_help_arg(values[0]):
            return settings_color_help(shell)
        return settings_color(shell, values)
    if topic.lower() in {"functionkey", "function_key"}:
        if not is_tui_shell(shell):
            print(f"{theme.error('Unknown settings section')}: {theme.value(topic)}")
            return True
        if values and is_help_arg(values[0]):
            return settings_function_key_help(shell)
        return settings_function_key(shell, values)
    if topic == "size":
        if not is_tui_shell(shell):
            print(f"{theme.error('Unknown settings section')}: {theme.value(topic)}")
            return True
        if values and is_help_arg(values[0]):
            return settings_size_help(shell)
        return settings_size(shell, values)
    if topic in {"palete", "palette"}:
        if values and is_help_arg(values[0]):
            return settings_palete_help(shell)
        return settings_palete(shell, values)
    if topic in {"language", "lang", "languages"}:
        if values and is_help_arg(values[0]):
            return settings_language_help(shell)
        return settings_language(shell, values)

    usage = "settings <debug|log|color|palete|language> [value]"
    if is_tui_shell(shell):
        usage = "settings <debug|log|color|functionKey|size|palete|language> [value]"
    print(f"Usage: {theme.highlight_usage(usage)}")
    return True


def is_tui_shell(shell: CliShell) -> bool:
    return "TUI" in getattr(shell, "app_name", "").upper()


def is_help_arg(value: str) -> bool:
    return value.lower() in {"/?", "?", "help", "--help", "-h"}


def settings_help(shell: CliShell) -> bool:
    print(theme.title("Settings help"))
    print_help_header(shell)
    print_help_row("settings", "", "show current CLI settings")
    print_help_row("settings", "debug <on|off>", "enable or disable debug output")
    print_help_row("settings", "log [DEBUG|INFO|USER|WARNING|ERROR|CRITICAL]", "show or change PC log level")
    print_help_row("settings", "color <on|off>", "enable or disable terminal colors")
    if is_tui_shell(shell):
        print_help_row("settings", "functionKey <on|off>", "enable or disable TUI function keys")
        print_help_row("settings", "size <cols> <lines>", "set terminal size used on next startup")
    print_help_row("settings", "palete <name|list|reload|status>", "show, select, or reload color palettes")
    print_help_row("settings", "language <name|list|reload|status>", "show, select, or reload languages")
    print()
    print(theme.muted("Secondary help: settings <section> /?"))
    return True


def settings_debug_help(shell: CliShell) -> bool:
    print(theme.title("Settings debug"))
    print_help_header(shell)
    print_help_row("settings", "debug", "show debug state")
    print_help_row("settings", "debug <on|off>", "enable or disable debug output")
    return True


def settings_log_help(shell: CliShell) -> bool:
    print(theme.title("Settings log"))
    print_help_header(shell)
    print_help_row("settings", "log", "show log status")
    print_help_row("settings", "log <level>", "set PC log level")
    print()
    print(f"Levels: {theme.value(DeskBridgeLogManager.levels_text())}")
    return True


def settings_color_help(shell: CliShell) -> bool:
    print(theme.title("Settings color"))
    print_help_header(shell)
    print_help_row("settings", "color", "show color state")
    print_help_row("settings", "color <on|off>", "enable or disable terminal colors")
    return True


def settings_function_key_help(shell: CliShell) -> bool:
    print(theme.title("Settings functionKey"))
    print_help_header(shell)
    print_help_row("settings", "functionKey", "show TUI function-key state")
    print_help_row("settings", "functionKey <on|off>", "enable or disable F5/F6/F8/F9/F10/F11")
    print()
    print(theme.muted("Arrow scroll remains active."))
    return True


def settings_size_help(shell: CliShell) -> bool:
    print(theme.title("Settings size"))
    print_help_header(shell)
    print_help_row("settings", "size", "show configured terminal size")
    print_help_row("settings", "size <cols> <lines>", "save terminal size for next startup")
    return True


def settings_palete_help(shell: CliShell) -> bool:
    print(theme.title("Settings palete"))
    print_help_header(shell)
    print_help_row("settings", "palete", "show selected palette details")
    print_help_row("settings", "palete list", "list available palette files")
    print_help_row("settings", "palete reload", "rescan palete.config and JSON files")
    print_help_row("settings", "palete <name>", "select a palette by id")
    print()
    print(f"Available: {theme.value(', '.join(theme.palette_names()))}")
    return True


def settings_language_help(shell: CliShell) -> bool:
    print(theme.title("Settings language"))
    print_help_header(shell)
    print_help_row("settings", "language", "show selected language details")
    print_help_row("settings", "language list", "list available language files")
    print_help_row("settings", "language reload", "rescan language.config and JSON files")
    print_help_row("settings", "language <name>", "select a language by id")
    print()
    print(theme.muted("Alias: settings languages <list|reload>"))
    print(f"Available: {theme.value(', '.join(shell.language.names()))}")
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


def settings_color(shell: CliShell, values: list[str]) -> bool:
    config = shell.device.log_manager.config
    if not values:
        print_status_row("color", "on" if config.terminal_color else "off", positive=config.terminal_color)
        return True

    value = values[0].lower()
    if value in {"on", "1", "true", "yes", "si"}:
        config.set_terminal_color(True)
        theme.reload()
    elif value in {"off", "0", "false", "no"}:
        config.set_terminal_color(False)
        theme.reload()
    else:
        print(f"Usage: {theme.highlight_command_line('settings color <on|off>')}")
        return True

    print_status_row("color", "on" if config.terminal_color else "off", positive=config.terminal_color)
    return True


def settings_function_key(shell: CliShell, values: list[str]) -> bool:
    config = shell.device.log_manager.config
    if not values:
        print_status_row("functionKey", "on" if config.function_key_active else "off", positive=config.function_key_active)
        return True

    value = values[0].lower()
    if value in {"active", "on", "1", "true", "yes", "si"}:
        config.set_function_key_active(True)
    elif value in {"disabled", "disable", "off", "0", "false", "no"}:
        config.set_function_key_active(False)
    else:
        print(f"Usage: {theme.highlight_command_line('settings functionKey <on|off>')}")
        return True

    print_status_row("functionKey", "on" if config.function_key_active else "off", positive=config.function_key_active)
    return True


def settings_size(shell: CliShell, values: list[str]) -> bool:
    config = shell.device.log_manager.config
    if not values:
        print_status_row("size", f"{config.terminal_cols}x{config.terminal_lines}")
        return True
    if len(values) != 2:
        print(f"Usage: {theme.highlight_usage('settings size <cols> <lines>')}")
        return True
    try:
        cols = int(values[0])
        lines = int(values[1])
    except ValueError:
        print(f"{theme.error('Invalid size')}: {theme.value(' '.join(values))}")
        return True
    config.set_terminal_size(cols, lines)
    print_status_row("size", f"{config.terminal_cols}x{config.terminal_lines}")
    return True


def settings_palete(shell: CliShell, values: list[str]) -> bool:
    config = shell.device.log_manager.config
    if not values or values[0].lower() == "status":
        print(theme.title("Palete"))
        print_status_row("selected", theme.normalize_palette_name(config.terminal_palette))
        print_status_row("available", ", ".join(theme.palette_names()))
        return True

    action = values[0].lower()
    if action == "list":
        print(theme.title("Paletes"))
        selected = theme.normalize_palette_name(config.terminal_palette)
        for name in theme.palette_names():
            marker = "*" if selected == name else " "
            print(f"  {theme.muted(marker)} {theme.value(name)}")
        return True

    if action == "reload":
        theme.reload()
        print(theme.success("Paletes reloaded."))
        return settings_palete(shell, ParsedCommand("palete", ["list"], "settings palete list").args)

    try:
        theme.set_palette(action)
    except ValueError:
        print(f"Usage: {theme.highlight_command_line('settings palete <name|list|reload|status>')}")
        print(f"Available: {theme.value(', '.join(theme.palette_names()))}")
        return True

    selected = theme.normalize_palette_name(action)
    config.values["terminal_palette"] = selected
    print_status_row("palete", selected)
    return True


def settings_language(shell: CliShell, values: list[str]) -> bool:
    if not values or values[0].lower() == "status":
        print_language_status(shell)
        return True

    action = values[0].lower()
    if action == "list":
        print_languages(shell)
        return True
    if action == "reload":
        shell.language.reload()
        print(theme.success("Languages reloaded."))
        print_languages(shell)
        return True

    try:
        shell.language.set_language(values[0])
    except ValueError:
        print(f"{theme.error('Unknown language')}: {theme.value(values[0])}")
        print(f"Available: {theme.value(', '.join(shell.language.names()))}")
        return True

    print(f"{theme.variable('language')} {theme.muted(':')} {theme.value(shell.language.current)}")
    return True


def print_language_status(shell: CliShell) -> None:
    print(theme.title("Language"))
    print_status_row("selected", shell.language.current)
    print_status_row("available", ", ".join(shell.language.names()))


def print_languages(shell: CliShell) -> None:
    print(theme.title("Languages"))
    for name in shell.language.names():
        marker = "*" if name == shell.language.current else " "
        file_name = shell.language.available.get(name, "")
        print(f"  {theme.muted(marker)} {theme.value(name)} {theme.muted(file_name)}")


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
|       |---language
|       |       shell_english.json
|       |       shell_español.json
|       |       language.config
|       |
|       \---palete
|               palete.config
|               dark.json
|               light.json
|
\---program.exe            // futuro ejecutable principal""")
    return True


def connect(shell: CliShell, command: ParsedCommand) -> bool:
    if command.args:
        shell.device.set_port(command.args[0])
    try:
        port = shell.device.connect()
    except Exception as exc:
        shell.device.log_manager.config.set_connect_state(False)
        print(f"{theme.error('Could not connect')}: {exc}")
        return True
    shell.device.log_manager.config.set_connect_state(True)
    shell.update_window_title()
    print(f"{theme.success('Connected')} {theme.muted(':')} {theme.value(port)}")
    return True


def disconnect(shell: CliShell) -> bool:
    shell.device.disconnect()
    shell.device.log_manager.config.set_connect_state(False)
    shell.update_window_title()
    print(theme.warning("Disconnected."))
    return True


def reconnect(shell: CliShell, command: ParsedCommand) -> bool:
    shell.device.disconnect()
    return connect(shell, command)


def status(shell: CliShell) -> bool:
    print(theme.title("CLI"))
    print_status_row("connected", "yes" if shell.device.connected else "no", positive=shell.device.connected)
    print_status_row("auto connect", shell.device.log_manager.config.connect_state)
    print_status_row("port", shell.device.port or "<auto>")
    print_status_row("debug", "on" if shell.debug.enabled else "off", positive=shell.debug.enabled)
    print_status_row("log", shell.device.log_manager.level_name)
    print_status_row("log file", shell.device.log_manager.relative_log_file)
    print_status_row("language", shell.language.current)
    print_status_row("history limit", shell.history.limit)
    print_status_row("history visual", shell.history.visual_size)
    return True


def backend(command: ParsedCommand) -> bool:
    if command.args and is_help_arg(command.args[0]):
        return backend_help()
    if not command.args:
        print(theme.title("Backend"))
        print_status_row("autorun", "on" if startup_enabled() else "off", positive=startup_enabled())
        print_status_row("process", "running" if backend_is_running() else "stopped", positive=backend_is_running())
        return True

    action = command.args[0].lower()
    if action == "start":
        start_backend_process()
        print_status_row("backend", "running", positive=True)
        return True
    if action == "stop":
        stopped = stop_backend_process()
        print_status_row("backend", f"stopped {stopped}", positive=True)
        return True
    if action == "autorun":
        if len(command.args) != 2:
            print(f"Usage: {theme.highlight_usage('backend autorun <on|off>')}")
            return True
        value = command.args[1].lower()
        if value == "on":
            install_startup_task()
            print_status_row("autorun", "on", positive=True)
            return True
        if value == "off":
            uninstall_startup_task()
            print_status_row("autorun", "off", positive=False)
            return True
        print(f"Usage: {theme.highlight_usage('backend autorun <on|off>')}")
        return True
    if action == "notify":
        return backend_notify(command.args[1:])

    print(f"Usage: {theme.highlight_usage('backend <start|stop|autorun|notify> [value]')}")
    return True


def backend_help() -> bool:
    print(theme.title("Backend"))
    print(f"  {theme.muted('COMMAND'.ljust(16))}{theme.muted('ARGS'.ljust(45))}{theme.muted('DESCRIPTION')}")
    print(f"  {theme.muted('-' * 92)}")
    print_help_row("backend", "", "show backend state")
    print_help_row("backend", "start", "start resident background process")
    print_help_row("backend", "stop", "stop resident background process")
    print_help_row("backend", "autorun <on|off>", "enable or disable Windows startup")
    print_help_row("backend", "notify", "show notification filter states")
    print_help_row("backend", "notify <type> <active|disabled>", "change notification filter")
    return True


def backend_notify(args: list[str]) -> bool:
    config = DeskBridgeLogManager().config
    if not args:
        print(theme.title("Backend notifications"))
        for name in backend_notify_names():
            enabled = backend_notify_enabled(config, name)
            print_status_row(name, "active" if enabled else "disabled", positive=enabled)
        return True

    if len(args) != 2:
        print(f"Usage: {theme.highlight_usage('backend notify <temperature|humidity|co2|lux> <active|disabled>')}")
        return True

    name = normalize_backend_notify_name(args[0])
    if name is None:
        print(f"{theme.error('Unknown notification')}: {theme.value(args[0])}")
        print(f"Available: {theme.value(', '.join(backend_notify_names()))}")
        return True

    state = normalize_active_state(args[1])
    if state is None:
        print(f"Usage: {theme.highlight_usage('backend notify <temperature|humidity|co2|lux> <active|disabled>')}")
        return True

    config.values[f"backend_notify_{name}"] = "on" if state else "off"
    config.save()
    print_status_row(name, "active" if state else "disabled", positive=state)
    return True


def backend_notify_names() -> list[str]:
    return ["temperature", "humidity", "co2", "lux"]


def backend_notify_enabled(config, name: str) -> bool:
    value = str(config.values.get(f"backend_notify_{name}") or "on").lower()
    return value in {"active", "on", "1", "true", "yes", "si"}


def normalize_backend_notify_name(value: str) -> str | None:
    normalized = value.lower()
    aliases = {
        "temperature": "temperature",
        "temp": "temperature",
        "humidity": "humidity",
        "hum": "humidity",
        "co2": "co2",
        "carbon": "co2",
        "lux": "lux",
        "light": "lux",
    }
    return aliases.get(normalized)


def normalize_active_state(value: str) -> bool | None:
    normalized = value.lower()
    if normalized in {"active", "on", "1", "true", "yes", "si"}:
        return True
    if normalized in {"disabled", "disable", "off", "0", "false", "no"}:
        return False
    return None


def start_backend_process() -> None:
    if getattr(sys, "frozen", False):
        subprocess.Popen([sys.executable, "--backend"], close_fds=True)
        return
    subprocess.Popen([sys.executable, str(PC_APP_DIR / "terminal.py"), "--backend"], close_fds=True)


def backend_is_running() -> bool:
    return backend_process_count() > 0


def stop_backend_process() -> int:
    command = (
        "$self=$PID; "
        "$items=Get-CimInstance Win32_Process | Where-Object { "
        "$_.CommandLine -match '--backend' -and $_.ProcessId -ne $self -and "
        "($_.Name -eq 'python.exe' -or $_.Name -eq 'pythonw.exe' -or $_.Name -eq 'DeskBridge.exe') "
        "}; "
        "$count=0; "
        "foreach($item in $items){ $result=Invoke-CimMethod -InputObject $item -MethodName Terminate; if($result.ReturnValue -eq 0){ $count++ } }; "
        "Write-Output $count"
    )
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-Command", command],
        check=False,
        capture_output=True,
        text=True,
    )
    try:
        return int((completed.stdout or "0").strip().splitlines()[-1])
    except (ValueError, IndexError):
        return 0


def backend_process_count() -> int:
    command = (
        "$self=$PID; "
        "$items=Get-CimInstance Win32_Process | Where-Object { "
        "$_.CommandLine -match '--backend' -and $_.ProcessId -ne $self -and "
        "($_.Name -eq 'python.exe' -or $_.Name -eq 'pythonw.exe' -or $_.Name -eq 'DeskBridge.exe') "
        "}; "
        "Write-Output @($items).Count"
    )
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-Command", command],
        check=False,
        capture_output=True,
        text=True,
    )
    try:
        return int((completed.stdout or "0").strip().splitlines()[-1])
    except (ValueError, IndexError):
        return 0


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
        f"  {theme.muted(shell.language.t('help.columns.command').ljust(16))}"
        f"{theme.muted(shell.language.t('help.columns.args').ljust(45))}"
        f"{theme.muted(shell.language.t('help.columns.description'))}"
    )
    print(f"  {theme.muted('-' * 92)}")


def print_help_row(command: str, args: str, description: str) -> None:
    print(
        f"  {pad_col(command, theme.command(command), 16)}"
        f"{pad_col(args, theme.highlight_usage(args) if args else '', 45)}"
        f"{theme.muted(description)}"
    )


def pad_col(raw: str, rendered: str, width: int) -> str:
    return rendered + (" " * max(2, width - len(raw)))


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
