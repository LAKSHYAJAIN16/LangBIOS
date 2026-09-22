# LangBIOS

[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux-blue)](#building-the-native-layer)
[![Python 3.11+](https://img.shields.io/badge/python-3.11%2B-blue)](langbios/)
[![Star on GitHub](https://img.shields.io/github/stars/LAKSHYAJAIN16/LangBIOS?style=social)](https://github.com/LAKSHYAJAIN16/LangBIOS)
[![Report a bug](https://img.shields.io/badge/report-a%20bug-red)](https://github.com/LAKSHYAJAIN16/LangBIOS/issues/new)

Talk to your computer's BIOS/UEFI settings in plain English, with a real hardware backend, not just a simulation.

```
langbios> enable secure boot
langbios> what's my fan profile
langbios> list settings
```

## How it's built

- `native/` (C++): the real engine. A rule-based parser, a dispatcher that routes each setting to the right real backend, platform-specific firmware access (`native/src/windows/`, `native/src/linux/`), and a bundled local semantic-matching fallback (`native/src/llm_fallback.cpp`) for phrasing the rule parser misses. Compiles to `langbios_native.{dll,so}` (loaded by Python) and a standalone `langbios_cli` binary.
- `langbios/` (Python): `native_backend.py` is a `ctypes` bridge into the compiled library; `llm_parser.py` is an *additional* optional local-LLM fallback (via Ollama, for genuinely open-ended phrasing a fixed intent set can't cover) layered on top; `cli.py` ties it together into a REPL.

This talks to **real firmware**: there is no simulated/mock mode. Reads are safe everywhere; writes change real NVRAM and need elevation (see below). Check the capability table further down for what's actually possible on *your* hardware before running a `set`/`enable`/`disable` command.

## Quick start (Windows)

Build and run the installer (`installer/LangBIOS.iss`, requires [Inno Setup](https://jrsoftware.org/isinfo.php)):

```powershell
powershell -ExecutionPolicy Bypass -File native/fetch-llm.ps1   # bundled local NL fallback, ~76MB, optional but recommended
powershell -ExecutionPolicy Bypass -File native/build.ps1
& "<Inno Setup install dir>\ISCC.exe" installer\LangBIOS.iss
installer\dist\LangBIOS-Setup.exe
```

That installs `langbios.exe` to `%LOCALAPPDATA%\Programs\LangBIOS` and adds it to your PATH (no admin needed to install). Open a **new** terminal and just run:

```powershell
langbios "list settings"
langbios                          # interactive REPL
```

No Python required for this path, and no API key or Ollama install either: phrasing the rule parser doesn't recognize falls back to a small bundled semantic-matching model (see "How the local fallback works" below), running fully offline, adding only ~46MB to the installer. See "Building the native layer" below for Linux, or to build the native pieces manually.

## Quick start (from source, any platform)

```bash
git clone https://github.com/LAKSHYAJAIN16/LangBIOS.git
cd LangBIOS

# Build the native layer first (see "Building the native layer" below), then:
python -m langbios.cli "list settings"
python -m langbios.cli            # interactive REPL
```

Optional local LLM fallback for phrasing the rule-parser doesn't recognize: install [Ollama](https://ollama.com), `ollama pull llama3.2`, `ollama serve`. Use `--no-llm` to disable it.

## How real BIOS access actually works

There's no way to "just write C" that talks to firmware directly: user-mode code on any modern OS is blocked from touching hardware/NVRAM itself. Every real operation here goes through the same chain:

1. **Compiled native code** (`native/build/langbios_native.{dll,so}`) calls a specific, privileged **OS API**: `GetFirmwareEnvironmentVariableW`/WMI on Windows, a read/write on `efivarfs` or `/sys/class/firmware-attributes` on Linux.
2. That call traps into the **kernel**, the only thing allowed to talk to firmware directly.
3. For **boot order / Secure Boot** (standardized by the UEFI spec, so this part is identical in spirit on both OSes): the kernel forwards the request to the **UEFI runtime services** the firmware itself exposes at boot.
4. For **vendor settings** (Dell/HP/Lenovo): the OEM ships a **kernel-mode driver** that triggers an **SMI (System Management Interrupt)**, a hardware trap that pauses the OS and runs firmware code in a special CPU mode (SMM) to actually touch NVRAM. Only OEMs that built this exist; there's no way around that from software, by anyone.

Python never does this directly. `langbios/native_backend.py` is a `ctypes` bridge into the compiled native library, which is what actually makes the privileged calls.

## How the local fallback works

Still a real small language model doing real neural inference, not string matching - `bge-small-en-v1.5`, a 33M-parameter transformer, genuinely understands phrasing that was never in its examples (`"crank up the fans"` correctly resolves `fan_profile=performance` by meaning, not keyword overlap). The difference is *what kind* of model: mapping one sentence onto one of a few dozen known settings/actions is a **classification** problem, not open-ended text generation, so instead of a generative LLM (which needs a large vocabulary-sized output layer just to be able to write text at all), the fallback is an **encoder** model - it turns text into a vector capturing its meaning, and the nearest known example wins by cosine similarity:

1. `native/data/canonical_intents.json` has ~94 curated example phrases covering every setting/action (e.g. `"crank up the fans"` → `set fan_profile=performance`).
2. `native/embed-intents.ps1` embeds all of them **once, offline**, using the bundled model, and writes the vectors to `native/data/canonical_embeddings.bin` (~143KB, committed to git - small enough, unlike the model itself).
3. At runtime, `native/src/llm_fallback.cpp` starts `llama-server.exe` in `--embedding` mode (bundled, ~76MB total with `bge-small-en-v1.5`), embeds only the user's input, and compares it against the precomputed vectors. Below a similarity threshold, it reports "didn't understand" instead of guessing.

This is deterministic and can't hallucinate an invalid setting the way a generative model could - it can only ever return one of the known canonical intents. It's also ~90% smaller than the generative approach this project shipped with initially (a 490MB Qwen2.5-0.5B-Instruct model), while being measurably *more* reliable on this project's own test phrases (see `native/fetch-llm.ps1` for the head-to-head notes).

## What's really possible, depending on your hardware

| Setting | Mechanism | Writable from software? | Works on |
|---|---|---|---|
| `boot_order` | Standard UEFI variables (`BootOrder`/`Boot####`) | **Yes** | Any UEFI machine (elevated) |
| `secure_boot` | Standard UEFI variable | No, firmware enforces this by design (authenticated-variable protection) | Any UEFI machine (elevated, read-only) |
| `tpm` | WMI (`Win32_Tpm`) / sysfs (`/sys/class/tpm`) | No, not an OS-writable setting anywhere | Any machine with a TPM (elevated) |
| `virtualization`, `fan_profile`, `power_profile`, `cpu_turbo`, `fast_boot`, `xmp` | Vendor WMI classes (Windows) / `/sys/class/firmware-attributes` (Linux) | Yes, **only if** the vendor stack is present | Dell / HP / Lenovo (Windows: their own WMI driver; Linux: `dell-wmi-sysman`/`think-lmi`/`hp-wmi` kernel module) |

On anything else (most DIY desktops, and this project's own dev machine, a Microsoft Surface), that last row honestly reports "no vendor BIOS management interface available" instead of pretending to succeed.

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

> **Honesty note:** the Linux backend (`native/src/linux/`) was written against the documented kernel ABIs (`efivarfs`, `/sys/class/firmware-attributes`, `/sys/class/tpm`) but developed and tested only on Windows, with no Linux hardware in this environment to verify it against real firmware. The Windows backend *has* been verified end-to-end against real hardware (see below). Please test the Linux write path carefully before trusting it, ideally starting with read-only commands.

## Elevation / permissions

Real firmware access needs elevated privileges, on both OSes, by design:

- **Windows**: `SeSystemEnvironmentPrivilege`, only ever granted to Administrators, must additionally be enabled in-process. Run from an elevated terminal, or enable `sudo` under Settings → Privacy & Security → For developers.
- **Linux**: root / `CAP_SYS_ADMIN`. Run with `sudo`.

Non-elevated runs still work and report exactly *why* an operation needs elevation, rather than failing silently.

## Safety notes

- **Boot order writes refuse partial reorders.** You must specify every current boot entry, in order, so a typo can never silently drop a boot device from the real `BootOrder` variable.
- **Secure Boot and TPM are always read-only** from every code path. The firmware itself enforces this; no version of this tool can bypass it, nor should it.
- **Vendor attribute writes** (Dell/HP/Lenovo) may only take effect after a reboot or a pending-changes commit, and can fail if a BIOS admin password is set; both are reported explicitly rather than assumed to have worked.

## Verified against real hardware

On this project's own dev machine (Microsoft Surface, Snapdragon/ARM64, no vendor BIOS management stack):

- Real elevated read + write round-trip of the actual `BootOrder` UEFI variable (read current order, write the same order back, read again to confirm), genuinely persisted to firmware via `SetFirmwareEnvironmentVariableExW`.
- Real elevated read of Secure Boot state and WMI-based TPM query.
- Correct, honest non-elevated error messages (privilege/access-denied) when not run as Administrator.
- Correct "no vendor BIOS interface available" report, since this hardware has none.

## Tests

Tests live on the `tests` branch, not `main`. Check it out separately:

```bash
git checkout tests -- tests/ requirements-dev.txt
pip install -r requirements-dev.txt
python -m pytest
```

## More badges you can add

The badges at the top are real for *this* repo. If you fork this project, here are drop-in options for the rest, pick whichever fit and fill in your own usernames/links:

**Community / contact** (pick one or more):
```md
[![Discussions](https://img.shields.io/github/discussions/LAKSHYAJAIN16/LangBIOS)](https://github.com/LAKSHYAJAIN16/LangBIOS/discussions)
[![Discord](https://img.shields.io/discord/YOUR_SERVER_ID?logo=discord&logoColor=white)](https://discord.gg/YOUR_INVITE)
[![Docs](https://img.shields.io/badge/docs-README-informational)](#quick-start)
```

**Status** (this project has no live service to monitor, so pick whichever is honest for your fork):
```md
<!-- Real: your own CI build status -->
[![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPO/actions)

<!-- Real, only if you actually run a status page for something this connects to -->
[![Status](https://img.shields.io/uptimerobot/status/YOUR_MONITOR_ID)](https://stats.uptimerobot.com/YOUR_PAGE_ID)

<!-- Cosmetic only, no monitoring behind it, use only if that's genuinely fine for your use case -->
[![Status](https://img.shields.io/badge/status-operational-brightgreen)](#)
```
