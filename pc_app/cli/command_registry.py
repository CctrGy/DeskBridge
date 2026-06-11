"""Command registry and dispatcher."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field

from .parser import ParsedCommand


CommandHandler = Callable[[ParsedCommand], bool | None]


@dataclass(slots=True)
class CommandRegistry:
    handlers: dict[str, CommandHandler] = field(default_factory=dict)

    def register(self, name: str, handler: CommandHandler, *aliases: str) -> None:
        self.handlers[name] = handler
        for alias in aliases:
            self.handlers[alias] = handler

    def dispatch(self, command: ParsedCommand) -> bool | None:
        handler = self.handlers.get(command.name)
        if handler is None:
            return None
        return handler(command)
