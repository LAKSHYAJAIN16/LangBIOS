"""Mock BIOS/UEFI settings store.

This simulates a machine's BIOS configuration so the natural-language layer
has something real to query and mutate, without needing actual firmware
access (which isn't available from a running OS in a portable way).
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

DEFAULT_STATE_PATH = Path(__file__).parent / "data" / "bios_state.json"


@dataclass
class SettingSpec:
    name: str
    type: str  # "bool" | "enum" | "list" | "int"
    description: str
    choices: list[str] | None = None
    min_value: int | None = None
    max_value: int | None = None


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
}

DEFAULTS: dict[str, Any] = {
    "secure_boot": True,
    "virtualization": False,
    "tpm": True,
    "fast_boot": True,
    "xmp": False,
    "cpu_turbo": True,
    "power_profile": "balanced",
    "fan_profile": "standard",
    "boot_order": ["ssd", "usb", "network", "hdd", "cdrom"],
}


class UnknownSettingError(ValueError):
    pass


class InvalidValueError(ValueError):
    pass


@dataclass
class BiosState:
    path: Path = field(default_factory=lambda: DEFAULT_STATE_PATH)
    values: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def load(cls, path: Path | None = None) -> "BiosState":
        p = path or DEFAULT_STATE_PATH
        if p.exists():
            values = json.loads(p.read_text())
        else:
            values = dict(DEFAULTS)
        return cls(path=p, values=values)

    def save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self.values, indent=2))

    def get(self, name: str) -> Any:
        if name not in SETTINGS:
            raise UnknownSettingError(name)
        return self.values.get(name, DEFAULTS.get(name))

    def set(self, name: str, value: Any) -> Any:
        if name not in SETTINGS:
            raise UnknownSettingError(name)
        spec = SETTINGS[name]
        coerced = self._coerce(spec, value)
        self.values[name] = coerced
        self.save()
        return coerced

    def reset(self) -> None:
        self.values = dict(DEFAULTS)
        self.save()

    def all(self) -> dict[str, Any]:
        return {name: self.get(name) for name in SETTINGS}

    def _coerce(self, spec: SettingSpec, value: Any) -> Any:
        if spec.type == "bool":
            if isinstance(value, bool):
                return value
            if isinstance(value, str):
                if value.lower() in ("on", "true", "enable", "enabled", "yes", "1"):
                    return True
                if value.lower() in ("off", "false", "disable", "disabled", "no", "0"):
                    return False
            raise InvalidValueError(f"{spec.name} expects on/off, got {value!r}")
        if spec.type == "enum":
            v = str(value).lower().replace(" ", "_").replace("-", "_")
            if v not in spec.choices:
                raise InvalidValueError(
                    f"{spec.name} expects one of {spec.choices}, got {value!r}"
                )
            return v
        if spec.type == "list":
            if isinstance(value, list):
                items = [str(v).lower() for v in value]
            else:
                items = [s.strip().lower() for s in str(value).split(",")]
            for item in items:
                if item not in spec.choices:
                    raise InvalidValueError(
                        f"{spec.name} entries must be one of {spec.choices}, got {item!r}"
                    )
            return items
        raise InvalidValueError(f"unsupported setting type {spec.type!r}")
