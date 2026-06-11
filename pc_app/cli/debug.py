"""CLI debug state."""

from dataclasses import dataclass


@dataclass(slots=True)
class DebugState:
    enabled: bool = False

    def set(self, value: bool) -> None:
        self.enabled = value
