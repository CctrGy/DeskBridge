"""Reduced command shell used inside the TUI."""

from __future__ import annotations

from contextlib import redirect_stdout
from io import StringIO
import re
import unicodedata

from cli.command_registry import CommandRegistry
from cli.debug import DebugState
from cli.device_commands import is_device_help_request, register_device_commands
from cli.history import History
from cli.language import LanguageManager
from cli.local_commands import register_local_commands
from cli.parser import ParsedCommand, parse_command
from cli import theme
from sdk import DeviceSDK
from sdk.protocol import ProtocolHeader, TuiNotificationMessage, split_header
from sdk.result import CommandResult


DEVICE_ROOT_COMMANDS = {
    "ping",
    "info",
    "usb",
    "program",
    "chip",
    "periferic",
    "peripheral",
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


class TuiNotificationStore:
    """In-memory notification list used only by the TUI."""

    def __init__(self, limit: int = 64) -> None:
        self.limit = max(1, limit)
        self._order: list[str] = []
        self._items: dict[str, TuiNotificationMessage] = {}

    def add(self, message: TuiNotificationMessage) -> None:
        code = normalize_notify_code(message.code)
        item = TuiNotificationMessage(message.kind, message.raw, code, message.summary, message.detail)
        if code not in self._items:
            self._order.append(code)
        self._items[code] = item
        while len(self._order) > self.limit:
            oldest = self._order.pop(0)
            self._items.pop(oldest, None)

    def info_lines(self, limit: int = 3) -> list[str]:
        lines: list[str] = []
        for code in self._order[-max(1, limit) :]:
            item = self._items.get(code)
            if item is not None:
                lines.append(f"TFI {item.code} {item.summary or 'notification'}")
        return lines

    def list_lines(self) -> list[str]:
        if not self._order:
            return ["No peripheral notifications."]
        lines = ["Peripheral notifications"]
        for code in self._order:
            item = self._items[code]
            lines.append(f"  {item.code:<10} {item.summary or 'notification'}")
        return lines

    def detail_lines(self, code: str) -> list[str]:
        item = self._items.get(normalize_notify_code(code))
        if item is None:
            return [f"Notification not found: {code}"]
        return [
            "Peripheral notification",
            f"  code    : {item.code}",
            f"  summary : {item.summary or '-'}",
            f"  detail  : {item.detail or item.summary or '-'}",
            f"  raw     : {item.raw}",
        ]


class TuiCommandShell:
    """CLI-compatible shell subset for the TUI console region."""

    app_name = "DeskBridge TUI"

    def __init__(self, device: DeviceSDK) -> None:
        self.device = device
        self.debug = DebugState()
        self.language = LanguageManager()
        self.history = History(
            limit=self.device.log_manager.config.history_limit,
            visual_size=self.device.log_manager.config.history_visual_size,
        )
        self.registry = CommandRegistry()
        self.clear_requested = False
        self.side_raw_lines: list[str] = []
        self.side_error_lines: list[str] = []
        self.side_event_lines: list[str] = []
        self.notifications = TuiNotificationStore()
        register_local_commands(self)
        self._register_tui_local_overrides()
        register_device_commands(self)

    def execute(self, line: str) -> tuple[bool, list[str]]:
        self.side_raw_lines.clear()
        self.side_error_lines.clear()
        self.side_event_lines.clear()
        if line.strip():
            self.history.add(line.strip())
        output = StringIO()
        with redirect_stdout(output):
            should_continue = self._execute(line)
        lines = [entry for entry in output.getvalue().splitlines() if entry.strip()]
        return should_continue, lines

    def capture_result(self, result: CommandResult) -> None:
        for message in result.messages or []:
            if isinstance(message, TuiNotificationMessage):
                self.record_notification(message)

        if result.raw_response:
            for raw_line in result.raw_response.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
                line = raw_line.strip()
                if not line or line == "db>":
                    continue
                self.side_raw_lines.append(line)
                upper = line.upper()
                if upper.startswith("[ERR]") or upper.startswith("ERR"):
                    self.side_error_lines.append(normalize_error_code(line))
                elif upper.startswith("[EVT]") or upper.startswith("[EVENT"):
                    self.side_event_lines.append(line)

        if not result.ok and result.error:
            self.side_error_lines.append(normalize_error_code(result.error))

    def pop_side_output(self) -> tuple[list[str], list[str], list[str]]:
        raw = list(self.side_raw_lines)
        errors = list(self.side_error_lines)
        events = list(self.side_event_lines)
        self.side_raw_lines.clear()
        self.side_error_lines.clear()
        self.side_event_lines.clear()
        return raw, errors, events

    def _execute(self, line: str) -> bool:
        normalized = normalize_user_line(line)
        try:
            command = parse_command(normalized)
        except ValueError as exc:
            print(f"Invalid command: {exc}")
            return True

        if not command.name:
            return True

        if command.name in {"debug"} or (
            command.name in {"settings", "config"} and command.args and command.args[0].lower() == "debug"
        ):
            print(f"{theme.warning('Unavailable in TUI')}: settings debug")
            return True

        self.device.command_channel = ProtocolHeader.COMMAND
        self.device.log_manager.user("tui command: %s", normalized)
        if command.name in DEVICE_ROOT_COMMANDS and not self.device.connected and not is_device_help_request(command):
            print(f"{theme.warning('Peripheral disconnected')}: use {theme.command('connect')} or F8 first.")
            return True
        result = self.registry.dispatch(command)
        if result is None:
            print(f"{theme.error('Unknown command')}: {theme.command(command.name)}. Use {theme.command('help')}.")
            return True
        return result is not False

    def update_window_title(self) -> None:
        return None

    def prompt(self) -> str:
        return "DB/> "

    def try_auto_connect(self) -> None:
        return None

    def _register_tui_local_overrides(self) -> None:
        self.registry.register("help", lambda command: self._help(command))
        self.registry.register("clear", lambda command: self._clear(), "cls")
        self.registry.register("notify", lambda command: self._notify(command))

    def _help(self, command: ParsedCommand) -> bool:
        scope = command.args[0].lower() if command.args else "all"
        if scope in {"/?", "?", "help", "--help", "-h"}:
            print("Usage: help <local|peripheral|functKeys>")
            return True
        if scope in {"functkeys", "functionkeys", "functionkey", "keys"}:
            self._function_key_help()
            return True
        if scope in {"local", "periferic", "peripheral", "device", "all"}:
            self._show_tui_help(scope)
            return True
        print("Usage: help <local|peripheral|functKeys>")
        return True

    def _show_tui_help(self, scope: str) -> None:
        if scope in {"local", "all"}:
            self._print_local_help()
        if scope == "all":
            print()
        if scope in {"periferic", "peripheral", "device", "all"}:
            if self.device.connected:
                self._print_peripheral_help()
            elif scope != "all":
                print("Peripheral disconnected.")
                return
        print()
        print(self.language.t("help.legend"))

    def _print_local_help(self) -> None:
        print(self.language.t("help.local.title"))
        print_help_header()
        print_help_row("help", "<local|peripheral|functKeys>", self.language.t("help.local.help"))
        print_help_row("connect", "[COMx]", self.language.t("help.local.connect"))
        print_help_row("disconnect", "", self.language.t("help.local.disconnect"))
        print_help_row("reconnect", "[COMx]", self.language.t("help.local.reconnect"))
        print_help_row("status", "", self.language.t("help.local.status"))
        print_help_row("backend", "<start|stop|autorun|notify> [value]", self.language.t("help.local.backend"))
        print_help_row("settings", "<section> [value]", self.language.t("help.local.settings"))
        print_help_row("paths", "[tree]", self.language.t("help.local.paths"))
        print_help_row("history", "<limit|visualSize> [value]", self.language.t("help.local.history"))
        print_help_row("notify", "-list|-view [code]", "peripheral TFI notifications")
        print_help_row("clear", "", self.language.t("help.local.clear"))
        print_help_row("firmware_upload", "<chip>", self.language.t("help.local.firmware_upload"))
        print_help_row("exit", "", self.language.t("help.local.exit"))

    def _print_peripheral_help(self) -> None:
        print(self.language.t("help.periferic.title"))
        print_help_header()
        print_help_row("ping", "", self.language.t("help.periferic.ping"))
        print_help_row("info", "", self.language.t("help.periferic.info"))
        print_help_row("usb", "", self.language.t("help.periferic.usb"))
        print_help_row("program", "", self.language.t("help.periferic.program"))
        print_help_row("peripheral", "<chip:core|keypad|bios|cnio|list>", self.language.t("help.periferic.periferic"))
        print_help_row("bus", "<scan|sensor|timeRead|samples> [value]", self.language.t("help.periferic.bus"))
        print_help_row("keypad", "[action list|action <0..4> <action>]", self.language.t("help.periferic.keypad"))
        print_help_row("light", "<power|field> [value]", self.language.t("help.periferic.light"))
        print_help_row("wireless", "<state|on|off|pair|enabled|name|apply|write|rx> [value]", self.language.t("help.periferic.wireless"))
        print_help_row("reset", "<peripherals|keypad|settings|core>", self.language.t("help.periferic.reset"))
        print_help_row("rtc", "<status|get|set|sync> [value]", self.language.t("help.periferic.rtc"))

    def _function_key_help(self) -> None:
        print(theme.title("TUI controls"))
        print("  ARROW UP/DOWN       scroll SHELL output")
        print("  PAGE UP/DOWN        scroll SHELL output faster")
        print("  HOME/END            jump to oldest/newest SHELL output")
        print("  F5                  refresh language, palete and peripheral search")
        print("  F6                  sync peripheral RTC from PC time")
        print("  F8                  connect/disconnect peripheral")
        print("  F9                  exit TUI")
        print("  F10                 show help")
        print("  F11                 maximize")

    def _clear(self) -> bool:
        self.clear_requested = True
        return True

    def record_notification(self, message: TuiNotificationMessage) -> None:
        self.notifications.add(message)

    def notification_info_lines(self) -> list[str]:
        return self.notifications.info_lines()

    def _notify(self, command: ParsedCommand) -> bool:
        action = command.args[0].lower() if command.args else "-list"
        if action in {"-list", "list"}:
            for line in self.notifications.list_lines():
                print(line)
            return True
        if action in {"-view", "view"}:
            if len(command.args) < 2:
                print("Usage: notify -view [code]")
                return True
            for line in self.notifications.detail_lines(command.args[1]):
                print(line)
            return True
        print("Usage: notify -list | notify -view [code]")
        return True

    def _connect_for_device_command(self) -> bool:
        try:
            port = self.device.connect()
        except Exception as exc:
            print(f"{theme.error('Could not connect')}: {exc}")
            return False
        self.device.log_manager.config.set_connect_state(True)
        print(f"{theme.success('Connected')} {theme.muted(':')} {theme.value(port)}")
        return True

def normalize_user_line(line: str) -> str:
    line = sanitize_command_text(line)
    header, payload = split_header(line)
    if header in {ProtocolHeader.COMMAND.value, ProtocolHeader.SHELL.value} and payload:
        return payload
    return line


def sanitize_command_text(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value)
    return "".join(char for char in normalized if 32 <= ord(char) <= 126)


def normalize_error_code(value: str) -> str:
    text = str(value).strip()
    lowered = text.lower()
    if "timed out waiting" in lowered or "timeout" in lowered:
        return "0000 0000"

    compact_match = re.search(r"\b(?:0x)?([0-9a-fA-F]{8})\b", text)
    if compact_match:
        return format_error_code(compact_match.group(1))

    grouped_match = re.search(r"\b(?:0x)?([0-9a-fA-F]{4})[\s:_-]+(?:0x)?([0-9a-fA-F]{4})\b", text)
    if grouped_match:
        return f"{grouped_match.group(1).upper()} {grouped_match.group(2).upper()}"

    short_match = re.search(r"\b(?:0x)?([0-9a-fA-F]{1,7})\b", text)
    if short_match:
        return format_error_code(short_match.group(1))

    return "FFFF FFFF"


def normalize_notify_code(value: str) -> str:
    text = str(value).strip().upper()
    return text or "TFI-0000"


def format_error_code(value: str) -> str:
    normalized = value.upper().removeprefix("0X").zfill(8)[-8:]
    return f"{normalized[:4]} {normalized[4:]}"


HELP_COMMAND_WIDTH = 16
HELP_ARGS_WIDTH = 42
HELP_DESC_WIDTH = 35
HELP_DESC_START = 2 + HELP_COMMAND_WIDTH + HELP_ARGS_WIDTH


def print_help_header() -> None:
    print(f"  {'COMMAND':<{HELP_COMMAND_WIDTH}}{'ARGS':<{HELP_ARGS_WIDTH}}DESCRIPTION")
    print(f"  {'-' * 91}")


def print_help_row(command: str, args: str, description: str) -> None:
    import textwrap

    desc_lines = textwrap.wrap(description, width=HELP_DESC_WIDTH) or [""]
    print(f"  {command:<{HELP_COMMAND_WIDTH}}{args:<{HELP_ARGS_WIDTH}}{desc_lines[0]}")
    for line in desc_lines[1:]:
        print(f"{' ' * HELP_DESC_START}{line}")
