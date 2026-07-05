"""Interactive CLI shell built on top of DeviceSDK."""

from __future__ import annotations

import os
import unicodedata

from sdk import DeviceSDK
from sdk.protocol import ProtocolHeader, split_header

from .command_registry import CommandRegistry
from .debug import DebugState
from .device_commands import register_device_commands
from .history import History
from .language import LanguageManager
from .local_commands import register_local_commands
from .parser import parse_command
from . import theme


class CliShell:
    app_name = "DeskBridge CLI"

    def __init__(self, device: DeviceSDK | None = None) -> None:
        self.device = device or DeviceSDK()
        self.device.log_manager.set_origin("CLI")
        self.registry = CommandRegistry()
        self.debug = DebugState()
        self.language = LanguageManager()
        self.history = History(
            limit=self.device.log_manager.config.history_limit,
            visual_size=self.device.log_manager.config.history_visual_size,
        )
        register_local_commands(self)
        register_device_commands(self)
        self.device.command_channel = ProtocolHeader.SHELL
        self.update_window_title()

    def run(self, *, auto_connect: bool = True) -> int:
        self.device.log_manager.pc("INFO", "interactive CLI started")
        print(theme.title("DeskBridge CLI"))
        print(self.language.t("cli.started"))
        if auto_connect and self.device.log_manager.config.should_auto_connect:
            self.try_auto_connect()

        while True:
            try:
                line = input(self.prompt()).strip()
            except EOFError:
                print()
                return 0
            except KeyboardInterrupt:
                print()
                return 0

            if not line:
                continue

            self.history.add(line)
            should_continue = self.execute_line(line)
            if should_continue is False:
                return 0

    def execute_line(self, line: str, *, channel: ProtocolHeader | None = None) -> bool:
        line = self.normalize_user_line(line)
        try:
            command = parse_command(line)
        except ValueError as exc:
            print(f"{theme.error('Invalid command')}: {exc}")
            print()
            return True

        if not command.name:
            return True

        self.device.command_channel = channel or ProtocolHeader.SHELL
        self.device.log_manager.user("cli command: %s", line)
        result = self.registry.dispatch(command)
        if result is None:
            print(f"{theme.error('Unknown command')}: {theme.command(command.name)}. Use {theme.command('help')}.")
            print()
            return True

        if result is not False:
            print()
        return result is not False

    def normalize_user_line(self, line: str) -> str:
        line = sanitize_command_text(line)
        header, payload = split_header(line)
        if header in {ProtocolHeader.COMMAND.value, ProtocolHeader.SHELL.value} and payload:
            return payload
        return line

    def try_auto_connect(self) -> None:
        if self.device.connected:
            return
        try:
            port = self.device.connect()
        except Exception as exc:
            self.device.log_manager.config.set_connect_state(False)
            print(f"{theme.warning('USB not connected')}: {exc}")
            print()
            return

        self.update_window_title()
        print(f"{theme.success('USB connected')}: {theme.value(port)}")
        print()

    def prompt(self) -> str:
        if self.device.connected and self.device.port:
            return theme.prompt(f"DESK/{self.device.port}:> ")
        return theme.prompt("DESK:> ")

    def update_window_title(self) -> None:
        title = self.app_name
        if self.device.connected and self.device.port:
            title = f"{title} {self.device.port}"
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


def sanitize_command_text(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value)
    return "".join(char for char in normalized if 32 <= ord(char) <= 126)
