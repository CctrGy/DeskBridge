"""Fixed 120x30 TUI layout primitives."""

from __future__ import annotations

import re


WIDTH = 120
HEIGHT = 30
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b][^\a]*(?:\a|\x1b\\)")

TL = "\u250c"
TR = "\u2510"
BL = "\u2514"
BR = "\u2518"
H = "\u2500"
V = "\u2502"
LT = "\u251c"
RT = "\u2524"
TT = "\u252c"
BT = "\u2534"


class Canvas:
    """Small plain-text canvas for the first TUI layout."""

    def __init__(self, width: int = WIDTH, height: int = HEIGHT) -> None:
        self.width = width
        self.height = height
        self._rows = [[" " for _ in range(width)] for _ in range(height)]

    def text(self, x: int, y: int, value: object, *, width: int | None = None) -> None:
        if y < 0 or y >= self.height or x >= self.width:
            return
        text = strip_ansi(str(value))
        if width is not None:
            text = text[:width].ljust(width)
        for offset, char in enumerate(text):
            column = x + offset
            if 0 <= column < self.width:
                self._rows[y][column] = char

    def line_text(self, y: int, value: str) -> None:
        self.text(0, y, value[: self.width].ljust(self.width))

    def render(self) -> str:
        return "\n".join("".join(row) for row in self._rows)


def strip_ansi(value: str) -> str:
    return ANSI_RE.sub("", value)
