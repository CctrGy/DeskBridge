"""Base objects for the DeskBridge Text User Interface."""

from __future__ import annotations

from dataclasses import dataclass, field
import os
import shutil
import sys
from time import monotonic, sleep

from sdk.transport import SerialTransport
from sdk import DeviceSDK
from sdk.protocol import MessageKind, ProtocolHeader, TuiNotificationMessage
from cli import theme

from .renderer import TuiRenderer, TuiSnapshot, wrap_lines
from .shell import TuiCommandShell, normalize_error_code


@dataclass(slots=True)
class TuiWindowSpec:
    """Plain data placeholder for future text-window definitions."""

    name: str
    title: str
    region: str | None = None
    options: dict[str, object] = field(default_factory=dict)


class TuiApp:
    """Skeleton for the future multi-window text UI."""

    app_name = "DeskBridge TUI"

    def __init__(self, device: DeviceSDK | None = None) -> None:
        self.device = device or DeviceSDK()
        self.device.log_manager.set_origin("TUI")
        self.windows: list[TuiWindowSpec] = []
        self.renderer = TuiRenderer()
        self._configure_renderer_from_config()
        self.shell = TuiCommandShell(self.device)
        self.device.command_channel = ProtocolHeader.COMMAND
        self.raw_lines: list[str] = []
        self.shell_lines: list[str] = []
        self.error_lines: list[str] = []
        self.event_lines: list[str] = []
        self.shell_scroll = 0
        self._input_buffer = ""
        self._history_index: int | None = None
        self.uid = "-"
        self.firmware = "-"
        self.protocol = "-"
        self.rtc = "--:--:--"
        self.oled = "-"
        self.profile = "-"
        self.config = "-"
        self._applied_terminal_size = (
            self.device.log_manager.config.terminal_cols,
            self.device.log_manager.config.terminal_lines,
        )
        self._needs_full_clear = False

    def register_window(self, window: TuiWindowSpec) -> None:
        self.windows.append(window)

    def run(self, *, auto_connect: bool = True) -> int:
        self.device.log_manager.pc("INFO", "TUI entry initialized")
        if auto_connect and self.device.log_manager.config.should_auto_connect:
            self._try_connect()
        setup_terminal(
            self._title(),
            function_keys_active=self._function_keys_active(),
            cols=self.device.log_manager.config.terminal_cols,
            lines=self.device.log_manager.config.terminal_lines,
        )
        self._configure_renderer_from_config()
        try:
            self._render()
            while True:
                action, line = self._read_shell_action()
                if action == "exit":
                    return 0
                if action == "refresh":
                    self._quick_refresh()
                    self._render()
                    continue
                if action == "toggle_connection":
                    self._toggle_connection()
                    self._render()
                    continue
                if action == "sync_rtc":
                    self._sync_rtc()
                    self._render()
                    continue
                if action == "redraw":
                    self._render()
                    continue
                if action != "submit" or not line:
                    continue
                self.shell_lines.append(f"DB/> {line}")
                should_continue, output = self.shell.execute(line)
                self._sync_shell_commands()
                self._apply_configured_terminal_size_if_changed()
                self._route_side_output()
                self._route_output(output)
                if self.device.connected:
                    self._refresh_device_summary()
                set_terminal_title(self._title())
                self._render()
                if not should_continue:
                    return 0
        finally:
            restore_terminal()

    def _try_connect(self) -> None:
        if self.device.connected:
            return
        try:
            self.device.connect()
            self.device.log_manager.config.set_connect_state(True)
            self._refresh_device_summary()
        except Exception as exc:
            self.device.log_manager.config.set_connect_state(False)
            self.device.log_manager.pc("WARNING", "TUI auto-connect skipped: %s", exc)
            self.error_lines.append(normalize_error_code(str(exc)))

    def _configure_renderer_from_config(self) -> None:
        config = self.device.log_manager.config
        self.renderer.configure_terminal_size(
            config.terminal_cols,
            config.terminal_lines,
            function_keys_active=self._function_keys_active(),
        )

    def _apply_configured_terminal_size_if_changed(self) -> None:
        config = self.device.log_manager.config
        current = (config.terminal_cols, config.terminal_lines)
        if current != self._applied_terminal_size:
            apply_terminal_size(
                cols=config.terminal_cols,
                lines=config.terminal_lines,
                function_keys_active=self._function_keys_active(),
            )
            self._applied_terminal_size = current
        self._configure_renderer_from_config()

    def _snapshot(self) -> TuiSnapshot:
        return TuiSnapshot(
            connected=self.device.connected,
            port=self.device.port,
            uid=self.uid,
            rtc=self.rtc if self.device.connected else "--:--:--",
            firmware=self.firmware,
            protocol=self.protocol,
            status="connected" if self.device.connected else "offline",
            raw_lines=self.raw_lines[-5:],
            oled=self.oled,
            profile=self.profile,
            config=self.config,
            info_lines=self._info_link_lines(),
            shell_lines=self.shell_lines,
            error_lines=self.error_lines[-13:],
            event_lines=self.event_lines[-13:],
            shell_scroll=self.shell_scroll,
            input_line=self._input_buffer,
            function_keys_active=self._function_keys_active(),
        )

    def _status_text(self) -> str:
        return "connected" if self.device.connected else "offline"

    def _info_link_lines(self) -> list[str]:
        notifications = self.shell.notification_info_lines()
        if notifications:
            return notifications
        return [f"oled={self.oled}  profile={self.profile}  config={self.config}"]

    def _title(self) -> str:
        port = f" {self.device.port}" if self.device.connected and self.device.port else ""
        return f"{self.app_name}{port}"

    def _function_keys_active(self) -> bool:
        return self.device.log_manager.config.function_key_active

    def _refresh_device_summary(self) -> None:
        try:
            program = self.device.core.program()
            if program.ok and program.data:
                self.firmware = str(
                    program.data.get("program.version")
                    or program.data.get("version")
                    or program.data.get("fw")
                    or self.firmware
                )
                self.protocol = str(
                    program.data.get("program.proto")
                    or program.data.get("program.protocol")
                    or program.data.get("protocol")
                    or self.protocol
                )
        except Exception:
            pass

        try:
            info = self.device.core.info()
            if info.ok and info.data:
                self.uid = str(
                    info.data.get("uid")
                    or info.data.get("device.uid")
                    or info.data.get("id")
                    or self.uid
                )
        except Exception:
            pass

        try:
            rtc = self.device.rtc.get_time()
            if rtc.ok and rtc.data and rtc.data.get("time"):
                self.rtc = str(rtc.data["time"])
        except Exception:
            pass

    def _route_output(self, lines: list[str]) -> None:
        for line in lines:
            if is_raw_firmware_line(line):
                self.raw_lines.append(line)
                if line.startswith("[ERR]"):
                    self.error_lines.append(normalize_error_code(line))
                elif line.startswith("[EVT]") or line.startswith("[EVENT"):
                    self.event_lines.append(line)
                elif line.startswith("[TFI]"):
                    parsed = self.device.parser.parse_line(line)
                    if isinstance(parsed, TuiNotificationMessage):
                        self.shell.record_notification(parsed)
                continue

            stripped = line.strip()
            lowered = stripped.lower()
            if stripped.startswith("[ERR]") or lowered.startswith(("error", "err ")):
                self.error_lines.append(normalize_error_code(line))
            elif stripped.startswith(("[EVT]", "[EVENT")) or lowered.startswith("event "):
                self.event_lines.append(line)
            else:
                self.shell_lines.append(line)
                self.shell_scroll = 0

    def _route_side_output(self) -> None:
        raw, errors, events = self.shell.pop_side_output()
        self.raw_lines.extend(raw)
        self.error_lines.extend(errors)
        self.event_lines.extend(events)

    def _sync_shell_commands(self) -> None:
        if self.shell.clear_requested:
            self.shell_lines = []
            self.error_lines.clear()
            self.event_lines.clear()
            self.shell_scroll = 0
            self.shell.clear_requested = False

    def _render(self) -> None:
        self._poll_device()
        if self._needs_full_clear:
            sys.stdout.write("\033[2J\033[H")
            self._needs_full_clear = False
        else:
            sys.stdout.write("\033[H")
        sys.stdout.write(self.renderer.render(self._snapshot()))
        sys.stdout.flush()

    def _sync_actual_terminal_size(self) -> None:
        if not sys.stdout.isatty():
            return
        cols, lines = scan_terminal_size(function_keys_active=self._function_keys_active())
        if (cols, lines) == self._applied_terminal_size:
            return
        self._apply_terminal_canvas_size(cols, lines, persist=True)

    def _poll_device(self) -> None:
        if not self.device.connected:
            return
        try:
            messages = self.device.poll(limit=16, timeout=0.01)
        except Exception:
            return

        for message in messages:
            if message.kind is MessageKind.PROMPT:
                continue
            if message.raw:
                self.raw_lines.append(message.raw)
            if message.kind is MessageKind.ERROR:
                self.error_lines.append(normalize_error_code(message.raw))
            elif message.kind is MessageKind.EVENT:
                self.event_lines.append(message.raw)
            elif isinstance(message, TuiNotificationMessage):
                self.shell.record_notification(message)

    def _read_shell_action(self) -> tuple[str, str]:
        if os.name != "nt" or not sys.stdin.isatty():
            line = self._read_shell_line()
            return ("submit", line) if line else ("redraw", "")

        reader = WindowsKeyReader()
        self._draw_input()
        while True:
            event = reader.read()
            if event == "enter":
                value = self._input_buffer.strip()
                self._clear_input()
                self._history_index = None
                return ("submit", value)
            if event == "escape":
                self._input_buffer = ""
                self._draw_input()
                continue
            if event == "ctrl_c":
                return ("exit", "")
            if event == "backspace":
                self._input_buffer = self._input_buffer[:-1]
                self._draw_input()
                continue
            if event == "arrow_up":
                self._scroll_shell(1)
                return ("redraw", "")
            if event == "arrow_down":
                self._scroll_shell(-1)
                return ("redraw", "")
            if event == "page_up":
                self._scroll_shell(8)
                return ("redraw", "")
            if event == "page_down":
                self._scroll_shell(-8)
                return ("redraw", "")
            if event == "home":
                self._scroll_shell_home()
                return ("redraw", "")
            if event == "end":
                self.shell_scroll = 0
                return ("redraw", "")
            if event == "f5":
                if not self._function_keys_active():
                    continue
                return ("refresh", "")
            if event == "f6":
                if not self._function_keys_active():
                    continue
                return ("sync_rtc", "")
            if event == "f8":
                if not self._function_keys_active():
                    continue
                return ("toggle_connection", "")
            if event == "f9":
                if self._function_keys_active():
                    return ("exit", "")
                continue
            if event == "f10":
                if self._function_keys_active():
                    return ("submit", "help")
                continue
            if event == "f11":
                if self._function_keys_active():
                    self._toggle_fullscreen()
                    return ("redraw", "")
                continue
            if len(event) == 1 and event >= " ":
                self._input_buffer += event
                self._draw_input()

    def _read_shell_line(self) -> str:
        row = self._prompt_row()
        width = self._prompt_width()
        sys.stdout.write(f"\033[{row};8H")
        sys.stdout.write(" " * width)
        sys.stdout.write(f"\033[{row};8H")
        sys.stdout.flush()
        try:
            return input().strip()
        except (EOFError, KeyboardInterrupt):
            return "exit"

    def _draw_input(self) -> None:
        row = self._prompt_row()
        width = self._prompt_width()
        sys.stdout.write(f"\033[{row};8H")
        sys.stdout.write(" " * width)
        sys.stdout.write(f"\033[{row};8H")
        sys.stdout.write(self._input_buffer[:width])
        sys.stdout.flush()

    def _prompt_row(self) -> int:
        return max(1, self.renderer.height - 1)

    def _prompt_width(self) -> int:
        return max(1, self.renderer.left_text_width - 6)

    def _clear_input(self) -> None:
        self._input_buffer = ""
        self._draw_input()

    def _scroll_shell(self, delta: int) -> None:
        max_offset = self._max_shell_scroll()
        self.shell_scroll = min(max_offset, max(0, self.shell_scroll + delta))

    def _scroll_shell_home(self) -> None:
        self.shell_scroll = self._max_shell_scroll()

    def _max_shell_scroll(self) -> int:
        rendered_lines = wrap_lines(self.shell_lines, self.renderer.left_text_width)
        return max(0, len(rendered_lines) - self.renderer.shell_visible_rows)

    def _history_previous(self) -> None:
        if not self.shell.history.entries:
            return
        if self._history_index is None:
            self._history_index = len(self.shell.history.entries) - 1
        else:
            self._history_index = max(0, self._history_index - 1)
        self._input_buffer = self.shell.history.entries[self._history_index]

    def _history_next(self) -> None:
        if self._history_index is None:
            return
        self._history_index += 1
        if self._history_index >= len(self.shell.history.entries):
            self._history_index = None
            self._input_buffer = ""
            return
        self._input_buffer = self.shell.history.entries[self._history_index]

    def _quick_refresh(self) -> None:
        self.shell_lines.append("F5 refresh")
        for command in ("settings language list", "settings palete list"):
            _, output = self.shell.execute(command)
            self._route_output(output)
        try:
            port = SerialTransport.find_port()
            self.shell_lines.append(f"peripheral: found {port}")
        except Exception as exc:
            self.shell_lines.append(f"peripheral: {exc}")
        if self.device.connected:
            self._refresh_device_summary()
        self.shell_scroll = 0

    def _toggle_connection(self) -> None:
        if self.device.connected:
            port = self.device.port
            self.device.disconnect()
            self.device.log_manager.config.set_connect_state(False)
            self.rtc = "--:--:--"
            self.shell_lines.append(f"F8 disconnected: {port or '-'}")
            set_terminal_title(self._title())
            return

        try:
            port = self.device.connect()
            self.device.log_manager.config.set_connect_state(True)
            self._refresh_device_summary()
            self.shell_lines.append(f"F8 connected: {port}")
        except Exception as exc:
            self.device.log_manager.config.set_connect_state(False)
            self.shell_lines.append(f"F8 connection error: {exc}")
        set_terminal_title(self._title())
        self.shell_scroll = 0

    def _sync_rtc(self) -> None:
        if not self.device.connected:
            try:
                self.device.connect()
                self.device.log_manager.config.set_connect_state(True)
            except Exception as exc:
                self.device.log_manager.config.set_connect_state(False)
                self.shell_lines.append(f"F6 RTC sync error: {exc}")
                self.shell_scroll = 0
                return

        result = self.device.rtc.sync()
        if result.ok:
            data = result.data or {}
            self.rtc = str(data.get("time") or "--:--:--")
            self.shell_lines.append("F6 RTC sync: OK")
        else:
            self.shell_lines.append(f"F6 RTC sync error: {result.error or 'failed'}")
        self.shell_scroll = 0

    def _toggle_fullscreen(self) -> None:
        if os.name != "nt":
            self.shell_lines.append("F11 fullscreen is only available on Windows consoles.")
            return
        try:
            import ctypes

            hwnd = ctypes.windll.kernel32.GetConsoleWindow()
            ctypes.windll.user32.ShowWindow(hwnd, 3)
            cols, lines = wait_for_terminal_size(function_keys_active=self._function_keys_active())
            self._apply_terminal_canvas_size(cols, lines, persist=True)
            self.shell_lines.append(f"F11 size: {cols}x{lines}")
        except OSError as exc:
            self.shell_lines.append(f"F11 fullscreen error: {exc}")
        self.shell_scroll = 0

    def _apply_terminal_canvas_size(self, cols: int, lines: int, *, persist: bool) -> None:
        if persist:
            self.device.log_manager.config.set_terminal_size(cols, lines)
        self.renderer.configure_terminal_size(cols, lines, function_keys_active=self._function_keys_active())
        self._applied_terminal_size = (cols, lines)
        self._needs_full_clear = True


def setup_terminal(title: str, *, function_keys_active: bool = True, cols: int = 120, lines: int = 31) -> None:
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass

    if os.name == "nt":
        os.system("chcp 65001 >nul")
        apply_terminal_size(cols=cols, lines=lines, function_keys_active=function_keys_active)
        theme.reload()
        os.system("cls")
        set_terminal_title(title)
        return

    sys.stdout.write("\033[2J\033[H")
    set_terminal_title(title)


def apply_terminal_size(*, cols: int = 120, lines: int = 31, function_keys_active: bool) -> None:
    if os.name == "nt":
        base_lines = 31 if function_keys_active else 30
        cols = max(80, int(cols or 120))
        lines = max(base_lines, int(lines or base_lines))
        os.system(f"mode con cols={cols} lines={lines} >nul")


def scan_terminal_size(*, function_keys_active: bool) -> tuple[int, int]:
    if os.name == "nt":
        size = windows_console_window_size()
        if size is not None:
            return size
    fallback = (120, 31 if function_keys_active else 30)
    size = shutil.get_terminal_size(fallback=fallback)
    cols = max(80, size.columns)
    lines = max(31 if function_keys_active else 30, size.lines)
    return cols, lines


def wait_for_terminal_size(*, function_keys_active: bool, timeout: float = 1.2) -> tuple[int, int]:
    deadline = monotonic() + timeout
    last = scan_terminal_size(function_keys_active=function_keys_active)
    stable_count = 0

    while monotonic() < deadline:
        sleep(0.08)
        current = scan_terminal_size(function_keys_active=function_keys_active)
        if current == last:
            stable_count += 1
            if stable_count >= 3:
                return current
        else:
            stable_count = 0
            last = current
    return last


def windows_console_window_size() -> tuple[int, int] | None:
    try:
        import ctypes
        from ctypes import wintypes

        class Coord(ctypes.Structure):
            _fields_ = [("X", wintypes.SHORT), ("Y", wintypes.SHORT)]

        class SmallRect(ctypes.Structure):
            _fields_ = [
                ("Left", wintypes.SHORT),
                ("Top", wintypes.SHORT),
                ("Right", wintypes.SHORT),
                ("Bottom", wintypes.SHORT),
            ]

        class ConsoleScreenBufferInfo(ctypes.Structure):
            _fields_ = [
                ("dwSize", Coord),
                ("dwCursorPosition", Coord),
                ("wAttributes", wintypes.WORD),
                ("srWindow", SmallRect),
                ("dwMaximumWindowSize", Coord),
            ]

        info = ConsoleScreenBufferInfo()
        handle = ctypes.windll.kernel32.GetStdHandle(-11)
        if not ctypes.windll.kernel32.GetConsoleScreenBufferInfo(handle, ctypes.byref(info)):
            return None
        cols = int(info.srWindow.Right - info.srWindow.Left + 1)
        lines = int(info.srWindow.Bottom - info.srWindow.Top + 1)
        return max(80, cols), max(24, lines)
    except Exception:
        return None


def set_terminal_title(title: str) -> None:
    if os.name == "nt":
        try:
            import ctypes

            ctypes.windll.kernel32.SetConsoleTitleW(title)
        except OSError:
            pass
        return

    sys.stdout.write(f"\033]0;{title}\007")


def restore_terminal() -> None:
    sys.stdout.write("\033[0m")
    if os.name == "nt":
        os.system("color")
        os.system("cls")
        set_terminal_title("DeskBridge Terminal")
    else:
        sys.stdout.write("\033[2J\033[H")
    sys.stdout.flush()


def is_raw_firmware_line(line: str) -> bool:
    stripped = line.strip()
    return stripped.startswith(
        (
            "[RSP]",
            "[ERR]",
            "[LOG]",
            "[EVT]",
            "[EVENT",
            "[DAT]",
            "[STE]",
            "[STK]",
            "[INF]",
            "[MRK]",
            "[TSK]",
            "[TFI]",
        )
    )


class WindowsKeyReader:
    KEY_EVENT = 0x0001
    KEY_DOWN = 0x28
    KEY_UP = 0x26
    KEY_PAGE_UP = 0x21
    KEY_PAGE_DOWN = 0x22
    KEY_HOME = 0x24
    KEY_END = 0x23
    KEY_RETURN = 0x0D
    KEY_ESCAPE = 0x1B
    KEY_BACKSPACE = 0x08
    KEY_F5 = 0x74
    KEY_F6 = 0x75
    KEY_F8 = 0x77
    KEY_F9 = 0x78
    KEY_F10 = 0x79
    KEY_F11 = 0x7A
    CTRL_PRESSED = 0x0008 | 0x0004

    def __init__(self) -> None:
        import ctypes
        from ctypes import wintypes

        class CharUnion(ctypes.Union):
            _fields_ = [("UnicodeChar", wintypes.WCHAR), ("AsciiChar", ctypes.c_char)]

        class KeyEventRecord(ctypes.Structure):
            _fields_ = [
                ("bKeyDown", wintypes.BOOL),
                ("wRepeatCount", wintypes.WORD),
                ("wVirtualKeyCode", wintypes.WORD),
                ("wVirtualScanCode", wintypes.WORD),
                ("uChar", CharUnion),
                ("dwControlKeyState", wintypes.DWORD),
            ]

        class EventUnion(ctypes.Union):
            _fields_ = [("KeyEvent", KeyEventRecord)]

        class InputRecord(ctypes.Structure):
            _fields_ = [("EventType", wintypes.WORD), ("Event", EventUnion)]

        self._ctypes = ctypes
        self._wintypes = wintypes
        self._record_type = InputRecord
        self._kernel32 = ctypes.windll.kernel32
        self._handle = self._kernel32.GetStdHandle(-10)

    def read(self) -> str:
        record = self._record_type()
        read_count = self._wintypes.DWORD()

        while True:
            ok = self._kernel32.ReadConsoleInputW(self._handle, self._ctypes.byref(record), 1, self._ctypes.byref(read_count))
            if not ok or record.EventType != self.KEY_EVENT:
                continue

            event = record.Event.KeyEvent
            if not event.bKeyDown:
                continue

            key = int(event.wVirtualKeyCode)
            control = int(event.dwControlKeyState)
            controlled = bool(control & self.CTRL_PRESSED)
            char = event.uChar.UnicodeChar

            if controlled and char and char.lower() == "c":
                return "ctrl_c"
            if key == self.KEY_RETURN:
                return "enter"
            if key == self.KEY_ESCAPE:
                return "escape"
            if key == self.KEY_BACKSPACE:
                return "backspace"
            if key == self.KEY_UP:
                return "arrow_up"
            if key == self.KEY_DOWN:
                return "arrow_down"
            if key == self.KEY_PAGE_UP:
                return "page_up"
            if key == self.KEY_PAGE_DOWN:
                return "page_down"
            if key == self.KEY_HOME:
                return "home"
            if key == self.KEY_END:
                return "end"
            if key == self.KEY_F5:
                return "f5"
            if key == self.KEY_F6:
                return "f6"
            if key == self.KEY_F8:
                return "f8"
            if key == self.KEY_F9:
                return "f9"
            if key == self.KEY_F10:
                return "f10"
            if key == self.KEY_F11:
                return "f11"
            if char:
                return char
