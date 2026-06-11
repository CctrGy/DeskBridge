"""Base class for device feature modules."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from sdk.device import DeviceSDK


class DeviceModule:
    """Common helper base for modules such as RTC, bus, and system."""

    def __init__(self, device: DeviceSDK) -> None:
        self._device = device

    def command(self, line: str, *, timeout: float | None = None):
        return self._device.command(line, timeout=timeout)

    def command_result(
        self,
        command_name: str,
        raw_command: str,
        *,
        timeout: float | None = None,
    ):
        return self._device.command_result(command_name, raw_command, timeout=timeout)
