"""RTC module using the real chip_core shell commands."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime

from sdk.exceptions import DeviceCommandError
from sdk.protocol import MessageKind, ValueMessage
from sdk.result import CommandResult

from .base import DeviceModule
from .utils import dict_for, replace_result


@dataclass(frozen=True, slots=True)
class RtcStatus:
    state: bool
    lost_power: bool


@dataclass(frozen=True, slots=True)
class RtcDateTime:
    value: datetime

    @property
    def date(self) -> str:
        return self.value.strftime("%d/%m/%Y")

    @property
    def time(self) -> str:
        return self.value.strftime("%H:%M:%S")

    @property
    def firmware_value(self) -> str:
        return self.value.strftime("%Y-%m-%dT%H:%M:%S")


class RtcModule(DeviceModule):
    """High-level RTC API.

    Verified chip_core commands:
      rtc?
      rtc get
      rtc set YYYY-MM-DDTHH:MM:SS

    Convenience methods such as get_date(), set_time(), and sync() are built on
    top of those commands without inventing unsupported firmware commands.
    """

    def status(self) -> CommandResult:
        result = self.command_result("rtc.status", "rtc?")
        if not result.ok:
            return result

        values = dict_for(result, "rtc")
        status = RtcStatus(
            state=bool(values.get("state", False)),
            lost_power=bool(values.get("lost_pwr", False)),
        )
        return replace_data(
            result,
            {
                "state": status.state,
                "lost_power": status.lost_power,
            },
        )

    def get(self) -> CommandResult:
        result = self.command_result("rtc.get", "rtc get")
        if not result.ok:
            return result

        values = dict_for(result, "rtc_time")
        raw_value = values.get("value")
        if not isinstance(raw_value, str):
            return CommandResult(
                ok=False,
                command_name=result.command_name,
                raw_command=result.raw_command,
                raw_response=result.raw_response,
                error="RTC response did not contain a datetime value.",
            )

        rtc = RtcDateTime(datetime.strptime(raw_value, "%Y-%m-%dT%H:%M:%S"))
        return replace_data(
            result,
            {
                "date": rtc.date,
                "time": rtc.time,
                "datetime": rtc.firmware_value,
            },
        )

    def get_date(self) -> CommandResult:
        result = self.get()
        if not result.ok:
            return result
        data = result.data or {}
        return replace_result(result, command_name="rtc.get_date", data={"date": data.get("date")})

    def get_time(self) -> CommandResult:
        result = self.get()
        if not result.ok:
            return result
        data = result.data or {}
        return replace_result(result, command_name="rtc.get_time", data={"time": data.get("time")})

    def set(self, value: datetime | str) -> CommandResult:
        if isinstance(value, datetime):
            firmware_value = value.strftime("%Y-%m-%dT%H:%M:%S")
        else:
            firmware_value = value
        return self.command_result("rtc.set", f"rtc set {firmware_value}")

    def set_date(self, value: str) -> CommandResult:
        current_result = self.get()
        if not current_result.ok:
            return replace_result(current_result, command_name="rtc.set_date")

        current = datetime.strptime(str((current_result.data or {}).get("datetime")), "%Y-%m-%dT%H:%M:%S")
        day, month, year = parse_date(value)
        result = self.set(current.replace(year=year, month=month, day=day))
        return replace_result(
            result,
            command_name="rtc.set_date",
            data={"date": value, "firmware_command": result.raw_command},
        )

    def set_time(self, value: str) -> CommandResult:
        current_result = self.get()
        if not current_result.ok:
            return replace_result(current_result, command_name="rtc.set_time")

        current = datetime.strptime(str((current_result.data or {}).get("datetime")), "%Y-%m-%dT%H:%M:%S")
        hour, minute, second = parse_time(value)
        result = self.set(current.replace(hour=hour, minute=minute, second=second))
        return replace_result(
            result,
            command_name="rtc.set_time",
            data={"time": value, "firmware_command": result.raw_command},
        )

    def sync(self) -> CommandResult:
        now = datetime.now().replace(microsecond=0)
        result = self.set(now)
        return replace_result(
            result,
            command_name="rtc.sync",
            data={
                "source": "pc_time_now",
                "date": now.strftime("%d/%m/%Y"),
                "time": now.strftime("%H:%M:%S"),
                "datetime": now.strftime("%Y-%m-%dT%H:%M:%S"),
                "firmware_command": result.raw_command,
            },
        )


def values_by_key(messages) -> dict[str, object]:
    values: dict[str, object] = {}
    for message in messages:
        if isinstance(message, ValueMessage) or message.kind == MessageKind.VALUE:
            values[message.key] = message.value
    return values


def parse_date(value: str) -> tuple[int, int, int]:
    parsed = datetime.strptime(value, "%d/%m/%Y")
    return parsed.day, parsed.month, parsed.year


def parse_time(value: str) -> tuple[int, int, int]:
    parsed = datetime.strptime(value, "%H:%M:%S")
    return parsed.hour, parsed.minute, parsed.second


def replace_data(result: CommandResult, data: dict[str, object]) -> CommandResult:
    return replace_result(result, data=data)
