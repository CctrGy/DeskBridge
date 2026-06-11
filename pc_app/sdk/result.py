"""Structured command results returned by SDK modules."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True, slots=True)
class CommandResult:
    ok: bool
    command_name: str
    raw_command: str
    raw_response: str | None = None
    data: dict[str, Any] | None = None
    message: str | None = None
    error: str | None = None
