"""Shared types passed between the LLM parser and the interpreter."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Literal

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
