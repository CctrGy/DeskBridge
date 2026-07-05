"""DeskBridge USB serial protocol headers."""

from __future__ import annotations

from enum import Enum
import re


class ProtocolHeader(str, Enum):
    """Known three-letter headers used by DeskBridge serial messages."""

    SHELL = "shl"
    COMMAND = "cmd"
    CONFIG = "cnf"
    PROGRAM = "prg"
    RESPONSE = "rsp"
    LOG = "log"
    EVENT = "evt"
    TASK = "tsk"
    DATA = "dat"
    MARK = "mrk"
    ERROR = "err"
    STATE = "ste"
    STACK = "stk"
    INFO = "inf"
    TUI_NOTIFICATION = "tfi"

    @property
    def firmware(self) -> str:
        return self.value.upper()


KNOWN_HEADERS = frozenset(header.value for header in ProtocolHeader)
KNOWN_FIRMWARE_HEADERS = frozenset(header.firmware for header in ProtocolHeader)

HEADER_RE = re.compile(r"^\s*\[([A-Za-z0-9_]{3})]\s*(.*)$")


def split_header(line: str) -> tuple[str | None, str]:
    match = HEADER_RE.match(line)
    if not match:
        return None, line.strip()
    return match.group(1).lower(), match.group(2).strip()


def has_header(line: str) -> bool:
    header, _ = split_header(line)
    return header is not None


def ensure_header(line: str, header: ProtocolHeader | str = ProtocolHeader.SHELL) -> str:
    text = line.strip()
    existing_header, payload = split_header(text)
    if existing_header is not None:
        if existing_header in KNOWN_HEADERS:
            return f"[{existing_header.upper()}] {payload}"
        return text
    value = header.value if isinstance(header, ProtocolHeader) else str(header).strip().lower()
    value = value.removeprefix("[").removesuffix("]")
    return f"[{value.upper()}] {text}"
