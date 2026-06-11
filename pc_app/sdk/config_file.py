"""Small .config file helper for DeskBridge runtime settings."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


DEFAULT_CONTENT = """{
  last_connection: None
  log_level: INFO
  language: english
  terminal_palette: default
  history_limit: 200
  history_visual_size: 25
}
"""


@dataclass(slots=True)
class AppConfig:
    path: Path
    values: dict[str, str | None]

    @classmethod
    def default_path(cls) -> Path:
        return Path(__file__).resolve().parents[1] / "data" / "data.config"

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
        return str(self.values.get("terminal_palette") or "default").lower()

    @property
    def history_limit(self) -> int:
        return self._int_value("history_limit", 200, minimum=1)

    def set_history_limit(self, value: int) -> None:
        self.values["history_limit"] = str(max(1, value))
        self.save()

    @property
    def history_visual_size(self) -> int:
        return self._int_value("history_visual_size", 25, minimum=1)

    def set_history_visual_size(self, value: int) -> None:
        self.values["history_visual_size"] = str(max(1, value))
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
        "terminal_palette": "default",
        "history_limit": "200",
        "history_visual_size": "25",
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
