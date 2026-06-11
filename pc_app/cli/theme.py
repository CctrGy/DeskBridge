"""CLI color theme.

The CLI uses colorama when available. If it is not installed, all helpers fall
back to plain text so the SDK and CLI remain usable in minimal environments.
"""

from __future__ import annotations

import os
import re
import json
from pathlib import Path

try:
    from colorama import Back, Fore, Style, just_fix_windows_console
except ImportError:  # pragma: no cover - exercised only before dependencies install.
    class _AnsiFore:
        BLACK = "\033[30m"
        RED = "\033[31m"
        GREEN = "\033[32m"
        YELLOW = "\033[33m"
        BLUE = "\033[34m"
        MAGENTA = "\033[35m"
        CYAN = "\033[36m"
        WHITE = "\033[37m"
        LIGHTBLACK_EX = "\033[90m"

    class _AnsiBack:
        BLACK = "\033[40m"
        RED = "\033[41m"
        GREEN = "\033[42m"
        YELLOW = "\033[43m"
        BLUE = "\033[44m"
        MAGENTA = "\033[45m"
        CYAN = "\033[46m"
        WHITE = "\033[47m"

    class _AnsiStyle:
        BRIGHT = "\033[1m"
        NORMAL = "\033[22m"
        DIM = "\033[2m"
        RESET_ALL = "\033[0m"

    Back = _AnsiBack()
    Fore = _AnsiFore()
    Style = _AnsiStyle()

    def just_fix_windows_console() -> None:
        return None


_COLOR_ENABLED = os.environ.get("NO_COLOR") is None
if _COLOR_ENABLED:
    just_fix_windows_console()

PC_APP_DIR = Path(__file__).resolve().parents[1]
PALETTE_FILE = PC_APP_DIR / "data" / "settings" / "terminal_palete.json"


def _load_palette() -> dict[str, str]:
    try:
        data = json.loads(PALETTE_FILE.read_text(encoding="utf-8"))
        selected = str(data.get("selected", "default"))
        palettes = data.get("palettes", {})
        palette = palettes.get(selected, palettes.get("default", {}))
        return dict(palette)
    except (OSError, ValueError, TypeError):
        return {}


_PALETTE = _load_palette()


_COLOR_CODES = {
    "black": Fore.BLACK,
    "red": Fore.RED,
    "green": Fore.GREEN,
    "yellow": Fore.YELLOW,
    "blue": Fore.BLUE,
    "magenta": Fore.MAGENTA,
    "cyan": Fore.CYAN,
    "white": Fore.WHITE,
    "gray": Fore.LIGHTBLACK_EX,
    "red_bright": Style.BRIGHT + Fore.RED,
    "green_bright": Style.BRIGHT + Fore.GREEN,
    "yellow_bright": Style.BRIGHT + Fore.YELLOW,
    "blue_bright": Style.BRIGHT + Fore.BLUE,
    "magenta_bright": Style.BRIGHT + Fore.MAGENTA,
    "cyan_bright": Style.BRIGHT + Fore.CYAN,
    "white_bright": Style.BRIGHT + Fore.WHITE,
}


def color(text: object, *codes: str) -> str:
    value = str(text)
    if not _COLOR_ENABLED or Fore is None or not codes:
        return value
    return "".join(codes) + value + Style.RESET_ALL


def role(text: object, name: str, *fallback: str) -> str:
    configured = _PALETTE.get(name)
    if configured:
        code = _COLOR_CODES.get(configured)
        if code:
            return color(text, code)
    return color(text, *fallback)


def title(text: object) -> str:
    return role(text, "title", Style.BRIGHT, Fore.CYAN)


def subtitle(text: object) -> str:
    return role(text, "subtitle", Fore.CYAN)


def command(text: object) -> str:
    return role(text, "command", Style.BRIGHT, Fore.GREEN)


def argument(text: object) -> str:
    return role(text, "argument", Fore.YELLOW)


def variable(text: object) -> str:
    return role(text, "variable", Fore.MAGENTA)


def value(text: object) -> str:
    return role(text, "value", Fore.WHITE)


def success(text: object) -> str:
    return role(text, "success", Style.BRIGHT, Fore.GREEN)


def warning(text: object) -> str:
    return role(text, "warning", Fore.YELLOW)


def error(text: object) -> str:
    return role(text, "error", Style.BRIGHT, Fore.RED)


def muted(text: object) -> str:
    return role(text, "muted", Fore.LIGHTBLACK_EX)


def prompt(text: object) -> str:
    return role(text, "prompt", Style.BRIGHT, Fore.BLUE)


def debug_marker(text: object) -> str:
    return role(text, "debug", Style.BRIGHT, Fore.MAGENTA)


def highlight_command_line(line: str) -> str:
    """Color a command line after it has been entered or displayed."""

    parts = line.split()
    if not parts:
        return line

    highlighted = [command(parts[0])]
    for part in parts[1:]:
        if is_variable_token(part):
            highlighted.append(variable(part))
        elif is_option_token(part):
            highlighted.append(subtitle(part))
        else:
            highlighted.append(argument(part))
    return " ".join(highlighted)


def highlight_usage(line: str) -> str:
    parts = line.split()
    if not parts:
        return line

    highlighted = [command(parts[0])]
    for part in parts[1:]:
        if part.startswith("<") and part.endswith(">"):
            highlighted.append(variable(part))
        elif part.startswith("[") and part.endswith("]"):
            highlighted.append(argument(part))
        else:
            highlighted.append(argument(part))
    return " ".join(highlighted)


def is_variable_token(text: str) -> bool:
    return (
        "=" in text
        or text.startswith("<")
        or text.endswith(">")
        or re.fullmatch(r"\d{1,2}/\d{1,2}/\d{4}", text) is not None
        or re.fullmatch(r"\d{1,2}:\d{2}:\d{2}", text) is not None
    )


def is_option_token(text: str) -> bool:
    return text.startswith("-")
