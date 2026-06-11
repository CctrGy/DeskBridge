"""Pyserial transport restricted to DeskBridge VID/PID."""

from __future__ import annotations

from time import monotonic

from sdk.config_file import AppConfig
from sdk.exceptions import DeviceTimeoutError, TransportError

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - depends on the app environment.
    serial = None
    list_ports = None


class SerialTransport:
    """Move UTF-8 lines over the DeskBridge USB CDC port.

    This layer does not interpret the firmware protocol. It only finds the
    correct serial port, opens/closes it, and moves text lines.
    """

    VID = 0x1209
    PID = 0xDB01

    def __init__(
        self,
        port: str | None = None,
        *,
        baudrate: int = 115200,
        timeout: float = 0.1,
        write_timeout: float = 1.0,
        encoding: str = "utf-8",
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.write_timeout = write_timeout
        self.encoding = encoding
        self._serial = None
        self._rx_buffer = bytearray()

    @property
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    @classmethod
    def find_port(cls) -> str:
        cls._require_pyserial()
        for port in list_ports.comports():
            if port.vid == cls.VID and port.pid == cls.PID:
                return port.device
        raise TransportError(f"DeskBridge USB CDC not found ({cls.VID:04X}:{cls.PID:04X}).")

    @classmethod
    def port_matches_identity(cls, device: str) -> bool:
        cls._require_pyserial()
        normalized = device.upper()
        for port in list_ports.comports():
            if port.device.upper() == normalized:
                return port.vid == cls.VID and port.pid == cls.PID
        return False

    @classmethod
    def configured_or_detected_port(cls) -> str:
        config = AppConfig.load()
        configured_port = config.last_connection

        if configured_port and cls.port_matches_identity(configured_port):
            return configured_port

        detected_port = cls.find_port()
        config.set_last_connection(detected_port)
        return detected_port

    def connect(self) -> str:
        self._require_pyserial()
        if self.connected:
            return self._serial.port

        if self.port:
            if not self.port_matches_identity(self.port):
                raise TransportError(
                    f"{self.port} is not DeskBridge USB CDC ({self.VID:04X}:{self.PID:04X})."
                )
            selected_port = self.port
        else:
            selected_port = self.configured_or_detected_port()

        try:
            self._serial = serial.Serial(
                selected_port,
                baudrate=self.baudrate,
                timeout=self.timeout,
                write_timeout=self.write_timeout,
            )
        except serial.SerialException as exc:
            raise TransportError(f"Cannot open {selected_port}: {exc}") from exc

        self.port = selected_port
        AppConfig.load().set_last_connection(selected_port)
        self._rx_buffer.clear()
        return selected_port

    def disconnect(self) -> None:
        if self._serial is not None:
            self._serial.close()
        self._serial = None
        self._rx_buffer.clear()

    def write_line(self, line: str) -> None:
        port = self._require_connection()
        try:
            port.write(f"{line}\n".encode(self.encoding))
            port.flush()
        except serial.SerialException as exc:
            raise TransportError(f"Serial write failed: {exc}") from exc

    def read_line(self, timeout: float | None = None) -> str:
        port = self._require_connection()
        deadline = None if timeout is None else monotonic() + timeout

        while True:
            newline = self._rx_buffer.find(b"\n")
            if newline >= 0:
                raw = bytes(self._rx_buffer[:newline])
                del self._rx_buffer[: newline + 1]
                return raw.rstrip(b"\r").decode(self.encoding, errors="replace")

            if deadline is not None and monotonic() >= deadline:
                raise DeviceTimeoutError("Timed out waiting for a serial line.")

            try:
                waiting = port.in_waiting
                chunk = port.read(waiting or 1)
            except serial.SerialException as exc:
                raise TransportError(f"Serial read failed: {exc}") from exc

            if chunk:
                self._rx_buffer.extend(chunk)
            elif deadline is None:
                raise DeviceTimeoutError("No serial data available.")

    def read_until(self, marker: str, timeout: float | None = None) -> str:
        """Read text until marker appears. The marker may lack a newline."""

        port = self._require_connection()
        deadline = None if timeout is None else monotonic() + timeout
        marker_bytes = marker.encode(self.encoding)

        while True:
            marker_index = self._rx_buffer.find(marker_bytes)
            if marker_index >= 0:
                end = marker_index + len(marker_bytes)
                raw = bytes(self._rx_buffer[:end])
                del self._rx_buffer[:end]
                return raw.decode(self.encoding, errors="replace")

            if deadline is not None and monotonic() >= deadline:
                raise DeviceTimeoutError(f"Timed out waiting for {marker!r}.")

            try:
                waiting = port.in_waiting
                chunk = port.read(waiting or 1)
            except serial.SerialException as exc:
                raise TransportError(f"Serial read failed: {exc}") from exc

            if chunk:
                self._rx_buffer.extend(chunk)
            elif deadline is None:
                raise DeviceTimeoutError("No serial data available.")

    def _require_connection(self):
        if not self.connected:
            raise TransportError("DeskBridge serial transport is not connected.")
        return self._serial

    @staticmethod
    def _require_pyserial() -> None:
        if serial is None or list_ports is None:
            raise TransportError("pyserial is required for DeskBridge serial transport.")
