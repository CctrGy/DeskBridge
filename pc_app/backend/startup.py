"""Windows startup task management for the DeskBridge backend."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

TASK_NAME = "DeskBridgeBackend"
STARTUP_CMD = "DeskBridgeBackend.cmd"


def backend_command() -> str:
    if getattr(sys, "frozen", False):
        return f'"{sys.executable}" --backend'
    terminal = Path(__file__).resolve().parents[1] / "terminal.py"
    return f'"{sys.executable}" "{terminal}" --backend'


def install_startup_task() -> None:
    command = backend_command()
    try:
        subprocess.run(
            [
                "schtasks",
                "/Create",
                "/TN",
                TASK_NAME,
                "/SC",
                "ONLOGON",
                "/TR",
                command,
                "/RL",
                "LIMITED",
                "/F",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        remove_startup_command()
    except subprocess.CalledProcessError:
        write_startup_command(command)


def uninstall_startup_task() -> None:
    subprocess.run(
        ["schtasks", "/Delete", "/TN", TASK_NAME, "/F"],
        check=False,
        capture_output=True,
        text=True,
    )
    remove_startup_command()


def startup_enabled() -> bool:
    if (startup_folder() / STARTUP_CMD).exists():
        return True
    result = subprocess.run(
        ["schtasks", "/Query", "/TN", TASK_NAME],
        check=False,
        capture_output=True,
        text=True,
    )
    return result.returncode == 0


def startup_folder() -> Path:
    appdata = Path.home() / "AppData" / "Roaming"
    return appdata / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup"


def write_startup_command(command: str) -> None:
    folder = startup_folder()
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / STARTUP_CMD
    path.write_text(f"@echo off\nstart \"\" {command}\n", encoding="utf-8")


def remove_startup_command() -> None:
    path = startup_folder() / STARTUP_CMD
    if path.exists():
        path.unlink()
