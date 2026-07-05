"""Parsed firmware message types."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Any


class MessageKind(str, Enum):
    RESPONSE = "response"
    ERROR = "error"
    DATA = "data"
    LOG = "firmware_log"
    EVENT = "event"
    VALUE = "value"
    PROMPT = "prompt"
    TUI_NOTIFICATION = "tui_notification"
    UNKNOWN = "unknown"


@dataclass(frozen=True, slots=True)
class Message:
    kind: MessageKind
    raw: str


@dataclass(frozen=True, slots=True)
class ResponseMessage(Message):
    ok: bool
    text: str


@dataclass(frozen=True, slots=True)
class ErrorMessage(Message):
    code: str
    detail: str


@dataclass(frozen=True, slots=True)
class LogMessage(Message):
    text: str


@dataclass(frozen=True, slots=True)
class MarkerMessage(Message):
    code: str
    text: str


@dataclass(frozen=True, slots=True)
class EventMessage(Message):
    name: str
    args: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class TuiNotificationMessage(Message):
    code: str
    summary: str
    detail: str


@dataclass(frozen=True, slots=True)
class ValueMessage(Message):
    key: str
    value: Any


@dataclass(frozen=True, slots=True)
class PromptMessage(Message):
    prompt: str


@dataclass(frozen=True, slots=True)
class UnknownMessage(Message):
    text: str
