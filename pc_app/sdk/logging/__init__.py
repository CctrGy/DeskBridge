"""Logging helpers for PC and firmware log separation."""

import logging


pc_logger = logging.getLogger("deskbridge.pc")
firmware_logger = logging.getLogger("deskbridge.firmware")

__all__ = ["firmware_logger", "pc_logger"]
