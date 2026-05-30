"""DeskBridge serial USB connection and wire protocol helpers.

Windows exposes DeskBridge as one CDC ACM COM port. The current firmware shell
uses newline-terminated ASCII commands. The older framed helpers are kept here
for compatibility with experiments that still use binary frames.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from struct import Struct
from time import monotonic
from typing import Callable, Iterable

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - depends on the PC app environment.
    serial = None
    list_ports = None


class DeskBridgeUSBError(Exception):
    """Base error for the DeskBridge PC USB transport."""


class DeskBridgePortNotFound(DeskBridgeUSBError):
    """Raised when no serial port matches the DeskBridge USB identity."""


class DeskBridgeProtocolError(DeskBridgeUSBError):
    """Raised when bytes on the serial stream are not a valid protocol frame."""


class Channel(IntEnum):
    """Logical channels carried over the single DeskBridge COM port."""

    CONTROL = 0x01
    LOG = 0x02


class FrameType(IntEnum):
    """Initial frame kinds for the CONTROL and LOG channels."""

    DATA = 0x01
    EVENT = 0x02
    REQUEST = 0x03
    RESPONSE = 0x04
    ERROR = 0x7F


@dataclass(frozen=True, slots=True)
class Frame:
    """One decoded DeskBridge protocol frame."""

    channel: Channel
    frame_type: FrameType
    payload: bytes


class DeskBridgeUSB:
    """Serial transport for the DeskBridge CDC ACM interface.

    Frame layout, little endian:

        +----------+---------+---------+------+--------+---------+-------+
        | magic    | version | channel | type | length | payload | crc16 |
        | 2 bytes  | 1 byte  | 1 byte  | 1    | 2      | N bytes | 2     |
        +----------+---------+---------+------+--------+---------+-------+

    The CRC16-CCITT covers ``version`` through the end of ``payload``.  Magic
    bytes are only a stream synchronization marker and are not part of the CRC.
    """

    VID = 0x1209
    PID = 0xDB01

    MAGIC = b"\xDB\x01"
    VERSION = 0x01
    MAX_PAYLOAD = 4096

    _HEADER = Struct("<2sBBBH")
    _CRC = Struct("<H")

    def __init__(
        self,
        port: str | None = None,
        *,
        baudrate: int = 115200,
        timeout: float = 0.1,
        write_timeout: float = 1.0,
        max_payload: int = MAX_PAYLOAD,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.write_timeout = write_timeout
        self.max_payload = max_payload
        self._serial = None
        self._rx_buffer = bytearray()

    @property
    def connected(self) -> bool:
        """Whether the serial COM port is currently open."""

        return self._serial is not None and self._serial.is_open

    @classmethod
    def find_port(cls) -> str:
        """Return the COM port matching DeskBridge VID/PID."""

        cls._require_pyserial()
        matches = [
            port.device
            for port in list_ports.comports()
            if port.vid == cls.VID and port.pid == cls.PID
        ]
        if not matches:
            raise DeskBridgePortNotFound(
                f"No DeskBridge serial port found for VID={cls.VID:04X} "
                f"PID={cls.PID:04X}."
            )
        return matches[0]

    def connect(self) -> str:
        """Open the DeskBridge CDC ACM port and return its COM name."""

        self._require_pyserial()
        if self.connected:
            return self._serial.port

        selected_port = self.port or self.find_port()
        self._serial = serial.Serial(
            selected_port,
            baudrate=self.baudrate,
            timeout=self.timeout,
            write_timeout=self.write_timeout,
        )
        self.port = selected_port
        self._rx_buffer.clear()
        return selected_port

    def close(self) -> None:
        """Close the COM port if it is open."""

        if self._serial is not None:
            self._serial.close()
        self._serial = None
        self._rx_buffer.clear()

    def __enter__(self) -> "DeskBridgeUSB":
        self.connect()
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()

    def write_bytes(self, payload: bytes | bytearray | memoryview) -> None:
        """Write raw bytes to the DeskBridge serial shell."""

        port = self._require_connection()
        port.write(bytes(payload))
        port.flush()

    def write_text(self, text: str, *, encoding: str = "ascii") -> None:
        """Write raw text to the DeskBridge serial shell."""

        self.write_bytes(text.encode(encoding))

    def write_line(self, command: str = "", *, encoding: str = "ascii") -> None:
        """Write one shell command terminated by newline."""

        self.write_text(f"{command}\n", encoding=encoding)

    def read_bytes(self, timeout: float | None = None) -> bytes:
        """Read currently available raw serial bytes until timeout."""

        port = self._require_connection()
        deadline = None if timeout is None else monotonic() + timeout
        chunks = bytearray()

        while True:
            waiting = port.in_waiting
            chunk = port.read(waiting or 1)
            if chunk:
                chunks.extend(chunk)
                continue

            if deadline is None or monotonic() >= deadline:
                return bytes(chunks)

    def read_text(self, timeout: float | None = None, *, encoding: str = "ascii") -> str:
        """Read raw serial text until timeout."""

        return self.read_bytes(timeout=timeout).decode(encoding, errors="replace")

    def read_until_prompt(
        self,
        prompt: str = "db>",
        *,
        timeout: float = 2.0,
        encoding: str = "ascii",
    ) -> str:
        """Read shell text until the DeskBridge prompt appears or timeout passes."""

        port = self._require_connection()
        deadline = monotonic() + timeout
        chunks = bytearray()
        prompt_bytes = prompt.encode(encoding)

        while monotonic() < deadline:
            waiting = port.in_waiting
            chunk = port.read(waiting or 1)
            if chunk:
                chunks.extend(chunk)
                if prompt_bytes in chunks:
                    break

        return bytes(chunks).decode(encoding, errors="replace")

    def send_control(
        self,
        payload: bytes | bytearray | memoryview | str,
        *,
        frame_type: FrameType = FrameType.REQUEST,
    ) -> None:
        """Send bytes to the CONTROL channel."""

        self.send_frame(Channel.CONTROL, frame_type, payload)

    def send_log(
        self,
        payload: bytes | bytearray | memoryview | str,
        *,
        frame_type: FrameType = FrameType.DATA,
    ) -> None:
        """Send bytes to the LOG channel if host-to-device logs are needed."""

        self.send_frame(Channel.LOG, frame_type, payload)

    def send_frame(
        self,
        channel: Channel,
        frame_type: FrameType,
        payload: bytes | bytearray | memoryview | str = b"",
    ) -> None:
        """Encode and write one protocol frame to the COM port."""

        port = self._require_connection()
        packet = self.encode_frame(channel, frame_type, payload)
        port.write(packet)
        port.flush()

    def read_frame(self, timeout: float | None = None) -> Frame | None:
        """Read one decoded frame, returning ``None`` on timeout."""

        port = self._require_connection()
        deadline = None if timeout is None else monotonic() + timeout

        while True:
            frame = self._extract_frame()
            if frame is not None:
                return frame

            if deadline is not None and monotonic() >= deadline:
                return None

            waiting = port.in_waiting
            chunk = port.read(waiting or 1)
            if chunk:
                self._rx_buffer.extend(chunk)
            elif deadline is None:
                return None

    def iter_frames(self) -> Iterable[Frame]:
        """Yield frames while the COM port remains open."""

        while self.connected:
            frame = self.read_frame()
            if frame is not None:
                yield frame

    def dispatch(
        self,
        frame: Frame,
        *,
        on_control: Callable[[Frame], None] | None = None,
        on_log: Callable[[Frame], None] | None = None,
    ) -> None:
        """Route one decoded frame to a channel callback."""

        if frame.channel is Channel.CONTROL and on_control is not None:
            on_control(frame)
        elif frame.channel is Channel.LOG and on_log is not None:
            on_log(frame)

    @classmethod
    def encode_frame(
        cls,
        channel: Channel,
        frame_type: FrameType,
        payload: bytes | bytearray | memoryview | str = b"",
    ) -> bytes:
        """Build a wire-format protocol frame."""

        payload_bytes = cls._payload_bytes(payload)
        if len(payload_bytes) > cls.MAX_PAYLOAD:
            raise ValueError(
                f"DeskBridge payload is {len(payload_bytes)} bytes; "
                f"maximum is {cls.MAX_PAYLOAD}."
            )

        header = cls._HEADER.pack(
            cls.MAGIC,
            cls.VERSION,
            int(channel),
            int(frame_type),
            len(payload_bytes),
        )
        crc = cls.crc16_ccitt(header[len(cls.MAGIC) :] + payload_bytes)
        return header + payload_bytes + cls._CRC.pack(crc)

    @staticmethod
    def crc16_ccitt(data: bytes, initial: int = 0xFFFF) -> int:
        """Return CRC16-CCITT-FALSE for DeskBridge protocol bytes."""

        crc = initial
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
                crc &= 0xFFFF
        return crc

    def _extract_frame(self) -> Frame | None:
        """Decode one frame from the incremental serial receive buffer."""

        magic_index = self._rx_buffer.find(self.MAGIC)
        if magic_index < 0:
            self._keep_possible_magic_prefix()
            return None
        if magic_index:
            del self._rx_buffer[:magic_index]

        if len(self._rx_buffer) < self._HEADER.size:
            return None

        _, version, raw_channel, raw_type, payload_length = self._HEADER.unpack_from(
            self._rx_buffer
        )
        if version != self.VERSION:
            del self._rx_buffer[0]
            raise DeskBridgeProtocolError(
                f"Unsupported DeskBridge protocol version 0x{version:02X}."
            )
        if payload_length > self.max_payload:
            del self._rx_buffer[0]
            raise DeskBridgeProtocolError(
                f"DeskBridge payload length {payload_length} exceeds "
                f"configured maximum {self.max_payload}."
            )

        frame_length = self._HEADER.size + payload_length + self._CRC.size
        if len(self._rx_buffer) < frame_length:
            return None

        frame_bytes = bytes(self._rx_buffer[:frame_length])
        payload_start = self._HEADER.size
        payload_end = payload_start + payload_length
        payload = frame_bytes[payload_start:payload_end]
        expected_crc = self._CRC.unpack_from(frame_bytes, payload_end)[0]
        actual_crc = self.crc16_ccitt(frame_bytes[len(self.MAGIC) : payload_end])
        del self._rx_buffer[:frame_length]

        if actual_crc != expected_crc:
            raise DeskBridgeProtocolError(
                f"DeskBridge CRC mismatch: expected 0x{expected_crc:04X}, "
                f"got 0x{actual_crc:04X}."
            )

        try:
            channel = Channel(raw_channel)
            frame_type = FrameType(raw_type)
        except ValueError as exc:
            raise DeskBridgeProtocolError(
                f"Unknown DeskBridge channel/type 0x{raw_channel:02X}/"
                f"0x{raw_type:02X}."
            ) from exc

        return Frame(channel=channel, frame_type=frame_type, payload=payload)

    def _keep_possible_magic_prefix(self) -> None:
        if self._rx_buffer.endswith(self.MAGIC[:1]):
            self._rx_buffer[:] = self._rx_buffer[-1:]
        else:
            self._rx_buffer.clear()

    def _require_connection(self):
        if not self.connected:
            raise DeskBridgeUSBError("DeskBridge serial port is not connected.")
        return self._serial

    @staticmethod
    def _payload_bytes(payload: bytes | bytearray | memoryview | str) -> bytes:
        if isinstance(payload, str):
            return payload.encode("utf-8")
        return bytes(payload)

    @staticmethod
    def _require_pyserial() -> None:
        if serial is None or list_ports is None:
            raise DeskBridgeUSBError(
                "DeskBridge USB transport requires the pyserial package."
            )
