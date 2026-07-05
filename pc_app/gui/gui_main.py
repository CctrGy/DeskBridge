"""DeskBridge WebView graphical interface."""

from __future__ import annotations

import argparse
from pathlib import Path
from shutil import copytree
from typing import Any

import webview

from backend.startup import install_startup_task, startup_enabled, uninstall_startup_task
from sdk import DeviceSDK
from sdk.config_file import AppConfig
from sdk.exceptions import DeskBridgeError
from sdk.identity import USB_ID
from sdk.paths import bundle_root, data_dir
from sdk.result import CommandResult

try:
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - pyserial is a runtime dependency.
    list_ports = None


DEFAULTS = {
    "gui_oled_layout": "Home monitor",
    "gui_temperature_unit": "Celsius (C)",
    "gui_sensor_samples": "3",
    "gui_read_interval": "1000ms",
    "gui_wireless_enabled": "off",
    "gui_key_0_action": "none",
    "gui_key_1_action": "none",
    "gui_key_2_action": "none",
    "gui_key_3_action": "none",
    "gui_key_4_action": "none",
}

KEYPAD_ACTIONS = (
    "none",
    "mute",
    "volume_up",
    "volume_down",
    "play_pause",
    "next",
    "previous",
    "f13",
    "f14",
    "f15",
)


class DeskBridgeGui:
    """Native WebView window backed by editable HTML/CSS/JS files."""

    def __init__(self, *, auto_connect: bool = True, port: str | None = None) -> None:
        self.config = AppConfig.load()
        self.device = DeviceSDK()
        self.device.log_manager.set_origin("GUI")
        self.firmware = "-"
        self.protocol = "-"
        self.rtc = "--:--:--"
        self.profile = "Default"

        if port:
            self.device.set_port(port)

        self.assets_dir = ensure_web_assets()
        self.index_path = self.assets_dir / "index.html"
        self.auto_connect = auto_connect and self.config.should_auto_connect
        self._window = None

    def run(self) -> int:
        api = DeskBridgeWebApi(self)
        window = webview.create_window(
            "DeskBridge",
            self.index_path.as_uri(),
            js_api=api,
            width=1180,
            height=720,
            min_size=(1180, 720),
            resizable=False,
            frameless=True,
            easy_drag=False,
            background_color="#050b12",
        )
        self._window = window
        api._window = window
        webview.start(self._on_started, window, debug=False)
        return 0

    def _on_started(self, _window) -> None:
        if self.auto_connect:
            try:
                self.connect()
            except DeskBridgeError:
                return

    def startup_probe(self) -> CommandResult:
        if self.auto_connect and not self.device.connected:
            self.connect()
        if not self.device.connected:
            return CommandResult(
                True,
                "gui.startup_probe",
                "startup_probe",
                message="GUI started without peripheral connection.",
            )

        program_result = self.refresh_info(silent=True)
        rtc_result = self.device.rtc.get()
        self._update_rtc(rtc_result)
        if not program_result.ok:
            return program_result
        if not rtc_result.ok:
            return rtc_result
        return CommandResult(
            True,
            "gui.startup_probe",
            "startup_probe",
            data={
                "firmware": self.firmware,
                "protocol": self.protocol,
                "rtc": self.rtc,
            },
            message="Startup device data loaded.",
        )

    def connect(self, port: str | None = None) -> str:
        selected = normalize_port(port)
        if selected:
            self.device.set_port(selected)
        connected_port = self.device.connect()
        AppConfig.load().set_connect_state(True)
        self.refresh_info(silent=True)
        return connected_port

    def disconnect(self) -> None:
        self.device.disconnect()
        AppConfig.load().set_connect_state(False)
        self.firmware = "-"
        self.protocol = "-"
        self.rtc = "--:--:--"

    def refresh_info(self, *, silent: bool = False) -> CommandResult:
        result = self.device.core.program()
        if result.ok and result.data:
            self.firmware = str(
                result.data.get("program.version")
                or result.data.get("version")
                or result.data.get("program.name")
                or "-"
            )
            self.protocol = str(result.data.get("program.protocol") or result.data.get("protocol") or "-")
        if not silent:
            return result
        return result

    def run_action(self, action: str) -> CommandResult:
        if not self.device.connected:
            return CommandResult(False, action, action, error="Device disconnected.")
        if action == "rtc_sync":
            result = self.device.rtc.sync()
            self._update_rtc(result)
            return result
        if action == "rtc_read":
            result = self.device.rtc.get()
            self._update_rtc(result)
            return result
        if action == "refresh_info":
            return self.refresh_info()
        if action == "bus_scan":
            return self.device.bus.scan()
        if action == "wireless_pair":
            return self.device.wireless.pair()
        return CommandResult(False, action, action, error=f"Unknown GUI action: {action}")

    def execute_control(self, command: str, values: dict[str, Any] | None = None) -> CommandResult:
        values = values or {}
        if not self.device.connected:
            return CommandResult(False, command, command, error="Device disconnected.")

        if command == "core.ping":
            return self.device.core.ping()
        if command == "core.info":
            return self.device.core.info()
        if command == "core.usb":
            return self.device.core.usb()
        if command == "core.program":
            return self.refresh_info()
        if command == "system.local":
            return self.device.system.local()
        if command == "system.peripherals":
            return self.device.system.peripherals()

        if command == "bus.get":
            return self.device.bus.get()
        if command == "bus.scan":
            return self.device.bus.scan()
        if command == "bus.time_read.get":
            return self.device.bus.time_read()
        if command == "bus.time_read.set":
            return self.device.bus.time_read(str(values.get("value") or "1000ms"))
        if command == "bus.samples.get":
            return self.device.bus.samples()
        if command == "bus.samples.set":
            return self.device.bus.samples(int(values.get("value") or 3))
        if command == "bus.sensors":
            return self.device.bus.sensors()
        if command == "bus.sensor":
            return self.device.bus.sensor(values.get("selector") or "0")

        if command == "keypad.get":
            return self.device.keypad.get()
        if command == "keypad.action.list":
            return self.device.keypad.action_list()
        if command == "keypad.action.set":
            return self.device.keypad.set_action(values.get("button") or 0, values.get("action") or "none")
        if command == "keypad.actions.apply":
            return self.apply_keypad_actions(values.get("actions"))

        if command == "light.get":
            return self.device.light.get()
        if command == "light.power":
            return self.device.light.power()
        if command == "light.sync":
            return self.device.light.sync()
        if command == "light.set":
            return self.device.light.set(str(values.get("field") or "brightness"), values.get("value") or 0)

        if command == "rtc.status":
            return self.device.rtc.status()
        if command == "rtc.get":
            result = self.device.rtc.get()
            self._update_rtc(result)
            return result
        if command == "rtc.sync":
            result = self.device.rtc.sync()
            self._update_rtc(result)
            return result
        if command == "rtc.set":
            result = self.device.rtc.set(str(values.get("value") or ""))
            self._update_rtc(result)
            return result
        if command == "rtc.set_date":
            return self.device.rtc.set_date(str(values.get("value") or ""))
        if command == "rtc.set_time":
            return self.device.rtc.set_time(str(values.get("value") or ""))

        if command == "wireless.get":
            return self.device.wireless.get()
        if command == "wireless.state":
            return self.device.wireless.state()
        if command == "wireless.on":
            return self.device.wireless.on()
        if command == "wireless.off":
            return self.device.wireless.off()
        if command == "wireless.pair":
            return self.device.wireless.pair()
        if command == "wireless.enabled.get":
            return self.device.wireless.enabled()
        if command == "wireless.enabled.set":
            return self.device.wireless.enabled(str(values.get("value") or "off"))
        if command == "wireless.name.get":
            return self.device.wireless.name()
        if command == "wireless.name.set":
            return self.device.wireless.name(str(values.get("value") or "DeskBridge"))
        if command == "wireless.apply":
            return self.device.wireless.apply()
        if command == "wireless.rx":
            return self.device.wireless.rx()
        if command == "wireless.write":
            return self.device.wireless.write(str(values.get("value") or ""))

        if command == "reset.run":
            return self.device.reset.run(str(values.get("target") or "peripherals"))

        return CommandResult(False, command, command, error=f"Unknown GUI control: {command}")

    def save_preferences(self, values: dict[str, Any]) -> None:
        config = AppConfig.load()
        mapping = {
            "gui_oled_layout": values.get("oled_layout"),
            "gui_temperature_unit": values.get("temperature_unit"),
            "gui_sensor_samples": values.get("sensor_samples"),
            "gui_read_interval": values.get("read_interval"),
            "gui_wireless_enabled": "on" if values.get("wireless_enabled") else "off",
            "backend_notify_temperature": "on" if values.get("backend_notify_temperature") else "off",
            "backend_notify_humidity": "on" if values.get("backend_notify_humidity") else "off",
            "backend_notify_co2": "on" if values.get("backend_notify_co2") else "off",
            "backend_notify_lux": "on" if values.get("backend_notify_lux") else "off",
        }
        for key, value in mapping.items():
            if value is not None:
                config.values[key] = str(value)
        self._save_keypad_action_values(config, values.get("key_actions"))
        config.save()
        self.config = config

    def state(self) -> dict[str, object]:
        config = AppConfig.load()
        return {
            "connected": self.device.connected,
            "port": self.device.port or config.last_connection or "Auto",
            "firmware": self.firmware,
            "protocol": self.protocol,
            "usb_id": USB_ID,
            "rtc": self.rtc,
            "profile": self.profile,
            "oled_layout": config.values.get("gui_oled_layout") or DEFAULTS["gui_oled_layout"],
            "temperature_unit": config.values.get("gui_temperature_unit") or DEFAULTS["gui_temperature_unit"],
            "sensor_samples": config.values.get("gui_sensor_samples") or DEFAULTS["gui_sensor_samples"],
            "read_interval": config.values.get("gui_read_interval") or DEFAULTS["gui_read_interval"],
            "wireless_enabled": str(config.values.get("gui_wireless_enabled") or "off").lower() == "on",
            "backend_notify_temperature": config_bool(config, "backend_notify_temperature", True),
            "backend_startup_enabled": startup_enabled(),
            "backend_notify_humidity": config_bool(config, "backend_notify_humidity", True),
            "backend_notify_co2": config_bool(config, "backend_notify_co2", True),
            "backend_notify_lux": config_bool(config, "backend_notify_lux", True),
            "key_actions": self._keypad_action_values(config),
        }

    def _update_rtc(self, result: CommandResult) -> None:
        if result.ok and result.data:
            self.rtc = str(result.data.get("time") or result.data.get("datetime") or "--:--:--")

    def apply_keypad_actions(self, actions: object) -> CommandResult:
        selected = self._normalized_keypad_actions(actions)
        applied: dict[str, str] = {}
        for index, action in enumerate(selected):
            result = self.device.keypad.set_action(index, action)
            if not result.ok:
                return result
            applied[f"key{index}"] = action

        config = AppConfig.load()
        self._save_keypad_action_values(config, selected)
        config.save()
        self.config = config
        return CommandResult(
            True,
            "keypad.actions.apply",
            "keypad action apply",
            data=applied,
            message="KEY actions applied.",
        )

    def _keypad_action_values(self, config: AppConfig | None = None) -> list[str]:
        active_config = config or AppConfig.load()
        values = []
        for index in range(5):
            action = str(active_config.values.get(f"gui_key_{index}_action") or DEFAULTS[f"gui_key_{index}_action"])
            values.append(action if action in KEYPAD_ACTIONS else "none")
        return values

    def _normalized_keypad_actions(self, actions: object) -> list[str]:
        if not isinstance(actions, list):
            return self._keypad_action_values()
        normalized = self._keypad_action_values()
        for index in range(min(5, len(actions))):
            action = str(actions[index] or "none").strip().lower()
            if action not in KEYPAD_ACTIONS:
                raise ValueError(f"unknown keypad action: {action}")
            normalized[index] = action
        return normalized

    def _save_keypad_action_values(self, config: AppConfig, actions: object) -> None:
        for index, action in enumerate(self._normalized_keypad_actions(actions)):
            config.values[f"gui_key_{index}_action"] = action


class DeskBridgeWebApi:
    """Methods exposed to JavaScript through pywebview."""

    def __init__(self, app: DeskBridgeGui) -> None:
        self._app = app
        self._window = None

    def get_state(self) -> dict[str, object]:
        return self._app.state()

    def startup_probe(self) -> dict[str, object]:
        try:
            result = self._app.startup_probe()
        except Exception as exc:
            return response(self._app, "GUI", "ERROR", str(exc), error=str(exc))
        if result.ok:
            return response(self._app, "GUI", "STARTUP", result_summary(result))
        return response(self._app, "GUI", "ERROR", result.error or "Startup probe failed.", error=result.error)

    def list_ports(self) -> list[str]:
        ports = ["Auto"]
        if list_ports is None:
            return ports
        for port in list_ports.comports():
            ports.append(port.device)
        return ports

    def get_options(self) -> dict[str, object]:
        return {
            "keypad_actions": list(KEYPAD_ACTIONS),
            "light_fields": sorted(self._app.device.light.editable_fields),
            "reset_targets": sorted(self._app.device.reset.targets),
            "sensors": ["0 ENS160", "1 AHT21", "2 VEML7700", "3 SCD41", "4 SHT21"],
        }

    def connect(self, port: str | None = None) -> dict[str, object]:
        try:
            connected_port = self._app.connect(port)
            return response(self._app, "GUI", "LINK", f"Connected to {connected_port}.")
        except Exception as exc:
            return response(self._app, "GUI", "ERROR", str(exc), error=str(exc))

    def disconnect(self) -> dict[str, object]:
        self._app.disconnect()
        return response(self._app, "GUI", "LINK", "Disconnected.")

    def run_action(self, action: str) -> dict[str, object]:
        result = self._app.run_action(action)
        if result.ok:
            return response(self._app, action_source(action), "OK", result_summary(result))
        return response(self._app, action_source(action), "ERROR", result.error or "Command failed.", error=result.error)

    def execute_control(self, command: str, values: dict[str, Any] | None = None) -> dict[str, object]:
        try:
            result = self._app.execute_control(command, values)
        except Exception as exc:
            return response(self._app, control_source(command), "ERROR", str(exc), error=str(exc))
        if result.ok:
            return response(self._app, control_source(command), "OK", result_summary(result))
        return response(self._app, control_source(command), "ERROR", result.error or "Command failed.", error=result.error)

    def save_preferences(self, values: dict[str, Any]) -> dict[str, object]:
        self._app.save_preferences(values)
        return response(self._app, "GUI", "CONFIG", "Preferences saved.")

    def set_backend_startup(self, enabled: bool) -> dict[str, object]:
        try:
            if enabled:
                install_startup_task()
                message = "Background program enabled."
            else:
                uninstall_startup_task()
                message = "Background program disabled."
        except Exception as exc:
            return response(self._app, "GUI", "ERROR", str(exc), error=str(exc))
        return response(self._app, "GUI", "CONFIG", message)

    def window_minimize(self) -> None:
        if self._window is not None:
            self._window.minimize()

    def window_close(self) -> None:
        if self._window is not None:
            self._window.destroy()

    def window_drag(self, screen_x: int, screen_y: int, offset_x: int, offset_y: int) -> None:
        if self._window is not None:
            self._window.move(int(screen_x) - int(offset_x), int(screen_y) - int(offset_y))


def response(
    app: DeskBridgeGui,
    source: str,
    kind: str,
    message: str,
    *,
    error: str | None = None,
) -> dict[str, object]:
    data: dict[str, object] = {
        "state": app.state(),
        "notification": {
            "source": source,
            "kind": kind,
            "message": message,
        },
    }
    if error:
        data["error"] = error
    return data


def ensure_web_assets() -> Path:
    target = data_dir() / "gui_web"
    source = bundle_root() / "data" / "gui_web"
    if target.exists():
        copy_missing_assets(source, target)
        return target
    if source.exists():
        copytree(source, target, dirs_exist_ok=True)
    else:
        target.mkdir(parents=True, exist_ok=True)
    return target


def copy_missing_assets(source: Path, target: Path) -> None:
    if not source.exists() or source == target:
        return
    for item in source.rglob("*"):
        relative = item.relative_to(source)
        destination = target / relative
        if item.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
        elif not destination.exists():
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(item.read_bytes())


def normalize_port(port: str | None) -> str | None:
    value = str(port or "").strip()
    if not value or value.lower() == "auto":
        return None
    return value


def config_bool(config: AppConfig, key: str, default: bool) -> bool:
    value = str(config.values.get(key) or ("on" if default else "off")).lower()
    return value in {"on", "true", "yes", "1", "active"}


def action_source(action: str) -> str:
    return action.split("_", 1)[0].upper()


def control_source(command: str) -> str:
    return command.split(".", 1)[0].upper()


def result_summary(result: CommandResult) -> str:
    if result.data:
        rendered = ", ".join(f"{key}={value}" for key, value in result.data.items())
        if rendered:
            return rendered
    if result.message:
        return result.message
    if result.raw_response:
        return "Response received."
    return "Done."


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        AppConfig.load()
        DeviceSDK()
        ensure_web_assets()
        print("DeskBridge GUI self-test OK")
        return 0

    return DeskBridgeGui(auto_connect=not args.no_autoconnect, port=args.port).run()


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="DeskBridge graphical interface")
    parser.add_argument("--no-autoconnect", action="store_true")
    parser.add_argument("--port")
    parser.add_argument("--self-test", action="store_true", help=argparse.SUPPRESS)
    return parser.parse_args(argv)
