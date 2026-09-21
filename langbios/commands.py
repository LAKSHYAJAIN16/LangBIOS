"""Structured commands that either parser (rule-based or LLM) produces,
and a single executor that runs them against a BiosState."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Literal

from .bios_state import BiosState, InvalidValueError, UnknownSettingError

Action = Literal["get", "set", "list", "reset", "unknown"]


@dataclass
class Command:
    action: Action
    setting: str | None = None
    value: Any = None
    raw_text: str = ""
    confidence: float = 1.0
    source: str = "rule"  # "rule" | "llm"


@dataclass
class Result:
    ok: bool
    message: str
    data: Any = None


def execute(cmd: Command, state: BiosState) -> Result:
    try:
        if cmd.action == "list":
            values = state.all()
            lines = [f"  {k}: {v}" for k, v in values.items()]
            return Result(True, "Current BIOS settings:\n" + "\n".join(lines), values)

        if cmd.action == "reset":
            state.reset()
            return Result(True, "All settings reset to factory defaults.")

        if cmd.action == "get":
            if not cmd.setting:
                return Result(False, "I didn't catch which setting you mean.")
            value = state.get(cmd.setting)
            return Result(True, f"{cmd.setting} = {value}", value)

        if cmd.action == "set":
            if not cmd.setting:
                return Result(False, "I didn't catch which setting you mean.")
            new_value = state.set(cmd.setting, cmd.value)
            return Result(True, f"Set {cmd.setting} = {new_value}", new_value)

        return Result(
            False,
            "I didn't understand that. Try things like "
            "'enable secure boot', 'what's my fan profile', or 'list settings'.",
        )
    except UnknownSettingError as e:
        return Result(False, f"Unknown setting: {e}")
    except InvalidValueError as e:
        return Result(False, str(e))
