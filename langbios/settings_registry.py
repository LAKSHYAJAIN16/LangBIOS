"""Shared vocabulary of settings this project understands - BIOS/UEFI
firmware settings plus real OS-level hardware-adjacent settings (audio).

Used by the LLM fallback to describe what it can map requests onto.
`boot_order`'s `choices` here are illustrative categories only - real
hardware exposes actual boot entry descriptions (e.g. "Windows Boot
Manager"), not these generic names; see native/README notes in the
top-level README for why.
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass
class SettingSpec:
    name: str
    type: str  # "bool" | "enum" | "list" | "int"
    description: str
    choices: list[str] | None = None


SETTINGS: dict[str, SettingSpec] = {
    "secure_boot": SettingSpec("secure_boot", "bool", "UEFI Secure Boot"),
    "virtualization": SettingSpec("virtualization", "bool", "CPU virtualization (VT-x/AMD-V)"),
    "tpm": SettingSpec("tpm", "bool", "Trusted Platform Module"),
    "fast_boot": SettingSpec("fast_boot", "bool", "Fast Boot (skips POST checks)"),
    "xmp": SettingSpec("xmp", "bool", "Memory XMP/DOCP overclock profile"),
    "cpu_turbo": SettingSpec("cpu_turbo", "bool", "CPU turbo/boost clocks"),
    "power_profile": SettingSpec(
        "power_profile", "enum", "Overall power profile",
        choices=["power_saver", "balanced", "performance"],
    ),
    "fan_profile": SettingSpec(
        "fan_profile", "enum", "Fan curve profile",
        choices=["silent", "standard", "performance", "full_speed"],
    ),
    "boot_order": SettingSpec(
        "boot_order", "list", "Device boot priority order",
        choices=["ssd", "hdd", "usb", "network", "cdrom"],
    ),
    "volume": SettingSpec("volume", "int", "System audio output volume, 0-100"),
    "mute": SettingSpec("mute", "bool", "System audio mute state"),
}
