"""DeskBridge application and peripheral event logging."""

from __future__ import annotations

import logging
from logging import LoggerAdapter
from datetime import datetime
from pathlib import Path

from sdk.config_file import AppConfig


LOG_FORMAT = "%(asctime)s | %(levelname)-8s | %(source)-10s | %(name)s | %(message)s"
DATE_FORMAT = "%Y-%m-%d %H:%M:%S"


class DeskBridgeLogManager:
    """Thin project-specific interface over Python logging."""

    USER_LEVEL = 25

    VALID_LEVELS = {
        "DEBUG": logging.DEBUG,
        "INFO": logging.INFO,
        "USER": USER_LEVEL,
        "WARNING": logging.WARNING,
        "ERROR": logging.ERROR,
        "CRITICAL": logging.CRITICAL,
    }

    def __init__(self, config: AppConfig | None = None, log_dir: Path | None = None) -> None:
        self.config = config or AppConfig.load()
        self.log_dir = log_dir or self.config.path.parent / "logs"
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.log_file = self.log_dir / datetime.now().strftime("%d-%m-%Y.log")

        logging.addLevelName(self.USER_LEVEL, "USER")

        self._base_logger = logging.getLogger("deskbridge")
        self._base_logger.handlers.clear()
        self._base_logger.setLevel(logging.DEBUG)
        self._base_logger.propagate = False

        handler = logging.FileHandler(self.log_file, encoding="utf-8")
        handler.setFormatter(logging.Formatter(LOG_FORMAT, DATE_FORMAT))
        self._base_logger.addHandler(handler)
        self._handler = handler

        self.pc_logger = LoggerAdapter(logging.getLogger("deskbridge.pc"), {"source": "PC"})
        self.user_logger = LoggerAdapter(logging.getLogger("deskbridge.user"), {"source": "USER"})
        self.peripheral_logger = LoggerAdapter(logging.getLogger("deskbridge.peripheral"), {"source": "PERIPHERAL"})
        self.origin = ""
        self.set_level(self.config.log_level, persist=False)
        self.pc("INFO", "DeskBridge PC app started | log=%s", self.relative_log_file)

    @property
    def level_name(self) -> str:
        return logging.getLevelName(self._handler.level)

    @property
    def relative_log_file(self) -> str:
        try:
            relative = self.log_file.relative_to(self.config.path.parent.parent)
        except ValueError:
            return str(self.log_file)
        return f".\\{relative}".replace("/", "\\")

    def set_level(self, level: str, *, persist: bool = True) -> str:
        normalized = level.upper()
        if normalized == "WARN":
            normalized = "WARNING"
        if normalized not in self.VALID_LEVELS:
            raise ValueError(f"Nivel de log no valido: {level}")

        numeric = self.VALID_LEVELS[normalized]
        self._handler.setLevel(numeric)
        self._base_logger.setLevel(logging.DEBUG)
        if persist:
            self.config.set_log_level(normalized)
        self.pc_logger.info("log level set to %s", normalized)
        return normalized

    def set_origin(self, origin: str) -> None:
        normalized = origin.strip().upper()
        self.origin = normalized
        prefix = f"{normalized}:" if normalized else ""
        self.pc_logger.extra["source"] = f"{prefix}PC"
        self.user_logger.extra["source"] = f"{prefix}USER"
        self.peripheral_logger.extra["source"] = f"{prefix}PERIPHERAL"

    def pc(self, level: str, message: str, *args: object) -> None:
        self._write(self.pc_logger, level, message, *args)

    def peripheral(self, level: str, message: str, *args: object) -> None:
        self._write(self.peripheral_logger, level, message, *args)

    def user(self, message: str, *args: object) -> None:
        self._write(self.user_logger, "USER", message, *args)

    @classmethod
    def levels_text(cls) -> str:
        return "DEBUG, INFO, USER, WARNING, ERROR, CRITICAL"

    @staticmethod
    def _write(logger: LoggerAdapter, level: str, message: str, *args: object) -> None:
        numeric = DeskBridgeLogManager.VALID_LEVELS.get(level.upper(), logging.INFO)
        logger.log(numeric, message, *args)
