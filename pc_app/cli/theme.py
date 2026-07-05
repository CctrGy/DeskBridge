"""CLI color theme.

The CLI uses colorama when available. If it is not installed, all helpers fall
back to plain text so the SDK and CLI remain usable in minimal environments.
"""

from __future__ import annotations

import os
import re
import json
from pathlib import Path

from sdk.config_file import AppConfig
from sdk.paths import data_dir

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


PC_APP_DIR = data_dir().parent
PALETTE_FILE = PC_APP_DIR / "data" / "settings" / "terminal_palette.json"
LEGACY_PALETTE_FILE = PC_APP_DIR / "data" / "settings" / "terminal_palete.json"
PALETTE_DIR = PC_APP_DIR / "data" / "settings" / "palete"
PALETTE_CONFIG = PALETTE_DIR / "palete.config"
PALETTE_DARK_FILE = PALETTE_DIR / "dark.json"
PALETTE_LIGHT_FILE = PALETTE_DIR / "light.json"
LEGACY_PALETTE_DARCK_FILE = PC_APP_DIR / "data" / "settings" / "terminal_paleteDarck.json"
LEGACY_PALETTE_LIGHT_FILE = PC_APP_DIR / "data" / "settings" / "terminal_paleteLight.json"
_COLOR_ENABLED = False
_AVAILABLE_PALETTES: dict[str, str] = {}
_CONSOLE_BACKGROUND_APPLIED = False


def _load_color_enabled() -> bool:
    if os.environ.get("NO_COLOR") is not None:
        return False
    return AppConfig.load().terminal_color


def _load_palette() -> dict[str, str]:
    try:
        config = AppConfig.load()
        path = _palette_path(config.terminal_palette)
        data = json.loads(path.read_text(encoding="utf-8"))
        selected = str(data.get("selected", "default"))
        palettes = data.get("palettes", {})
        palette = palettes.get(selected, palettes.get("default", {}))
        return dict(palette)
    except (OSError, ValueError, TypeError):
        return {}


def _palette_path(name: str) -> Path:
    palettes = load_available_palettes()
    normalized = normalize_palette_name(name)
    file_name = palettes.get(normalized)
    if file_name:
        return PALETTE_DIR / file_name
    if normalized == "dark" and LEGACY_PALETTE_DARCK_FILE.exists():
        return LEGACY_PALETTE_DARCK_FILE
    if normalized == "light" and PALETTE_LIGHT_FILE.exists():
        return PALETTE_LIGHT_FILE
    if normalized == "light" and LEGACY_PALETTE_LIGHT_FILE.exists():
        return LEGACY_PALETTE_LIGHT_FILE
    if PALETTE_FILE.exists():
        return PALETTE_FILE
    return LEGACY_PALETTE_FILE


def normalize_palette_name(name: str) -> str:
    normalized = str(name or "").strip().lower()
    return "dark" if normalized == "darck" else normalized


def ensure_default_palette_files() -> None:
    PALETTE_DIR.mkdir(parents=True, exist_ok=True)
    if not PALETTE_CONFIG.exists():
        PALETTE_CONFIG.write_text("selected: dark\ndark: dark.json\nlight: light.json\n", encoding="utf-8")
    if not PALETTE_DARK_FILE.exists() and LEGACY_PALETTE_DARCK_FILE.exists():
        PALETTE_DARK_FILE.write_text(LEGACY_PALETTE_DARCK_FILE.read_text(encoding="utf-8"), encoding="utf-8")
    if not PALETTE_LIGHT_FILE.exists() and LEGACY_PALETTE_LIGHT_FILE.exists():
        PALETTE_LIGHT_FILE.write_text(LEGACY_PALETTE_LIGHT_FILE.read_text(encoding="utf-8"), encoding="utf-8")


def load_available_palettes() -> dict[str, str]:
    ensure_default_palette_files()
    values: dict[str, str] = {}
    if PALETTE_CONFIG.exists():
        for line in PALETTE_CONFIG.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or ":" not in stripped:
                continue
            key, value = stripped.split(":", 1)
            key = normalize_palette_name(key)
            value = value.strip()
            if key == "selected":
                continue
            values[key] = value
    for path in sorted(PALETTE_DIR.glob("*.json")):
        values.setdefault(normalize_palette_name(path.stem), path.name)
    return values


def palette_names() -> list[str]:
    return sorted(_AVAILABLE_PALETTES or load_available_palettes())


def palette_value(name: str, default: str = "") -> str:
    return str(_PALETTE.get(name, default))


def reload() -> None:
    global _COLOR_ENABLED, _PALETTE, _AVAILABLE_PALETTES
    _COLOR_ENABLED = _load_color_enabled()
    if _COLOR_ENABLED:
        just_fix_windows_console()
    _AVAILABLE_PALETTES = load_available_palettes()
    _PALETTE = _load_palette()
    apply_console_background()


def set_color_enabled(enabled: bool) -> None:
    AppConfig.load().set_terminal_color(enabled)
    reload()


def set_palette(name: str) -> None:
    normalized = normalize_palette_name(name)
    palettes = load_available_palettes()
    if normalized not in palettes:
        raise ValueError(f"Unknown palete: {name}")
    AppConfig.load().set_terminal_palette(normalized)
    reload()


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
    "background_black": Back.BLACK,
    "background_red": Back.RED,
    "background_green": Back.GREEN,
    "background_yellow": Back.YELLOW,
    "background_blue": Back.BLUE,
    "background_magenta": Back.MAGENTA,
    "background_cyan": Back.CYAN,
    "background_white": Back.WHITE,
}


_CMD_COLOR_CODES = {
    "black": "0",
    "blue": "1",
    "green": "2",
    "cyan": "3",
    "red": "4",
    "magenta": "5",
    "yellow": "6",
    "white": "7",
    "gray": "8",
    "blue_bright": "9",
    "green_bright": "A",
    "cyan_bright": "B",
    "red_bright": "C",
    "magenta_bright": "D",
    "yellow_bright": "E",
    "white_bright": "F",
    "cmd_0": "0",
    "cmd_1": "1",
    "cmd_2": "2",
    "cmd_3": "3",
    "cmd_4": "4",
    "cmd_5": "5",
    "cmd_6": "6",
    "cmd_7": "7",
    "cmd_8": "8",
    "cmd_9": "9",
    "cmd_a": "A",
    "cmd_b": "B",
    "cmd_c": "C",
    "cmd_d": "D",
    "cmd_e": "E",
    "cmd_f": "F",
}


def apply_console_background() -> None:
    global _CONSOLE_BACKGROUND_APPLIED
    if not _COLOR_ENABLED or os.name != "nt":
        return

    background = str(_PALETTE.get("background") or "").lower()
    if not background:
        return

    background_code = _CMD_COLOR_CODES.get(background)
    foreground_code = _CMD_COLOR_CODES.get(str(_PALETTE.get("foreground") or "white").lower(), "7")
    if not background_code:
        return

    os.system(f"color {background_code}{foreground_code}")
    _CONSOLE_BACKGROUND_APPLIED = True


_PALETTE = _load_palette()
reload()


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
