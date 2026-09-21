"""ctypes bridge to the compiled native/build/langbios_native.dll.

Unlike bios_state.py (a mock JSON file), every call through here is a
real firmware/hardware operation: standard UEFI variable reads/writes,
WMI TPM queries, and (on Dell/HP/Lenovo) real vendor BIOS-setting writes.
See the top-level README for how that's actually possible from software.
"""
from __future__ import annotations

import ctypes
import json
from pathlib import Path

_DLL_PATH = Path(__file__).resolve().parent.parent / "native" / "build" / "langbios_native.dll"

_dll: ctypes.CDLL | None = None


class NativeUnavailableError(RuntimeError):
    pass


def _get_dll() -> ctypes.CDLL:
    global _dll
    if _dll is not None:
        return _dll
    if not _DLL_PATH.exists():
        raise NativeUnavailableError(
            f"Native library not found at {_DLL_PATH}. Build it with: "
            "powershell -ExecutionPolicy Bypass -File native/build.ps1"
        )
    dll = ctypes.CDLL(str(_DLL_PATH))
    dll.langbios_execute.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    dll.langbios_execute.restype = ctypes.c_int
    _dll = dll
    return dll


def execute(text: str) -> dict:
    """Runs `text` against real firmware via the native engine's own rule
    parser. Returns {"ok": bool, "message": str, "value": str}."""
    dll = _get_dll()
    buf_len = 8192
    buf = ctypes.create_string_buffer(buf_len)
    n = dll.langbios_execute(text.encode("utf-8"), buf, buf_len)
    if n < 0:
        buf_len = -n
        buf = ctypes.create_string_buffer(buf_len)
        n = dll.langbios_execute(text.encode("utf-8"), buf, buf_len)
        if n < 0:
            raise RuntimeError("native call failed unexpectedly even after resizing buffer")
    return json.loads(buf.raw[:n].decode("utf-8"))
