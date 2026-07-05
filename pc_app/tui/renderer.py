"""Renderer based on tuiDesign.cmd."""

from __future__ import annotations

from dataclasses import dataclass, field
import textwrap
import unicodedata

from cli import theme
from sdk.identity import USB_ID

from .layout import ANSI_RE, BL, BR, BT, H, LT, RT, TL, TR, TT, V, Canvas, HEIGHT, WIDTH, strip_ansi


LEFT_WIDTH = 98
RIGHT_WIDTH = 21
LEFT_TEXT_WIDTH = 95
RIGHT_TEXT_WIDTH = 18


@dataclass(frozen=True, slots=True)
class TuiSnapshot:
    connected: bool
    port: str | None = None
    vid_pid: str = USB_ID
    uid: str = "-"
    rtc: str = "--:--:--"
    firmware: str = "-"
    protocol: str = "-"
    status: str = "offline"
    oled: str = "-"
    profile: str = "-"
    config: str = "-"
    raw_lines: list[str] = field(default_factory=list)
    info_lines: list[str] = field(default_factory=list)
    shell_lines: list[str] = field(default_factory=list)
    error_lines: list[str] = field(default_factory=list)
    event_lines: list[str] = field(default_factory=list)
    shell_scroll: int = 0
    input_line: str = ""
    function_keys_active: bool = True


class TuiRenderer:
    """Render a fixed 120x30 monitor layout."""

    width = WIDTH
    height = HEIGHT
    shell_visible_rows = 18

    @property
    def right_width(self) -> int:
        return 21

    @property
    def left_width(self) -> int:
        return max(58, self.width - self.right_width - 1)

    @property
    def left_text_width(self) -> int:
        return max(20, self.left_width - 3)

    @property
    def right_text_width(self) -> int:
        return max(12, self.right_width - 3)

    @property
    def right_x(self) -> int:
        return self.left_width + 3

    def configure_terminal_size(self, cols: int, lines: int, *, function_keys_active: bool) -> None:
        self.width = max(80, int(cols))
        footer_lines = 1 if function_keys_active else 0
        minimum_body_lines = 23 if function_keys_active else 24
        self.height = max(minimum_body_lines, int(lines) - footer_lines)
        self.shell_visible_rows = max(1, self.height - 12)

    def render(self, snapshot: TuiSnapshot) -> str:
        canvas = Canvas(self.width, self.height)
        self._template(canvas, snapshot)
        self._content(canvas, snapshot)
        lines = canvas.render().splitlines()
        if not lines:
            return ""
        lines[0] = render_header(snapshot, self.width)
        colored = [lines[0]]
        for index, line in enumerate(lines[1:], start=1):
            colored.append(colorize_body_line(index, line, self.left_width, self.height))
        if snapshot.function_keys_active:
            colored.append(render_function_keys(self.width))
        return "\n".join(colored)

    def _template(self, canvas: Canvas, snapshot: TuiSnapshot) -> None:
        canvas.line_text(0, self._header(snapshot))
        canvas.line_text(1, TL + label_line("[ RAW RECIVED ]", self.left_width - 1) + TT + label_line("[ ERROR ]", self.right_width - 1) + TR)

        for row in range(2, 7):
            canvas.line_text(row, self._split_empty_line())

        canvas.line_text(7, LT + label_line("[ INFO LINK ]", self.left_width - 1) + RT + (" " * (self.right_width - 1)) + V)
        canvas.line_text(8, self._split_empty_line())
        canvas.line_text(9, LT + label_line("[ SHELL ]", self.left_width - 1) + RT + (" " * (self.right_width - 1)) + V)

        for row in range(10, 15):
            canvas.line_text(row, self._split_empty_line())

        canvas.line_text(15, V + (" " * (self.left_width - 1)) + LT + label_line("[ EVENT ]", self.right_width - 1) + RT)

        for row in range(16, self.height - 1):
            canvas.line_text(row, self._split_empty_line())

        canvas.line_text(self.height - 1, BL + (H * (self.left_width - 1)) + BT + (H * (self.right_width - 1)) + BR)

    def _header(self, snapshot: TuiSnapshot) -> str:
        port = snapshot.port or "COM-"
        return (
            f"  DESKBRIDGE TUI MONITOR                           {port:<7}"
            f"{snapshot.vid_pid:<10}"
            f"{snapshot.uid:<7}"
            f"FW {snapshot.firmware:<5}"
            f"PROTOCOL: {snapshot.protocol:<4}"
            f"RTC {snapshot.rtc:<14}"
        )[: self.width].ljust(self.width)

    def _content(self, canvas: Canvas, snapshot: TuiSnapshot) -> None:
        raw = wrap_lines(snapshot.raw_lines or [], self.left_text_width)
        write_block(canvas, 2, 6, 2, self.left_text_width, raw)

        info = wrap_lines(snapshot.info_lines or [
            "Actual status of periferic, ver cual es el actual menu de la oled y otros datos actuales",
            f"status={snapshot.status}  connection={'yes' if snapshot.connected else 'no'}",
        ], self.left_text_width)
        write_block(canvas, 8, 8, 2, self.left_text_width, info)

        shell_lines = wrap_lines(snapshot.shell_lines, self.left_text_width)
        shell = visible_lines(shell_lines, self.shell_visible_rows, snapshot.shell_scroll)
        write_block(canvas, 10, self.height - 3, 2, self.left_text_width, shell)
        canvas.text(2, self.height - 2, f"DB/> {snapshot.input_line}", width=self.left_text_width)

        errors = wrap_lines(snapshot.error_lines or [], self.right_text_width)
        write_block(canvas, 2, 14, self.right_x, self.right_text_width, errors)

        events = wrap_lines(snapshot.event_lines or [], self.right_text_width)
        write_block(canvas, 16, self.height - 2, self.right_x, self.right_text_width, events)

    def _split_empty_line(self) -> str:
        return V + (" " * (self.left_width - 1)) + V + (" " * (self.right_width - 1)) + V


def label_line(label: str, width: int) -> str:
    remaining = max(0, width - len(label))
    return (H * 2) + label + (H * max(0, remaining - 2))


def write_block(canvas: Canvas, start_y: int, end_y: int, x: int, width: int, lines: list[str]) -> None:
    row = start_y
    for line in lines:
        if row > end_y:
            break
        canvas.text(x, row, line, width=width)
        row += 1


def visible_lines(lines: list[str], height: int, scroll: int) -> list[str]:
    if len(lines) <= height:
        return lines
    offset = min(max(0, scroll), len(lines) - height)
    end = len(lines) - offset
    start = max(0, end - height)
    return lines[start:end]


def wrap_lines(lines: list[str], width: int) -> list[str]:
    wrapped: list[str] = []
    for line in lines:
        text = sanitize_display_text(strip_ansi(str(line)))
        if not text:
            wrapped.append("")
            continue
        if is_preformatted_shell_line(text):
            wrapped.append(text[:width])
            continue
        wrapped.extend(textwrap.wrap(text, width=width, subsequent_indent="  ", replace_whitespace=False) or [""])
    return wrapped


def is_preformatted_shell_line(text: str) -> bool:
    return (
        is_help_header(text.strip())
        or is_help_separator(text.strip())
        or is_help_row(text)
        or is_help_continuation(text)
    )


def sanitize_display_text(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value)
    return "".join(char for char in normalized if 32 <= ord(char) <= 126)


def render_header(snapshot: TuiSnapshot, width: int = WIDTH) -> str:
    port = snapshot.port or "COM-"
    com_role = "tui_com_connected" if snapshot.connected else "tui_com_disconnected"
    com_fallback = (theme.Style.BRIGHT, theme.Fore.WHITE) if snapshot.connected else (theme.Fore.LIGHTBLACK_EX,)
    segments = [
        theme.role("  DESKBRIDGE TUI MONITOR                           ", "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE),
        theme.role(f"{port:<7}", com_role, *com_fallback),
        theme.role(f"{snapshot.vid_pid:<10}", "tui_vidpid", theme.Style.BRIGHT, theme.Fore.CYAN),
        theme.role(f"{snapshot.uid:<7}", "tui_uid", theme.Fore.MAGENTA),
        theme.role("FW ", "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE),
        theme.role(f"{snapshot.firmware:<5}", "tui_fw", theme.Style.BRIGHT, theme.Fore.YELLOW),
        theme.role("PROTOCOL: ", "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE),
        theme.role(f"{snapshot.protocol:<4}", "tui_protocol", theme.Style.BRIGHT, theme.Fore.BLUE),
        theme.role("RTC ", "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE),
        theme.role(f"{snapshot.rtc:<14}", "tui_rtc", theme.Style.BRIGHT, theme.Fore.MAGENTA),
    ]
    rendered = "".join(segments)
    visible = strip_ansi(rendered)
    if len(visible) > width:
        return truncate_ansi(rendered, width)
    if len(visible) < width:
        rendered += theme.role(" " * (width - len(visible)), "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    return rendered


def colorize_body_line(index: int, line: str, left_width: int = LEFT_WIDTH, height: int = HEIGHT) -> str:
    if 10 <= index <= height - 2:
        return colorize_shell_line(line, left_width)
    if 2 <= index <= 8 or 16 <= index <= height - 2:
        return colorize_aux_line(index, line, left_width, height)
    return theme.role(line, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)


def colorize_aux_line(index: int, line: str, left_width: int, height: int) -> str:
    if len(line) < left_width:
        return theme.role(line, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)

    left_prefix = line[:2]
    left_content = line[2 : left_width - 1]
    middle = line[left_width - 1 : left_width + 2]
    right_content = line[left_width + 2 : -1]
    suffix = line[-1:]

    if 2 <= index <= 6:
        left_rendered = fit_colored_content(theme.role(left_content.rstrip(), "raw", theme.Fore.CYAN), len(left_content))
    elif index == 8:
        left_rendered = fit_colored_content(theme.role(left_content.rstrip(), "output", theme.Fore.WHITE), len(left_content))
    else:
        left_rendered = theme.role(left_content, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)

    if 2 <= index <= 14:
        right_rendered = fit_colored_content(theme.error(right_content.rstrip()), len(right_content))
    elif 16 <= index <= height - 2:
        right_rendered = fit_colored_content(theme.warning(right_content.rstrip()), len(right_content))
    else:
        right_rendered = theme.role(right_content, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)

    return (
        theme.role(left_prefix, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
        + left_rendered
        + theme.role(middle, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
        + right_rendered
        + theme.role(suffix, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    )


def colorize_shell_line(line: str, left_width: int = LEFT_WIDTH) -> str:
    if len(line) < left_width:
        return theme.role(line, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)

    prefix = line[:2]
    content = line[2 : left_width - 1]
    suffix = line[left_width - 1 :]
    return (
        theme.role(prefix, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
        + colorize_shell_content(content)
        + theme.role(suffix, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    )


def colorize_shell_content(content: str) -> str:
    text = content.rstrip()
    if not text:
        return theme.role(content, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    if is_help_header(text.strip()):
        return fit_colored_content(colorize_help_header(text), len(content))
    if is_help_separator(text.strip()):
        return fit_colored_content(theme.muted(text), len(content))
    if is_help_continuation(text):
        return fit_colored_content(colorize_help_continuation(text), len(content))
    if is_help_row(text):
        return fit_colored_content(colorize_help_row(text), len(content))

    stripped = text.strip()
    leading = text[: len(text) - len(text.lstrip())]
    core = text[len(leading):]
    base_leading = theme.role(leading, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)

    if core.startswith("DB/>"):
        command_part = core[4:].strip()
        rendered = theme.prompt("DB/>")
        if command_part:
            rendered += " " + theme.highlight_command_line(command_part)
        return fit_colored_content(base_leading + rendered, len(content))
    if is_help_title(core):
        return fit_colored_content(base_leading + theme.title(core), len(content))
    if is_tui_control_line(core):
        return fit_colored_content(base_leading + colorize_tui_control_line(core), len(content))
    if is_paths_tree_line(core):
        return fit_colored_content(base_leading + colorize_paths_tree_line(core), len(content))
    if core.lower().startswith(("error", "could not", "f8 connection error")):
        return fit_colored_content(base_leading + theme.error(core), len(content))
    if core.lower().startswith(("connected", "f8 connected", "f5 refresh", "paletes reloaded", "languages reloaded")):
        return fit_colored_content(base_leading + theme.success(core), len(content))
    if core.startswith("  ") and any(token in core for token in ("<", "[", "|")):
        return fit_colored_content(base_leading + theme.highlight_usage(core.strip()), len(content))
    if ":" in core and not core.startswith("http"):
        key, value = core.split(":", 1)
        return fit_colored_content(base_leading + theme.variable(key) + theme.muted(":") + theme.value(value), len(content))
    return fit_colored_content(base_leading + theme.role(core, "output", theme.Fore.WHITE), len(content))


def fit_colored_content(rendered: str, width: int) -> str:
    visible = strip_ansi(rendered)
    if len(visible) >= width:
        return truncate_ansi(rendered, width)
    padding = " " * (width - len(visible))
    return rendered + theme.role(padding, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)


def truncate_ansi(value: str, width: int) -> str:
    output: list[str] = []
    visible = 0
    index = 0
    while index < len(value) and visible < width:
        if value[index] == "\x1b":
            match = ANSI_RE.match(value, index)
            if match:
                output.append(match.group(0))
                index = match.end()
                continue
        output.append(value[index])
        visible += 1
        index += 1
    return "".join(output) + theme.Style.RESET_ALL


def is_help_title(text: str) -> bool:
    if text in {
        "Local commands",
        "Peripheral commands via SDK",
        "TUI controls",
        "Paths",
        "Project tree",
        "LOG",
    }:
        return True
    return text in {
        "Comandos locales",
        "Comandos del chip via SDK",
        "Comandos del chip vía SDK",
        "TUI controls",
        "Settings help",
        "Settings palete",
        "Settings language",
        "Settings functionKey",
        "Palete",
        "Paletes",
        "Language",
        "Languages",
    }


def is_help_header(text: str) -> bool:
    return ("COMANDO" in text or "COMMAND" in text) and "ARGS" in text


def is_help_separator(text: str) -> bool:
    stripped = text.strip()
    return len(stripped) >= 8 and set(stripped) == {"-"}


def is_tui_control_line(text: str) -> bool:
    stripped = text.strip()
    return stripped.startswith(("ARROW ", "PAGE ", "HOME/", "F5 ", "F6 ", "F8 ", "F9 ", "F10 ", "F11 "))


def is_paths_tree_line(text: str) -> bool:
    stripped = text.strip()
    if not stripped:
        return False
    return (
        stripped.startswith(("U:\\", "|", "\\---", "+---"))
        or "\\DeskBridge\\" in stripped
        or "\\pc_app" in stripped
    )


def is_help_continuation(text: str) -> bool:
    return text.startswith(" " * 60) and bool(text.strip())


def is_help_row(text: str) -> bool:
    stripped = text.strip()
    if not stripped or stripped.startswith("<...>"):
        return False
    first = stripped.split(maxsplit=1)[0]
    return first in {
        "help",
        "connect",
        "disconnect",
        "reconnect",
        "status",
        "settings",
        "config",
        "paths",
        "history",
        "notify",
        "clear",
        "firmware_upload",
        "exit",
        "ping",
        "info",
        "usb",
        "program",
        "peripheral",
        "bus",
        "keypad",
        "light",
        "wireless",
        "reset",
        "rtc",
    }


def colorize_help_header(text: str) -> str:
    return theme.muted(text)


def colorize_tui_control_line(text: str) -> str:
    parts = text.strip().split(maxsplit=1)
    key = parts[0] if parts else text.strip()
    rest = parts[1] if len(parts) > 1 else ""
    return f"{theme.command(key):<0} {theme.muted(rest)}"


def colorize_paths_tree_line(text: str) -> str:
    if "//" in text:
        path_part, comment = text.split("//", 1)
        return (
            theme.role(path_part.rstrip(), "path", theme.Fore.CYAN)
            + theme.role("  // " + comment.strip(), "muted", theme.Fore.LIGHTBLACK_EX)
        )
    return theme.role(text.rstrip(), "path", theme.Fore.CYAN)


def colorize_help_row(text: str) -> str:
    command_raw = text[:18]
    args_raw = text[18:60]
    desc_raw = text[60:]
    rendered = theme.command(command_raw.rstrip())
    rendered += theme.role(" " * max(0, len(command_raw) - len(command_raw.rstrip())), "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    if args_raw.strip():
        rendered += theme.highlight_usage(args_raw.strip())
        rendered += theme.role(" " * max(0, len(args_raw) - len(args_raw.strip())), "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    else:
        rendered += theme.role(args_raw, "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    if desc_raw.strip():
        rendered += theme.muted(desc_raw.strip())
    return rendered


def colorize_help_continuation(text: str) -> str:
    return theme.role(text[:60], "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE) + theme.muted(text[60:].rstrip())


def render_function_keys(width: int = WIDTH) -> str:
    segments = [theme.role("  ", "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)]
    for key, label in [
        ("F5", "Refresh all data"),
        ("F6", "Sync RTC"),
        ("F8", "Disconnect/Connect"),
        ("F11", "Full screen"),
        ("F10", "Help"),
        ("F9", "Exit"),
    ]:
        segments.append(function_key_badge(key))
        segments.append(theme.role(f"  {label}   ", "tui_function_text", theme.Style.BRIGHT, theme.Fore.WHITE))
    rendered = "".join(segments)
    visible = strip_ansi(rendered)
    if len(visible) > width:
        return truncate_ansi(rendered, width)
    if len(visible) < width:
        rendered += theme.role(" " * (width - len(visible)), "tui_base", theme.Style.BRIGHT, theme.Fore.WHITE)
    return rendered


def function_key_badge(key: str) -> str:
    return theme.color(f" {key} ", theme.Back.WHITE, theme.Fore.BLACK)
