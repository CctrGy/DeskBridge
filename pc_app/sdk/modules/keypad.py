"""Keypad peripheral commands."""

from __future__ import annotations

from sdk.result import CommandResult

from .base import DeviceModule


class KeypadModule(DeviceModule):
    actions = {
        "none": 0,
        "mute": 1,
        "media_mute": 1,
        "volume_up": 2,
        "vol_up": 2,
        "volume_down": 3,
        "vol_down": 3,
        "play": 4,
        "play_pause": 4,
        "next": 5,
        "media_next": 5,
        "previous": 6,
        "prev": 6,
        "media_previous": 6,
        "f13": 7,
        "f14": 8,
        "f15": 9,
    }

    def get(self) -> CommandResult:
        return self.command_result("keypad.get", "keypad?")

    def action_list(self) -> CommandResult:
        return self.command_result("keypad.action.list", "keypad action list")

    def set_action(self, button: int | str, action: int | str) -> CommandResult:
        button_index = int(button)
        action_value = self._action_value(action)
        return self.command_result("keypad.action.set", f"keypad action {button_index} {action_value}")

    def _action_value(self, action: int | str) -> int:
        if isinstance(action, int):
            return action
        text = str(action).strip().lower()
        if text.isdigit():
            return int(text)
        if text not in self.actions:
            raise ValueError(f"unknown keypad action: {action}")
        return self.actions[text]
