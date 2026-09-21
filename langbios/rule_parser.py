"""Fast, deterministic keyword/regex parser for common BIOS phrasings.

This is tried first. It only handles patterns it's confident about;
anything it can't confidently match returns None so the caller can fall
back to an LLM.
"""
from __future__ import annotations

import re

from .commands import Command

ON_WORDS = r"\b(?:turn on|enable|enabled|activate|on)\b"
OFF_WORDS = r"\b(?:turn off|disable|disabled|deactivate|off)\b"

# setting name -> list of phrases/aliases that refer to it
ALIASES: dict[str, list[str]] = {
    "secure_boot": ["secure boot", "secureboot"],
    "virtualization": ["virtualization", "virtualisation", "vt-x", "vtx", "svm", "amd-v"],
    "tpm": ["tpm", "trusted platform module"],
    "fast_boot": ["fast boot", "fastboot", "quick boot"],
    "xmp": ["xmp", "docp", "memory profile", "ram overclock"],
    "cpu_turbo": ["turbo", "cpu turbo", "boost clock", "turbo boost"],
    "power_profile": ["power profile", "power plan", "power mode"],
    "fan_profile": ["fan profile", "fan curve", "fan speed", "fan mode"],
    "boot_order": ["boot order", "boot priority", "boot sequence"],
}

BOOL_SETTINGS = {"secure_boot", "virtualization", "tpm", "fast_boot", "xmp", "cpu_turbo"}
ENUM_VALUES = {
    "power_profile": ["power_saver", "power saver", "balanced", "performance"],
    "fan_profile": ["silent", "standard", "performance", "full speed", "full_speed"],
}


def _find_setting(text: str) -> str | None:
    for setting, phrases in ALIASES.items():
        for phrase in phrases:
            if phrase in text:
                return setting
    return None


def parse(text: str) -> Command | None:
    t = text.strip().lower()

    if re.search(r"\b(list|show|dump)\b.*\bsettings?\b", t) or t in ("list", "settings", "status"):
        return Command(action="list", raw_text=text)

    if re.search(r"\breset\b.*\b(default|factory)", t):
        return Command(action="reset", raw_text=text)

    setting = _find_setting(t)
    if setting is None:
        return None

    # Enable/disable phrasing for boolean settings.
    if setting in BOOL_SETTINGS:
        if re.search(ON_WORDS, t):
            return Command(action="set", setting=setting, value=True, raw_text=text)
        if re.search(OFF_WORDS, t):
            return Command(action="set", setting=setting, value=False, raw_text=text)
        if re.search(r"\b(what'?s|what is|check|status of|is)\b", t):
            return Command(action="get", setting=setting, raw_text=text)
        return None

    # Enum settings: "set fan profile to silent" / "fan profile silent"
    if setting in ENUM_VALUES:
        for value in ENUM_VALUES[setting]:
            if value in t:
                return Command(action="set", setting=setting, value=value, raw_text=text)
        if re.search(r"\b(what'?s|what is|check|status of|is)\b", t):
            return Command(action="get", setting=setting, raw_text=text)
        return None

    # boot_order: "set boot order to ssd, usb, hdd"
    if setting == "boot_order":
        m = re.search(r"(?:to|:)\s*([a-z, ]+)$", t)
        if m:
            return Command(action="set", setting=setting, value=m.group(1), raw_text=text)
        if re.search(r"\b(what'?s|what is|show)\b", t):
            return Command(action="get", setting=setting, raw_text=text)
        return None

    return None
