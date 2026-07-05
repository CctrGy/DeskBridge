"""Single-instance guard for the persistent backend."""

from __future__ import annotations

import ctypes
from ctypes import wintypes

ERROR_ALREADY_EXISTS = 183


class SingleInstance:
    """Windows mutex wrapper used to prevent multiple backends."""

    def __init__(self, name: str) -> None:
        self.name = name
        self.handle = None

    def acquire(self) -> bool:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateMutexW.argtypes = (wintypes.LPVOID, wintypes.BOOL, wintypes.LPCWSTR)
        kernel32.CreateMutexW.restype = wintypes.HANDLE
        self.handle = kernel32.CreateMutexW(None, False, self.name)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        return ctypes.get_last_error() != ERROR_ALREADY_EXISTS

    def release(self) -> None:
        if not self.handle:
            return
        ctypes.WinDLL("kernel32", use_last_error=True).CloseHandle(self.handle)
        self.handle = None

