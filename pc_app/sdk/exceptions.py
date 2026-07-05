"""SDK exception hierarchy."""


class DeskBridgeError(Exception):
    """Base SDK error."""


class TransportError(DeskBridgeError):
    """Raised when the serial transport cannot connect, read, or write."""


class DeviceTimeoutError(DeskBridgeError):
    """Raised when a device response is not received in time."""


class DeviceCommandError(DeskBridgeError):
    """Raised when firmware replies with an ERR line."""

    def __init__(self, detail: str = "", *, code: str | None = None, raw: str | None = None) -> None:
        self.code = code
        self.detail = detail
        self.raw = raw
        message = f"{code} {detail}".strip() if code else detail
        super().__init__(message or raw or "device command failed")
