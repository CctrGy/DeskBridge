"""High-level DeskBridge device client."""

from __future__ import annotations

import logging

from sdk.event_log import DeskBridgeLogManager
from sdk.exceptions import DeskBridgeError, DeviceCommandError, DeviceTimeoutError
from sdk.modules.bus import BusModule
from sdk.modules.core import CoreModule
from sdk.modules.display import DisplayModule
from sdk.modules.keypad import KeypadModule
from sdk.modules.light import LightModule
from sdk.modules.reset import ResetModule
from sdk.modules.rtc import RtcModule
from sdk.modules.system import SystemModule
from sdk.modules.wireless import WirelessModule
from sdk.protocol import ErrorMessage, EventMessage, LogMessage, MarkerMessage, Message, MessageKind, ProtocolHeader, ProtocolParser, TuiNotificationMessage, ValueMessage, ensure_header
from sdk.result import CommandResult
from sdk.transport import SerialTransport


class DeviceSDK:
    """Device client composed from transport, protocol parser, and modules.

    The SDK has no main loop. Applications decide when to call connect(),
    command(), read_message(), or poll().
    """

    def __init__(
        self,
        transport: SerialTransport | None = None,
        parser: ProtocolParser | None = None,
        *,
        response_timeout: float = 2.0,
        firmware_logger: logging.Logger | None = None,
        log_manager: DeskBridgeLogManager | None = None,
    ) -> None:
        self.log_manager = log_manager or DeskBridgeLogManager()
        self.transport = transport or SerialTransport()
        self.parser = parser or ProtocolParser()
        self.response_timeout = response_timeout
        self.firmware_logger = firmware_logger or self.log_manager.peripheral_logger
        self.command_channel = ProtocolHeader.SHELL

        self.core = CoreModule(self)
        self.display = DisplayModule(self)
        self.bus = BusModule(self)
        self.keypad = KeypadModule(self)
        self.light = LightModule(self)
        self.reset = ResetModule(self)
        self.rtc = RtcModule(self)
        self.system = SystemModule(self)
        self.wireless = WirelessModule(self)

        # Common aliases for application code that prefers semantic names.
        self.program = self.core
        self.usb = self.core

    @property
    def connected(self) -> bool:
        return self.transport.connected

    @property
    def port(self) -> str | None:
        return self.transport.port

    def set_port(self, port: str | None) -> None:
        if self.connected:
            self.disconnect()
        self.transport.port = port
        self.log_manager.pc("INFO", "manual serial port selected: %s", port or "<auto>")

    def connect(self) -> str:
        port = self.transport.connect()
        self.log_manager.pc("INFO", "connected to %s", port)
        return port

    def disconnect(self) -> None:
        if self.connected:
            self.log_manager.pc("INFO", "disconnected from %s", self.port or "<unknown>")
        self.transport.disconnect()

    def ping(self) -> CommandResult:
        return self.core.ping()

    def command(self, line: str, *, timeout: float | None = None) -> list[Message]:
        messages, _ = self.command_messages(line, timeout=timeout)
        return messages

    def command_messages(
        self,
        line: str,
        *,
        timeout: float | None = None,
        raise_on_error: bool = True,
    ) -> tuple[list[Message], str]:
        outbound_line = self.prepare_command_line(line)
        self.transport.write_line(outbound_line)
        messages, raw_response = self.read_until_prompt(timeout=timeout)
        errors = [message for message in messages if isinstance(message, ErrorMessage)]
        if errors and raise_on_error:
            raise DeviceCommandError(errors[0].detail, code=errors[0].code, raw=errors[0].raw)
        return [message for message in messages if message.kind is not MessageKind.PROMPT], raw_response

    def prepare_command_line(self, line: str) -> str:
        """Return a serial line with the shell header required by protocol v3."""

        return ensure_header(line, self.command_channel)

    def command_result(
        self,
        command_name: str,
        raw_command: str,
        *,
        timeout: float | None = None,
    ) -> CommandResult:
        outbound_command = self.prepare_command_line(raw_command)
        self.log_manager.pc("DEBUG", "command send: %s", outbound_command)
        try:
            messages, raw_response = self.command_messages(raw_command, timeout=timeout, raise_on_error=False)
        except DeskBridgeError as exc:
            self.log_manager.pc("ERROR", "command failed: %s | %s", outbound_command, exc)
            return CommandResult(
                ok=False,
                command_name=command_name,
                raw_command=outbound_command,
                error=str(exc),
            )

        diagnostics = diagnostics_by_kind(messages)
        errors = diagnostics.get("errors") or []
        if errors:
            error_text = errors[0]["detail"] if isinstance(errors[0], dict) else str(errors[0])
            self.log_manager.pc("ERROR", "command failed: %s | %s", outbound_command, error_text)
            return CommandResult(
                ok=False,
                command_name=command_name,
                raw_command=outbound_command,
                raw_response=raw_response,
                data=values_by_key(messages),
                diagnostics=diagnostics,
                messages=messages,
                error=error_text,
            )

        self.log_manager.pc("DEBUG", "command ok: %s", outbound_command)
        return CommandResult(
            ok=True,
            command_name=command_name,
            raw_command=outbound_command,
            raw_response=raw_response,
            data=values_by_key(messages),
            diagnostics=diagnostics,
            messages=messages,
            message=message_text(messages),
        )

    def read_message(self, *, timeout: float | None = None) -> Message:
        line = self.transport.read_line(timeout=self.response_timeout if timeout is None else timeout)
        message = self.parser.parse_line(line)
        self._route_side_effects(message)
        return message

    def poll(self, *, limit: int = 32, timeout: float = 0.0) -> list[Message]:
        messages: list[Message] = []
        for _ in range(limit):
            try:
                messages.append(self.read_message(timeout=timeout))
            except DeviceTimeoutError:
                break
        return messages

    def read_until_prompt(self, *, timeout: float | None = None) -> tuple[list[Message], str]:
        text = self.transport.read_until(
            f"{self.parser.prompt} ",
            timeout=self.response_timeout if timeout is None else timeout,
        )
        messages = self._parse_response_text(text)
        for message in messages:
            self._route_side_effects(message)
        return messages, text

    def _parse_response_text(self, text: str) -> list[Message]:
        normalized = text.replace("\r\n", "\n").replace("\r", "\n")
        lines = normalized.split("\n")
        messages: list[Message] = []

        for line in lines:
            if not line:
                continue
            if line.endswith(f"{self.parser.prompt} "):
                content = line[: -len(f"{self.parser.prompt} ")].rstrip()
                if content:
                    messages.append(self.parser.parse_line(content))
                messages.append(self.parser.parse_line(self.parser.prompt))
                continue

            message = self.parser.parse_line(line)
            messages.append(message)

        if not messages or messages[-1].kind is not MessageKind.PROMPT:
            messages.append(self.parser.parse_line(self.parser.prompt))
        return messages

    def _route_side_effects(self, message: Message) -> None:
        if isinstance(message, LogMessage):
            self.firmware_logger.info(message.text)


def values_by_key(messages: list[Message]) -> dict[str, object]:
    values: dict[str, object] = {}
    for message in messages:
        if isinstance(message, ValueMessage):
            if message.key in values:
                current = values[message.key]
                if isinstance(current, list):
                    current.append(message.value)
                else:
                    values[message.key] = [current, message.value]
            else:
                values[message.key] = message.value
    return values


def diagnostics_by_kind(messages: list[Message]) -> dict[str, object]:
    diagnostics: dict[str, object] = {}
    errors: list[dict[str, str]] = []
    logs: list[str] = []
    events: list[dict[str, object]] = []
    markers: list[dict[str, str]] = []
    notifications: list[dict[str, str]] = []

    for message in messages:
        if isinstance(message, ErrorMessage):
            errors.append({"code": message.code, "detail": message.detail, "raw": message.raw})
        elif isinstance(message, LogMessage):
            logs.append(message.text)
        elif isinstance(message, EventMessage):
            events.append({"name": message.name, "args": list(message.args), "raw": message.raw})
        elif isinstance(message, MarkerMessage):
            markers.append({"code": message.code, "text": message.text, "raw": message.raw})
        elif isinstance(message, TuiNotificationMessage):
            notifications.append(
                {
                    "code": message.code,
                    "summary": message.summary,
                    "detail": message.detail,
                    "raw": message.raw,
                }
            )

    if errors:
        diagnostics["errors"] = errors
    if logs:
        diagnostics["logs"] = logs
    if events:
        diagnostics["events"] = events
    if markers:
        diagnostics["markers"] = markers
    if notifications:
        diagnostics["notifications"] = notifications
    return diagnostics


def message_text(messages: list[Message]) -> str | None:
    for message in messages:
        if hasattr(message, "text"):
            return str(message.text)
        if hasattr(message, "raw") and message.raw:
            return message.raw
    return None
