"""CLI input parser."""

from __future__ import annotations

import shlex
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class ParsedCommand:
    name: str
    args: list[str]
    raw: str


def parse_command(line: str) -> ParsedCommand:
    parts = shlex.split(line)
    if not parts:
        return ParsedCommand("", [], line)
    return ParsedCommand(parts[0].lower(), parts[1:], line)
