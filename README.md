# LangBIOS

[![CI](https://github.com/LAKSHYAJAIN16/LangBIOS/actions/workflows/ci.yml/badge.svg)](https://github.com/LAKSHYAJAIN16/LangBIOS/actions/workflows/ci.yml)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux-blue)](#building-the-native-layer)
[![Python 3.11+](https://img.shields.io/badge/python-3.11%2B-blue)](requirements-dev.txt)
[![Star on GitHub](https://img.shields.io/github/stars/LAKSHYAJAIN16/LangBIOS?style=social)](https://github.com/LAKSHYAJAIN16/LangBIOS)
[![Report a bug](https://img.shields.io/badge/report-a%20bug-red)](https://github.com/LAKSHYAJAIN16/LangBIOS/issues/new)

Talk to your computer's BIOS/UEFI settings in plain English — with a real hardware backend, not just a simulation.

```
langbios> enable secure boot
langbios> what's my fan profile
langbios> list settings
```

## How it's built

- `native/` (C++): the real engine. A rule-based parser, a dispatcher that routes each setting to the right real backend, and platform-specific firmware access (`native/src/windows/`, `native/src/linux/`). Compiles to `langbios_native.{dll,so}` (loaded by Python) and a standalone `langbios_cli` binary.
- `langbios/` (Python): `native_backend.py` is a `ctypes` bridge into the compiled library; `llm_parser.py` is an optional local-LLM fallback (via Ollama) for phrasing the native rule parser doesn't recognize; `cli.py` ties it together into a REPL.

This talks to **real firmware** — there is no simulated/mock mode. Reads are safe everywhere; writes change real NVRAM and need elevation (see below). Check the capability table further down for what's actually possible on *your* hardware before running a `set`/`enable`/`disable` command.

## Quick start

```bash
pip install -r requirements-dev.txt

# Build the native layer first (see "Building the native layer" below), then:
python -m langbios.cli "list settings"
python -m langbios.cli            # interactive REPL
```

Optional local LLM fallback for phrasing the rule-parser doesn't recognize: install [Ollama](https://ollama.com), `ollama pull llama3.2`, `ollama serve`. Use `--no-llm` to disable it.

## How real BIOS access actually works

There's no way to "just write C" that talks to firmware directly — user-mode code on any modern OS is blocked from touching hardware/NVRAM itself. Every real operation here goes through the same chain:

1. **Compiled native code** (`native/build/langbios_native.{dll,so}`) calls a specific, privileged **OS API** — `GetFirmwareEnvironmentVariableW`/WMI on Windows, a read/write on `efivarfs` or `/sys/class/firmware-attributes` on Linux.
2. That call traps into the **kernel**, the only thing allowed to talk to firmware directly.
3. For **boot order / Secure Boot** (standardized by the UEFI spec, so this part is identical in spirit on both OSes): the kernel forwards the request to the **UEFI runtime services** the firmware itself exposes at boot.
4. For **vendor settings** (Dell/HP/Lenovo): the OEM ships a **kernel-mode driver** that triggers an **SMI (System Management Interrupt)** — a hardware trap that pauses the OS and runs firmware code in a special CPU mode (SMM) to actually touch NVRAM. Only OEMs that built this exist; there's no way around that from software, by anyone.

Python never does this directly — `langbios/native_backend.py` is a `ctypes` bridge into the compiled native library, which is what actually makes the privileged calls.

## What's really possible, depending on your hardware

| Setting | Mechanism | Writable from software? | Works on |
|---|---|---|---|
| `boot_order` | Standard UEFI variables (`BootOrder`/`Boot####`) | **Yes** | Any UEFI machine (elevated) |
| `secure_boot` | Standard UEFI variable | No — firmware enforces this by design (authenticated-variable protection) | Any UEFI machine (elevated, read-only) |
| `tpm` | WMI (`Win32_Tpm`) / sysfs (`/sys/class/tpm`) | No — not an OS-writable setting anywhere | Any machine with a TPM (elevated) |
| `virtualization`, `fan_profile`, `power_profile`, `cpu_turbo`, `fast_boot`, `xmp` | Vendor WMI classes (Windows) / `/sys/class/firmware-attributes` (Linux) | Yes, **only if** the vendor stack is present | Dell / HP / Lenovo (Windows: their own WMI driver; Linux: `dell-wmi-sysman`/`think-lmi`/`hp-wmi` kernel module) |

On anything else (most DIY desktops, and this project's own dev machine — a Microsoft Surface), that last row honestly reports "no vendor BIOS management interface available" instead of pretending to succeed.

## Building the native layer

### Windows

Requires a C++20 compiler (clang++/LLVM or MSVC) with the Windows SDK.

```powershell
powershell -ExecutionPolicy Bypass -File native/build.ps1
```

Produces `native/build/langbios_native.dll` and `native/build/langbios_cli.exe`. The DLL is cross-compiled to match your **system Python's architecture** (edit the `--target` flag in `build.ps1` if yours isn't x86_64) since ctypes requires an exact match; the standalone CLI targets your host architecture directly.

### Linux

Requires g++ or clang++ with C++20 support.

```bash
./native/build.sh
```

Produces `native/build/langbios_native.so` and `native/build/langbios_cli`.

> **Honesty note:** the Linux backend (`native/src/linux/`) was written against the documented kernel ABIs (`efivarfs`, `/sys/class/firmware-attributes`, `/sys/class/tpm`) but developed and tested only on Windows — I don't have Linux hardware in this environment to verify it against real firmware. The Windows backend *has* been verified end-to-end against real hardware (see below). Please test the Linux write path carefully before trusting it, ideally starting with read-only commands.

## Elevation / permissions

Real firmware access needs elevated privileges, on both OSes, by design:

- **Windows**: `SeSystemEnvironmentPrivilege`, only ever granted to Administrators, must additionally be enabled in-process. Run from an elevated terminal, or enable `sudo` under Settings → Privacy & Security → For developers.
- **Linux**: root / `CAP_SYS_ADMIN`. Run with `sudo`.

Non-elevated runs still work and report exactly *why* an operation needs elevation, rather than failing silently.

## Safety notes

- **Boot order writes refuse partial reorders** — you must specify every current boot entry, in order, so a typo can never silently drop a boot device from the real `BootOrder` variable.
- **Secure Boot and TPM are always read-only** from every code path — the firmware itself enforces this; no version of this tool can bypass it, nor should it.
- **Vendor attribute writes** (Dell/HP/Lenovo) may only take effect after a reboot or a pending-changes commit, and can fail if a BIOS admin password is set — both are reported explicitly rather than assumed to have worked.

## Verified against real hardware

On this project's own dev machine (Microsoft Surface, Snapdragon/ARM64, no vendor BIOS management stack):

- Real elevated read + write round-trip of the actual `BootOrder` UEFI variable (read current order, write the same order back, read again to confirm) — genuinely persisted to firmware via `SetFirmwareEnvironmentVariableExW`.
- Real elevated read of Secure Boot state and WMI-based TPM query.
- Correct, honest non-elevated error messages (privilege/access-denied) when not run as Administrator.
- Correct "no vendor BIOS interface available" report, since this hardware has none.

## Tests

```bash
python -m pytest        # Python layer (LLM fallback parser) - runs in CI on Linux + Windows
```

The native layer has no automated test suite yet (COM/WMI and real firmware access don't sandbox well in CI) — it's been verified manually, as described above.

## More badges you can add

The badges at the top are real for *this* repo. If you fork this project, here are drop-in options for the rest — pick whichever fit, fill in your own usernames/links:

**Community / contact** (pick one or more):
```md
[![Discussions](https://img.shields.io/github/discussions/LAKSHYAJAIN16/LangBIOS)](https://github.com/LAKSHYAJAIN16/LangBIOS/discussions)
[![Discord](https://img.shields.io/discord/YOUR_SERVER_ID?logo=discord&logoColor=white)](https://discord.gg/YOUR_INVITE)
[![Docs](https://img.shields.io/badge/docs-README-informational)](#quick-start)
```

**Status** (this project has no live service to monitor, so pick whichever is honest for your fork):
```md
<!-- Real: your own CI build status (same idea as the badge already at the top) -->
[![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPO/actions)

<!-- Real, only if you actually run a status page for something this connects to -->
[![Status](https://img.shields.io/uptimerobot/status/YOUR_MONITOR_ID)](https://stats.uptimerobot.com/YOUR_PAGE_ID)

<!-- Cosmetic only - no monitoring behind it, use only if that's genuinely fine for your use case -->
[![Status](https://img.shields.io/badge/status-operational-brightgreen)](#)
```
