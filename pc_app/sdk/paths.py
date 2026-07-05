"""Runtime path helpers for source and PyInstaller builds."""

from __future__ import annotations

import sys
from shutil import copytree
from pathlib import Path


def bundle_root() -> Path:
    """Return the root that contains bundled app modules and data files."""
    frozen_root = getattr(sys, "_MEIPASS", None)
    if frozen_root:
        return Path(frozen_root).resolve()
    return Path(__file__).resolve().parents[1]


def app_dir() -> Path:
    """Return the editable/source application directory."""
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[1]


def data_dir() -> Path:
    """Return DeskBridge runtime data directory."""
    external_data = app_dir() / "data"
    if getattr(sys, "frozen", False):
        if external_data.exists():
            return external_data
        bundled_data = bundle_root() / "data"
        if bundled_data.exists():
            copytree(bundled_data, external_data, dirs_exist_ok=True)
            return external_data
        return external_data

    bundled_data = bundle_root() / "data"
    if bundled_data.exists():
        return bundled_data
    return external_data


def project_dir() -> Path:
    """Return the DeskBridge project directory when available."""
    return app_dir().parent
