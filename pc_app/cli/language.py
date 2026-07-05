"""Language configuration and translated CLI strings."""

from __future__ import annotations

import json
from pathlib import Path

from sdk.config_file import AppConfig
from sdk.paths import data_dir


LANGUAGE_DIR = data_dir() / "settings" / "language"
LEGACY_LANGUAGE_DIR = data_dir() / "settings" / "languaje"
LANGUAGE_CONFIG = LANGUAGE_DIR / "language.config"
DEFAULT_LANGUAGE = "english"


class LanguageStrings(dict):
    """Dictionary with a fallback-friendly text accessor."""

    def text(self, key: str, default: str | None = None) -> str:
        value = self.get(key, default if default is not None else key)
        return str(value)


class LanguageManager:
    """Load and persist the selected CLI language."""

    def __init__(self, directory: Path = LANGUAGE_DIR) -> None:
        if not directory.exists() and LEGACY_LANGUAGE_DIR.exists():
            directory = LEGACY_LANGUAGE_DIR
        self.directory = directory
        self.config_path = directory / "language.config"
        if not self.config_path.exists():
            legacy_config = directory / "languaje.config"
            if legacy_config.exists():
                self.config_path = legacy_config
        self.app_config = AppConfig.load()
        self.directory.mkdir(parents=True, exist_ok=True)
        self.available = self._load_available()
        self.current = self._load_current()
        self.strings = self.load(self.current)

    def set_language(self, name: str) -> None:
        normalized = name.strip().lower()
        if normalized not in self.available:
            raise ValueError(f"Unknown language: {name}")
        self.current = normalized
        self.strings = self.load(normalized)
        self._save_current()
        self.app_config.set_language(normalized)

    def load(self, name: str) -> LanguageStrings:
        file_name = self.available.get(name)
        if file_name is None:
            file_name = self.available[DEFAULT_LANGUAGE]
        path = self.directory / file_name
        return LanguageStrings(json.loads(path.read_text(encoding="utf-8")))

    def names(self) -> list[str]:
        return sorted(self.available)

    def reload(self) -> None:
        self.available = self._load_available()
        if self.current not in self.available:
            self.current = self._load_current()
        self.strings = self.load(self.current)
        self._save_current()

    def t(self, key: str, default: str | None = None) -> str:
        return self.strings.text(key, default)

    def _load_available(self) -> dict[str, str]:
        ensure_default_language_files(self.directory)
        values: dict[str, str] = {}
        for line in self.config_path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("#") or ":" not in stripped:
                continue
            key, value = stripped.split(":", 1)
            key = key.strip().lower()
            value = value.strip()
            if key == "selected":
                continue
            values[key] = value
        for path in sorted(self.directory.glob("*.json")):
            name = path.stem.removeprefix("shell_").lower()
            values.setdefault(name, path.name)
        return values or {DEFAULT_LANGUAGE: "shell_english.json"}

    def _load_current(self) -> str:
        selected = self.app_config.language or DEFAULT_LANGUAGE
        for line in self.config_path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped.startswith("selected:"):
                selected = stripped.split(":", 1)[1].strip().lower()
                break
        return selected if selected in self.available else DEFAULT_LANGUAGE

    def _save_current(self) -> None:
        lines = [f"selected: {self.current}"]
        for name in self.names():
            lines.append(f"{name}: {self.available[name]}")
        lines.append("")
        self.config_path.write_text("\n".join(lines), encoding="utf-8")


def ensure_default_language_files(directory: Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    config = directory / "language.config"
    english = directory / "shell_english.json"
    spanish = directory / "shell_español.json"

    if not config.exists():
        config.write_text(
            "selected: english\nenglish: shell_english.json\nespañol: shell_español.json\n",
            encoding="utf-8",
        )
    if not english.exists():
        english.write_text(json.dumps(ENGLISH_STRINGS, indent=2, ensure_ascii=False), encoding="utf-8")
    if not spanish.exists():
        spanish.write_text(json.dumps(SPANISH_STRINGS, indent=2, ensure_ascii=False), encoding="utf-8")


ENGLISH_STRINGS = {
    "help.local.title": "Local commands",
    "help.periferic.title": "Chip commands via SDK",
    "help.legend": "<...> selectable type    [...] optional variable",
    "help.columns.command": "COMMAND",
    "help.columns.args": "ARGS",
    "help.columns.description": "DESCRIPTION",
    "help.local.help": "show this help",
    "help.local.connect": "connect through the SDK",
    "help.local.disconnect": "disconnect through the SDK",
    "help.local.reconnect": "reconnect",
    "help.local.status": "show CLI/SDK status",
    "help.local.backend": "manage resident background process",
    "help.local.settings": "show or change CLI settings",
    "help.local.paths": "show runtime and main file paths",
    "help.local.history": "show command history",
    "help.local.clear": "clear the terminal",
    "help.local.firmware_upload": "upload firmware with PlatformIO",
    "help.local.exit": "exit",
    "help.periferic.ping": "check the connection",
    "help.periferic.info": "general chip information",
    "help.periferic.usb": "USB status",
    "help.periferic.program": "firmware version and build",
    "help.periferic.display": "OLED display settings",
    "help.periferic.periferic": "internal chip status",
    "help.periferic.bus": "sensor bus",
    "help.periferic.keypad": "keypad status",
    "help.periferic.light": "light and power manager",
    "help.periferic.wireless": "BLE wireless link",
    "help.periferic.reset": "controlled reset",
    "help.periferic.rtc": "RTC clock",
    "cli.started": "Type help to list commands.",
}


SPANISH_STRINGS = {
    "help.local.title": "Comandos locales",
    "help.periferic.title": "Comandos del chip vía SDK",
    "help.legend": "<...> selección tipo lista    [...] variable opcional",
    "help.columns.command": "COMANDO",
    "help.columns.args": "ARGS",
    "help.columns.description": "DESCRIPCIÓN",
    "help.local.help": "muestra esta ayuda",
    "help.local.connect": "conecta por SDK",
    "help.local.disconnect": "desconecta por SDK",
    "help.local.reconnect": "reconecta",
    "help.local.status": "muestra estado del CLI/SDK",
    "help.local.backend": "gestiona el programa residente en segundo plano",
    "help.local.settings": "muestra o cambia ajustes del CLI",
    "help.local.paths": "muestra rutas de ejecución y archivo principal",
    "help.local.history": "muestra comandos usados",
    "help.local.clear": "limpia la terminal",
    "help.local.firmware_upload": "sube firmware con PlatformIO",
    "help.local.exit": "salir",
    "help.periferic.ping": "comprueba conexión",
    "help.periferic.info": "información general del chip",
    "help.periferic.usb": "estado USB",
    "help.periferic.program": "versión y build del firmware",
    "help.periferic.display": "ajustes de la pantalla OLED",
    "help.periferic.periferic": "estado de chips internos",
    "help.periferic.bus": "bus de sensores",
    "help.periferic.keypad": "estado del keypad",
    "help.periferic.light": "luz y power manager",
    "help.periferic.wireless": "enlace inalambrico BLE",
    "help.periferic.reset": "reinicio controlado",
    "help.periferic.rtc": "reloj RTC",
    "cli.started": "Escribe help para ver comandos.",
}
