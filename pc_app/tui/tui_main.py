"""TUI entry point."""

from __future__ import annotations

from sdk import DeviceSDK

from .app import TuiApp


def run_tui(device: DeviceSDK | None = None, *, auto_connect: bool = True) -> int:
    app = TuiApp(device)
    return app.run(auto_connect=auto_connect)
