"""Top-level entry point: text in, Result out.

Tries the rule-based parser first (fast, free, deterministic). If it
can't confidently match, falls back to a local LLM via Ollama. If the
LLM is unavailable too, reports that clearly instead of failing silently.
"""
from __future__ import annotations

from .bios_state import BiosState
from .commands import Command, Result, execute
from . import llm_parser, rule_parser


def interpret(text: str, state: BiosState, use_llm_fallback: bool = True) -> Result:
    cmd = rule_parser.parse(text)
    if cmd is not None:
        return execute(cmd, state)

    if not use_llm_fallback:
        return Result(
            False,
            "I didn't understand that (no LLM fallback enabled). Try things like "
            "'enable secure boot' or 'list settings'.",
        )

    try:
        cmd = llm_parser.parse(text)
    except llm_parser.LLMUnavailableError as e:
        return Result(
            False,
            "I didn't recognize that phrasing, and the local LLM fallback "
            f"isn't reachable ({e}). Try rephrasing, e.g. 'enable secure boot'.",
        )

    if cmd.action == "unknown":
        return Result(
            False,
            "Even the LLM fallback couldn't confidently map that to a BIOS "
            "setting. Try being more specific, e.g. 'set fan profile to silent'.",
        )
    return execute(cmd, state)
