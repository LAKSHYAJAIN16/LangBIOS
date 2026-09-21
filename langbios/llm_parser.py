"""Local LLM fallback for phrasings the rule parser can't confidently handle.

Uses Ollama (https://ollama.com) running locally so the prototype stays
free, offline-capable, and doesn't require any API keys. If Ollama isn't
running or the requested model isn't pulled, `parse()` raises
LLMUnavailableError so the caller can degrade gracefully.
"""
from __future__ import annotations

import json
import os
import urllib.error
import urllib.request

from .settings_registry import SETTINGS
from .commands import Command

OLLAMA_URL = os.environ.get("LANGBIOS_OLLAMA_URL", "http://localhost:11434")
OLLAMA_MODEL = os.environ.get("LANGBIOS_OLLAMA_MODEL", "llama3.2")
TIMEOUT_SECONDS = float(os.environ.get("LANGBIOS_OLLAMA_TIMEOUT", "10"))


class LLMUnavailableError(RuntimeError):
    pass


def _system_prompt() -> str:
    setting_lines = "\n".join(
        f"- {name} ({spec.type}"
        + (f", choices: {spec.choices}" if spec.choices else "")
        + f"): {spec.description}"
        for name, spec in SETTINGS.items()
    )
    return f"""You translate a user's natural-language request about their computer's
BIOS/UEFI settings into a single strict JSON object, and nothing else.

Available settings:
{setting_lines}

Respond with ONLY a JSON object of this shape:
{{"action": "get"|"set"|"list"|"reset"|"unknown", "setting": "<name or null>", "value": <value or null>}}

Rules:
- action "list" means the user wants to see all current settings.
- action "reset" means the user wants factory defaults restored.
- action "get" means the user is asking the current value of one setting.
- action "set" means the user wants to change one setting; include "value".
- For bool settings use JSON true/false. For enum settings use one of the listed choices.
- If you cannot confidently map the request, respond with action "unknown".
- Output JSON only. No explanation, no markdown fences.
"""


def _call_ollama(prompt: str) -> str:
    payload = json.dumps(
        {
            "model": OLLAMA_MODEL,
            "prompt": prompt,
            "system": _system_prompt(),
            "stream": False,
            "format": "json",
        }
    ).encode("utf-8")
    req = urllib.request.Request(
        f"{OLLAMA_URL}/api/generate",
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT_SECONDS) as resp:
            body = json.loads(resp.read().decode("utf-8"))
    except (urllib.error.URLError, TimeoutError, OSError) as e:
        raise LLMUnavailableError(
            f"Could not reach Ollama at {OLLAMA_URL} (is `ollama serve` running "
            f"and has `{OLLAMA_MODEL}` been pulled?): {e}"
        ) from e
    return body.get("response", "")


def parse(text: str) -> Command:
    """Ask the local LLM to interpret `text`. Raises LLMUnavailableError if
    Ollama can't be reached; returns a Command with action="unknown" if the
    model itself couldn't map the request."""
    raw = _call_ollama(text)
    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError:
        return Command(action="unknown", raw_text=text, confidence=0.0, source="llm")

    action = parsed.get("action", "unknown")
    setting = parsed.get("setting")
    value = parsed.get("value")
    if action not in ("get", "set", "list", "reset", "unknown"):
        action = "unknown"
    if setting is not None and setting not in SETTINGS:
        action = "unknown"

    return Command(
        action=action,
        setting=setting,
        value=value,
        raw_text=text,
        confidence=0.7,
        source="llm",
    )
