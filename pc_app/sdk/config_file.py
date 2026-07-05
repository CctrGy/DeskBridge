"""Small .config file helper for DeskBridge runtime settings."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .paths import data_dir


DEFAULT_CONTENT = """{
  last_connection: None
  log_level: INFO
  language: english
  terminal_color: on
  terminal_palette: dark
  history_limit: 256
  history_visual_size: 16
  connect: disconnected
  functionKey: on
  terminal_cols: 120
  terminal_lines: 31
  backend_autoconnect: on
  backend_foreground_suspend: 600
  backend_monitor: on
  backend_notification_cooldown: 300
  backend_notify_co2: on
  backend_notify_humidity: on
  backend_notify_lux: on
  backend_notify_temperature: on
  backend_poll_interval: 30
  gui_key_0_action: none
  gui_key_1_action: none
  gui_key_2_action: none
  gui_key_3_action: none
  gui_key_4_action: none
}
"""


@dataclass(slots=True)
class AppConfig:
    path: Path
    values: dict[str, str | None]

    @classmethod
    def default_path(cls) -> Path:
        return data_dir() / "data.config"

    @classmethod
    def load(cls, path: Path | None = None) -> "AppConfig":
        config_path = path or cls.default_path()
        config_path.parent.mkdir(parents=True, exist_ok=True)

        if not config_path.exists():
            config_path.write_text(DEFAULT_CONTENT, encoding="utf-8")
            return cls(config_path, default_values())

        values = default_values()
        values.update(parse_config(config_path.read_text(encoding="utf-8")))
        return cls(config_path, values)

    @property
    def last_connection(self) -> str | None:
        value = self.values.get("last_connection")
        if value in {None, "", "None", "none", "null"}:
            return None
        return str(value)

    def set_last_connection(self, port: str | None) -> None:
        self.values["last_connection"] = port
        self.save()

    @property
    def connect_state(self) -> str:
        value = str(self.values.get("connect") or "disconnected").lower()
        return "connected" if value == "connected" else "disconnected"

    @property
    def should_auto_connect(self) -> bool:
        return self.connect_state == "connected"

    def set_connect_state(self, connected: bool) -> None:
        self.values["connect"] = "connected" if connected else "disconnected"
        self.save()

    @property
    def function_key_active(self) -> bool:
        value = str(self.values.get("functionKey") or "on").lower()
        return value in {"active", "on", "1", "true", "yes", "si"}

    def set_function_key_active(self, active: bool) -> None:
        self.values["functionKey"] = "on" if active else "off"
        self.save()

    @property
    def log_level(self) -> str:
        value = self.values.get("log_level")
        return str(value or "INFO").upper()

    def set_log_level(self, level: str) -> None:
        self.values["log_level"] = level.upper()
        self.save()

    @property
    def language(self) -> str:
        return str(self.values.get("language") or "english").lower()

    def set_language(self, language: str) -> None:
        self.values["language"] = language.lower()
        self.save()

    @property
    def terminal_palette(self) -> str:
        value = str(self.values.get("terminal_palette") or "dark").lower()
        return "dark" if value == "darck" else value

    def set_terminal_palette(self, palette: str) -> None:
        normalized = palette.lower()
        self.values["terminal_palette"] = "dark" if normalized == "darck" else normalized
        self.save()

    @property
    def terminal_color(self) -> bool:
        value = str(self.values.get("terminal_color") or "on").lower()
        return value in {"on", "1", "true", "yes", "si"}

    def set_terminal_color(self, enabled: bool) -> None:
        self.values["terminal_color"] = "on" if enabled else "off"
        self.save()

    @property
    def history_limit(self) -> int:
        return self._int_value("history_limit", 256, minimum=1)

    def set_history_limit(self, value: int) -> None:
        self.values["history_limit"] = str(max(1, value))
        self.save()

    @property
    def history_visual_size(self) -> int:
        return self._int_value("history_visual_size", 16, minimum=1)

    def set_history_visual_size(self, value: int) -> None:
        self.values["history_visual_size"] = str(max(1, value))
        self.save()

    @property
    def terminal_cols(self) -> int:
        return self._int_value("terminal_cols", 120, minimum=80)

    @property
    def terminal_lines(self) -> int:
        return self._int_value("terminal_lines", 31, minimum=24)

    def set_terminal_size(self, cols: int, lines: int) -> None:
        self.values["terminal_cols"] = str(max(80, cols))
        self.values["terminal_lines"] = str(max(24, lines))
        self.save()

    def _int_value(self, key: str, default: int, *, minimum: int) -> int:
        try:
            value = int(str(self.values.get(key) or default))
        except ValueError:
            return default
        return max(minimum, value)

    def save(self) -> None:
        lines = ["{"]
        for key in sorted(self.values):
            value = self.values[key]
            rendered = "None" if value is None else str(value)
            lines.append(f"  {key}: {rendered}")
        lines.append("}")
        lines.append("")
        self.path.write_text("\n".join(lines), encoding="utf-8")


def default_values() -> dict[str, str | None]:
    return {
        "last_connection": None,
        "log_level": "INFO",
        "language": "english",
        "terminal_color": "on",
        "terminal_palette": "dark",
        "history_limit": "256",
        "history_visual_size": "16",
        "connect": "disconnected",
        "functionKey": "on",
        "terminal_cols": "120",
        "terminal_lines": "31",
        "backend_autoconnect": "on",
        "backend_foreground_suspend": "600",
        "backend_monitor": "on",
        "backend_notification_cooldown": "300",
        "backend_notify_co2": "on",
        "backend_notify_humidity": "on",
        "backend_notify_lux": "on",
        "backend_notify_temperature": "on",
        "backend_poll_interval": "30",
        "gui_key_0_action": "none",
        "gui_key_1_action": "none",
        "gui_key_2_action": "none",
        "gui_key_3_action": "none",
        "gui_key_4_action": "none",
    }


def parse_config(text: str) -> dict[str, str | None]:
    values: dict[str, str | None] = {}
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line in {"{", "}"} or line.startswith("#"):
            continue
        if ":" not in line:
            continue

        key, raw_value = line.split(":", 1)
        key = key.strip()
        value = raw_value.strip().rstrip(",")
        if value in {"None", "none", "null", ""}:
            values[key] = None
        else:
            values[key] = value.strip('"').strip("'")
    return values
