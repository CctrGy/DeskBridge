"""Human-facing CLI formatting."""

from __future__ import annotations

from sdk.result import CommandResult

from . import theme


def format_result(result: CommandResult, *, debug: bool = False) -> str:
    sections: list[str] = []

    if debug:
        sections.append(f"{theme.debug_marker('>>')} {theme.highlight_command_line(result.raw_command)}")
        if result.raw_response is not None:
            sections.append(f"{theme.debug_marker('<< raw')}\n{theme.muted(result.raw_response.rstrip())}")
        sections.append(f"{theme.debug_marker('<< data')}\n{theme.value(result.data)}")

    if not result.ok:
        sections.append(f"{theme.error('ERROR')}: {result.error or 'comando fallido'}")
        return "\n\n".join(sections)

    if result.command_name.startswith("rtc."):
        sections.append(format_rtc(result))
    elif result.data:
        sections.append(format_table(title=result.command_name.upper(), data=result.data))
    elif result.message:
        sections.append(theme.success(result.message))
    else:
        sections.append(theme.success("OK"))

    return "\n\n".join(section for section in sections if section)


def format_rtc(result: CommandResult) -> str:
    data = result.data or {}
    if result.command_name == "rtc.status":
        return format_table(
            title="RTC",
            data={
                "Estado": "Activo" if data.get("state") else "Fallo",
                "Perdida energia": "Si" if data.get("lost_power") else "No",
            },
        )

    labels = {}
    if "date" in data:
        labels["Fecha"] = data["date"]
    if "time" in data:
        labels["Hora"] = data["time"]
    if result.command_name in {"rtc.set", "rtc.set_date", "rtc.set_time", "rtc.sync"}:
        labels["Resultado"] = "OK"

    return format_table("RTC", labels or data)


def format_table(title: str, data: dict[str, object]) -> str:
    if not data:
        return title
    flat = flatten_data(data)
    width = max(len(str(key)) for key in flat)
    lines = [theme.title(title), theme.muted("-" * max(len(title), 16))]
    for key, value in flat.items():
        lines.append(f"{theme.variable(f'{key:<{width}}')} {theme.muted(':')} {format_value(value, key)}")
    return "\n".join(lines)


def format_value(raw_value: object, key: str = "") -> str:
    if isinstance(raw_value, bool):
        return theme.success("Si") if raw_value else theme.error("No")
    if isinstance(raw_value, int) and key.lower().rsplit(".", 1)[-1] in {"vid", "pid", "bcd"}:
        return theme.value(f"0x{raw_value:04X}")
    if str(raw_value).upper() == "OK":
        return theme.success(raw_value)
    return theme.value(raw_value)


def flatten_data(data: dict[str, object]) -> dict[str, object]:
    flat: dict[str, object] = {}
    for key, value in data.items():
        if isinstance(value, dict):
            for child_key, child_value in value.items():
                flat[f"{key}.{child_key}"] = child_value
        elif isinstance(value, list):
            for index, item in enumerate(value):
                if isinstance(item, dict):
                    for child_key, child_value in item.items():
                        flat[f"{key}[{index}].{child_key}"] = child_value
                else:
                    flat[f"{key}[{index}]"] = item
        else:
            flat[key] = value
    return flat
