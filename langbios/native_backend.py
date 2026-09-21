"""ctypes bridge to the compiled native/build/langbios_native.{dll,so}.

Every call through here is a real firmware/hardware operation: standard
UEFI variable reads/writes,
WMI (Windows) or sysfs (Linux) TPM queries, and (on Dell/HP/Lenovo, or
any Linux box with a firmware-attributes driver bound) real vendor
BIOS-setting writes. See the top-level README for how that's actually
possible from software.
"""
from __future__ import annotations

import ctypes
import json
import platform
from pathlib import Path

_LIB_NAME = "langbios_native.dll" if platform.system() == "Windows" else "langbios_native.so"
_LIB_PATH = Path(__file__).resolve().parent.parent / "native" / "build" / _LIB_NAME
_BUILD_SCRIPT = "native/build.ps1" if platform.system() == "Windows" else "native/build.sh"

_dll: ctypes.CDLL | None = None


class NativeUnavailableError(RuntimeError):
    pass


def _get_dll() -> ctypes.CDLL:
    global _dll
    if _dll is not None:
        return _dll
    if not _LIB_PATH.exists():
        raise NativeUnavailableError(
            f"Native library not found at {_LIB_PATH}. Build it with: {_BUILD_SCRIPT}"
        )
    dll = ctypes.CDLL(str(_LIB_PATH))
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
