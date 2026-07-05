"""Windows notification helpers for the background backend."""

from __future__ import annotations

import ctypes
from pathlib import Path

from sdk.paths import app_dir, bundle_root, project_dir


class WindowsNotifier:
    """Small wrapper around Windows toast notifications with a safe fallback."""

    def __init__(self) -> None:
        self._toast_cls = None
        self._audio_cls = None
        try:
            from winotify import Notification, audio
        except Exception:
            return
        self._toast_cls = Notification
        self._audio_cls = audio

    def warning(self, title: str, message: str) -> None:
        if self._toast_cls is None:
            ctypes.windll.user32.MessageBoxW(None, message, title, 0x30)
            return

        toast = self._toast_cls(
            app_id="DeskBridge",
            title=title,
            msg=message,
            icon=str(icon_path()) if icon_path().exists() else "",
        )
        if self._audio_cls is not None:
            toast.set_audio(self._audio_cls.Default, loop=False)
        toast.show()


def icon_path() -> Path:
    candidates = [
        app_dir() / "icon.png",
        project_dir() / "icon.png",
        bundle_root() / "icon.png",
        app_dir() / "data" / "icon.png",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]
