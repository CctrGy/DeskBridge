"""Shared helpers for SDK modules."""

from __future__ import annotations

from sdk.result import CommandResult


def replace_result(
    result: CommandResult,
    *,
    command_name: str | None = None,
    data: dict[str, object] | None = None,
    message: str | None = None,
    error: str | None = None,
) -> CommandResult:
    return CommandResult(
        ok=result.ok if error is None else False,
        command_name=command_name or result.command_name,
        raw_command=result.raw_command,
        raw_response=result.raw_response,
        data=data if data is not None else result.data,
        message=message if message is not None else result.message,
        error=error if error is not None else result.error,
    )


def require_args(command_name: str, raw_command: str, ok: bool, usage: str) -> CommandResult | None:
    if ok:
        return None
    return CommandResult(ok=False, command_name=command_name, raw_command=raw_command, error=usage)


def value_for(result: CommandResult, key: str) -> object:
    return (result.data or {}).get(key)


def dict_for(result: CommandResult, key: str) -> dict[str, object]:
    value = value_for(result, key)
    return value if isinstance(value, dict) else {}
