"""Persistent DeskBridge background backend with Windows tray integration."""

from __future__ import annotations

import argparse
import subprocess
import sys
import threading
import time
from pathlib import Path

import pystray
from PIL import Image, ImageDraw

from backend.single_instance import SingleInstance
from backend.startup import install_startup_task, uninstall_startup_task
from backend.windows_notify import WindowsNotifier, icon_path
from sdk import DeviceSDK
from sdk.config_file import AppConfig
from sdk.exceptions import DeskBridgeError
from sdk.protocol import TuiNotificationMessage


NOTIFY_CATEGORIES = ("temperature", "humidity", "co2", "lux")
CATEGORY_ALIASES = {
    "temperature": ("temperature", "temp", "celsius"),
    "humidity": ("humidity", "hum", "rh"),
    "co2": ("co2", "carbon", "scd41"),
    "lux": ("lux", "light", "illuminance", "veml"),
}


class DeskBridgeBackend:
    """Long-running background process for tray access and critical alerts."""

    def __init__(self, *, auto_connect: bool = True) -> None:
        self.config = AppConfig.load()
        self.device = DeviceSDK(response_timeout=1.5)
        self.device.log_manager.set_origin("BACKEND")
        self.auto_connect = auto_connect and self.config.should_auto_connect
        self.notifier = WindowsNotifier()
        self.stop_event = threading.Event()
        self.last_notifications = {category: 0.0 for category in NOTIFY_CATEGORIES}
        self.icon: pystray.Icon | None = None
        self.monitor_thread: threading.Thread | None = None
        self.foreground_suspend_until = 0.0

    def run(self) -> int:
        guard = SingleInstance("Global\\DeskBridgeBackend")
        if not guard.acquire():
            print("DeskBridge backend is already running.")
            return 0

        try:
            if self.auto_connect:
                self.try_connect()

            self.monitor_thread = threading.Thread(target=self.monitor_loop, name="DeskBridgeMonitor", daemon=True)
            self.monitor_thread.start()
            self.icon = self.create_tray_icon()
            self.icon.run()
            return 0
        finally:
            self.stop_event.set()
            if self.monitor_thread and self.monitor_thread.is_alive():
                self.monitor_thread.join(timeout=2.0)
            self.device.disconnect()
            guard.release()

    def create_tray_icon(self) -> pystray.Icon:
        icon = pystray.Icon(
            "DeskBridge",
            load_tray_image(),
            "DeskBridge",
            menu=self.build_menu(),
        )
        icon.visible = True
        return icon

    def build_menu(self) -> pystray.Menu:
        return pystray.Menu(
            pystray.MenuItem(lambda _item: self.port_menu_text(), None, enabled=False),
            pystray.MenuItem("Open", lambda _icon, _item: self.open_gui(), default=True),
            pystray.MenuItem("Terminal", lambda _icon, _item: self.open_terminal()),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Exit", lambda icon, _item: self.exit_backend(icon)),
        )

    def port_menu_text(self) -> str:
        if self.device.connected and self.device.port:
            return f"COM: {self.device.port}"
        return "COM: disconnected"

    def try_connect(self) -> None:
        if self.device.connected:
            return
        try:
            self.device.connect()
            AppConfig.load().set_connect_state(True)
        except DeskBridgeError:
            AppConfig.load().set_connect_state(False)

    def monitor_loop(self) -> None:
        while not self.stop_event.wait(1.0):
            if not self.config_bool("backend_monitor", True):
                continue
            if not self.device.connected:
                if self.config_bool("backend_autoconnect", True) and time.monotonic() >= self.foreground_suspend_until:
                    self.try_connect()
                continue
            self.poll_notifications()

    def poll_notifications(self) -> None:
        for message in self.device.poll(limit=16, timeout=0.0):
            if isinstance(message, TuiNotificationMessage):
                self.handle_notification(message)

    def handle_notification(self, message: TuiNotificationMessage) -> None:
        category = notification_category(message)
        if category is None:
            return
        if not self.config_bool(f"backend_notify_{category}", True):
            return
        if not self.cooldown_ready(category):
            return
        self.last_notifications[category] = time.monotonic()
        summary = message.summary or f"{category.upper()} notification"
        detail = message.detail or message.raw
        self.notifier.warning(f"DeskBridge {summary}", detail)

    def cooldown_ready(self, category: str) -> bool:
        last_alert = self.last_notifications.get(category, 0.0)
        return time.monotonic() - last_alert >= self.config_float("backend_notification_cooldown", 300.0)

    def poll_interval_seconds(self) -> float:
        return max(5.0, self.config_float("backend_poll_interval", 30.0))

    def config_bool(self, key: str, default: bool) -> bool:
        value = str(AppConfig.load().values.get(key) or ("on" if default else "off")).lower()
        return value in {"on", "true", "yes", "1", "active"}

    def config_float(self, key: str, default: float) -> float:
        try:
            return float(str(AppConfig.load().values.get(key) or default))
        except ValueError:
            return default

    def open_gui(self) -> None:
        self.release_for_foreground()
        launch_app(["--gui"])

    def open_terminal(self) -> None:
        self.release_for_foreground()
        launch_terminal()

    def release_for_foreground(self) -> None:
        self.foreground_suspend_until = time.monotonic() + self.config_float("backend_foreground_suspend", 600.0)
        if self.device.connected:
            self.device.disconnect()

    def exit_backend(self, icon: pystray.Icon) -> None:
        self.stop_event.set()
        icon.stop()


def notification_category(message: TuiNotificationMessage) -> str | None:
    text = " ".join((message.code, message.summary, message.detail, message.raw)).lower()
    for category, aliases in CATEGORY_ALIASES.items():
        if any(alias in text for alias in aliases):
            return category
    return None


def load_tray_image() -> Image.Image:
    path = icon_path()
    if path.exists():
        return Image.open(path).resize((64, 64))
    image = Image.new("RGBA", (64, 64), (5, 11, 18, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle((6, 6, 58, 58), fill=(34, 167, 242, 255))
    draw.text((14, 22), "DB", fill=(7, 19, 28, 255))
    return image


def launch_app(args: list[str]) -> None:
    if getattr(sys, "frozen", False):
        subprocess.Popen([sys.executable, *args], close_fds=True)
        return
    terminal = Path(__file__).resolve().parents[1] / "terminal.py"
    subprocess.Popen([sys.executable, str(terminal), *args], close_fds=True)


def launch_terminal() -> None:
    if getattr(sys, "frozen", False):
        subprocess.Popen(["cmd", "/c", "start", "DeskBridge CLI", sys.executable, "--cli"], shell=False)
        return
    terminal = Path(__file__).resolve().parents[1] / "terminal.py"
    subprocess.Popen(["cmd", "/c", "start", "DeskBridge CLI", sys.executable, str(terminal), "--cli"], shell=False)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        AppConfig.load()
        load_tray_image()
        print("DeskBridge backend self-test OK")
        return 0
    if args.install_startup:
        install_startup_task()
        print("DeskBridge backend startup task installed.")
        return 0
    if args.uninstall_startup:
        uninstall_startup_task()
        print("DeskBridge backend startup task removed.")
        return 0
    return DeskBridgeBackend(auto_connect=not args.no_autoconnect).run()


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="DeskBridge background backend")
    parser.add_argument("--no-autoconnect", action="store_true")
    parser.add_argument("--install-startup", action="store_true")
    parser.add_argument("--uninstall-startup", action="store_true")
    parser.add_argument("--self-test", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args(argv)


if __name__ == "__main__":
    raise SystemExit(main())
