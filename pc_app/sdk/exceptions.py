"""SDK exception hierarchy."""


class DeskBridgeError(Exception):
    """Base SDK error."""


class TransportError(DeskBridgeError):
    """Raised when the serial transport cannot connect, read, or write."""


class DeviceTimeoutError(DeskBridgeError):
    """Raised when a device response is not received in time."""


class DeviceCommandError(DeskBridgeError):
    """Raised when firmware replies with an ERR line."""
