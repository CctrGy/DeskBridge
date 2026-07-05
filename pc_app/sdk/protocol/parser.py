"""Line parser for the DeskBridge firmware shell."""

from __future__ import annotations

import re

from .headers import KNOWN_FIRMWARE_HEADERS
from .messages import (
    ErrorMessage,
    EventMessage,
    LogMessage,
    MarkerMessage,
    Message,
    MessageKind,
    PromptMessage,
    ResponseMessage,
    TuiNotificationMessage,
    UnknownMessage,
    ValueMessage,
)


class ProtocolParser:
    """Classify firmware shell lines into typed messages."""

    prompt = "db>"

    _channel_re = re.compile(
        rf"^\[({'|'.join(sorted(KNOWN_FIRMWARE_HEADERS))})]\s*(.*)$",
        re.IGNORECASE,
    )
    _event_re = re.compile(r"^\[EVENT\s+(.+)]$")
    _log_re = re.compile(r"^\[LOG]\s*(.*)$")
    _value_re = re.compile(r"^\[value\s+([^\s]+)\s+(.+)]$")
    _header_re = re.compile(r"^\[([^\]]+)]$")
    _kv_re = re.compile(r"^\s*([^:]+?)\s*:\s*(.+?)\s*$")
    _list_re = re.compile(r"^\s*([a-zA-Z_][\w]*)\s*:\s*(.+?)\s*$")

    def parse_line(self, line: str) -> Message:
        raw = line.rstrip("\r\n")
        text = raw.strip()

        if text in {self.prompt, f"{self.prompt}"}:
            return PromptMessage(MessageKind.PROMPT, raw, text)

        if not text:
            return UnknownMessage(MessageKind.UNKNOWN, raw, raw)

        channel_match = self._channel_re.match(text)
        if channel_match:
            channel = channel_match.group(1).upper()
            payload = channel_match.group(2).strip()
            if channel == "RSP":
                return self.parse_line(payload)
            if channel == "ERR":
                code, detail = split_code_payload(payload)
                return ErrorMessage(MessageKind.ERROR, raw, code, detail)
            if channel == "LOG":
                return LogMessage(MessageKind.LOG, raw, payload)
            if channel == "EVT":
                tokens = tuple(payload.split())
                name = tokens[0] if tokens else ""
                return EventMessage(MessageKind.EVENT, raw, name, tokens[1:])
            if channel == "TFI":
                code, summary, detail = split_notification_payload(payload)
                return TuiNotificationMessage(MessageKind.TUI_NOTIFICATION, raw, code, summary, detail)
            if channel == "MRK":
                code, text_value = split_code_payload(payload)
                return MarkerMessage(MessageKind.DATA, raw, code, text_value)
            if channel in {"DAT", "STE", "STK"}:
                if ":" in payload:
                    key, value = payload.split(":", 1)
                    parsed_value = self.parse_mapping(value.strip()) if ":" in value else self.parse_scalar(value)
                    return ValueMessage(MessageKind.VALUE, raw, key.strip(), parsed_value)
                return ValueMessage(MessageKind.VALUE, raw, channel.lower(), self.parse_scalar(payload))
            if channel in {"MRK", "INF", "TSK"}:
                return ResponseMessage(MessageKind.DATA, raw, True, payload)
            return ResponseMessage(MessageKind.DATA, raw, True, payload)

        if text.startswith("ERR"):
            parts = text.split(maxsplit=1)
            code, detail = split_code_payload(parts[1] if len(parts) > 1 else "")
            return ErrorMessage(MessageKind.ERROR, raw, code, detail)

        if text.startswith("OK"):
            return ResponseMessage(MessageKind.RESPONSE, raw, True, text)

        if text == "pong":
            return ResponseMessage(MessageKind.RESPONSE, raw, True, text)

        log_match = self._log_re.match(text)
        if log_match:
            return LogMessage(MessageKind.LOG, raw, log_match.group(1))

        event_match = self._event_re.match(text)
        if event_match:
            tokens = tuple(event_match.group(1).split())
            name = tokens[0] if tokens else ""
            return EventMessage(MessageKind.EVENT, raw, name, tokens[1:])

        value_match = self._value_re.match(text)
        if value_match:
            return ValueMessage(
                MessageKind.VALUE,
                raw,
                value_match.group(1),
                self.parse_scalar(value_match.group(2)),
            )

        list_match = self._list_re.match(raw)
        if list_match and ":" in list_match.group(2):
            return ValueMessage(
                MessageKind.VALUE,
                raw,
                list_match.group(1).strip(),
                self.parse_mapping(list_match.group(2).strip()),
            )

        key_value_match = self._kv_re.match(raw)
        if key_value_match:
            key = key_value_match.group(1).strip().replace(" ", "_")
            value = key_value_match.group(2).strip()
            return ValueMessage(MessageKind.VALUE, raw, key, self.parse_scalar(value))

        if self._header_re.match(text):
            return ResponseMessage(MessageKind.DATA, raw, True, text)

        return UnknownMessage(MessageKind.UNKNOWN, raw, text)

    def parse_lines(self, lines: list[str]) -> list[Message]:
        return [self.parse_line(line) for line in lines]

    @staticmethod
    def parse_scalar(value: str):
        value = value.strip()
        lowered = value.lower()
        if lowered in {"yes", "true", "on", "ok", "open", "mounted"}:
            return True
        if lowered in {"no", "false", "off", "fail", "closed", "unmounted"}:
            return False

        try:
            return int(value, 0)
        except ValueError:
            pass

        try:
            return float(value)
        except ValueError:
            return value

    @classmethod
    def parse_mapping(cls, text: str) -> dict[str, object]:
        values: dict[str, object] = {}
        for item in text.split(","):
            if ":" not in item:
                continue
            key, raw_value = item.split(":", 1)
            values[key.strip()] = cls.parse_scalar(raw_value.strip())
        return values


def split_code_payload(payload: str) -> tuple[str, str]:
    parts = payload.split(maxsplit=1)
    if not parts:
        return "", ""
    if len(parts) == 1:
        return parts[0], ""
    return parts[0], parts[1]


def split_notification_payload(payload: str) -> tuple[str, str, str]:
    """Parse `[TFI] CODE summary | detail` with safe fallbacks."""

    code, text = split_code_payload(payload)
    if "|" in text:
        summary, detail = text.split("|", 1)
        return code, summary.strip(), detail.strip()
    return code, text.strip(), text.strip()
