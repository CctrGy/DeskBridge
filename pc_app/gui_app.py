"""Compatibility entry point for the DeskBridge GUI."""

from __future__ import annotations

import sys

from gui.gui_main import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
