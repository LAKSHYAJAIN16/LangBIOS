"""Top-level entry point: text in, Result out.

Every call goes through the native engine (native/build/langbios_native.*
via ctypes) against real firmware. If its own rule parser can't
confidently match, falls back to a local LLM via Ollama, converting the
result back into a canonical phrase the native engine can execute. If
the LLM is unavailable too, reports that clearly instead of failing
silently.
"""
from __future__ import annotations

from .commands import Command, Result
from . import llm_parser, native_backend

_NOT_UNDERSTOOD_MARKER = "I didn't understand that"


def _command_to_phrase(cmd: Command) -> str:
    """Turns a structured Command (from the LLM fallback) back into a short
    canonical phrase the native engine's own rule parser can match."""
    name = (cmd.setting or "").replace("_", " ")
    if cmd.action == "list":
        return "list settings"
    if cmd.action == "reset":
        return "reset to factory defaults"
    if cmd.action == "get":
        return f"what's my {name}"
    if cmd.action == "set":
        if isinstance(cmd.value, bool):
            return f"{'enable' if cmd.value else 'disable'} {name}"
        return f"set {name} to {cmd.value}"
    return cmd.raw_text


def interpret(text: str, use_llm_fallback: bool = True) -> Result:
    try:
        raw = native_backend.execute(text)
    except native_backend.NativeUnavailableError as e:
        return Result(False, str(e))

    if raw["ok"] or _NOT_UNDERSTOOD_MARKER not in raw["message"]:
        return Result(raw["ok"], raw["message"], raw.get("value"))

    if not use_llm_fallback:
        return Result(False, raw["message"])

    try:
        cmd = llm_parser.parse(text)
    except llm_parser.LLMUnavailableError as e:
        return Result(
            False,
            "The native engine didn't recognize that phrasing, and the local "
            f"LLM fallback isn't reachable ({e}). Try rephrasing, e.g. "
            "'enable secure boot'.",
        )

    if cmd.action == "unknown":
        return Result(
            False,
            "Even the LLM fallback couldn't confidently map that to a BIOS "
            "setting. Try being more specific, e.g. 'set fan profile to silent'.",
        )

    raw2 = native_backend.execute(_command_to_phrase(cmd))
    return Result(raw2["ok"], raw2["message"], raw2.get("value"))
